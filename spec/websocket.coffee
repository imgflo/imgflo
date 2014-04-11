
fs = require 'fs'
child_process = require 'child_process'
EventEmitter = (require 'events').EventEmitter

websocket = require 'websocket'
chai = require 'chai'

# TODO: move into library, also use in MicroFlo and other FBP runtime implementations?
class MockUi extends EventEmitter

    constructor: ->
        @client = new websocket.client()
        @connection = null

        @components = {}
        @runtimeinfo = {}
        @networkrunning = false

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
    constructor: ->
        @process = null
        @started = false

    start: (success) ->
        exec = './install/env.sh'
        args = ['./install/bin/noflo-gegl-runtime', '--port', '3888']
        @process = child_process.spawn exec, args
        @process.on 'error', (err) ->
            throw err
        @process.on 'exit', (code, signal) ->
            if code != 0
                throw new Error 'Runtime exited with non-zero code: ' + code

        @process.stderr.on 'data', (d) ->
            console.log d.toString()

        stdout = ""
        @process.stdout.on 'data', (d) ->
            stdout += d.toString()
            if stdout.indexOf 'running on port' != -1
                if not @started
                    @started = true
                    success process.pid

    stop: ->
        @process.kill()


describe 'NoFlo UI WebSocket API', () ->
    runtime = new RuntimeProcess
    ui = new MockUi

    outfile = null

    before (done) ->
        runtime.start ->
            ui.connect()
            ui.on 'connected', () ->
                done()
    after ->
        runtime.stop()
        ui.disconnect()

    describe 'runtime info', ->
        info = null
        it 'should be returned on getruntime', (done) ->
            ui.send "runtime", "getruntime"
            ui.once 'runtime-info-changed', () ->
                info = ui.runtimeinfo
                chai.expect(info).to.be.an 'object'
                done()
        it 'type should be "noflo-gegl"', ->
            chai.expect(info.type).to.equal "noflo-gegl"
        it 'protocol version should be "0.4"', ->
            chai.expect(info.version).to.be.a "string"
            chai.expect(info.version).to.equal "0.4"
        it 'capabilities should include "protocol:component"', ->
            chai.expect(info.capabilities).to.be.an "array"
            chai.expect(info.capabilities.length).to.equal 1
            chai.expect((info.capabilities.filter -> 'protocol:component')[0]).to.be.a "string"

    describe 'sending component list', ->
        it 'should return more than 100 components', (done) ->
            ui.send "component", "list"
            ui.on 'component-added', (name, definition) ->
                numberOfComponents = Object.keys(ui.components).length
                if numberOfComponents == 100
                    done()
        it 'should contain gegl:crop', ->
            chai.expect(ui.components['gegl/crop']).to.be.an 'object'

        describe 'gegl:crop component', ->
            it 'should have a "input" buffer port', ->
                input = ui.components['gegl/crop'].inPorts.filter (p) -> p.id == 'input'
                chai.expect(input.length).to.equal 1
                chai.expect(input[0].type).to.equal "buffer"
            it 'should have a "output" buffer port', ->
                output = ui.components['gegl/crop'].outPorts.filter (p) -> p.id == 'output'
                chai.expect(output.length).to.equal 1
                chai.expect(output[0].type).to.equal "buffer"
            it 'should also have inports for properties "x", "y", "width" and "height"', ->
                c = ui.components['gegl/crop']
                chai.expect(Object.keys(c.inPorts).length).to.equal 5
                chai.expect((c.inPorts.filter (p) -> p.id == 'width')[0].type).to.not.equal 'buffer'
                chai.expect((c.inPorts.filter (p) -> p.id == 'height')[0].type).to.not.equal 'buffer'
                chai.expect((c.inPorts.filter (p) -> p.id == 'x')[0].type).to.not.equal 'buffer'
                chai.expect((c.inPorts.filter (p) -> p.id == 'y')[0].type).to.not.equal 'buffer'

    describe 'graph building', ->
        outfile = 'testtemp/protocol-crop.png'
        it 'should not crash', (done) ->
            # FIXME: check and assert no errors on stderr of runtime process

            ui.send "graph", "clear"
            ui.send "graph", "addnode", {id: 'in', component: 'gegl/load'}
            ui.send "graph", "addnode", {id: 'filter', component: 'gegl/crop'}
            ui.send "graph", "addnode", {id: 'out', component: 'gegl/png-save'}
            ui.send "graph", "addedge", {src: {node: 'in', port: 'output'}, tgt: {node: 'filter', port: 'input'}}
            ui.send "graph", "addedge", {src: {node: 'filter', port: 'output'}, tgt: {node: 'out', port: 'input'}}
            ui.send "graph", "addinitial", {src: {data: 'examples/grid-toastybob.jpg'}, tgt: {node: 'in', port: 'path'}}
            ui.send "graph", "addinitial", {src: {data: outfile}, tgt: {node: 'out', port: 'path'}}

            ui.send "runtime", "getruntime"
            ui.once 'runtime-info-changed', ->
                done()

    describe 'starting the network', ->
        it 'should respond with network started', (done) ->
            ui.send "network", "start"
            ui.once 'network-running', (running) ->
                done() if running
        it 'should result in a created PNG file', (done) ->
            # TODO: add better API for getting notified of results
            # Maybe have a Processor node which sends string with URL of output when done?
            checkExistence = ->
                fs.exists outfile, (exists) ->
                    done() if exists
            setInterval checkExistence, 50

