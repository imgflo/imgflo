async = require 'async'
path = require 'path'

server = require './server'

main = () ->

    # FIXME: allow to specify all arguments on commandline
    filenames = process.argv[2..]
    outPrefix = './out/'
    concurrency = 5
    dryrun = false
    graph = require './graphs/gradientmap2.json'
    attributes = {
        'color1': '#131925ff'
        'color2': '#d7b879ff'
    }

    console.log filenames.length, filenames[0], '...'

    proc = new server.Processor false
    processFile = (filename, callback) ->
        outpath = outPrefix + path.basename filename
        g = server.prepareGraph graph, attributes, filename, outpath
        if dryrun
            return callback null, outpath
        else
            proc.run g, (err, stderr) ->
                return callback err, outpath

    async.mapLimit filenames, concurrency, processFile, (err, outputfiles) ->
        console.log err, outputfiles.length, outputfiles[0], '...'

exports.main = main
