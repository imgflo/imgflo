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

static gchar *image_path = NULL;
static gchar *image_url = NULL;

static GOptionEntry entries[] = {
    { "file", 'f', 0, G_OPTION_ARG_STRING, &image_path, "Image to stat, as path to local file", NULL },
    { "uri", 'u', 0, G_OPTION_ARG_STRING, &image_url, "Image to stat, as fully-qualified URI/URL", NULL },
    { NULL }
};

// FIXME: support URI
static gchar *stat_image(const gchar *filepath) {
  GeglNode *graph, *src, *sink;
  GeglBuffer *buffer = NULL;

  // FIXME: error handling
  graph = gegl_node_new ();
  src   = gegl_node_new_child (graph,
                               "operation", "gegl:load",
                               "path", filepath,
                                NULL);
  sink  = gegl_node_new_child (graph,
                               "operation", "gegl:buffer-sink",
                               "buffer", &buffer,
                                NULL);

  gegl_node_link_many (src, sink, NULL);

  gegl_node_process (sink);

  if (!buffer) {
      return NULL;
  }

  const int width = gegl_buffer_get_width(buffer);
  const int height = gegl_buffer_get_height(buffer);

  g_object_unref (graph);
  g_object_unref (buffer);

  // FIXME: GEGL load return invalid width/height when image fails to load
  if (width && height) {
      return g_strdup_printf("{\n\t\"width\": %d,\n\t\"height\": %d\n}\n", width, height);
  } else {
      return NULL;
  }
}

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

        if (!(image_path || image_url)) {
            g_printerr("%s", "Either --file or --uri must be specified\n");
            g_printerr("%s", g_option_context_get_help (opts, TRUE, NULL));
            exit(1);
        }

        g_option_context_free (opts);
    }

    // Run
    {
        signal(SIGINT, quit);
        gegl_init(0, NULL);

        char *output = stat_image(image_path);
        if (output) {
            g_print("%s", output);
        } else {
            g_printerr("%s", "Unknown ERROR");
            exit(2);
        }

        gegl_exit();
    }

	return 0;
}
