
websocket = require 'websocket'
child_process = require 'child_process'
EventEmitter = (require 'events').EventEmitter
chai = require 'chai'

# TODO: move into library, also use in MicroFlo and other FBP runtime implementations?
class MockUi extends EventEmitter
    @components = {}
    constructor: ->
        @client = new websocket.client()
        @connection = null
        @components = {}

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
            comp = d.payload
            id = comp.name
            @components[id] = comp
            @emit 'component-added', id, comp
        else
            console.log 'unknown message', d

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

    before (done) ->
        runtime.start ->
            ui.connect()
            ui.on 'connected', () ->
                done()
    after ->
        runtime.stop()
        ui.disconnect()

    describe 'sending component list command', ->
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

