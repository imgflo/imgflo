#     imgflo - Flowhub.io Image-processing runtime
#     (c) 2014 The Grid
#     imgflo may be freely distributed under the MIT license

server = require '../server'
chai = require 'chai'
yaml = require 'js-yaml'

http = require 'http'
fs = require 'fs'
path = require 'path'
url = require 'url'
child = require 'child_process'

urlbase = 'localhost:8888'

compareImages = (actual, expected, callback) ->
    cmd = "./install/env.sh ./install/bin/gegl-imgcmp #{actual} #{expected}"
    options =
        timeout: 1000
    child.exec cmd, options, (error, stdout, stderr) ->
        return callback error, stderr

# End-to-end tests of image processing pipeline and included graphs
describe 'Graphs', ->
    s = null
    testcases = yaml.safeLoad fs.readFileSync 'spec/graphtests.yml', 'utf-8'

    before ->
        wd = './graphteststemp'
        if fs.existsSync wd
            for f in fs.readdirSync wd
                fs.unlinkSync path.join wd, f
        s = new server.Server wd
        s.listen 8888
    after ->
        s.close()

    for testcase in testcases

        describe "#{testcase._name}", ->
            datadir = 'spec/data/'
            reference = path.join datadir, "#{testcase._name}.reference.png"
            output = path.join datadir, "#{testcase._name}.out.png"

            it 'should have a reference result', (done) ->
                fs.exists reference, (exists) ->
                    chai.assert exists, 'Not found: '+reference
                    done()
            describe 'graph execution', ->
                graph = testcase._graph
                props = {}
                for key of testcase
                    props[key] = testcase[key] if key != '_name' and key != '_graph'
                request = url.format { protocol: 'http:', host: urlbase, pathname: '/graph/'+graph, query: props}

                it 'should output a file', (done) ->
                    http.get request, (response) ->
                        chai.expect(response.statusCode).to.equal 200
                        response.on 'data', (chunk) ->
                            fs.appendFile output, chunk, ->
                                #
                        response.on 'end', ->
                            return done()

                it 'results should be equal to reference', (done) ->
                    compareImages output, reference, (error, msg) ->
                        chai.assert error == null, msg
                        done()


