let http = new XMLHttpRequest();
const ip = "172.22.37.119";
let url = "http://" + ip + ":9000/image";
let img = undefined;

http.onreadystatechange = function () {
    if (http.readyState === 4 && http.status === 200) {
        let response = http.response.split(",");
        let coordinates = response[0].split(" ");
        const x = coordinates[0];
        const y = coordinates[2];
        const angle = response[1];
        draw(x, y, angle);
    } else if (http.readyState === 4 && http.status !== 200) {
        document.getElementById("myAlert").style.visibility = "visible";
        document.getElementById("alertText").innerHTML = "No se pudo relocalizar.</br>Por favor intente con otra foto";
    }
};

function postImgB64(element) {
    if (isAccessible) {
	document.getElementById("myAlert").style.visibility = "hidden";
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


let isAccessible = true;

function checkConnection() {
    $.ajax({
        url: "http://" + ip + ":9000/test",
        type: "post",
        crossDomain : true,
        complete : function(xhr, responseText, thrownError) {
            if(xhr.status === 200) {
                isAccessible = true;
            }
            else {
                isAccessible = false;
                document.getElementById("myAlert").style.visibility = "visible";
                document.getElementById("alertText").innerHTML = "El servidor no esta funcionando.</br>Por favor intente mas tarde";
		document.getElementById("fixedbutton").style.backgroundColor = "grey";
		document.getElementById("pic").disabled = true;
            }
        }
    });
}
