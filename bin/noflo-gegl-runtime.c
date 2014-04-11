#include "lib/graph.c"
#include "lib/ui.c"


static void
quit(int sig)
{
	/* Exit cleanly on ^C in case we're valgrinding. */
	exit(0);
}

static int port = 3569;

static GOptionEntry entries[] = {
	{ "port", 'p', 0,
	  G_OPTION_ARG_INT, &port,
	  "Port to listen on", NULL },
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
        GMainLoop *loop = g_main_loop_new(NULL, TRUE);
	    UiConnection *ui = ui_connection_new(port);

	    if (!ui) {
		    g_printerr("Unable to bind to server port %d\n", port);
		    exit(1);
	    }
	    g_print("\nRuntime running on port %d\n", soup_server_get_port(ui->server));

	    g_main_loop_run (loop);

        ui_connection_free(ui);
        g_main_loop_unref(loop);
        gegl_exit();
    }

	return 0;
}
