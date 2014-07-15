#     imgflo - Flowhub.io Image-processing runtime
#     (c) 2014 The Grid
#     imgflo may be freely distributed under the MIT license

child_process = require 'child_process'
EventEmitter = (require 'events').EventEmitter

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
        #

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
        @process.on 'exit', (code, signal) ->
            if code != 0
                throw new Error 'Runtime exited with non-zero code: ' + code

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

    stop: ->
        if @debug
            return
        @process.kill()

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

exports.MockUi = MockUi
exports.RuntimeProcess = RuntimeProcess
exports.processNode = processNode

