let http = new XMLHttpRequest();
let url = "http://localhost:9000/image";
let img = undefined;

http.onreadystatechange = function () {
    if (http.readyState === 4 && http.status === 200) {
        let response = http.response.split(",");
        let coordinates = response[0].split(" ");
        const x = coordinates[0];
        const y = coordinates[2];
        const angle = response[1];
        draw(x, y, angle);
    } else if (http.status === 404) {
        document.getElementById("myAlert").style.visibility = "visible";
        document.getElementById("alertText").innerHTML = "No se pudo relocalizar.</br>Por favor intente con otra foto";
    } else if (http.status === 500) {
        document.getElementById("myAlert").style.visibility = "visible";
        document.getElementById("alertText").innerHTML = "El servidor no esta funcionando.</br>Por favor intente mas tarde";
    }
};

function postImgB64(element) {
    document.getElementById("myAlert").style.visibility = "hidden";
    if (isAccessible) {
        http.open("POST", url, true);
        init();
        const file = element.files[0];
        let reader = new FileReader();
        reader.onloadend = function () {
            img = reader.result.split(',')[1];
            const data = JSON.stringify({"uri": img});
            http.send(data);
        };
        reader.readAsDataURL(file);
    }
}


let isAccessible = null;

function checkConnection() {
    $.ajax({
        url: "http://localhost:9000/test",
        type: "post",
        crossDomain : true,
        complete : function(xhr, responseText, thrownError) {
            if(xhr.status == "200") {
                isAccessible = true;
            }
            else {
                isAccessible = false;
                document.getElementById("myAlert").style.visibility = "visible";
                document.getElementById("alertText").innerHTML = "El servidor no esta funcionando.</br>Por favor intente mas tarde";
            }
        }
    });
}
