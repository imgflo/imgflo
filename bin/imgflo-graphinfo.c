//     imgflo - Flowhub.io Image-processing runtime
//     (c) 2014-2016 The Grid
//     imgflo may be freely distributed under the MIT license

// imgflo-graphinfo: Introspect graph and populate metadata

#include "lib/utils.c"
#include "lib/png.c"
#include "lib/processor.c"
#include "lib/library.c"
#include "lib/graph.c"
#include "lib/network.c"

static void
quit(int sig)
{
	/* Exit cleanly on ^C in case we're valgrinding. */
	exit(0);
}

static gchar *graphfile = "";

static GOptionEntry entries[] = {
    { "graph", 'g', 0, G_OPTION_ARG_STRING, &graphfile, "Graph to read", NULL },
	{ NULL }
};


void
set_port_type(JsonObject *port, char *type) {
    if (!type) { return; }

    if (!json_object_has_member(port, "metadata")) {
        json_object_set_object_member(port, "metadata", json_object_new());
    }
    JsonObject *metadata = json_object_get_object_member(port, "metadata");
    json_object_set_string_member(metadata, "type", type);

}

gboolean
inject_exported_port_types(Graph *g, JsonObject *root) {
    if (json_object_has_member(root, "inports")) {
        JsonObject *inports = json_object_get_object_member(root, "inports");
        GList *inport_names = json_object_get_members(inports);
        for (int i=0; i<g_list_length(inport_names); i++) {
            const gchar *name = g_list_nth_data(inport_names, i);
            char *type = graph_inport_type(g, name);
            //g_print("exported inport %s type=%s\n", name, type);
            JsonObject *port = json_object_get_object_member(inports, name);
            set_port_type(port, type);
        }
    }

    if (json_object_has_member(root, "outports")) {
        JsonObject *outports = json_object_get_object_member(root, "outports");
        GList *outport_names = json_object_get_members(outports);
        for (int i=0; i<g_list_length(outport_names); i++) {
            const gchar *name = g_list_nth_data(outport_names, i);
            char *type = graph_outport_type(g, name);
            //g_print("exported outport %s type=%s\n", name, type);
            JsonObject *port = json_object_get_object_member(outports, name);
            set_port_type(port, type);
        }
    }
    return TRUE;
}

JsonObject *
add_graphinfo(char *data, ssize_t length) {
    GError *err = NULL;
    Library *lib = library_new();
    Graph *g = graph_new("default/main", lib);
    gboolean loaded = graph_load_json_data(g, data, length, &err);
    if (!loaded) {
        g_printerr("Failed to load graph: %s", err->message);
        return NULL;
    }

    // To ensure we preserve all metadata, we operate directly on the JSON,
    // instead of potentially lossy roundtrip in Graph datastructure
    JsonParser *parser = json_parser_new();
    gboolean success = json_parser_load_from_data(parser, data, length, &err);
    if (!success) {
        g_printerr("Failed to load graph: %s", err->message);
        return NULL;
    }

    JsonNode *rootnode = json_parser_get_root(parser);
    g_assert(JSON_NODE_HOLDS_OBJECT(rootnode));
    JsonObject *root = json_node_get_object(rootnode);

    inject_exported_port_types(g, root);
    return root;
}

int
main (int argc, char **argv)
{
    // Parse options
    {
	    GOptionContext *opts;
	    GError *error = NULL;

	    opts = g_option_context_new (NULL);
	    g_option_context_add_main_entries (opts, entries, NULL);
	    if (!g_option_context_parse (opts, &argc, &argv, &error)) {
		    g_printerr("Could not parse arguments: %s\n", error->message);
		    g_printerr("%s", g_option_context_get_help (opts, TRUE, NULL));
		    exit(1);
	    }
	    if (argc != 1) {
		    g_printerr("%s", g_option_context_get_help (opts, TRUE, NULL));
		    exit(1);
	    }
	    g_option_context_free (opts);
    }

    // Run
    {
	    signal(SIGINT, quit);

        gegl_init(0, NULL);

        gchar *input_data = NULL;
        gsize input_data_length = 0;
        if (strlen(graphfile) == 0) {
            g_printerr("No --graph specified");
            return 1;
        } else if (g_strcmp0(graphfile, "-") == 0) {
            // stdint
            size_t max_bytes = 1e6;
            void * buffer = g_malloc(max_bytes);
            size_t bytes_read = 0;
            GInputStream *stdin_stream = g_unix_input_stream_new(STDIN_FILENO, FALSE);
            GError *error = NULL;
            gboolean stdin_read = g_input_stream_read_all(stdin_stream, buffer, max_bytes, &bytes_read, NULL, &error);
            if (!stdin_read) {
                return 1;
            }
            input_data = buffer;
            input_data_length = bytes_read;
        } else {
            // file path
            GFile *file = g_file_new_for_path(graphfile);
            GError *error = NULL;
            gboolean file_read = g_file_load_contents(file, NULL, &input_data, &input_data_length, NULL, &error);
            g_object_unref(file);
            if (!file_read) {
                return 1;
            }
        }

        g_assert(input_data);

        JsonObject *out = add_graphinfo(input_data, input_data_length);
        g_free(input_data); input_data_length = 0;
        if (!out) {
            return 1;
        }

        gchar *output = json_stringify(out, NULL);
        g_print("%s", output);
        g_free(output);



        gegl_exit();
    }

	return 0;
}
