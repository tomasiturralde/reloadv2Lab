#include "crow.h"

#include <iostream>
#include <algorithm>
#include <fstream>
#include <chrono>
#include <unistd.h>
#include <thread>

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

#include <System.h>
#include <Viewer.h>
#include <Tracking.h>
#include <FrameDrawer.h>
#include <Serializer.h>
#include <Video.h>

#include <base64.h>

#include <nlohmann/json.hpp>

using namespace std;
using namespace cv;
using json = nlohmann::json;


bool loadedMap = false;
bool bogusImage = false;
ORB_SLAM2::System *Sistema;


void loadMap(const string &rutaMapa);
Mat operate(const Mat &image, const string &mapRoute);
string getVectorAsString(const Mat &vector);
Mat loadInitialMatrix(const Mat &image, const string &mapRoute);
Mat calculateLocation(const Mat &initialMatrix, const Mat &relocMatrix, const Mat &initialVector, double factor);


int main(int argc, char **argv) {

    std::ifstream ifs("app.settings.json");
    json configuration = json::parse(ifs);

    const string mapRoute = configuration["map"];
    const string initialImageLocation = configuration["initialImage"];
    const double meterFactor = configuration["meterFactor"];
    const string rutaConfiguracion = configuration["webcam"];	// Configuración por defecto, para webcam.


    cout	<< "Iniciando ORB-SLAM.  Línea de comando:" << endl;

    // Parámetros de la línea de comando
    char* rutaVideo = nullptr;

    ORB_SLAM2::System SLAM(configuration["vocabulary"], rutaConfiguracion,ORB_SLAM2::System::MONOCULAR,true);

    // Puntero global al sistema singleton
    Sistema = &SLAM;

    ORB_SLAM2::Video video;
    new thread(&ORB_SLAM2::Video::Run, &video);
//
    switch(argc){
        case 1:	// Sin argumentos, webcam por defecto y webcam.yaml como configuración
            cout << "Sin argumentos, webcam con esta configuración: " << rutaConfiguracion << endl;
            break;
        default: break;
    }

    // init crow
    crow::SimpleApp app;

    CROW_ROUTE(app, "/image")
            .methods("POST"_method)
                    ([configuration, mapRoute, meterFactor](const crow::request &req) {
                        crow::response resp;
                        resp = crow::response(200);
                        auto body = crow::json::load(req.body);
                        if (!body) {
                            cout << "Imagen no pudo ser recibida" << endl;
                            return crow::response(400);
                        }
                        const crow::json::detail::r_string &uri_string = body["uri"].s();
                        std::vector<BYTE> decodedData = base64_decode(uri_string);
                        cv::Mat img = cv::imdecode(decodedData, cv::IMREAD_UNCHANGED);

                        namedWindow( "Display window", cv::WINDOW_NORMAL );   // Create a window for display.
                        cv::imshow( "Display window", img );               // Show our image inside it.

                        cv::Mat initialMatrix;
                        initialMatrix = loadInitialMatrix(img, mapRoute);
                        float x = initialMatrix.at<float>(0, 0);
                        float y = initialMatrix.at<float>(1, 0);
                        float z = initialMatrix.at<float>(2, 0);
                        if (x == 0 && y == -1 && z == 0) {
                            cerr << "Can't initialize properly" << endl;
                            cerr << "The system can't localize the initial point" << endl;
                            exit(1);
                        }
                        cv::Mat initialVector = initialMatrix * initialMatrix.col(3);

                        Mat relocMatrix = operate(img, mapRoute);
                        cout << relocMatrix << endl;
                        Mat displacementVector;
                        if (!bogusImage) {
                            displacementVector = calculateLocation(initialMatrix, relocMatrix, initialVector, meterFactor);
                            displacementVector.at<float>(1, 0) = 1;
                        } else {
                            displacementVector = relocMatrix;
                        }
                        string message = getVectorAsString(displacementVector);
                        cout << "The resultant vector is: " + message << endl;
                        resp.body = message;

                        return resp;
                    });
    app.port(9000).multithreaded().run();
}

