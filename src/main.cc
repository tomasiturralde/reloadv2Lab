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
#include <cmath>

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

Mat loadMatrix(const string &imageLocation, const string &mapRoute);

Mat calculateLocation(const Mat &initialMatrix, const Mat &relocMatrix, const Mat &initialVector, double factor);

Mat displace(const Mat &relocMatrix);

Mat substractTranslations(const Mat &relocMatrix, const Mat &displacedRelocMatrix);

float getAngle(Mat mat);

int main(int argc, char **argv) {

    std::ifstream ifs("app.settings.json");
    json configuration = json::parse(ifs);

    const string mapRoute = configuration["map"];
    const string initialImageLocation = configuration["initialImage"];
    const double meterFactor = configuration["meterFactor"];
    const string rutaConfiguracion = configuration["webcam"];    // Configuración por defecto, para webcam.


    cout << "Iniciando ORB-SLAM.  Línea de comando:" << endl;

    // Parámetros de la línea de comando
    ORB_SLAM2::System SLAM(configuration["vocabulary"], rutaConfiguracion, ORB_SLAM2::System::MONOCULAR, true);

    // Puntero global al sistema singleton
    Sistema = &SLAM;

    ORB_SLAM2::Video video;
    new thread(&ORB_SLAM2::Video::Run, &video);
//
    switch (argc) {
        case 1:    // Sin argumentos, webcam por defecto y webcam.yaml como configuración
            cout << "Sin argumentos, webcam con esta configuración: " << rutaConfiguracion << endl;
            break;
        default:
            break;
    }

    // inicializacion crow
    crow::SimpleApp app;

    CROW_ROUTE(app, "/image")
            .methods("OPTIONS"_method)
                    ([](const crow::request &req) {
                        crow::response resp;
                        resp.add_header("Access-Control-Allow-Origin", "*");
                        return resp;
                    });


    CROW_ROUTE(app, "/image")
            .methods("POST"_method)
                    ([configuration, mapRoute, meterFactor, initialImageLocation](const crow::request &req) {
                        crow::response resp;
                        resp = crow::response(200);
                        auto body = crow::json::load(req.body); // imagne llega via json
                        if (!body) {
                            cout << "Imagen no pudo ser recibida" << endl;
                            return crow::response(400);
                        }
                        // parametro de json que quiero es "uri", lo paso a String con .s()
                        const crow::json::detail::r_string &uri_string = body["uri"].s();
                        // decodifico imagen que fue enviada en base 64
                        std::vector<BYTE> decodedData = base64_decode(uri_string);
                        cv::Mat img = cv::imdecode(decodedData, cv::IMREAD_UNCHANGED);

                        namedWindow("Display window", cv::WINDOW_NORMAL);   // Creo una ventana para mostrar imagenes
                        cv::imshow("Display window", img);               // Ponemos la imagen que llego

                        cv::Mat initialMatrix;
                        initialMatrix = loadMatrix(initialImageLocation, mapRoute);
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
                        Mat displacedRelocMatrix = displace(relocMatrix);
                        cout << "Displaced reloc matrix" << endl;
                        cout << displacedRelocMatrix << endl;
                        cout << "Relocation matrix: " << endl;
                        cout << relocMatrix << endl;

                        Mat resVector = substractTranslations(relocMatrix, displacedRelocMatrix);
                        float angle = getAngle(resVector);

                        Mat displacementVector;
                        if (!bogusImage) {
                            displacementVector = calculateLocation(initialMatrix, relocMatrix, initialVector,
                                                                   meterFactor);
                            displacementVector.at<float>(1, 0) = 1;
                        } else {
                            displacementVector = relocMatrix;
                        }
                        string message = getVectorAsString(displacementVector);
                        cout << "The resultant displacement vector is: " + message << endl;
                        // mensaje final => x y z, angulo (en grados)
                        resp.body = message + "," + std::to_string(angle);
                        resp.add_header("Content-Type", "text/plain");
                        resp.add_header("Access-Control-Allow-Origin", "*");
                        resp.add_header("Access-Control-Allow-Methods", "POST");
                        return resp;
                    });
    app.port(9000).multithreaded().run(); // puerto local 9000
}

/**
 * Pose de la camara respecto al sistema de OrbSlam, que es una matriz de rototraslacion
 * @param image imagen de la que quiero la pose
 * @param mapRoute ruta relativa al .osMap
 * @return la matriz de rototraslacion correspondiente
 */
