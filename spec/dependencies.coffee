#     imgflo - Flowhub.io Image-processing runtime
#     (c) 2014 The Grid
#     imgflo may be freely distributed under the MIT license

chai = require 'chai'
request = require 'request'
JSFtp = require 'jsftp'

fs = require 'fs'
path = require 'path'
url = require 'url'

trimmed = (s) ->
    return s.replace /^\s+|\s+$/g, ""

listVendorUrls = (service, callback) ->
    p = '.vendor_urls'
    fs.readFile p, {encoding: 'utf-8'}, (err, data) ->
        return callback err, null if err
        lines = data.split '\n'
        urls = (trimmed line.replace 'heroku', service for line in lines when (trimmed line))
        return callback null, urls

ftpFileExists = (fileUrl, callback) ->
    ftp = new JSFtp { host: 'vps.jonnor.com' }
    fileUrl = fileUrl.replace "ftp://#{ftp.host}/", ''
    ftp.get fileUrl, (err, res) ->
        return callback err, not err

describe 'Dependencies', ->

    describe "for Travis", ->
        it 'should be released', (done) ->
            @timeout 5000
            listVendorUrls 'travis', (err, urls) ->
                for u in urls
                    ftpFileExists u, (err, res) ->
                        chai.expect(res).to.equal true
                        done()

    describe "for Heroku", ->
        it 'should be released', (done) ->
            @timeout 5000
            listVendorUrls 'heroku', (err, urls) ->
                for u in urls
                    ftpFileExists u, (err, res) ->
                        chai.expect(res).to.equal true
                        done()
