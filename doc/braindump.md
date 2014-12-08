
Usecases
=========

Interactive GUI applications/tools
----------------------------------

Could be used as
* Configurable plugin system, for post/pre-processing
* Non-destructive image processing core

Would benefit from integration with
* NoFlo, noflo-gnome for general-purpose computation
* gegl-gtk/qt/clutter, for interactive preview
* libmypaint, for high-performance brush engine

Would benefit a lot from pre-build versions, including dependencies,
for Windows and OSX. Ideally also for Android and iOS.

Possible demos:
* FilterFun. Allow to apply filters to your images
* Sketchy. Paint/draw interactively on canvas

Image processing server
-------------------------

Offload image processing to "the cloud".
Especially interesting for non-interactive workloads,
where only one-time setup of the graphs are needed, or provided by image-analytics.

Possible demos:
* Facefinder. Gives you images cropped around faces
* Resizor. Resizes & optimizes images to fit within given envelope

Batch-processing usecases would benefit from an API where one can
queue new jobs, monitor and manipulate existing ones.
Film/3d-animation renderfarms might be interested in this type functionality

Commandline tool
-----------------------------------

Replacement for tools like imagemagick/graphicmagick
Should be easy to integrate into build-systems for software, like grunt/Make etc
Would benefit from PSD, XCF and good SVG support.
Would maybe benefit from internal support for .fbp files. Hack using noflo/fbp perhaps good-enough?

Possible demos:
* FilmTinter. Filter frames for 3d movies through a pipeline
* AssetGenerator. Exports/generates size/color variations of an design asset, like icons from SVG

Ideally there would be a "imgflo-client" executable that would
* Allow file uploads to a public service (could be imgflo server)
* Be able to start/stop/monitor jobs for batch-processing


Combined client+server-side processing
======================================

Major challenge: getting identical results on the two widely different platforms

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


General purpose FBP support
===========================
Scary extension of scope....

One could also generalize the current abstraction that represents "Processor"
as a FBP component into a mini C/glib-based FBP runtime.
It should then have solid GObject Introspection bindings, and be possible to use
the operations without modification in noflo-gnome.



Functinal high-performance programming
======================================

Can we express the C functions/blocks that 
performs the processing as something we can reason about
and manipulate using a functional language like Haskell?

Can we use this as a basis for automatic paralelliztion and inlining?

pixel -> pixel
pixels -> pixels

monoid:
closure
associative
identity
endomorphisms:functions with input and output of same type, is a monoid

"any function containing an endomorphism can be converted into a monoid"

monad laws are just monoid definitions in disguise

References

* monoids explanation slide 200+: http://www.slideshare.net/ScottWlaschin/fp-patterns-buildstufflt




