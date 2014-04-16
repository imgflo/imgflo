
node_static = require 'node-static'
http = require 'http'
fs = require 'fs'
child_process = require 'child_process'
EventEmitter = (require 'events').EventEmitter
url = require 'url'
querystring = require 'querystring'
path = require 'path'

class Processor extends EventEmitter

    constructor: () ->
        #

    run: (graph, callback) ->
        cmd = "./install/env.sh ./install/bin/imgflo #{graph}"
        console.log 'executing', cmd
        process = child_process.exec cmd, (err, stdout, stderr) ->
            console.log stdout, stderr
            return callback err

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

class Server
    constructor: (workdir) ->
        @workdir = workdir
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
            # TODO: namespace input and output files
            filepath = new Buffer(u.search).toString('base64')
            workdir_filepath = path.join @workdir, filepath

            fs.exists workdir_filepath, (exists) =>
                if exists
                    @fileserver.serveFile filepath, 200, {}, request, response
                else
                    @processHttpRequest workdir_filepath, request.url, (err) =>
                        if err
                            response.writeHead(500);
                            response.end();
                        else
                            # requested file shall now be present
                            @fileserver.serveFile filepath, 200, {}, request, response
                    
        ).resume()

    processHttpRequest: (outf, request_url, callback) =>

        u = url.parse request_url, true
        attr = u.query

        graph = (u.pathname.replace '/', '') + '.json'
        graph = path.join 'examples', graph

        # TODO: transform urls to downloaded images for all attributes, not just "input"
        src = (new Buffer attr.input, 'base64').toString()

        downloadFile = (src, out, finish) ->
            http.get src, (response) ->
                response.on 'data', (chunk) ->
                    fs.appendFile out, chunk, ->
                        #
                response.on 'end', ->
                    return finish()

        # Add extension so GEGL load op can use the correct file loader
        # FIXME: look up extension of input attribute
        to = path.join @workdir, attr.input + '.jpg'

        downloadFile src, to, =>
            # FIXME: make async
            def = JSON.parse fs.readFileSync graph

            delete attr.input
            def = prepareGraph def, attr, to, outf            

            # FIXME: make async, don't use file or at least use temporary
            tempgraph = path.join @workdir, 'execgraph.json'
            s = JSON.stringify def, null, "  "
            fs.writeFileSync tempgraph, s

            @processor.run tempgraph, callback

exports.Server = Server

exports.main = ->
    port = process.env.PORT || 8080
    workdir = './temp'

    server = new Server workdir
    server.listen port

    console.log 'Server listening at port', port, "with workdir", workdir
