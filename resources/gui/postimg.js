var http = new XMLHttpRequest();
var url = "http://localhost:9000/image";
var img = undefined;
http.open("POST", url, true);
http.withCredentials = true;
http.setRequestHeader("Content-Type", "application/json");
http.setRequestHeader("Access-Control-Allow-Origin", "*");
http.setRequestHeader("Access-Control-Allow-Methods", "POST");
http.onreadystatechange = function () {
    if (http.readyState === 4 && http.status === 200) {
        // document.getElementById("texto").innerHTML = http.response;
        // $("#myModal").modal()
        draw(http.response.split(" "));
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
        // draw([780, 287, 0]);
        //    y, x, rot
    };
    reader.readAsDataURL(file);
}
