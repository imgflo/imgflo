
http = require 'http'
fs = require 'fs'
child_process = require 'child_process'
EventEmitter = (require 'events').EventEmitter
url = require 'url'
querystring = require 'querystring'
path = require 'path'
crypto = require 'crypto'
request = require 'request'
tmp = require 'tmp'

node_static = require 'node-static'
async = require 'async'

clone = (obj) ->
  if not obj? or typeof obj isnt 'object'
    return obj

  if obj instanceof Date
    return new Date(obj.getTime())

  if obj instanceof RegExp
    flags = ''
    flags += 'g' if obj.global?
    flags += 'i' if obj.ignoreCase?
    flags += 'm' if obj.multiline?
    flags += 'y' if obj.sticky?
    return new RegExp(obj.source, flags)

  newInstance = new obj.constructor()

  for key of obj
    newInstance[key] = clone obj[key]

  return newInstance

# TODO: support using long-lived workers as Processors, use FBP WebSocket API to control

installdir = __dirname + '/install/'

class Processor
    constructor: (verbose) ->
        @verbose = verbose

    # FIXME: clean up interface
    # callback should be called with (err, error_string)
    process: (outputFile, outType, graph, iips, inputFile, inputType, callback) ->
        throw new Error 'Processor.process() not implemented'

class NoFloProcessor extends Processor
    constructor: (verbose) ->
        @verbose = verbose

    process: (outputFile, outputType, graph, iips, inputFile, inputType, callback) ->
        g = prepareNoFloGraph graph, iips, inputFile, outputFile, inputType
        @run g, callback

    run: (graph, callback) ->
        s = JSON.stringify graph, null, "  "
        cmd = 'node_modules/noflo-canvas/node_modules/.bin/noflo'
        console.log s if @verbose

        # TODO: add support for reading from stdin to NoFlo?
        tmp.file {postfix: '.json'}, (err, graphPath) =>
            return callback err, null if err
            fs.writeFile graphPath, s, () =>
                @execute cmd, [ graphPath ], callback

    execute: (cmd, args, callback) ->
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

prepareNoFloGraph = (basegraph, attributes, inpath, outpath, type) ->

    # Avoid mutating original
    def = clone basegraph

    # Note: We drop inpath on the floor, only support pure generative for now

    # Add a input node
    def.processes.canvas = { component: 'canvas/CreateCanvas' }

    # Add a output node
    def.processes.repeat = { component: 'core/RepeatAsync' }
    def.processes.save = { component: 'canvas/SavePNG' }

    # Attach filepaths as IIPs
    def.connections.push { data: outpath, tgt: { process: 'save', port: 'filename'} }

    # Connect to actual graph
    canvas = def.inports.canvas
    def.connections.push { src: {process: 'canvas', port: 'canvas'}, tgt: canvas }

    out = def.outports.output
    def.connections.push { src: out, tgt: {process: 'repeat', port: 'in'} }
    def.connections.push { src: {process: 'repeat', port: 'out'}, tgt: {process: 'save', port: 'canvas'} }

    # Defaults
    if attributes.height?
        attributes.height = parseInt attributes.height
    else
        attributes.height = 400

    if attributes.width?
        attributes.width = parseInt attributes.width
    else
        attributes.width = 600

    # Attach processing parameters as IIPs
    for k, v of attributes
        tgt = def.inports[k]
        def.connections.push { data: v, tgt: tgt }

    # Clean up
    delete def.inports
    delete def.outports

    return def


class ImgfloProcessor extends Processor

    constructor: (verbose) ->
        @verbose = verbose

    process: (outputFile, outputType, graph, iips, inputFile, inputType, callback) ->
        g = prepareImgfloGraph graph, iips, inputFile, outputFile, inputType, outputType
        @run g, callback

    run: (graph, callback) ->

        s = JSON.stringify graph, null, "  "
        cmd = installdir+'env.sh'
        args = [ installdir+'bin/imgflo', "-"]

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

prepareImgfloGraph = (basegraph, attributes, inpath, outpath, type, outtype) ->

    # Avoid mutating original
    def = clone basegraph

    loader = 'gegl/load'
    if type
        loader = "gegl/#{type}-load"

    # Add load, save, process nodes
    def.processes.load = { component: loader }
    def.processes.save = { component: "gegl/#{outtype}-save" }
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

downloadFile = (src, out, callback) ->
    req = request src, (error, response) ->
        if error
            return callback error, null
        if response.statusCode != 200
            return callback response.statusCode, null

        callback null, response.headers['content-type']

    stream = fs.createWriteStream out
    s = req.pipe stream

waitForDownloads = (files, callback) ->
    f = files.input
    if f?
        downloadFile f.src, f.path, (err, contentType) ->
            return callback err, null if err
            f.type = contentType
            return callback null, files
    else
        return callback null, files

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
                enrichGraphDefinition def, true
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

typeFromMime = (mime) ->
    type = null
    if mime == 'image/jpeg'
        type = 'jpg'
    else if mime == 'image/png'
        type = 'png'
    return type

runtimeForGraph = (g) ->
    runtime = 'imgflo'
    if g.properties and g.properties.environment and g.properties.environment.type
        runtime = g.properties.environment.type
    return runtime

