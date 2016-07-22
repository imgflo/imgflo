[![Build Status](https://travis-ci.org/jonnor/imgflo.svg?branch=master)](https://travis-ci.org/jonnor/imgflo)
[![Coverity Scan Status](https://scan.coverity.com/projects/3446/badge.svg)](https://scan.coverity.com/projects/3446)

imgflo
==========
imgflo is an image-processing runtime built on top of [GEGL](http://gegl.org)
which can be visually programmed using [Flowhub.io](http://flowhub.io).

It is used by [imgflo-server](https://github.com/imgflo/imgflo-server), which
adds a HTTP API for image processing.

imgflo is pronounced "Imageflo(w)".

[![Deploy](https://www.herokucdn.com/deploy/button.png)](https://heroku.com/deploy)

Changelog
----------
See [./CHANGES.md](./CHANGES.md)

License
--------
MIT

Note: GEGL itself is under LGPLv3.


About
--------
The imgflo runtime implements the [FBP runtime protocol]
(http://noflojs.org/documentation/protocol) over WebSockets.
It also provides an executable that can load and run a FBP graph defined as
[JSON](http://noflojs.org/documentation/json).

The runtime uses GEGLs native graph structure, wrapped to be compatible with
FBP conventions and protocols:

* All *GEGL operations* are automatically made available as *imgflo components*
* Each *imgflo process* is a *GeglNode*
* *imgflo edges* can pass data flowing between *GeglPad*

The edge limitation means that only ports with type *GeglBuffer* (image data) can be connected together.
Other data-types are in GEGL exposed as a *GProperty*, and can currently only be set it to a *FBP IIP* literal.
In the future, support for streaming property changes from outside is planned.

One exception is for the special *Processor* component, which is specific to imgflo.
This component is attached to outputs which are to be computed interactively.
Because GEGL does processing fully *on-demand*, something needs to *pull* at the edges
where image data should be realized.


Deploying to Heroku
==========================


Configure. Flowhub user id is found in the Flowhub user interface (Settings or Register runtime)

    heroku config:set HOSTNAME=YOURAPP.herokuapp.com
    heroku config:set FLOWHUB_USER_ID=MYUSERID

Check the log that the initial registration was successful, and then save the runtime ID permanently

    heroku logs
    heroku config:set IMGFLO_RUNTIME_ID=MYRUNTIMEID

See "Run the runtime" for more detail

Developing and running locally
==========================
Note: imgflo has only been tested on GNU/Linux and Mac OSX systems.
_Root is not needed_ for any of the build.

Pre-requisites
---------------
imgflo requires git master of GEGL and BABL, as well as a custom version of libsoup.
It is recommended to let make setup this for you, but you can use existing checkouts
by customizing PREFIX.

Install [node.js](https://nodejs.org/) dependencies. Only needed for tests

    npm install

For Mac OSX, you must [install Homebrew](http://brew.sh/)

### Building dependencies from source (recommended on Linux)

You only need to build the dependencies once, or when they have changed. See 'git log -- thirdparty'

    git submodule update --init
    make dependencies

_If_ you are on an older distribution, you may also need a newer glib version

    # make glib # only for older distros, where GEGL fails to build due to too old glib

### Installing pre-built dependencies (recommended on OSX)

If you use this option, you must specify `RELOCATE_DEPS=true` in the commands below.

    make travis-deps

Build
-------
Now you can build & install imgflo itself

    make install

To verify that things are working, run the test suite

    make check

Run
----
To start the runtime

    make run GRAPH=graphs/checker.json

This should open Flowhub automatically in your browser and connect to the runtime.

If the browser does not open, and you get "Operation not supported", add `NOAUTOLAUNCH=1`.
Then you need to copy/paste the "Live URL:" into your browser manually to connect.


## Registering runtime

imgflo can automatically ping Flowhub when it is available.

* Open [Flowhub](http://app.flowhub.io)
* Login with your Github account
* Click "Register" under "runtimes" to find your user ID. Copy it and paste in command below

Set up registration

    export FLOWHUB_USER_ID=MYUSERID

Finally, to run the Flowhub.io runtime use.
You can customize the port used by setting PORT=3322

    make run

If successful, you should see a message 'Registered runtime' with the new id.
You should save this, and on subsequent runs on same machine use this id.

    export IMGFLO_RUNTIME_ID=MYID

In Flowhub, refresh the runtimes and you should see your new "imgflo" instance. 
Note: sometimes a page refresh is needed.

You should now be able to create a new project in Flowhub of the "imgflo" type,
select your local runtime and create image processing graphs!

