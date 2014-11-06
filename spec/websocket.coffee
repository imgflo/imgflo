#     imgflo - Flowhub.io Image-processing runtime
#     (c) 2014 The Grid
#     imgflo may be freely distributed under the MIT license

utils = require './utils'
fs = require 'fs'
path = require 'path'

chai = require 'chai'

debug = process.env.IMGFLO_TESTS_DEBUG?
# Used for checks which cannot be evaluated when running in debug,
# when we don't get the stdout of the runtime for instance.
itSkipDebug = if debug then it.skip else it

projectDir = path.resolve __dirname, '..'
testDataDir = path.join projectDir, 'spec/data'
tempDir = path.join projectDir, 'spec/out'

describe 'FBP runtime protocol,', () ->
    runtime = new utils.RuntimeProcess debug
    ui = new utils.MockUi

    utils.rmrf tempDir
    fs.mkdirSync tempDir
    outfile = null

    before (done) ->
        runtime.start ->
            ui.connect()
            ui.on 'connected', () ->
                done()
    after (done) ->
        ui.disconnect()
        ui.on 'disconnected', () ->
            runtime.stop () ->
                done()

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
        describe 'capabilities"', ->
            it 'should be an array', ->
                chai.expect(info.capabilities).to.be.an "array"
            it 'should include "protocol:component"', ->
                chai.expect(info.capabilities).to.include "protocol:component"
            it 'should include "protocol:graph"', ->
                chai.expect(info.capabilities).to.include "protocol:graph"
            it 'should include "protocol:network"', ->
                chai.expect(info.capabilities).to.include "protocol:network"
            it 'should include "component:getsource"', ->
                chai.expect(info.capabilities).to.include "component:getsource"
            it 'should include "component:setsource"', ->
                chai.expect(info.capabilities).to.include "component:setsource"

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

        # TODO: move these checks to
        describe 'gegl:jpg-load component', ->
            c = 'gegl/jpg-load'
            it 'should have a "output" port', ->
                output = ui.components[c].outPorts.filter (p) -> p.id == 'output'
                chai.expect(output.length).to.equal 1
                chai.expect(output[0].type).to.equal "buffer"

        describe 'gegl:jpg-save component', ->
            c = 'gegl/jpg-save'
            it 'should have a "input" buffer port', ->
                input = ui.components[c].inPorts.filter (p) -> p.id == 'input'
                chai.expect(input.length).to.equal 1
                chai.expect(input[0].type).to.equal "buffer"

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
        graph = 'graph1'
        # TODO: verify responses being received
        send = (protocol, cmd, pay, g) ->
            if graph?
                pay.graph = g
            ui.send protocol, cmd, pay
        ui.graph1 =
            send: (cmd, pay) ->
                send "graph", cmd, pay, graph

        outfile = 'spec/out/protocol-crop.png'
        it 'should not crash', (done) ->
            ui.send "graph", "clear", {id: graph}
            ui.graph1.send "addnode", {id: 'in', component: 'gegl/load'}
            ui.graph1.send "addnode", {id: 'filter', component: 'gegl/crop'}
            ui.graph1.send "addnode", {id: 'out', component: 'gegl/png-save'}
            ui.graph1.send "addnode", {id: 'proc', component: 'Processor'}
            ui.graph1.send "addedge", {src: {node: 'in', port: 'output'}, tgt: {node: 'filter', port: 'input'}}
            ui.graph1.send "addedge", {src: {node: 'filter', port: 'output'}, tgt: {node: 'out', port: 'input'}}
            ui.graph1.send "addedge", {src: {node: 'out', port: 'ANY'}, tgt: {node: 'proc', port: 'node'}}
            ui.graph1.send "addinitial", {src: {data: testDataDir+'/grid-toastybob.jpg'}, tgt: {node: 'in', port: 'path'}}
            ui.graph1.send "addinitial", {src: {data: outfile}, tgt: {node: 'out', port: 'path'}}

            ui.send "runtime", "getruntime"
            ui.once 'runtime-info-changed', ->
                done()

        itSkipDebug 'should not have produced any errors', ->
            chai.expect(runtime.popErrors()).to.eql []

    describe 'starting the network', ->
        graph = 'graph1'

        it 'should respond with network started', (done) ->
            ui.send "network", "start", {graph: graph}
            ui.once 'network-running', (running) ->
                done() if running
        it 'should result in a created PNG file', (done) ->
            # TODO: add better API for getting notified of results
            # Maybe have a Processor node which sends string with URL of output when done?
            checkInterval = null
            checkExistence = ->
                fs.exists outfile, (exists) ->
                    done() if exists
                    clearInterval checkInterval
            checkInterval = setInterval checkExistence, 50
        itSkipDebug 'should not have produced any errors', ->
            chai.expect(runtime.popErrors()).to.eql []

    describe 'stopping the network', ->
        graph = 'graph1'

        it 'should respond with network stopped', (done) ->
            ui.send "network", "stop", {graph: graph}
            ui.once 'network-running', (running) ->
                done() if not running
        itSkipDebug 'should not have produced any errors', ->
            chai.expect(runtime.popErrors()).to.eql []

    describe 'graph tear down', ->

        it 'should not crash', (done) ->

            ui.graph1.send "removeinitial", {tgt: {node: 'in', port: 'path'}}
            ui.graph1.send "removeinitial", {tgt: {node: 'out', port: 'path'}}
            ui.graph1.send "removeedge", {src: {node: 'in', port: 'output'}, tgt: {node: 'filter', port: 'input'}}
            ui.graph1.send "removeedge", {src: {node: 'filter', port: 'output'}, tgt: {node: 'out', port: 'input'}}
            ui.graph1.send "removeedge", {src: {node: 'out', port: 'ANY'}, tgt: {node: 'proc', port: 'node'}}
            ui.graph1.send "removenode", {id: 'in'}
            ui.graph1.send "removenode", {id: 'filter'}
            ui.graph1.send "removenode", {id: 'out'}
            ui.graph1.send "removenode", {id: 'proc'}

            # TODO: use getgraph command to verify graph is now empty

            ui.send "runtime", "getruntime"
            ui.once 'runtime-info-changed', ->
                done()

        it 'should not have produced any errors', ->
            chai.expect(runtime.popErrors()).to.eql []

    describe 'processing a node without anything connected', ->

        it 'should give 404', (done) ->
            graph = 'not-connected-graph'
            ui.send "graph", "clear", {id: graph}
            ui.send "graph", "addnode", {id: 'proc', component: 'Processor', graph: graph}
            ui.send "runtime", "getruntime"
            ui.once 'runtime-info-changed', ->
                utils.processNode graph, 'proc', (err, resp) ->
                    chai.expect(err).to.equal null
                    chai.expect(resp.statusCode).to.equal 400
                    done()

        itSkipDebug 'should have produced 1 error', ->
            errors = runtime.popErrors()
            chai.expect(errors).to.have.length 1, errors.toString()

    describe 'processing a node with infinite bounding box', ->

        it 'should give 200 OK', (done) ->
            graph = 'infinite-graph'
            ui.send "graph", "clear", {id: graph}
            ui.send "graph", "addnode", {id: 'in', component: 'gegl/checkerboard', graph: graph}
            ui.send "graph", "addnode", {id: 'proc', component: 'Processor', graph: graph}
            ui.send "graph", "addedge", {src: {node: 'in', port: 'output'}, tgt: {node: 'proc', port: 'input'}, graph: graph}

            # TODO: test that image is should be cropped
            ui.send "runtime", "getruntime"
            ui.once 'runtime-info-changed', ->
                utils.processNode graph, 'proc', (err, resp) ->
                    chai.expect(err).to.equal null
                    chai.expect(resp.statusCode).to.equal 200
                    chai.expect(resp.headers['content-type']).to.equal "image/png"
                    done()

        itSkipDebug 'should have produced 2 errors', ->
            errors = runtime.popErrors()
            chai.expect(errors).to.have.length 2, errors.toString()

    describe 'adding a component using component:source', ->
        @timeout 2000
        code = utils.testData 'dynamiccomponent1.c'
        opname = 'dynamiccomponent1'
        it 'should give component:component', (done) ->
            ui.send "component", "source",
                name: opname,
                language: 'c',
                library: 'imgflo'
                code: code
            ui.once 'component-added', ->
                chai.expect(ui.components).to.include.keys opname
                x = ui.components[opname].inPorts.filter (p) -> p.id == 'x'
                chai.expect(x).to.have.length 0
                done()

        itSkipDebug 'should not have produced any errors', ->
            chai.expect(runtime.popErrors()).to.eql []

    describe 'getting a component using component:getsource', ->
        code = utils.testData 'dynamiccomponent1.c'
        opname = 'dynamiccomponent1'
        it 'should give component:source', (done) ->
            ui.send "component", "getsource",
                name: opname
            ui.once 'component-source', (id) ->
                chai.expect(ui.components).to.include.keys opname
                chai.expect(ui.components[opname]).to.include.keys 'source'
                chai.expect(ui.components[opname].source).to.equal code
                done()

        itSkipDebug 'should not have produced any errors', ->
            chai.expect(runtime.popErrors()).to.eql []

    describe 'changing a component using component:source', ->
        @timeout 2000
        code = utils.testData 'dynamiccomponent1-withprop.c'
        opname = 'dynamiccomponent1'
        it 'should now have a X property', (done) ->
            ui.send "component", "source",
                name: opname,
                language: 'c',
                library: 'imgflo'
                code: code
            ui.once 'component-added', ->
                chai.expect(ui.components).to.include.keys opname
                x = ui.components[opname].inPorts.filter (p) -> p.id == 'x'
                chai.expect(x).to.have.length 1
                chai.expect(x[0].type).to.equal 'number'
                done()

        itSkipDebug 'should not have produced any errors', ->
            chai.expect(runtime.popErrors()).to.eql []
