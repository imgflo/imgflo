
http = require 'http'
fs = require 'fs'
child_process = require 'child_process'
EventEmitter = (require 'events').EventEmitter
url = require 'url'
querystring = require 'querystring'
path = require 'path'

node_static = require 'node-static'

# TODO: support using long-lived workers as Processors, use FBP WebSocket API to control

class Processor extends EventEmitter

    constructor: () ->
        #

    run: (graph, callback) ->
        s = JSON.stringify graph, null, "  "
        cmd = './install/env.sh'
        args = ['./install/bin/imgflo', "-"]

        console.log 'executing', cmd, args
        process = child_process.spawn cmd, args, { stdio: ['pipe', 'pipe', 'pipe'] }
        process.on 'close', (exitcode) ->
            err = if exitcode then new Error "processor returned exitcode: #{exitcode}" else null
            return callback err
        process.stdout.on 'data', (d)->
            console.log d.toString()
        process.stderr.on 'data', (d)->
            console.log d.toString()
        process.stdin.write s
        process.stdin.end()

prepareGraph = (def, attributes, inpath, outpath) ->

    # Add load, save, process nodes
    def.processes.load = { component: 'gegl/load' }
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
        response.on 'data', (chunk) ->
            fs.appendFile out, chunk, ->
                #
        response.on 'end', ->
            return finish()

class Server
    constructor: (workdir, resourcedir) ->
        @workdir = workdir
        @resourcedir = resourcedir || './examples'
        @resourceserver = new node_static.Server resourcedir
        @fileserver = new node_static.Server workdir
        @httpserver = http.createServer @handleHttpRequest
        @processor = new Processor

        if not fs.existsSync workdir
            fs.mkdirSync workdir

    listen: (port) ->
        @httpserver.listen port
    close: ->
        @httpserver.close()

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
            demodata =
                graphs: ["crop"],
                inputimages: ["grid-toastybob.jpg"]
                pipelines: ["/graph/crop?x=200&y=230&height=110&width=130&input="]
            response.statusCode = 200
            response.end JSON.stringify demodata

    handleGraphRequest: (request, response) ->
        u = url.parse request.url, true
        filepath = new Buffer(u.search).toString('base64')
        workdir_filepath = path.join @workdir, filepath

        fs.exists workdir_filepath, (exists) =>
            if exists
                @fileserver.serveFile filepath, 200, {}, request, response
            else
                @processGraphRequest workdir_filepath, request.url, (err) =>
                    if err
                        response.writeHead(500);
                        response.end();
                    else
                        # requested file shall now be present
                        @fileserver.serveFile filepath, 200, {}, request, response

    processGraphRequest: (outf, request_url, callback) =>

        u = url.parse request_url, true
        attr = u.query

        graph = (u.pathname.replace '/graph', '') + '.json'
        graph = path.join @resourcedir, graph

        # TODO: transform urls to downloaded images for all attributes, not just "input"
        src = (new Buffer attr.input, 'base64').toString()

        # Add extension so GEGL load op can use the correct file loader
        # FIXME: look up extension of input attribute
        to = path.join @workdir, attr.input + '.jpg'

        downloadFile src, to, =>
            fs.readFile graph, (err, contents) =>
                if err
                    throw err

                def = JSON.parse contents
                delete attr.input
                def = prepareGraph def, attr, to, outf
                @processor.run def, callback

exports.Server = Server

exports.main = ->
    port = process.env.PORT || 8080
    workdir = './temp'

    server = new Server workdir
    server.listen port

    console.log 'Server listening at port', port, "with workdir", workdir
