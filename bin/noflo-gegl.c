#include "lib/graph.c"

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

    if (!graph_load_json_file(graph, "./examples/first.json", NULL)) {
        fprintf(stderr, "Failed to load file!\n");
        return 1;
    }

    fprintf(stdout, "Processing\n");
    // FIXME: determine last node automatically
    GeglNode *last = g_hash_table_lookup(graph->node_map, "out");
    g_assert(last);
    gegl_node_process(last);

    graph_free(graph);

    gegl_exit();

	return 0;
}
