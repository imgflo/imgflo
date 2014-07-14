#     imgflo - Flowhub.io Image-processing runtime
#     (c) 2014 The Grid
#     imgflo may be freely distributed under the MIT license

utils = require './utils'
fs = require 'fs'

chai = require 'chai'

debug = process.env.IMGFLO_TESTS_DEBUG?

describe 'NoFlo runtime API,', () ->
    runtime = new utils.RuntimeProcess debug
    ui = new utils.MockUi

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
        it 'type should be "imgflo"', ->
            chai.expect(info.type).to.equal "imgflo"
        it 'protocol version should be "0.4"', ->
            chai.expect(info.version).to.be.a "string"
            chai.expect(info.version).to.equal "0.4"
        it 'capabilities should include "protocol:component"', ->
            chai.expect(info.capabilities).to.be.an "array"
            chai.expect(info.capabilities.length).to.equal 1
            chai.expect((info.capabilities.filter -> 'protocol:component')[0]).to.be.a "string"

    describe 'sending component list', ->
        it 'should return more than 100 components', (done) ->
            @timeout 5000 # FIXME: make faster
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
                chai.expect((c.inPorts.filter (p) -> p.id == 'width')[0].type).to.equal 'number'
                chai.expect((c.inPorts.filter (p) -> p.id == 'height')[0].type).to.equal 'number'
                chai.expect((c.inPorts.filter (p) -> p.id == 'x')[0].type).to.equal 'number'
                chai.expect((c.inPorts.filter (p) -> p.id == 'y')[0].type).to.equal 'number'
            it 'should have default value for properties', ->
                c = ui.components['gegl/crop']
                chai.expect((c.inPorts.filter (p) -> p.id == 'width')[0].default).to.be.a.number
                chai.expect((c.inPorts.filter (p) -> p.id == 'width')[0].default).to.equal 10
                chai.expect((c.inPorts.filter (p) -> p.id == 'height')[0].default).to.equal 10
                chai.expect((c.inPorts.filter (p) -> p.id == 'x')[0].default).to.equal 0
                chai.expect((c.inPorts.filter (p) -> p.id == 'y')[0].default).to.equal 0
            it 'should have descriptions value for properties', ->
                c = ui.components['gegl/crop']
                p = (c.inPorts.filter (p) -> p.id == 'width')[0]
                chai.expect(p.description).to.be.a.string
                chai.expect(p.description).to.equal "Width"
                p = (c.inPorts.filter (p) -> p.id == 'x')[0]
                chai.expect(p.description).to.be.a.string
                chai.expect(p.description).to.equal "X"
            it 'should have icon "fa-crop"', ->
                chai.expect(ui.components['gegl/crop'].icon).to.equal 'crop'
            it 'should have description', ->
                chai.expect(ui.components['gegl/crop'].description).to.equal 'Crop a buffer'

        describe 'gegl:png-save component', ->
            c = 'gegl/png-save'
            it 'should have a "input" buffer port', ->
                input = ui.components[c].inPorts.filter (p) -> p.id == 'input'
                chai.expect(input.length).to.equal 1
                chai.expect(input[0].type).to.equal "buffer"
            it 'should have a dummy "process" port', ->
                proccess = ui.components[c].outPorts.filter (p) -> p.id == 'process'
                chai.expect(proccess.length).to.equal 1
                chai.expect(proccess[0].type).to.equal "N/A"

        describe 'gegl:png-load component', ->
            c = 'gegl/png-load'
            it 'should not have a "input" buffer port', ->
                input = ui.components[c].inPorts.filter (p) -> p.id == 'input'
                chai.expect(input.length).to.equal 0
            it 'should have a "output" port', ->
                output = ui.components[c].outPorts.filter (p) -> p.id == 'output'
                chai.expect(output.length).to.equal 1
                chai.expect(output[0].type).to.equal "buffer"

        describe 'gegl/add component', ->
            c = 'gegl/add'
            it 'should have "input" and "aux" buffer ports', ->
                input = ui.components[c].inPorts.filter (p) -> p.id == 'input'
                aux = ui.components[c].inPorts.filter (p) -> p.id == 'aux'
                chai.expect(input.length).to.equal 1
                chai.expect(input[0].type).to.equal "buffer"
                chai.expect(aux.length).to.equal 1
                chai.expect(aux[0].type).to.equal "buffer"
            it 'should have a "output" port', ->
                output = ui.components[c].outPorts.filter (p) -> p.id == 'output'
                chai.expect(output.length).to.equal 1
                chai.expect(output[0].type).to.equal "buffer"

        describe 'gegl/shear component', ->
            c = 'gegl/shear'
            it 'should have an GeglSamplerType enum for its "sampler" port', ->
                port = ui.components[c].inPorts.filter (p) -> p.id == 'sampler'
                chai.expect(port.length).to.equal 1
                chai.expect(port[0].type).to.equal "enum"
                chai.expect(port[0].values).to.deep.equal ["nearest", "linear", "cubic", "nohalo"]


    describe 'graph building', ->

        # TODO: verify responses being received
        # TODO: add graph identifier

        outfile = 'testtemp/protocol-crop.png'
        it 'should not crash', (done) ->
            ui.send "graph", "clear"
            ui.send "graph", "addnode", {id: 'in', component: 'gegl/load'}
            ui.send "graph", "addnode", {id: 'filter', component: 'gegl/crop'}
            ui.send "graph", "addnode", {id: 'out', component: 'gegl/png-save'}
            ui.send "graph", "addnode", {id: 'proc', component: 'Processor'}
            ui.send "graph", "addedge", {src: {node: 'in', port: 'output'}, tgt: {node: 'filter', port: 'input'}}
            ui.send "graph", "addedge", {src: {node: 'filter', port: 'output'}, tgt: {node: 'out', port: 'input'}}
            ui.send "graph", "addedge", {src: {node: 'out', port: 'ANY'}, tgt: {node: 'proc', port: 'node'}}
            ui.send "graph", "addinitial", {src: {data: 'examples/grid-toastybob.jpg'}, tgt: {node: 'in', port: 'path'}}
            ui.send "graph", "addinitial", {src: {data: outfile}, tgt: {node: 'out', port: 'path'}}

            ui.send "runtime", "getruntime"
            ui.once 'runtime-info-changed', ->
                done()

        it 'should not have produced any errors', ->
            chai.expect(runtime.popErrors()).to.eql []

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
        it 'should not have produced any errors', ->
            chai.expect(runtime.popErrors()).to.eql []

    describe 'stopping the network', ->

        it 'should respond with network stopped', (done) ->
            ui.send "network", "stop"
            ui.once 'network-running', (running) ->
                done() if not running
        it 'should not have produced any errors', ->
            chai.expect(runtime.popErrors()).to.eql []

    describe 'graph tear down', ->

        it 'should not crash', (done) ->

            ui.send "graph", "removeinitial", {tgt: {node: 'in', port: 'path'}}
            ui.send "graph", "removeinitial", {tgt: {node: 'out', port: 'path'}}
            ui.send "graph", "removeedge", {src: {node: 'in', port: 'output'}, tgt: {node: 'filter', port: 'input'}}
            ui.send "graph", "removeedge", {src: {node: 'filter', port: 'output'}, tgt: {node: 'out', port: 'input'}}
            ui.send "graph", "removeedge", {src: {node: 'out', port: 'ANY'}, tgt: {node: 'proc', port: 'node'}}
            ui.send "graph", "removenode", {id: 'in'}
            ui.send "graph", "removenode", {id: 'filter'}
            ui.send "graph", "removenode", {id: 'out'}
            ui.send "graph", "removenode", {id: 'proc'}

            # TODO: use getgraph command to verify graph is now empty

            ui.send "runtime", "getruntime"
            ui.once 'runtime-info-changed', ->
                done()

        it 'should not have produced any errors', ->
            chai.expect(runtime.popErrors()).to.eql []

    describe 'processing a node with infinite bounding box', ->

        it 'should give 200 OK', (done) ->
            ui.send "graph", "clear"
            ui.send "graph", "addnode", {id: 'in', component: 'gegl/checkerboard'}
            ui.send "graph", "addnode", {id: 'proc', component: 'Processor'}
            ui.send "graph", "addedge", {src: {node: 'in', port: 'output'}, tgt: {node: 'proc', port: 'input'}}

            # TODO: test, and should be cropped
            ui.send "runtime", "getruntime"
            ui.once 'runtime-info-changed', ->
                utils.processNode 'proc', (err, resp) ->
                    chai.expect(err).to.equal null
                    chai.expect(resp.statusCode).to.equal 200
                    chai.expect(resp.headers['content-type']).to.equal "image/png"
                    done()

        it 'should have produced two errors', ->
            errors = runtime.popErrors()
            chai.expect(errors).to.have.length 2
