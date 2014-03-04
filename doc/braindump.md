
Usecases
---------
* Interactive GUI applications/tools
* Image processing server (see jonnor/gegl-server)
* Commandline, batch processing jobs

Implementation
-----------------
* Follow similar path as noflo-clutter. Should maybe split out the core things to common repo?
* Use a GeglNode/GeglOp baseclass, uses (GI) introspection + annotations to bind a specific OP
* Support both Node.js+node-gir and GJS, try to encapsulate the differences in baseclass
* Create an "interactive widget" for web, which sends new image over WebSocket/HTTP and displays in browser on changes
* Also wrap gegl-gtk and/or gegl-clutter view widget, for use in desktop-style apps
* How to handle that some GEGL ops are not always available?
