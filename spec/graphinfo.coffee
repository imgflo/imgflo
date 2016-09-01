#     imgflo - Flowhub.io Image-processing runtime
#     (c) 2014-2016 The Grid
#     imgflo may be freely distributed under the MIT license

utils = require './utils'
fs = require 'fs'
path = require 'path'
os = require 'os'

chai = require 'chai'

# Used for checks which cannot be evaluated when running in debug,
# when we don't get the stdout of the runtime for instance.
debug = process.env.IMGFLO_TESTS_DEBUG?
itSkipDebug = if debug then it.skip else it
describeSkipMac = if os.platform() == 'darwin' then describe.skip else describe

projectDir = path.resolve __dirname, '..'
testDataDir = path.join projectDir, 'spec/data'

fixture = (name) ->
    return path.join testDataDir, 'graphs', name

graphInfo = (graphpath, callback) ->
    childProcess = require 'child_process'
    prog = './install/env.sh'
    args = ['imgflo-graphinfo', '--graph', graphpath]
    child = childProcess.execFile prog, args, callback

describeSkipMac 'imgflo-graphinfo', () ->

    describe 'with a basic graph', ->
        p = fixture 'enhancelowres.json'
        outgraph = null
        it 'should exit with success', (done) ->
            graphInfo p, (err, stdout, stderr) ->
                chai.expect(err).to.not.exist
                chai.expect(stderr).to.equal ""
                outgraph = JSON.parse stdout
                return done()
        it 'graph now has type on exported ports', () ->
            o = outgraph
            chai.expect(o.outports.output.metadata.type, 'output').to.equal 'buffer'
            chai.expect(o.inports.input.metadata.type, 'input').to.equal 'buffer'
            chai.expect(o.inports.iterations.metadata.type, 'iterations').to.equal 'int'
        it 'graph now has decription on exported ports', () ->
            [i, o] = [outgraph.inports, outgraph.outports]
            chai.expect(o.output.metadata.description, 'output').to.equal 'Image buffer'
            chai.expect(i.input.metadata.description, 'input').to.equal 'Image buffer'
            chai.expect(i.iterations.metadata.description).to.contain 'Controls the number of iterations'
        it 'unknown node metadata is preserved', () ->
            p = outgraph.outports.output
            chai.expect(p.metadata.x).to.be.a 'number'
            chai.expect(p.metadata.y).to.be.a 'number'

    describe 'graph with enum inport', ->
        p = fixture 'gaussianblur.json'
        outgraph = null
        it 'should exit with success', (done) ->
            graphInfo p, (err, stdout, stderr) ->
                chai.expect(err).to.not.exist
                chai.expect(stderr).to.equal ""
                outgraph = JSON.parse stdout
                return done()
        it 'inport now has enum values', () ->
            p = outgraph.inports['abyss-policy']
            chai.expect(p.metadata.type).to.equal 'enum'
            chai.expect(p.metadata.values).eql ['none', 'clamp', 'black']
        it 'inport now has default value', () ->
            p = outgraph.inports['abyss-policy']
            chai.expect(p.metadata.default).to.equal 'clamp'

    describe 'graph with numeric inport', ->
        it 'should exit with success'
        it 'should now have default value'
        it 'should now have min&max value'

    describe 'graph with default value set as IIP', ->
        it 'should exit with success'
        it 'default value comes from IIP not port'

    describe 'graph with port description set already', ->
        p = fixture 'enhancelowres_description.json'
        outgraph = null
        it 'should exit with success', (done) ->
            graphInfo p, (err, stdout, stderr) ->
                chai.expect(err).to.not.exist
                chai.expect(stderr).to.equal ""
                outgraph = JSON.parse stdout
                return done()
        it 'description should be kept', ->
            p = outgraph.inports.iterations
            chai.expect(p.metadata.description).to.contain 'Manually set description'

    describe 'non-imgflo graph', ->
        p = fixture 'noflo_insta_hefe.json'
        output = null
        it 'should exit with success', (done) ->
            graphInfo p, (err, stdout, stderr) ->
                output = stdout
                chai.expect(err).to.not.exist
                chai.expect(stderr).to.equal ""
                return done()
        it 'should output graph unchanged', (done) ->
            fs.readFile p, 'utf-8', (err, inp) ->
                chai.expect(err).to.not.exist
                input = JSON.parse inp
                out = JSON.parse output
                chai.expect(out).to.eql input
                return done()
