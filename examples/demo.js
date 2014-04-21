var getDemoData = function(callback) {
    req=new XMLHttpRequest();
    req.onreadystatechange = function() {
        if (req.readyState === 4) {
            if (req.status === 200) {
                var d = JSON.parse(req.responseText);
                return callback(null, d);
            } else {
                var e = new Error(req.status);
                return callback(e, null);
            }
        }
    }
    req.open("GET", "/demo", true);
    req.send();
}

var displayExampleImages = function(container, images) {
    for (var i=0; i<images.length; i++) {
        var src = images[i];
        var img = document.createElement("img");
        img.src = "demo/"+src;
        img.className = "image";
        container.appendChild(img);
    }
}

var main = function() {

    getDemoData(function(err, res) {
        if (err) {
            throw err;
        }
        var c = document.getElementById("exampleInputs");
        console.log(res, c);
        displayExampleImages(c, res.inputimages);
    });

}
window.onload = main;
