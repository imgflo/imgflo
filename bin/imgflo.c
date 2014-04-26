//     imgflo - Flowhub.io Image-processing runtime
//     (c) 2014 The Grid
//     imgflo may be freely distributed under the MIT license

#include <glib.h>
#include <gegl.h>

#include "lib/processor.c"
#include "lib/graph.c"
#include "lib/network.c"

void print_available_ops() {
    guint no_ops = 0;
    gchar **operation_names = gegl_list_operations(&no_ops);

    for (int i=0; i<no_ops; i++) {
        g_print("%d: %s\n", i, operation_names[i]);
    }
}

int main(int argc, char *argv[]) {

    if (argc != 2) {
        g_printerr("Error: Expected 1 argument, got %d\n", argc-1);
        g_printerr("Usage: %s graph.json\n", argv[0]);
        return 1;
    }
    const char *path = argv[1];

    gegl_init(0, NULL);

    //print_available_ops();

    Graph *graph = graph_new();
    Network *net = network_new();
    network_set_graph(net, graph);

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
    graph_free(graph);

    gegl_exit();
	return 0;
}
