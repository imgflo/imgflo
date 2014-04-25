
VERSION=$(shell echo `git describe`)
#PREFIX=/opt/imgflo
PREFIX=$(shell echo `pwd`/install)
FLAGS=-Wall -Werror -std=c99 -g
DEBUGPROG=
PORT=3569

ifneq ("$(wildcard /app)","")
# Heroku build. TODO: find better way to detect
PKGCONFIG_ARGS:=--define-variable=prefix=$(PREFIX)
else
PKGCONFIG_ARGS:=
endif

LIBS=gegl-0.3 gio-unix-2.0 json-glib-1.0 libsoup-2.4
DEPS=$(shell $(PREFIX)/env.sh pkg-config $(PKGCONFIG_ARGS) --libs --cflags $(LIBS))

GNOME_SOURCES=http://ftp.gnome.org/pub/gnome/sources

GLIB_MAJOR=2.38
GLIB_VERSION=2.38.2
GLIB_TARNAME=glib-$(GLIB_VERSION)

JSON_GLIB_MAJOR=1.0
JSON_GLIB_VERSION=1.0.0
JSON_GLIB_TARNAME=json-glib-$(JSON_GLIB_VERSION)

LIBFFI_VERSION=3.0.13
LIBFFI_TARNAME=libffi-$(LIBFFI_VERSION)

INTLTOOL_MAJOR=0.40
INTLTOOL_VERSION=0.40.6
INTLTOOL_TARNAME=intltool-$(INTLTOOL_VERSION)

SQLITE_TARNAME=sqlite-autoconf-3080403

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

sqlite: env
	cd thirdparty && curl -o $(SQLITE_TARNAME).tar.gz http://sqlite.org/2014/$(SQLITE_TARNAME).tar.gz
	cd thirdparty && tar -xf $(SQLITE_TARNAME).tar.gz
	cd thirdparty/$(SQLITE_TARNAME) && $(PREFIX)/env.sh ./configure --prefix=$(PREFIX)
	cd thirdparty/$(SQLITE_TARNAME) && $(PREFIX)/env.sh make -j4 install

intltool: env
	cd thirdparty && curl -o $(INTLTOOL_TARNAME).tar.gz $(GNOME_SOURCES)/intltool/$(INTLTOOL_MAJOR)/$(INTLTOOL_TARNAME).tar.gz
	cd thirdparty && tar -xf $(INTLTOOL_TARNAME).tar.gz
	cd thirdparty/$(INTLTOOL_TARNAME) && $(PREFIX)/env.sh ./configure --prefix=$(PREFIX)
	cd thirdparty/$(INTLTOOL_TARNAME) && $(PREFIX)/env.sh make -j4 install

libffi: env
	cd thirdparty && curl -o $(LIBFFI_TARNAME).tar.gz ftp://sourceware.org/pub/libffi/$(LIBFFI_TARNAME).tar.gz
	cd thirdparty && tar -xf $(LIBFFI_TARNAME).tar.gz
	cd thirdparty/$(LIBFFI_TARNAME) && $(PREFIX)/env.sh ./configure --prefix=$(PREFIX)
	cd thirdparty/$(LIBFFI_TARNAME) && $(PREFIX)/env.sh make -j4 install

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

heroku-deps: intltool libffi glib json-glib sqlite

dependencies: gegl babl libsoup

check: install
	npm test

clean:
	git clean -dfx --exclude node_modules --exclude install

release: check
	cd $(PREFIX) && tar -caf ../imgflo-$(VERSION).tgz ./

.PHONY=all imgflo imgflo-runtime run
