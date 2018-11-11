function init() {
    var canvas = document.getElementById("e");
    var context = canvas.getContext("2d");
    var plane = new Image();
    plane.src = "plano.jpg";
    plane.onload = function() {
        context.drawImage(plane, 0, 0, 480, 800);
    };

}

function draw(x, y, rot) {
    var canvas = document.getElementById("e");
    var context = canvas.getContext("2d");
    var posX = x;
    var posY = y;
    var rotate = rot;

    var img = document.getElementById("arrow");
    img.style.setProperty("--element-top", posX + 'px');
    img.style.setProperty("--element-left", posY + 'px');
    img.style.setProperty("--element-rotate", rotate + 'deg');
    img.style.visibility = 'visible';
}