Mat operate(const Mat &image, const string &mapRoute) {

    bool operate = true;
    Mat result;
    int maxFrames = 20;
    while (operate && maxFrames != 0) {
        // Pasarle la imagen al sistema SLAM
        (*Sistema).TrackMonocular(image, 1);

        if ((*Sistema).mpTracker->mState == 2 && (*Sistema).mpTracker->mbOnlyTracking) {
            result = (*Sistema).mpTracker->mCurrentFrame.mTcw.inv();
            operate = false;
            bogusImage = false;
        }

        // Chequear si hay senial para cargar el mapa: debe hacerse desde este thread
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

/**
 * Vector traslacion, ajustado con un factor a metros arbitrario
 * @param initialMatrix pose correspondiente al origen respecto al sistema de referencia SLAM
 * @param relocMatrix pose correspondiente a la foto enviada respecto al sistema de referencia SLAM
 * @param initialVector vector traslacion de la foto de origen
 * @param factor numero arbitrario para ajustar los vectores resultantes a metros
 * @return vector de desplazamiento de la imagen enviada
 */
cv::Mat calculateLocation(const Mat &initialMatrix, const Mat &relocMatrix,
                          const Mat &initialVector, const double factor) {
    Mat relocVector = initialMatrix * (relocMatrix.col(3));
    Mat resultantVector = relocVector - initialVector;
    cout << "Resultant Vector (no factor):" << endl;
    cout << resultantVector << endl;
    resultantVector = resultantVector * factor;
    cout << "Resultant Vector (with factor):" << endl;
    cout << resultantVector << endl;
    return resultantVector;
}

/**
 * Consigue un strin correspondiente a un Mat::vector
 * @param vector target
 * @return string con coordenadas definidas como "x y z"
 */
string getVectorAsString(const Mat &vector) {
    float x = vector.at<float>(0, 0);
    float y = vector.at<float>(1, 0);
    float z = vector.at<float>(2, 0);
    return to_string(x) + " " + to_string(y) + " " + to_string(z);
}

/**
 * Wrapper para operate
 * @param imageLocation path relativo de la imagen target
 * @param mapRoute path relativo del .osMap correspondiente
 * @return matriz de rototraslacion correspondiente
 */
Mat loadMatrix(const string &imageLocation, const string &mapRoute) {
    Mat image = imread(imageLocation, CV_LOAD_IMAGE_GRAYSCALE);
    Mat result = operate(image, mapRoute);
    cout << "Initial image matrix: " << endl;
    cout << result << endl;
    return result;
}

/**
 *
 * @param rutaMapa ubicacion relativa del .osMap que deseo cargar en el sistema SLAM
 */
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

/**
 * Desplazar una matriz de rototraslacion (4x4) una unidad en el eje z positivo
 * @param relocMatrix matriz de rototraslacion target
 * @return la matriz de rototraslacion desplazada
 */
Mat displace(const Mat &relocMatrix) {
    Mat advanceZAxis;
    advanceZAxis = Mat::zeros(4, 4, CV_32F);
    // Armo matriz identidad con un 1 en la posicion z de la parte de traslacion
    advanceZAxis.at<float>(0, 0) = 1;
    advanceZAxis.at<float>(1, 1) = 1;
    advanceZAxis.at<float>(2, 2) = 1;
    advanceZAxis.at<float>(3, 3) = 1;
    advanceZAxis.at<float>(2, 3) = 1;
    Mat advanceZ = relocMatrix * advanceZAxis;
    cout << "Z + 1: " << endl;
    cout << advanceZ << endl;
    return advanceZ;
}

/**
 * Hace la resta de la parte de traslacion de una matriz de rototraslacion de 4x4
 * @param relocMatrix primer termino
 * @param displacedRelocMatrix segundo termino
 * @return la matriz de traslacion correspondiente
 */
Mat substractTranslations(const Mat &relocMatrix, const Mat &displacedRelocMatrix) {
    Mat rslt = displacedRelocMatrix.col(3) - relocMatrix.col(3);
    cout << "Resultant translation: " << endl;
    cout << rslt << endl;
    return rslt;
}

/**
 * Angulo respecto al eje horizontal positivo (+x) de un vector.
 * @param mat vector correspondiente
 * @return el angulo resultante en grados
 */
float getAngle(Mat mat) {
    // nos concentramos en el plano x z. La coordenada Y no nos interesa para pasar a un plano
    float x = mat.at<float>(0, 0);
    float z = mat.at<float>(2, 0);
    auto angle = static_cast<float>(std::atan2(z, x) * 180 / 3.14159265359);
    cout << "Angle: " << endl;
    cout << angle << endl;
    return angle;
}
