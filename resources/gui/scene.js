function init() {
    var canvas = document.getElementById("e");
    var context = canvas.getContext("2d");
    var plane = new Image();
    plane.src = "plano.jpg";
    plane.onload = function() {
        context.drawImage(plane, 0, 0, 480, 800);
    };
    enableScroll();
}

function draw(x, y, rot) {
    var canvas = document.getElementById("e");
    var context = canvas.getContext("2d");
    var posX = 225 + (float2int(x)*15);
    var posY = 780 - (float2int(y)*10);
    var rotate = 90 - rot;

    var img = new Image();
    img.src = "redarrow.png";
    img.onload = function() {
	context.save();
    	context.translate(posX, posY);
    	context.translate(7, 10);
    	context.rotate(rotate*(Math.PI/180));
    	context.drawImage(img, -7, -10, 15, 20);
    	context.restore();
	window.scroll(posX, posY);
    }
}

function float2int (value) {
   return value | 0;
}

function preventDefault(e) {
  e = e || window.event;
  if (e.preventDefault)
      e.preventDefault();
  e.returnValue = false;  
}

function disableScroll() {
  if (window.addEventListener) // older FF
      window.addEventListener('DOMMouseScroll', preventDefault, false);
  window.onwheel = preventDefault; // modern standard
  window.onmousewheel = document.onmousewheel = preventDefault; // older browsers, IE
  window.ontouchmove  = preventDefault; // mobile
  document.onkeydown  = preventDefaultForScrollKeys;
}

function enableScroll() {
    if (window.removeEventListener)
        window.removeEventListener('DOMMouseScroll', preventDefault, false);
    window.onmousewheel = document.onmousewheel = null; 
    window.onwheel = null; 
    window.ontouchmove = null;  
    document.onkeydown = null;  
}
