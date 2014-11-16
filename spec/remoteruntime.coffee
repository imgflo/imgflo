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

describe 'FBP protocol, remote runtime,', () ->
    runtime = new utils.RuntimeProcess debug, 'graphs/checker.json'
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

    describe 'initial ports information', ->
        info = null
        it 'should be returned on getruntime', (done) ->
            ui.send "runtime", "getruntime"
            ui.once 'runtime-ports-changed', (i) ->
                info = i
                chai.expect(info).to.be.an 'object'
                done()
        it 'should have intial graph', ->
            chai.expect(info.graph).to.equal 'default/main'
        it 'should have inports', ->
            chai.expect(info.inPorts).to.be.an 'array'
            chai.expect(info.inPorts).to.have.length 1
        it 'should have outports', ->
            chai.expect(info.outPorts).to.be.an 'array'
            chai.expect(info.outPorts).to.have.length 1

        itSkipDebug 'should not have produced any errors', ->
            chai.expect(runtime.popErrors()).to.eql []

    describe 'sending packet in', ->
        graphName = 'default/main'
        it 'gives packet out', (done) ->
            ui.on 'runtime-packet', (data) ->
                chai.expect(data.event).to.equal 'data'
                chai.expect(data.graph).to.equal graphName
                chai.expect(data.port).to.equal 'output'
                chai.expect(data.payload).to.contain 'http://localhost'
                chai.expect(data.payload).to.contain '/process'
                done()
            ui.send 'runtime', 'packet',
                event: 'data'
                graph: graphName
                port: 'x'
                payload: 32

        itSkipDebug 'should not have produced any errors', ->
            chai.expect(runtime.popErrors()).to.eql []
