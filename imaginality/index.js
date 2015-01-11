
// Input data
// TODO: allow to change live?

var evensteps = function(start, stop, n) {
    var diff = stop-start;
    var step = diff/n;
    var out = [];
    for (var i=0; i<n; i++) {
        var o = start+(step*i);
        out.push(o);
    }
    return out;
}

/*
    A more general model would perhaps be:
    - have a list of dimensions, of type
    (ranged-scalar, ranged-point, enumeration, rgbcolor/ranged-3dpoint)
    could be introspected from graph/runtime data
    
*/
var generateCardData = function() {
    var images = [
        'http://tctechcrunch2011.files.wordpress.com/2015/01/happy-kitten-kittens-5890512-1600-1200.jpg',
        'http://cdn.decorenable.com/images/www.funkyfurnitureandstuff.com/bolzano_red.JPG'
    ];

    var data = [];

    // For now simple 2d grid over one val
    evensteps(10, 100, 3).forEach(function(val, y) {
       data[y] = [];
       images.forEach(function(url, x) {
           // annoying there are two
           // FIXME: hardcoded properties for a given graph
           var props = {
                'input': url,
                'std-dev-x': val,
                'std-dev-y': val
           };
           data[y][x] = {
                id: x.toString()+"-"+y.toString(),
                properties: props
           };
       });
    });

    console.log(data)

    return data;
}

// User interface

var iterate2dCards = function(cards, itemcallback) {
    cards.forEach(function(row, x) {
        row.forEach(function(card, y) {
            return callback(card, x, y);
        });
    });
}

// TODO: separate more strongly between data for cards and how they are presented
var setupImages = function (container, cards) {
    var images = [];

    var columns = cards.length;
    var rows = cards[0].length;

    var div = function(klass, parent) {
        var e = document.createElement("div");
        e.className = klass;
        parent.appendChild(e);
        return e;
    }

    var table = div('table', container);

    cards.forEach(function(r, y) {
        var row = div('tr', table);
        r.forEach(function(card, x) {
            var cell = div('td', row);

            var img = document.createElement("img");
            img.className = "image";
            img.id = card.id;
            img.hack_properties = card.properties;

            cell.appendChild(img);
            images.push(img);
        });
    });

    return images;
}


// Processing/rendering
var processImageRuntime = function(img, callback) {

    // FIXME: find good way to do process isolation with imgflo runtime
    // Having dataURLs would make them more stable over current HTTP ones at least...

    // Send new data to inports over FBP/WS
    // Wait for output url
    // Add url to img.src
    // Wait for url to be loaded
    // When loaded, call done
};

// For imgflo-server
// Could use the-grid/imgflo-url library here
var createRequestUrl = function(graphname, parameters, apiKey, apiSecret) {
    var hasQuery = Object.keys(parameters).length > 0;
    var search = graphname + (hasQuery ? '?' : '');
    for (var key in parameters) {
        var value = encodeURIComponent(parameters[key]);
        search += key+'='+value+'&';
    }
    if (hasQuery) {
        search = search.substring(0, search.length-1); // strip trailing &
    }

    var url = '/graph/'+search;
    if (apiKey || apiSecret) {
        var token = CryptoJS.MD5(search+apiSecret);
        url = '/graph/'+apiKey+'/'+token+'/'+search;
    }

    return url;
}



// TODO: make this a class, emitting events on state changes, allowing re-processing
var process = function (images, graph, server, key, secret, done) {
    // Curretly processing image elements using imgflo-server
    // FIXME: use imgflo runtime directly instead

    images.forEach(function(img) {
        var params = img.hack_properties;
        img.src = server+createRequestUrl(graph, params, key, secret);
    });

    // FIXME: wait for .onload events

    return done();
}


var main = function() {
    var id = function(n) {
        return document.getElementById(n);
    }

    var data = generateCardData();
    var images = setupImages(id("cards"), data);

    var key = localStorage["imgflo-server-api-key"];
    var secret = localStorage["imgflo-server-api-secret"];

    if (!key || !secret) {
        alert('Missing imgflo-server API key/secret!');
        return;
    }

    // TODO: allow to trigger without reloading page
    var server = 'http://localhost:8081';
    id("status").innerHTML = 'working';
    process(images, 'gaussianblur', server, key, secret, function() {
        id("status").innerHTML = 'done';
    });
}

window.onload = main;
