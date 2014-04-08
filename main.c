/*
gcc -o main main.c -std=c99 `pkg-config --libs --cflags gegl-0.3 json-glib-1.0` && ./main
*/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <glib.h>
#include <gegl.h>
#include <json-glib/json-glib.h>

// GOAL: get JSON serialization support upstream, support building meta-operations with it

typedef struct {
    GeglNode *top;
    GHashTable *node_map;
} Graph;


Graph *
graph_new() {
    Graph *self = g_new(Graph, 1);
    self->top = gegl_node_new();
    self->node_map = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    return self;
}

void
graph_free(Graph *self) {

    g_object_unref(self->top);
    g_hash_table_destroy(self->node_map);

    g_free(self);
}

void
graph_load_json(Graph *self, JsonParser *parser) {

    JsonNode *rootnode = json_parser_get_root(parser);
    g_assert(JSON_NODE_HOLDS_OBJECT(rootnode));
    JsonObject *root = json_node_get_object(rootnode);

    // Processes
    JsonObject *processes = json_object_get_object_member(root, "processes");
    g_assert(processes);

    GList *process_names = json_object_get_members(processes);
    for (int i=0; i<g_list_length(process_names); i++) {
        const gchar *name = g_list_nth_data(process_names, i);
        JsonObject *proc = json_object_get_object_member(processes, name);
        const gchar *op = json_object_get_string_member(proc, "component");
        fprintf(stdout, "%s(%s)\n", name, op);

        GeglNode *n = gegl_node_new_child(self->top, "operation", op, NULL);
        g_assert(n);
        g_hash_table_insert(self->node_map, (gpointer)g_strdup(name), (gpointer)n);
    }

    //g_free(process_names); crashes??

    // Connections
    JsonArray *connections = json_object_get_array_member(root, "connections");
    g_assert(connections);
    for (int i=0; i<json_array_get_length(connections); i++) {
        JsonObject *conn = json_array_get_object_element(connections, i);

        JsonObject *tgt = json_object_get_object_member(conn, "tgt");
        const gchar *tgt_proc = json_object_get_string_member(tgt, "process");
        const gchar *tgt_port = json_object_get_string_member(tgt, "port");
        GeglNode *t = g_hash_table_lookup(self->node_map, tgt_proc);

        JsonNode *srcnode = json_object_get_member(conn, "src");
        if (srcnode) {
            // Connection
            JsonObject *src = json_object_get_object_member(conn, "src");
            const gchar *src_proc = json_object_get_string_member(src, "process");
            const gchar *src_port = json_object_get_string_member(src, "port");

            fprintf(stdout, "%s %s -> %s %s\n", src_proc, src_port,
                                                tgt_port, tgt_proc);

            GeglNode *s = g_hash_table_lookup(self->node_map, src_proc);
            g_assert(s);
            gegl_node_connect_to(s, src_port, t, tgt_port);

        } else {
            // IIP
            JsonNode *datanode = json_object_get_member(conn, "data");
            GValue value = G_VALUE_INIT;
            g_assert(JSON_NODE_HOLDS_VALUE(datanode));
            json_node_get_value(datanode, &value);

            // TODO: print IIP
            // TODO: convert values when needed. Maybe lookup paramspec?
            fprintf(stdout, "'%s' -> %s %s\n", " ", tgt_port, tgt_proc);
            gegl_node_set_property(t, tgt_port, &value);
            g_value_unset(&value);
        }
    }
}

gboolean
graph_load_json_file(Graph *self, const gchar *path, GError **error) {

    JsonParser *parser = json_parser_new();

    gboolean success = json_parser_load_from_file(parser, path, error);
    if (success) {
        graph_load_json(self, parser);
    }

    g_object_unref(parser);
    return success;
}

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
