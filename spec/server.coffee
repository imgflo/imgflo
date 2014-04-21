#     imgflo - Flowhub.io Image-processing runtime
#     (c) 2014 The Grid
#     imgflo may be freely distributed under the MIT license

server = require '../server'
chai = require 'chai'
http = require 'http'
fs = require 'fs'
path = require 'path'

urlbase = 'http://localhost:8888'

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
        # TODO: use a CC image
        # TODO: serve file locally, avoid always fetching from internet
        img = "http://www.petfinder.com/wp-content/uploads/2012/11/101418789-cat-panleukopenia-fact-sheet-632x475.jpg"
        src = new Buffer(img).toString('base64')
        url = urlbase+"/graph/crop?x=200&y=230&height=110&width=130&input=#{src}"

        it 'should be created on demand', (finish) ->
            http.get url, (response) ->
                chai.expect(response.statusCode).to.equal 200
                responseData = ""
                response.on 'data', (chunk) ->
                    fs.appendFile 'testout.png', chunk, ->
                        #
                response.on 'end', ->
                    return finish()

    after ->
        s.close()
