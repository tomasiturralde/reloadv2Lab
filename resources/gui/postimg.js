var http = new XMLHttpRequest();
var url = "http://localhost:9000/image";
var img = undefined;
const ORIGIN_X =  780; //240
const ORIGIN_Y = 287; //370
http.open("POST", url, true);
http.withCredentials = true;
http.setRequestHeader("Content-Type", "application/json");
http.setRequestHeader("Access-Control-Allow-Origin", "*");
http.setRequestHeader("Access-Control-Allow-Methods", "POST");
http.onreadystatechange = function () {
    if (http.readyState === 4 && http.status === 200) {
        // document.getElementById("texto").innerHTML = http.response;
        // $("#myModal").modal()
        coordinates = http.response.split(" ");
        const x = coordinates[0] + ORIGIN_X;
        const y = coordinates[2] + ORIGIN_Y;
        draw(x, y, 0);
    }
};

function postImgB64(element) {
    init();
    var file = element.files[0];
    var reader = new FileReader();
    reader.onloadend = function() {
        img = reader.result.split(',')[1];
        var data = JSON.stringify({"uri": img});
        http.send(data);
    };
    reader.readAsDataURL(file);
}
