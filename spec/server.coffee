#     imgflo - Flowhub.io Image-processing runtime
#     (c) 2014 The Grid
#     imgflo may be freely distributed under the MIT license

server = require '../server'
chai = require 'chai'
http = require 'http'
fs = require 'fs'
path = require 'path'
url = require 'url'

urlbase = 'localhost:8888'

describe 'Server', ->
    s = null

    before ->
        wd = './testtemp'
        if fs.existsSync wd
            for f in fs.readdirSync wd
                fs.unlinkSync path.join wd, f
        s = new server.Server wd
        s.listen 8888

    describe 'Get image with custom p', ->
        d =
            input: "demo/grid-toastybob.jpg"
            height: 110
            width: 130
            x: 200
            y: 230
        u = url.format { protocol: 'http:', host: urlbase, pathname: '/graph/crop', query: d}
        console.log u

        it 'should be created on demand', (finish) ->
            http.get u, (response) ->
                chai.expect(response.statusCode).to.equal 200
                responseData = ""
                response.on 'data', (chunk) ->
                    fs.appendFile 'testout.png', chunk, ->
                        #
                response.on 'end', ->
                    return finish()

    after ->
        s.close()
