
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
        if fs.exists wd
            for f in fs.readdirSync wd
                fs.unlinkSync path.join wd, f
        s = new server.Server wd
        s.listen 8888

    describe 'Get new image', ->
        it 'should be processed on demand', (finish) ->
            http.get urlbase+'/out.png', (response) ->
                chai.expect(response.statusCode).to.equal 200
                responseData = ""
                response.on 'data', (chunk) ->
                    fs.appendFile 'testout.png', chunk, ->
                        #
                response.on 'end', ->
                    return finish()

    after ->
        s.close()
