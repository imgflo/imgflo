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

projectDir = path.resolve __dirname, '..'
testDataDir = path.join projectDir, 'spec/data'

graphInfo = (graphpath, callback) ->
    child_process = require 'child_process'

describe 'imgflo-graphinfo', () ->

    describe 'with a base graph', ->
        it 'should exit with success'
        it 'graph now has type on exported ports'
        it 'graph now has decription on exported ports'
        it 'graph still has unknown node metadata'

    describe 'graph with enum inport', ->
        it 'should exit with success'
        it 'inport now has enum values'
        it 'inport now has default value'

    describe 'graph with numeric inport', ->
        it 'should exit with success'
        it 'should now have default value'
        it 'should now have min&max value'

    describe 'graph with default value set as IIP', ->
        it 'should exit with success'
        it 'default value comes from IIP not port'

    describe 'graph with port description set already', ->
        it 'should exit with success'
        it 'description should be kept'

    describe 'non-imgflo graph', ->
        it 'should exit with success'
        it 'should output graph unchanged'
