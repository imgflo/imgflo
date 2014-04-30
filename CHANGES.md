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
