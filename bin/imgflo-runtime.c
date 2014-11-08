//     imgflo - Flowhub.io Image-processing runtime
//     (c) 2014 The Grid
//     imgflo may be freely distributed under the MIT license

#include "lib/utils.c"
#include "lib/png.c"
#include "lib/processor.c"
#include "lib/library.c"
#include "lib/graph.c"
#include "lib/network.c"
#include "lib/uuid.c"
#include "lib/registry.c"
#include "lib/ui.c"

static void
quit(int sig)
{
	/* Exit cleanly on ^C in case we're valgrinding. */
	exit(0);
}

static int port = 3569;
static int extport = 3569;
static gchar *host = "localhost";
static gchar *defaultgraph = "";

static GOptionEntry entries[] = {
	{ "port", 'p', 0, G_OPTION_ARG_INT, &port, "Port to listen on", NULL },
    { "external-port", 'e', 0, G_OPTION_ARG_INT, &extport, "Port we are available on for clients", NULL },
    { "host", 'h', 0, G_OPTION_ARG_STRING, &host, "Hostname", NULL },
    { "graph", 'g', 0, G_OPTION_ARG_STRING, &defaultgraph, "Default graph", NULL },
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
	    UiConnection *ui = ui_connection_new(host, port, extport);

        if (strlen(defaultgraph) > 0) {
            GError *err = NULL;
            Graph *g = graph_new("default/main", ui->component_lib);
            Network *n = network_new(g);
            gboolean loaded = graph_load_json_file(g, defaultgraph, &err);
            if (!loaded) {
                g_printerr("Failed to load graph: %s", err->message);
                return 1;
            }
            ui_connection_set_default_network(ui, n);
        }

        GMainLoop *loop = g_main_loop_new(NULL, TRUE);

	    if (!ui) {
		    g_printerr("Unable to bind to server port %d\n", port);
		    exit(1);
	    }
	    g_print("\nRuntime running on port %d, external port %d\n", port, extport);

        ui_connection_try_register(ui);

	    g_main_loop_run (loop);

        g_main_loop_unref(loop);
        ui_connection_free(ui);
        gegl_exit();
    }

	return 0;
}
