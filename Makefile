
#PREFIX=/opt/imgflo
PREFIX=$(shell echo `pwd`/install)
DEPS=$(shell $(PREFIX)/env.sh pkg-config --libs --cflags gegl-0.3 json-glib-1.0 libsoup-2.4)
FLAGS=-Wall -Werror -std=c99 -g
DEBUGPROG=

GLIB_MAJOR=2.38
GLIB_VERSION=2.38.2

all: run

run: install
	$(PREFIX)/env.sh $(DEBUGPROG) ./bin/imgflo-runtime

install: imgflo imgflo-runtime
	cp ./bin/imgflo $(PREFIX)/bin/
	cp ./bin/imgflo-runtime $(PREFIX)/bin/

imgflo:
	$(PREFIX)/env.sh gcc -o ./bin/imgflo bin/imgflo.c -I. $(FLAGS) $(DEPS)

imgflo-runtime:
	$(PREFIX)/env.sh gcc -o ./bin/imgflo-runtime bin/imgflo-runtime.c -I. $(FLAGS) $(DEPS)

env:
	mkdir -p $(PREFIX) || true
	sed -e 's|@PREFIX@|$(PREFIX)|' env.sh.in > $(PREFIX)/env.sh
	chmod +x $(PREFIX)/env.sh

glib: env
	cd thirdparty && wget http://ftp.gnome.org/pub/gnome/sources/glib/$(GLIB_MAJOR)/glib-$(GLIB_VERSION).tar.xz
	cd thirdparty && tar -xf glib-$(GLIB_VERSION).tar.xz
	cd thirdparty/glib-$(GLIB_VERSION) && $(PREFIX)/env.sh ./autogen.sh --prefix=$(PREFIX)
	cd thirdparty/glib-$(GLIB_VERSION) && $(PREFIX)/env.sh make -j4 install

babl: env
	cd thirdparty/babl && $(PREFIX)/env.sh ./autogen.sh --prefix=$(PREFIX)
	cd thirdparty/babl && $(PREFIX)/env.sh make -j4 install

gegl: babl env
	cd thirdparty/gegl && $(PREFIX)/env.sh ./autogen.sh --prefix=$(PREFIX)
	cd thirdparty/gegl && $(PREFIX)/env.sh make -j4 install

libsoup: env
	cd thirdparty/libsoup && $(PREFIX)/env.sh ./autogen.sh --prefix=$(PREFIX) --disable-tls-check
	cd thirdparty/libsoup && $(PREFIX)/env.sh make -j4 install

dependencies: gegl babl libsoup

check: install
	npm test

.PHONY=all imgflo imgflo-runtime run
