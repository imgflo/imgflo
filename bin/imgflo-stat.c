//     imgflo - Flowhub.io Image-processing runtime
//     (c) 2014-2016 The Grid
//     imgflo may be freely distributed under the MIT license

// imgflo-stat: Extract and display statistics about images

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

static GOptionEntry entries[] = {
    { NULL }
};



// NOTE: nothing particularly imgflo specific about this tool. Could just as well be upstream in GEGL?
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


        gegl_exit();
    }

	return 0;
}
