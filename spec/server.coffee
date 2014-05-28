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

urlbase = 'localhost:8889'
port = 8889

graph_url = (graph, props) ->
    return url.format { protocol: 'http:', host: urlbase, pathname: '/graph/'+graph, query: props }

describe 'Server', ->
    s = null

    before ->
        wd = './testtemp'
        if fs.existsSync wd
            for f in fs.readdirSync wd
                fs.unlinkSync path.join wd, f
        s = new server.Server wd
        s.listen port

    after ->
        s.close()

    describe 'List graphs', ->
        expected = []
        for g in fs.readdirSync './graphs'
            expected.push g.replace '.json', '' if (g.indexOf '.json') != -1
        responseData = ""
        it 'HTTP request', (done) ->
            u = url.format {protocol:'http:',host: urlbase, pathname:'/demo'}
            http.get u, (response) ->
                chai.expect(response.statusCode).to.equal 200
                response.on 'data', (chunk) ->
                    responseData += chunk.toString()
                response.on 'end', () ->
                    done()
        it 'should return all graphs', () ->
            d = JSON.parse responseData
            actual = Object.keys d.graphs
            chai.expect(actual).to.deep.equal expected

    describe 'Graph request', ->
        describe 'with invalid graph parameters', ->
            u = graph_url 'gradientmap', {skeke: 299, oooor:222, input: "demo/grid-toastybob.jpg"}
            data = ""
            it 'should give HTTP 449', (done) ->
                http.get u, (response) ->
                    chai.expect(response.statusCode).to.equal 449
                    response.on 'data', (chunk) ->
                        data += chunk.toString()
                    response.on 'end', () ->
                        done()
            it 'should list valid parameters', ->
                d = JSON.parse data
                chai.expect(Object.keys(d.inports)).to.deep.equal ['input', 'color1', 'color2']

    describe 'Get image', ->
        it 'should be created on demand', (done) ->
            u = graph_url 'crop', { height: 110, width: 130, x: 200, y: 230, input: "demo/grid-toastybob.jpg" }
            http.get u, (response) ->
                chai.expect(response.statusCode).to.equal 200
                response.on 'data', (chunk) ->
                    fs.appendFile 'testout.png', chunk, ->
                        #
                response.on 'end', ->
                    fs.exists 'testout.png', (exists) ->
                        chai.assert exists, 'testout.png does not exist'
                        done()