Mat operate(const Mat &image, const string &mapRoute) {

    bool operate = true;
    Mat result;
    int maxFrames = 20;
    while (operate && maxFrames != 0) {
        // Pass the image to the SLAM system
        (*Sistema).TrackMonocular(image, 1);

        if ((*Sistema).mpTracker->mState == 2 && (*Sistema).mpTracker->mbOnlyTracking) {
            result = (*Sistema).mpTracker->mCurrentFrame.mTcw.inv();
            operate = false;
            bogusImage = false;
        }

        // Ver si hay señal para cargar el mapa, que debe hacerse desde este thread
        if (!loadedMap) {
            cout << "Cargando mapa" << endl;
            loadMap(mapRoute);
            loadedMap = true;
        }
        maxFrames--;
    }
    if (maxFrames == 0) {
        result = Mat::zeros(3, 1, CV_32F);
        result.at<float>(0, 0) = 0;
        result.at<float>(1, 0) = -1;
        result.at<float>(2, 0) = 0;
        bogusImage = true;
    }
    return result;
}

cv::Mat calculateLocation(const Mat &initialMatrix, const Mat &relocMatrix,
                          const Mat &initialVector, const double factor) {
    Mat relocVector = initialMatrix * (relocMatrix.col(3));
    Mat resultantVector = relocVector - initialVector;
    cout << resultantVector << endl;
    resultantVector = resultantVector * factor;
    return resultantVector;
}

string getVectorAsString(const Mat &vector) {
    float x = vector.at<float>(0, 0);
    float y = vector.at<float>(1, 0);
    float z = vector.at<float>(2, 0);
    return to_string(x) + " " + to_string(y) + " " + to_string(z);
}

Mat loadInitialMatrix(const cv::Mat &image, const string &mapRoute) {
    // Mat image = imread(initialImageLocation, CV_LOAD_IMAGE_GRAYSCALE);
    Mat result = operate(image, mapRoute);
    cout << result << endl;
    return result;
}

void loadMap(const string &rutaMapa) {
    (*Sistema).mpViewer->cargarMapa = false;

    (*Sistema).mpTracker->mState = ORB_SLAM2::Tracking::NOT_INITIALIZED;
    // El reset subsiguiente requiere que LocalMapping no esté pausado.
    (*Sistema).mpLocalMapper->Release();

    // Limpia el mapa de todos los singletons
    (*Sistema).mpTracker->Reset();
    // En este punto el sistema está reseteado.

    // Espera a que se detenga LocalMapping y  Viewer
    (*Sistema).mpLocalMapper->RequestStop();
    (*Sistema).mpViewer->RequestStop();

    //FILE *f = popen("zenity --file-selection", "r");
    //fgets(charchivo, 1024, f);

    while (!(*Sistema).mpLocalMapper->isStopped()) usleep(1000);
    while (!(*Sistema).mpViewer->isStopped()) usleep(1000);

    //std::string nombreArchivo(charchivo);
    //nombreArchivo.pop_back();	// Quita el \n final
    cout << "Abriendo archivo " << rutaMapa << endl;
    (*Sistema).serializer->mapLoad(rutaMapa);
    cout << "Mapa cargado." << endl;


    (*Sistema).mpTracker->mState = ORB_SLAM2::Tracking::LOST;

    // Reactiva viewer.  No reactiva el mapeador, pues el sistema queda en sólo tracking luego de cargar.
    (*Sistema).mpViewer->Release();

    // Por las dudas, es lo que hace Tracking luego que el estado pase a LOST.
    // Como tiene un mutex, por las dudas lo invoco después de viewer.release.
    (*Sistema).mpFrameDrawer->Update((*Sistema).mpTracker);
    (*Sistema).mpTracker->mbOnlyTracking = true;
}
