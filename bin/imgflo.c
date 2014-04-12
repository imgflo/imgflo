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
        fprintf(stdout, "%d: %s\n", i, operation_names[i]);
    }
}

int main(int argc, char *argv[]) {

    gegl_init(0, NULL);

    //print_available_ops();

    Graph *graph = graph_new();
    Network *net = network_new();
    network_set_graph(net, graph);

    if (!graph_load_json_file(graph, "./examples/first.json", NULL)) {
        fprintf(stderr, "Failed to load file!\n");
        return 1;
    }
    network_process(net);

    network_free(net);
    graph_free(graph);

    gegl_exit();
	return 0;
}
