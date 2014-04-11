
node_static = require 'node-static'
http = require 'http'
fs = require 'fs'
child_process = require 'child_process'
EventEmitter = (require 'events').EventEmitter

class Processor extends EventEmitter

    constructor: () ->
        #

    run: (callback) ->
        cmd = './install/env.sh ./install/bin/noflo-gegl'
        console.log 'executing', cmd
        process = child_process.exec cmd, (err, stdout, stderr) ->
            console.log stdout, stderr
            return callback err

class Server
    constructor: (workdir) ->
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
        # FIXME: allow specifying
        # inputfile: HTTP URL
        # graph: name of a local .fbp/.json graph
        # parameters: key,value pairs of IIPs for graph
        request.addListener('end', () =>
            @fileserver.serve request, response, (err, result) =>
                if err
                    @processHttpRequest request, response
        ).resume()

    processHttpRequest: (request, response) =>

        @processor.run (err) =>
            if err
                response.writeHead(500);
                response.end();
            else
                # requested file shall now be present
                @fileserver.serve request, response


exports.Server = Server

exports.main = ->
    port = 8080
    workdir = './temp'

    server = new Server workdir
    server.listen port

    console.log 'Server listening at port', port, "with workdir", workdir
