
DEPS=`pkg-config --libs --cflags gegl-0.3 json-glib-1.0`
FLAGS=-Wall -Werror -std=c99

all: run

run: noflo-gegl
	./bin/noflo-gegl

noflo-gegl:
	gcc -o ./bin/noflo-gegl bin/noflo-gegl.c -I. $(FLAGS) $(DEPS)


.PHONY=all noflo-gegl run
