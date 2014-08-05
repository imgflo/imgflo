
VERSION=$(shell echo `git describe --tags`)
#PREFIX=/opt/imgflo
PREFIX=$(shell echo `pwd`/install)
FLAGS=-Wall -Werror -std=c99 -g -DHAVE_UUID -I$(PREFIX)/include/uuid
DEBUGPROG=
PORT=3569
HOST=localhost
EXTPORT=$(PORT)

ifneq ("$(wildcard /app)","")
# Heroku build. TODO: find better way to detect
RELOCATE_DEPS:=true
endif

ifdef RELOCATE_DEPS
PKGCONFIG_ARGS:=--define-variable=prefix=$(PREFIX)
else
PKGCONFIG_ARGS:=
endif

LIBS=gegl-0.3 gio-unix-2.0 json-glib-1.0 libsoup-2.4 libpng uuid
DEPS=$(shell $(PREFIX)/env.sh pkg-config $(PKGCONFIG_ARGS) --libs --cflags $(LIBS))
TRAVIS_DEPENDENCIES=$(shell echo `cat .vendor_urls | sed -e "s/heroku/travis/" | tr -d '\n'`)

all: install

server: install
	npm start

run-noinstall:
	$(PREFIX)/env.sh $(DEBUGPROG) ./bin/imgflo-runtime --port $(PORT) --host $(HOST) --external-port=$(EXTPORT)

run: install run-noinstall

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

travis-deps:
	wget -O imgflo-dependencies.tgz $(TRAVIS_DEPENDENCIES)
	tar -xf imgflo-dependencies.tgz

dependencies:
	cd dependencies && make PREFIX=$(PREFIX) dependencies

gegl:
	cd dependencies && make PREFIX=$(PREFIX) gegl

glib:
	cd dependencies && make PREFIX=$(PREFIX) glib

check: install
	$(PREFIX)/env.sh npm test

clean:
	git clean -dfx --exclude node_modules --exclude install

release: check
	cd $(PREFIX) && tar -caf ../imgflo-$(VERSION).tgz ./

.PHONY:all imgflo imgflo-runtime run dependencies
