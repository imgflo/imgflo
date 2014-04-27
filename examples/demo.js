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

var createGraphProperties = function(name, graph) {
    if (typeof graph.inports === 'undefined') {
        return null;
    }

    var graphInfo = document.createElement('div');
    var graphName = document.createElement('p');
    var portsInfo = document.createElement('div');
    graphName.innerHTML = name;
    //graphInfo.appendChild(graphName);
    graphInfo.appendChild(portsInfo);

    var attributeList = document.createElement('ul');

    var inports = Object.keys(graph.inports);
    inports.forEach(function (name) {
        var port = inports[name];

        var portInfo = document.createElement('div');
        portInfo.className = 'line';

        var portName = document.createElement('label');
        portName.className = "portLabel";
        portName.innerHTML = name;
        var portInput = document.createElement('input');
        portInput.name = name;
        portInput.className = "portInput";

        // TODO: show information about type,value ranges, default value, description etc
        portInfo.appendChild(portName);
        portInfo.appendChild(portInput);
        portsInfo.appendChild(portInfo);
    });

    return graphInfo;
}

var createGraphList = function(graphs, onClicked) {
    var list = document.createElement('li');
    list.onclick = onClicked;

    Object.keys(graphs).forEach(function(name) {
        if (typeof graphs[name].inports !== 'undefined') {
            var e = document.createElement('ul');
            e.className = "graphEntry";
            e.innerHTML = name;
            list.appendChild(e);
        }
    });
    return list;
}

var createLogEntry = function(url) {
    var img = document.createElement("img");
    img.className = "image";
    img.src = url;

    var req = document.createElement("p");
    req.innerHTML = "GET " + url;
    req.className = "request";

    var div = document.createElement("div");
    div.className = "logEntry";
    div.appendChild(req);
    div.appendChild(img);
    return div;
}

var createRequestUrl = function(graphname, parameters) {
    var hasQuery = Object.keys(parameters).length > 0;
    var url = '/graph/'+graphname+(hasQuery ? '?' : '');
    for (var key in parameters) {
        var value = parameters[key];
        if (value.indexOf('http://') !== -1) {
            value = btoa(value);
        }
        url += key+'='+value+'&';
    }
    if (hasQuery) {
        url = url.substring(0, url.length-1); // strip trailing &
    }
    return url;
}

var getGraphProperties = function(container, name, graphdef) {
    var props = {};
    var inputs = container.getElementsByTagName('input');
    for (var i=0; i<inputs.length; i++) {
        var input = inputs[i];
        if (input.value !== '') {
            props[input.name] = input.value;
        }
    }
    return props;
}

var main = function() {

    var id = function(n) {
        return document.getElementById(n);
    }

    var addEntry = function(url) {
        var e = createLogEntry(url);
        id('historySection').insertBefore(e, id('historySection').firstChild);
    }

    var activeGraphName = null;
    var availableGraphs = null;

    id('runButton').onclick = function () {
        var graph = activeGraphName;
        var props = getGraphProperties(id('graphProperties'), graph, availableGraphs[graph]);
        var u = createRequestUrl(graph, props);
        addEntry(u);
    };

    var setActiveGraph = function(name) {
        if (typeof availableGraphs[name] === 'undefined') {
            throw new Error('No such graph: ' + name);
        }
        activeGraphName = name;
        var container = id('graphProperties');
        if (container.children.length) {
            container.removeChild(container.children[0]);
        }
        var e = createGraphProperties(name, availableGraphs[name]);
        container.appendChild(e);
    }

    var onGraphClicked = function(event) {
        var name = event.target.innerText;
        console.log("onGraphClicked", name);
        setActiveGraph(name);
    }

    getDemoData(function(err, res) {
        if (err) {
            throw err;
        }

        availableGraphs = res.graphs;
        setActiveGraph('crop');

        var l = createGraphList(res.graphs, onGraphClicked);
        id('graphList').appendChild(l);
        res.images.forEach(function(image) {
            addEntry(image);
        });
    });

}
window.onload = main;
