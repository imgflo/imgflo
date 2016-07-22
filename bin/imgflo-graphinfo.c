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

        if (strlen(graphfile) > 0) {
            GError *err = NULL;
            Library *lib = library_new();
            Graph *g = graph_new("default/main", lib);
            //Network *n = network_new(g);
            gboolean loaded = graph_load_json_file(g, graphfile, &err);
            if (!loaded) {
                g_printerr("Failed to load graph: %s", err->message);
                return 1;
            }


            GHashTableIter iter;
            gpointer key, value;

            g_hash_table_iter_init(&iter, g->inports);
            while (g_hash_table_iter_next(&iter, &key, &value))
            {
                const gchar *port = (const gchar *)key;
                char *type = graph_inport_type(g, port);
                g_print("exported inport %s type=%s\n", port, type);
            }

            g_hash_table_iter_init(&iter, g->outports);
            while (g_hash_table_iter_next(&iter, &key, &value))
            {
                const gchar *port = (const gchar *)key;
                char *type = graph_outport_type(g, port);
                g_print("exported outport %s type=%s\n", port, type);
            }

            // FIXME: put into exported port .metadata

        } else {
            g_printerr("No --graph specified");
            return 1;
        }

        gegl_exit();
    }

	return 0;
}
