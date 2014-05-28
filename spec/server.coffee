#     imgflo - Flowhub.io Image-processing runtime
#     (c) 2014 The Grid
#     imgflo may be freely distributed under the MIT license

# TODO: add more tests
# processing new (200 or 301 to image/ ? )
#   works - with remote HTTP URL
#   w - remote HTTP with .PNG
#   w - remote HTTP with .JPG
#   w - remote HTTP without extension, content type image/png
#   w - remote HTTP without extension, .. image/jpeg
#   w - HTTP url with redirect
#
#   ? - content-type different from extension
#
#   maybe; w - content type application/x-octet-stream, but valid JPG/PNG
#
#   error - unsupported content type (449)
#   e - input parameter missing
#   e - url gives 404 (404)
#   e - url does not load for other reasons (504)
#   e - invalid parameters, return valid params (449)
#   e - invalid parameter value (422?)
#      -- too big/small numbers
#   e - valid ext,content-type, but invalid image
#
#   w - get cached / previously processed image (301)
#       .. test all the processing cases
#
# stress
# works - multiple requests at same time
#
# error - too many requests (429?)
# e - too big file download, bytes: (413)
# e - too large image, width/height (413)
#
# adverse conditions
# e - compression explosion exploits
# e - code-in-metadata-chunks
# e - input url points to localhost (local to server)
# e - infinetely/circular redirects
# e - chained/recursive processing requests
#
# introspection
#  w - list available graphs, and their properties

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

    after ->
        s.close()

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


