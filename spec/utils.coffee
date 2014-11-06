#     imgflo - Flowhub.io Image-processing runtime
#     (c) 2014 The Grid
#     imgflo may be freely distributed under the MIT license

child_process = require 'child_process'
EventEmitter = (require 'events').EventEmitter
path = require 'path'
fs = require 'fs'

websocket = require 'websocket'
needle = require 'needle'

# TODO: move into library, also use in MicroFlo and other FBP runtime implementations?
class MockUi extends EventEmitter

    constructor: ->
        @client = new websocket.client()
        @connection = null

        @components = {}
        @runtimeinfo = {}
        @networkrunning = false
        @networkoutput = {}

        @client.on 'connect', (connection) =>
            @connection = connection
            @connection.on 'error', (error) =>
                throw error
            @connection.on 'close', (error) =>
                @emit 'disconnected'
            @connection.on 'message', (message) =>
                @handleMessage message
            @emit 'connected', connection

    handleMessage: (message) ->
        if not message.type == 'utf8'
            throw new Error "Received non-UTF8 message: " + message

        d = JSON.parse message.utf8Data
        if d.protocol == "component" and d.command == "component"
            id = d.payload.name
            @components[id] = d.payload
            @emit 'component-added', id, @components[id]
        else if d.protocol == "component" and d.command == "source"
            id = d.payload.name
            @components[id] = {} if not @components[id]?
            @components[id].source = d.payload.code
            @emit 'component-source', id, @components[id]
        else if d.protocol == "runtime" and d.command == "runtime"
            @runtimeinfo = d.payload
            @emit 'runtime-info-changed', @runtimeinfo
        else if d.protocol == "network" and d.command == "started"
            @networkrunning = true
            @emit 'network-running', @networkrunning
        else if d.protocol == "network" and d.command == "stopped"
            @networkrunning = false
            @emit 'network-running', @networkrunning
        else if d.protocol == "network" and d.command == "output"
            @networkoutput = d.payload
            @emit 'network-output', @networkoutput
        else
            console.log 'UI received unknown message', d

    connect: ->
        @client.connect 'ws://localhost:3888/', "noflo"
    disconnect: ->
        @connection.close()
        @emit 'disconnected'

    send: (protocol, command, payload) ->
        msg =
            protocol: protocol
            command: command
            payload: payload || {}
        @sendMsg msg

    sendMsg: (msg) ->
        @connection.sendUTF JSON.stringify msg

class RuntimeProcess
    constructor: (debug) ->
        @process = null
        @started = false
        @debug = debug
        @errors = []

    start: (success) ->
        exec = './install/env.sh'
        args = ['./install/bin/imgflo-runtime', '--port', '3888']
        if @debug
            console.log 'Debug mode: setup runtime yourself!', exec, args
            return success 0

        @process = child_process.spawn exec, args
        @process.on 'error', (err) ->
            throw err
        @process.on 'exit', (code, signal) =>
            if code != 0
                e = @errors.join '\n '
                m = "Runtime exited with non-zero code: #{code} #{signal}, errors(#{@errors.length}): " + e
                throw new Error m

        @process.stderr.on 'data', (d) =>
            output = d.toString()
            lines = output.split '\n'
            for line in lines
                err = line.trim()
                @errors.push err if err

        stdout = ""
        @process.stdout.on 'data', (d) ->
            if @debug
                console.log d
            stdout += d.toString()
            if stdout.indexOf 'running on port' != -1
                if not @started
                    @started = true
                    success process.pid

    stop: (callback) ->
        if @debug
            return callback()
        @process.once 'exit', ->
            return callback()
        @process.kill 'SIGINT'

    popErrors: ->
        errors = @errors
        @errors = []
        return errors

processNode = (graphId, nodeId, callback) ->
    base = "http://localhost:3888"
    data =
        graph: graphId
        node: nodeId
    needle.request 'get', base+'/process', data, callback

rmrf = (dir) ->
    if fs.existsSync dir
        for f in fs.readdirSync dir
            f = path.join dir, f
            try
                fs.unlinkSync f
            catch e
                if e.code == 'EISDIR'
                    rmrf f
                else
                    throw e
        fs.rmdirSync dir

exports.MockUi = MockUi
exports.RuntimeProcess = RuntimeProcess
exports.processNode = processNode

exports.testData = (file) ->
    p = path.join (path.resolve __dirname), '..', 'spec/data', file
    console.log p
    return fs.readFileSync p, { encoding: 'utf-8' }


exports.rmrf = rmrf
