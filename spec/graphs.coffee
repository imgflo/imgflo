#     imgflo - Flowhub.io Image-processing runtime
#     (c) 2014 The Grid
#     imgflo may be freely distributed under the MIT license

server = require '../server'
chai = require 'chai'
yaml = require 'js-yaml'
request = require 'request'

http = require 'http'
fs = require 'fs'
path = require 'path'
url = require 'url'
child = require 'child_process'

urlbase = 'localhost:8888'

verbose = process.env.IMGFLO_TESTS_VERBOSE?

compareImages = (actual, expected, callback) ->
    cmd = "./install/env.sh ./install/bin/gegl-imgcmp #{actual} #{expected}"
    options =
        timeout: 2000
    child.exec cmd, options, (error, stdout, stderr) ->
        return callback error, stderr, stdout

requestUrl = (testcase) ->
    u = null
    if testcase._url
        u = 'http://'+ urlbase + testcase._url
    else
        graph = testcase._graph
        props = {}
        for key of testcase
            props[key] = testcase[key] if key != '_name' and key != '_graph'
        u = url.format { protocol: 'http:', host: urlbase, pathname: '/graph/'+graph, query: props}
    return u

requestFileFormat = (u) ->
    parsed = url.parse u
    graph = parsed.pathname.replace '/graph/', ''
    ext = (path.extname graph).replace '.', ''
    return ext || 'png'

class LogHandler
    @errors = null
    constructor: (server) ->
        @errors = []
        server.on 'logevent', @logEvent

    logEvent: (id, data) =>
        if id == 'process-request-end'
            if data.stderr
                for e in data.stderr.split '\n'
                    e = e.trim()
                    @errors.push e if e
            if data.err
                @errors.push data.err
        else if (id.indexOf 'error') != -1
            if data.err
                @errors.push data
        else if id == 'request-received' or id == 'serve-processed-file'
            #
        else
            console.log 'WARNING: unhandled log event', id

    popErrors: () ->
        errors = (e for e in @errors)
        @errors = []
        return errors

# End-to-end tests of image processing pipeline and included graphs
describe 'Graphs', ->
    s = null
    testcases = yaml.safeLoad fs.readFileSync 'spec/graphtests.yml', 'utf-8'
    l = null

    before ->
        wd = './graphteststemp'
        if fs.existsSync wd
            for f in fs.readdirSync wd
                fs.unlinkSync path.join wd, f
        s = new server.Server wd, null, null, verbose
        l = new LogHandler s
        s.listen 8888
    after ->
        s.close()

    for testcase in testcases

        describe "#{testcase._name}", ->
            reqUrl = requestUrl testcase
            ext = requestFileFormat reqUrl
            datadir = 'spec/data/'
            reference = path.join datadir, "#{testcase._name}.reference.#{ext}"
            output = path.join datadir, "#{testcase._name}.out.#{ext}"
            fs.unlinkSync output if fs.existsSync output

            it 'should have a reference result', (done) ->
                fs.exists reference, (exists) ->
                    chai.assert exists, 'Not found: '+reference
                    done()

            describe "GET #{reqUrl}", ->
                it 'should output a file', (done) ->
                    @timeout 8000
                    req = request reqUrl, (err, response) ->
                        done()
                    req.pipe fs.createWriteStream output

                it 'results should be equal to reference', (done) ->
                    compareImages output, reference, (error, stderr, stdout) ->
                        msg = "image comparison failed\n#{stderr}\n#{stdout}"
                        chai.assert not error?, msg
                        done()

                it 'should not cause errors', ->
                    chai.expect(l.popErrors()).to.deep.equal []

