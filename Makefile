
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

LIBS=gegl-0.3 gio-unix-2.0 json-glib-1.0 libsoup-2.4 libpng
DEPS=$(shell $(PREFIX)/env.sh pkg-config $(PKGCONFIG_ARGS) --libs --cflags $(LIBS))


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

dependencies:
	cd dependencies && make PREFIX=$(PREFIX) dependencies

check: install
	npm test

clean:
	git clean -dfx --exclude node_modules --exclude install

release: check
	cd $(PREFIX) && tar -caf ../imgflo-$(VERSION).tgz ./

.PHONY:all imgflo imgflo-runtime run dependencies
