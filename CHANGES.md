imgflo 0.3.0
=============
Released: January 23, 2015

See-also [imgflo-server release notes](https://github.com/jonnor/imgflo-server)

Changes
--------

Add support for editing component (GEGL/C code) in Flowhub. Most GEGL operations
now also have the source code available as metadata, and imgflo can return this over FBP protocol.

Support for Flowhub live-mode. Default webpage of runtime has link to open live-mode.

Support runtime:packet and runtime:ports messages, which allows
imgflo runtimes to be used as components in other FBP protocol runtimes (like NoFlo).
`make run GRAPH=mygraph.json` can be used to specify the initial graph.

Flowhub will now show preview images on edges when you click on them.

Error/status reporting has been improved. The runtime is now shown as *running* while
actively processing and then returns to finished when done. We also send graph changes
as output, and some errors. Improving error reporting further requires work in upstream GEGL.

Fix output preview when deployed to Heroku without settings HOST env-var.

imgflo codebase is now ran regularly against the Coverity Scan static analyzer,
and kept at zero defects.

imgflo 0.2.0
=============
Released: October 28th, 2014

server
-------
Moved to separate git repository [jonnor/imgflo-server](https://github.com/jonnor/imgflo-server)

runtime
--------
Registration as Flowhub.io runtime now done in main `imgflo` executable,
use environment variables `FLOWHUB_USER_ID` and `IMGFLO_RUNTIME_ID` to specify.

Support for running on Heroku out-of-the-box, just set above envvars
and push git repo to an heroku app and connect from [Flowhub](http://flowhub.io).

Added annotations for many more port types; including number, booleans, enums, colors.
This lets Flowhub bring up more suitable UIs than the general string input.
Added support for default values for IIPs.

Multiple graphs are now supported, allowing to switch
between graphs in a Flowhub project without wierd issues.

Fix an issue with Flowhub 0.2.0+ due to not setting runtime capabilities correctly.

The `Processor` component now checks that bounds have a sane value,
and disables processing if 0x0 or clips if above `max_size`.
NB: Currently *max_size limit not configurable*.

Default GEGL build has been updated to recent git master version,
and now includes the 'workshop' operations.


imgflo 0.1.0
=============
Released: April 30th, 2014

imgflo now consists of two complimentary parts:
a Flowhub-compatible runtime for interactively building image processing graphs,
and an image processing server for image processing on the web.

The runtime combined with the Flowhub IDE allows to visually create image
processing pipelines in a node-based manner, similar to tools like the Blender node compositor.
Live image output is displayed in the preview area in Flowhub, and will
update automatically when changing the graph.

The server provides a HTTP API for processing an input image with an imgflo graph.

    GET /graph/mygraph?input=urlencode(http://example.com/input-image.jpeg)&param1=foo&param2=bar

The input and processed image result will be cached on disk.
On later requests to the same URL, the image will be served from cache.

The server can be deployed to Heroku with zero setup, just push the git repository to an Heroku app.

The operations used in imgflo are provided by GEGL, and new operations can be added using the C API.
A (somewhat outdated) list of operations can be seen here: http://gegl.org/operations.html

Blogpost: http://www.jonnor.com/2014/04/imgflo-0-1-an-image-processing-server-and-flowhub-runtime

imgflo 0.0.1
=============
Released: April 8th, 2014

Can execute a simple graphics pipeline with GEGL operations defined as FBP .json.
