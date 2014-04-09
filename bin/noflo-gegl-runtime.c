#include "lib/graph.c"
#include "lib/ui.c"

static void
quit (int sig)
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
	GOptionContext *opts;
	GMainLoop *loop;
	SoupServer *server;
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

	signal (SIGINT, quit);

	server = soup_server_new (SOUP_SERVER_PORT, port,
				  SOUP_SERVER_SERVER_HEADER, "simple-httpd ",
				  NULL);
	if (!server) {
		g_printerr ("Unable to bind to server port %d\n", port);
		exit (1);
	}

    gegl_init(0, NULL);

    soup_server_add_websocket_handler(server, NULL, NULL, NULL, websocket_callback, NULL, g_free);

	soup_server_add_handler (server, NULL, server_callback, NULL, NULL);
	g_print ("\nStarting Server on port %d\n", soup_server_get_port (server));
	soup_server_run_async (server);

	g_print ("\nWaiting for requests...\n");

	loop = g_main_loop_new (NULL, TRUE);
	g_main_loop_run (loop);

    gegl_exit();

	return 0;
}
