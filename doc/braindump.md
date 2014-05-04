
Usecases
---------
* Interactive GUI applications/tools
* Image processing server (see jonnor/gegl-server)
* Commandline, batch processing jobs

Combined client+server-side processing
--------------------------------------

Major challenge: getting identical results on the two widely different

Two technologies may allow to implement operations in the same way for both targets.
* WebGL, using OpenGL ES 2.0 GLSL shaders for processing. Also uses GPU.
* Emscripten+asm.js, compiling C/C++ code to JS. CPU-only.

In the future, WebCL may also be an option, but that looks to be a few years from now at least.

emscripten:
* http://blog.bitops.com/blog/2013/06/04/webraw-asmjs/

WebGL:
* http://evanw.github.io/webgl-filter/
* https://github.com/brianchirls/Seriously.js + https://github.com/forresto/noflo-seriously
* http://games.greggman.com/game/webgl-image-processing-continued/
* http://schibum.blogspot.no/2013/03/webgl-image-processing.html



