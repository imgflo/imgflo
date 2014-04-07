/*
gcc -o main main.c -std=c99 `pkg-config --libs --cflags gegl-0.3 json-glib-1.0` && ./main
*/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <glib.h>
#include <gegl.h>
#include <json-glib/json-glib.h>



int main(int argc, char *argv[]) {

    gegl_init(0, NULL);

    guint no_ops = 0;
    gchar **operation_names = gegl_list_operations(&no_ops);

    for (int i=0; i<no_ops; i++) {
        //fprintf(stdout, "%d: %s\n", i, operation_names[i]);
    }


    JsonParser *parser = json_parser_new();
    gboolean success = json_parser_load_from_file(parser, "./examples/first.json", NULL);
    if (!success) {
        fprintf(stderr, "Failed to load file!\n");
        return 1;
    }

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
    }

    //g_free(process_names); crashes??

    // Connections
    JsonArray *connections = json_object_get_array_member(root, "connections");
    g_assert(connections);
    for (int i=0; i<json_array_get_length(connections); i++) {
        JsonObject *conn = json_array_get_object_element(connections, i);
        JsonObject *src = json_object_get_object_member(conn, "src");
        JsonObject *tgt = json_object_get_object_member(conn, "tgt");

        const gchar *src_proc = json_object_get_string_member(src, "process");
        const gchar *src_port = json_object_get_string_member(src, "port");
        const gchar *tgt_proc = json_object_get_string_member(tgt, "process");
        const gchar *tgt_port = json_object_get_string_member(tgt, "port");

        fprintf(stdout, "%s %s -> %s %s\n", src_proc, src_port,
                                            tgt_port, tgt_proc);
    }

    gegl_exit();

	return 0;
}