enrichGraphDefinition = (graph, publicOnly) ->
    runtime = runtimeForGraph graph
    if (runtime.indexOf 'noflo') != -1
        # All noflo-canvas graphs take height+width, set up by NoFloProcessor
        # This is wired up using an internal canvas inport
        delete graph.inports.canvas if publicOnly
        graph.inports.height =
            process: 'canvas'
            port: 'height'
        graph.inports.width =
            process: 'canvas'
            port: 'width'

parseRequestUrl = (u) ->
    parsedUrl = url.parse u, true

    files = {} # port -> {}
    # TODO: transform urls to downloaded images for all attributes, not just "input"
    if parsedUrl.query.input
        src = parsedUrl.query.input.toString()

        # Add extension so GEGL load op can use the correct file loader
        ext = path.extname src
        if ext not in ['.png', '.jpg', '.jpeg']
            ext = ''

        files.input = { src: src, extension: ext, path: null }

    iips = {}
    for key, value of parsedUrl.query
        iips[key] = value if key != 'input'

    p = parsedUrl.pathname.replace '/graph/', ''
    outtype = (path.extname p).replace '.', ''
    graph = path.basename p, path.extname p
    if not outtype
        outtype = 'png'

    out =
        graph: graph
        files: files
        iips: iips
        outtype: outtype
    return out

class Server extends EventEmitter
    constructor: (workdir, resourcedir, graphdir, verbose) ->
        @workdir = workdir
        @resourcedir = resourcedir || './examples'
        @graphdir = graphdir || './graphs'
        @resourceserver = new node_static.Server resourcedir
        @fileserver = new node_static.Server workdir
        @httpserver = http.createServer @handleHttpRequest
        @port = null

        n = new NoFloProcessor verbose
        @processors =
            imgflo: new ImgfloProcessor verbose
            'noflo-browser': n
            'noflo-nodejs': n

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
        @logEvent 'request-received', { request: request.url }
        request.addListener('end', () =>
            u = url.parse request.url, true
            if u.pathname == '/'
                u.pathname = '/demo/index.html'

            if (u.pathname.indexOf "/demo") == 0
                @serveDemoPage request, response
            else if (u.pathname.indexOf "/graph") == 0
                @handleGraphRequest request, response
            else
                @logEvent 'unknown-request', { request: request.url, path: u.pathname }
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

    getGraph: (name, callback) ->
        # TODO: cache graphs
        graphPath = path.join @graphdir, name + '.json'
        fs.readFile graphPath, (err, contents) =>
            return callback err, null if err
            def = JSON.parse contents
            enrichGraphDefinition def
            return callback null, def

    handleGraphRequest: (request, response) ->
        u = url.parse request.url, true
        filepath = hashFile u.path
        workdir_filepath = path.join @workdir, filepath

        fs.exists workdir_filepath, (exists) =>
            if exists
                @logEvent 'serve-from-cache', { request: request.url, file: filepath }
                @fileserver.serveFile filepath, 200, {}, request, response
            else
                @processGraphRequest workdir_filepath, request.url, (err, stderr) =>
                    @logEvent 'process-request-end', { request: request.url, err: err, stderr: stderr }
                    if err
                        if err.code?
                            response.writeHead err.code, { 'Content-Type': 'application/json' }
                            response.end JSON.stringify err.result
                        else
                            response.writeHead 500
                            response.end()
                    else
                        # requested file shall now be present
                        @logEvent 'serve-processed-file', { request: request.url, file: filepath }
                        @fileserver.serveFile filepath, 200, {}, request, response

    processGraphRequest: (outf, request_url, callback) =>
        req = parseRequestUrl request_url

        for port, file of req.files
            file.path = path.join @workdir, (hashFile file.src) + file.extension
            if (file.src.indexOf 'http://') == -1 and (file.src.indexOf 'https://') == -1
                file.src = 'http://localhost:'+@port+'/'+file.src

        @getGraph req.graph, (err, graph) =>
            if err
                @logEvent 'read-graph-error', { request: request_url, err: err, file: graph }
                return callback err, null

            invalid = keysNotIn req.iips, graph.inports
            if invalid.length > 0
                @logEvent 'invalid-graph-properties-error', { request: request_url, props: invalid }
                return callback { code: 449, result: graph }, null

            runtime = runtimeForGraph graph
            processor = @processors[runtime]
            if not processor?
                e =
                    request: request_url
                    runtime: runtime
                    valid: Object.keys @processors
                @logEvent 'no-processor-for-runtime-error', e
                return callback { code: 500, result: {} }, null

            waitForDownloads req.files, (err, downloads) =>
                if err
                    @logEvent 'download-input-error', { request: request_url, files: req.files, err: err }
                    return callback { code: 504, result: {} }, null

                inputType = if downloads.input? then typeFromMime downloads.input.type else null
                inputFile = if downloads.input? then downloads.input.path else null
                processor.process outf, req.outtype, graph, req.iips, inputFile, inputType, callback

exports.Processor = Processor
exports.Server = Server

exports.main = ->
    process.on 'uncaughtException', (err) ->
        console.log 'Uncaught exception: ', err

    port = process.env.PORT || 8080
    workdir = './temp'

    server = new Server workdir
    server.listen port
    server.on 'logevent', (id, data) ->
        console.log "EVENT: #{id}:", data

    console.log 'Server listening at port', port, "with workdir", workdir
