//     imgflo - Flowhub.io Image-processing runtime
//     (c) 2014 The Grid
//     imgflo may be freely distributed under the MIT license

#include <glib.h>
#include <gegl.h>

#include "lib/processor.c"
#include "lib/library.c"
#include "lib/graph.c"
#include "lib/network.c"

int main(int argc, char *argv[]) {

    if (argc != 2) {
        g_printerr("Error: Expected 1 argument, got %d\n", argc-1);
        g_printerr("Usage: %s graph.json\n", argv[0]);
        return 1;
    }
    const char *path = argv[1];

    gegl_init(0, NULL);

    Library *lib = library_new();
    Graph *graph = graph_new("stdin", lib);
    Network *net = network_new(graph);

    if (g_strcmp0(path, "-") == 0) {
        if (!graph_load_stdin(graph, NULL)) {
            g_printerr("Error: Failed to load graph from stdin\n");
            return 2;
        }

    } else {
        if (!graph_load_json_file(graph, path, NULL)) {
            g_printerr("Failed to load file!\n");
            return 2;
        }
    }

    network_process(net);

    network_free(net);
    library_free(lib);

    gegl_exit();
	return 0;
}
