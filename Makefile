
VERSION=$(shell echo `git describe --tags`)
#PREFIX=/opt/imgflo
PREFIX=$(shell echo `pwd`/install)
FLAGS=-Wall -Werror -std=c99 -g -DHAVE_UUID -I$(PREFIX)/include/uuid
DEBUGPROG=
PORT=3569
HOST=localhost
EXTPORT=$(PORT)
#GRAPH=graphs/checker.json

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

RUN_ARGUMENTS:=--port $(PORT) --host $(HOST) --external-port=$(EXTPORT)
ifdef GRAPH
RUN_ARGUMENTS+=--graph $(GRAPH)
endif


all: install

server: install
	npm start

run-noinstall:
	$(PREFIX)/env.sh $(DEBUGPROG) ./bin/imgflo-runtime $(RUN_ARGUMENTS)

run: install run-noinstall

install: env imgflo imgflo-runtime
	cp ./bin/imgflo $(PREFIX)/bin/
	cp ./bin/imgflo-runtime $(PREFIX)/bin/

imgflo:
	$(PREFIX)/env.sh $(CC) -o ./bin/imgflo bin/imgflo.c -I. $(FLAGS) $(DEPS)

imgflo-runtime:
	$(PREFIX)/env.sh $(CC) -o ./bin/imgflo-runtime bin/imgflo-runtime.c -I. $(FLAGS) $(DEPS)

env:
	mkdir -p $(PREFIX) || true
	sed -e 's|@PREFIX@|$(PREFIX)|' env.sh.in > $(PREFIX)/env.sh
	chmod +x $(PREFIX)/env.sh

travis-deps:
	wget -O imgflo-dependencies.tgz $(TRAVIS_DEPENDENCIES)
	tar -xf imgflo-dependencies.tgz

COMPONENTINSTALLDIR=$(PREFIX)/lib/imgflo/operations
COMPONENT_SOURCES = $(wildcard $(COMPONENTDIR)/*.c)
COMPONENT_PLUGINS = $(patsubst $(COMPONENTDIR)/%.c,$(COMPONENTINSTALLDIR)/%.so,$(COMPONENT_SOURCES))
COMPONENT_OUT = $(patsubst %.c,$(COMPONENTINSTALLDIR)/%.so,$(COMPONENT))

COMPONENT_FLAGS = -shared -rdynamic -fPIC -I$(COMPONENTDIR) $(FLAGS)
ifdef COMPONENT_NAME_PREFIX
COMPONENT_FLAGS += -DIMGFLO_OP_NAME\(orig\)=\"$(COMPONENT_NAME_PREFIX)\"orig\"$(COMPONENT_NAME_SUFFIX)\"
endif

component-install-dir: env
	rm -rf $(COMPONENTINSTALLDIR)
	mkdir -p $(COMPONENTINSTALLDIR) || true
components: component-install-dir $(COMPONENT_PLUGINS)
component: component-install-dir $(COMPONENT_OUT)

$(COMPONENTINSTALLDIR)/%.so: $(COMPONENTDIR)/%.c
	$(PREFIX)/env.sh $(CC) -o $@ $< -DGEGL_OP_C_FILE=\"`basename $<`\" $(COMPONENT_FLAGS) $(DEPS)

dependencies:
	cd dependencies && make PREFIX=$(PREFIX) dependencies

gegl:
	cd dependencies && make PREFIX=$(PREFIX) gegl
libsoup:
	cd dependencies && make PREFIX=$(PREFIX) libsoup
glib:
	cd dependencies && make PREFIX=$(PREFIX) glib

check: install
	$(PREFIX)/env.sh npm test

clean:
	git clean -dfx --exclude node_modules --exclude install

release: check
	cd $(PREFIX) && tar -caf ../imgflo-$(VERSION).tgz ./

.PHONY:all imgflo imgflo-runtime run dependencies
