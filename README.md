[![Build Status](https://travis-ci.org/jonnor/imgflo.svg?branch=master)](https://travis-ci.org/jonnor/imgflo)

imgflo
==========
imgflo is an image-processing server build on top of [GEGL](http://gegl.org)
which can be visually programmed using [Flowhub.io](http://flowhub.io).

imgflo is pronounced "Imageflo(w)".


Changelog
----------
See [./CHANGES.md](./CHANGES.md)

License
--------
MIT

Note: GEGL itself is under LGPLv3.

Deploying to Heroku
==========================

Server
--------
Register/log-in with [Heroku](http://heroku.com), and create a new app. First one is free.

Specify the multi buildpack, either at app creation time, in Heroku webui or using

    heroku config:set BUILDPACK_URL=https://github.com/ddollar/heroku-buildpack-multi.git

In your git checkout of imgflo, add your Heroku app as a remote.

    git remote add heroku git@heroku.com:YOURAPP.git

Deploy, will take ~1 minute

    git push heroku

You should now see the imgflo server running at http://YOURAPP.herokuapps.com


Runtime
--------
TODO


Developing and running locally
==========================
Note: imgflo has only been tested on GNU/Linux systems.
_Root is not needed_ for any of the build.

Pre-requisites
---------------
imgflo requires git master of GEGL and BABL, as well as a custom version of libsoup.
It is recommended to let make setup this for you, but you can use existing checkouts
by customizing PREFIX.
You only need to install the dependencies once, or when they have changed, see 'git log -- thirdparty'

    git submodule update --init
    make dependencies

_If_ you are on an older distribution, you may also need a newer glib version

    # make glib # only for older distros, where GEGL fails to build due to too old glib

Install node.js dependencies

    npm install

Build
-------
Now you can build & install imgflo itself

    make install

To verify that things are working, run the test suite

    make check

Run the runtime
----------------

To actually be able to use it from Flowhub, you need to register the runtime (once).
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

Running server
----------------

    node index.js

You should see your server running at http://localhost:8080


