
VERSION=$(shell echo `git describe`)
#PREFIX=/opt/imgflo
PREFIX=$(shell echo `pwd`/install)
DEPS=$(shell $(PREFIX)/env.sh pkg-config --libs --cflags gegl-0.3 gio-unix-2.0 json-glib-1.0 libsoup-2.4)
FLAGS=-Wall -Werror -std=c99 -g
DEBUGPROG=
PORT=3569

GNOME_SOURCES=http://ftp.gnome.org/pub/gnome/sources

GLIB_MAJOR=2.38
GLIB_VERSION=2.38.2
GLIB_TARNAME=glib-$(GLIB_VERSION)

JSON_GLIB_MAJOR=1.0
JSON_GLIB_VERSION=1.0.0
JSON_GLIB_TARNAME=json-glib-$(JSON_GLIB_VERSION)

all: install

server: install
	npm start

run: install
	$(PREFIX)/env.sh $(DEBUGPROG) ./bin/imgflo-runtime --port $(PORT)

install: env imgflo imgflo-runtime
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

json-glib: env
	cd thirdparty && curl -o $(JSON_GLIB_TARNAME).tar.xz $(GNOME_SOURCES)/json-glib/$(JSON_GLIB_MAJOR)/$(JSON_GLIB_TARNAME).tar.xz
	cd thirdparty && tar -xf $(JSON_GLIB_TARNAME).tar.xz
	cd thirdparty/$(JSON_GLIB_TARNAME) && $(PREFIX)/env.sh ./configure --prefix=$(PREFIX)
	cd thirdparty/$(JSON_GLIB_TARNAME) && $(PREFIX)/env.sh make -j4 install

glib: env
	cd thirdparty && curl -o $(GLIB_TARNAME).tar.xz $(GNOME_SOURCES)/glib/$(GLIB_MAJOR)/$(GLIB_TARNAME).tar.xz
	cd thirdparty && tar -xf $(GLIB_TARNAME).tar.xz
	cd thirdparty/$(GLIB_TARNAME) && $(PREFIX)/env.sh ./autogen.sh --prefix=$(PREFIX)
	cd thirdparty/$(GLIB_TARNAME) && $(PREFIX)/env.sh make -j4 install

babl: env
	cd thirdparty/babl && $(PREFIX)/env.sh ./autogen.sh --prefix=$(PREFIX)
	cd thirdparty/babl && $(PREFIX)/env.sh make -j4 install

gegl: babl env
	cd thirdparty/gegl && $(PREFIX)/env.sh ./autogen.sh --prefix=$(PREFIX)
	cd thirdparty/gegl && $(PREFIX)/env.sh make -j4 install

libsoup: env
	cd thirdparty/libsoup && $(PREFIX)/env.sh ./autogen.sh --prefix=$(PREFIX) --disable-tls-check
	cd thirdparty/libsoup && $(PREFIX)/env.sh make -j4 install

heroku-deps: glib json-glib

dependencies: gegl babl libsoup

check: install
	npm test

clean:
	git clean -dfx --exclude node_modules --exclude install

release: check
	cd $(PREFIX) && tar -caf ../imgflo-$(VERSION).tgz ./

.PHONY=all imgflo imgflo-runtime run
