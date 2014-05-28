
http = require 'http'
fs = require 'fs'
child_process = require 'child_process'
EventEmitter = (require 'events').EventEmitter
url = require 'url'
querystring = require 'querystring'
path = require 'path'
crypto = require 'crypto'

node_static = require 'node-static'
async = require 'async'

# TODO: support using long-lived workers as Processors, use FBP WebSocket API to control

class Processor extends EventEmitter

    constructor: (verbose) ->
        @verbose = verbose

    run: (graph, callback) ->
        s = JSON.stringify graph, null, "  "
        cmd = './install/env.sh'
        args = ['./install/bin/imgflo', "-"]

        console.log 'executing', cmd, args if @verbose
        stderr = ""
        process = child_process.spawn cmd, args, { stdio: ['pipe', 'pipe', 'pipe'] }
        process.on 'close', (exitcode) ->
            err = if exitcode then new Error "processor returned exitcode: #{exitcode}" else null
            return callback err, stderr
        process.stdout.on 'data', (d) =>
            console.log d.toString() if @verbose
        process.stderr.on 'data', (d)->
            stderr += d.toString()
        console.log s if @verbose
        process.stdin.write s
        process.stdin.end()

prepareGraph = (def, attributes, inpath, outpath, type) ->

    loader = 'gegl/load'
    if type
        loader = "gegl/#{type}-load"

    # Add load, save, process nodes
    def.processes.load = { component: loader }
    def.processes.save = { component: 'gegl/png-save' }
    def.processes.proc = { component: 'Processor' }

    # Connect them to actual graph
    out = def.outports.output
    inp = def.inports.input

    def.connections.push { src: {process: 'load', port: 'output'}, tgt: inp }
    def.connections.push { src: out, tgt: {process: 'save', port: 'input'} }
    def.connections.push { src: {process: 'save', port: 'output'}, tgt: {process: 'proc', port: 'node'} }

    # Attach filepaths as IIPs
    def.connections.push { data: inpath, tgt: { process: 'load', port: 'path'} }
    def.connections.push { data: outpath, tgt: { process: 'save', port: 'path'} }

    # Attach processing parameters as IIPs
    for k, v of attributes
        tgt = def.inports[k]
        def.connections.push { data: v, tgt: tgt }

    # Clean up
    delete def.inports
    delete def.outports

    return def

downloadFile = (src, out, finish) ->
    http.get src, (response) ->
        if response.statusCode == 301
            return downloadFile response.headers.location, out, finish

        response.on 'data', (chunk) ->
            fs.appendFile out, chunk, ->
                #
        response.on 'end', ->
            return finish(response.headers['content-type'])


getGraphs = (directory, callback) ->
    graphs = {}

    fs.readdir directory, (err, files) ->
        if err
            callback err, null

        graphfiles = []
        for f in files
            graphfiles.push path.join directory, f if (path.extname f) == '.json'

        async.map graphfiles, fs.readFile, (err, results) ->
            if err
                return callback err, null

            for i in [0...results.length]
                name = path.basename graphfiles[i]
                name = (name.split '.')[0]
                def = JSON.parse results[i]
                graphs[name] = def

            return callback null, graphs


hashFile = (path) ->
    hash = crypto.createHash 'sha1'
    hash.update path
    return hash.digest 'hex'

keysNotIn = (A, B) ->
    notIn = []
    for a in Object.keys A
        isIn = false
        for b in Object.keys B
            if b == a
                isIn = true
        if not isIn
            notIn.push a
    return notIn

class Server extends EventEmitter
    constructor: (workdir, resourcedir, graphdir, verbose) ->
        @workdir = workdir
        @resourcedir = resourcedir || './examples'
        @graphdir = graphdir || './graphs'
        @resourceserver = new node_static.Server resourcedir
        @fileserver = new node_static.Server workdir
        @httpserver = http.createServer @handleHttpRequest
        @processor = new Processor verbose
        @port = null

        if not fs.existsSync workdir
            fs.mkdirSync workdir

    listen: (port) ->
        @port = port
        @httpserver.listen port
    close: ->
        @httpserver.close()

    logEvent: (id, data) ->
        @emit 'logevent', id, data

    handleHttpRequest: (request, response) =>
        request.addListener('end', () =>
            u = url.parse request.url, true
            if u.pathname == '/'
                u.pathname = '/demo/index.html'

            if (u.pathname.indexOf "/demo") == 0
                @serveDemoPage request, response
            else if (u.pathname.indexOf "/graph") == 0
                @handleGraphRequest request, response
            else
                console.log "Got unknown HTTP request: ", u.pathname
                response.statusCode = 404
                response.end "Cannot find #{u.pathname}"
        ).resume()

    getDemoData: (callback) ->

        getGraphs @graphdir, (err, res) =>
            if err
                throw err
            d =
                graphs: res
                images: ["demo/grid-toastybob.jpg", "http://thegrid.io/img/thegrid-overlay.png"]
            return callback null, d


    serveDemoPage: (request, response) ->
        u = url.parse request.url
        p = u.pathname
        if p == '/'
            p = '/demo/index.html'
        p = p.replace '/demo', ''
        if p
            p = path.join @resourcedir, p
            @resourceserver.serveFile p, 200, {}, request, response
        else
            @getDemoData (err, data) ->
                response.statusCode = 200
                response.end JSON.stringify data

    handleGraphRequest: (request, response) ->
        u = url.parse request.url, true
        filepath = hashFile u.search
        workdir_filepath = path.join @workdir, filepath

        fs.exists workdir_filepath, (exists) =>
            if exists
                @fileserver.serveFile filepath, 200, {}, request, response
            else
                @processGraphRequest workdir_filepath, request.url, (err, stderr) =>
                    @logEvent 'process-request-end', { error: err, stderr: stderr }
                    if err
                        if err.code?
                            response.writeHead err.code, { 'Content-Type': 'application/json' }
                            response.end JSON.stringify err.result
                        else
                            response.writeHead 500
                            response.end()
                    else
                        # requested file shall now be present
                        @fileserver.serveFile filepath, 200, {}, request, response

    processGraphRequest: (outf, request_url, callback) =>

        u = url.parse request_url, true
        attr = u.query

        graph = (u.pathname.replace '/graph', '') + '.json'
        graph = path.join @graphdir, graph

        # TODO: transform urls to downloaded images for all attributes, not just "input"
        src = attr.input

        # Add extension so GEGL load op can use the correct file loader
        ext = path.extname src
        if (ext.indexOf '&') != -1
            ext = ''

        to = path.join @workdir, (hashFile src) + ext
        if (src.indexOf 'http://') == -1
            src = 'http://localhost:'+@port+'/'+src

        downloadFile src, to, (contentType) =>
            fs.readFile graph, (err, contents) =>
                if err
                    return callback err, null
                # TODO: read graphs once
                def = JSON.parse contents
                invalid = keysNotIn attr, def.inports
                if invalid.length > 0
                    return callback { code: 449, result: def }, null

                delete attr.input
                type = null
                if ext == ''
                    if contentType == 'image/jpeg'
                        type = 'jpg'
                    else if contentType == 'image/png'
                        type = 'png'

                def = prepareGraph def, attr, to, outf, type
                @processor.run def, callback

exports.Server = Server

exports.main = ->
    process.on 'uncaughtException', (err) ->
        console.log 'Uncaught exception: ', err

    port = process.env.PORT || 8080
    workdir = './temp'

    server = new Server workdir
    server.listen port

    console.log 'Server listening at port', port, "with workdir", workdir
