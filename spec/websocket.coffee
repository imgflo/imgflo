
websocket = require 'websocket'
assert = require 'assert'
child_process = require 'child_process'

describe 'NoFlo UI WebSocket API', () ->
    process = null    

    before ->
        args = ['./install/bin/noflo-gegl-runtime', '--port', '3888']
        process = child_process.spawn './install/env.sh', args
        process.on 'error', (err) ->
            assert.fail 'starting noflo-gegl-runtime', '', err.toString()
        process.on 'exit', (code, signal) ->
            assert.equal code, 0

        console.log "PID: ", process.pid
        process.stdout.on 'data', (d) ->
            console.log d.toString()
        process.stderr.on 'data', (d) ->
            console.log d.toString()
    after ->
        process.kill

    describe 'sending component list command', ->
        it 'should return all available components', (done) ->

# TODO: factor out into a NoFloUiMock object
            client = new websocket.client()

#            var expectedComponents = []
#            var receivedComponent = []

            client.on 'connect', (connection) ->
                connection.on 'error', (error) ->
                    assert.fail "connect OK", "connect error", error.toString();
                connection.on 'message', (message) ->
                    if message.type == 'utf8'
                        response = JSON.parse message.utf8Data
                        assert.equal response.protocol, "component"
                        assert.equal response.command, "component"

                        done()
# FIXME: verify
#                        receivedComponent.push(response.payload);


                msg = {"protocol": "component", "command": "list"};
                connection.sendUTF JSON.stringify msg

            client.connect 'ws://localhost:3888/', "noflo"

