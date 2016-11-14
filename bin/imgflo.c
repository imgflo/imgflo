//     imgflo - Flowhub.io Image-processing runtime
//     (c) 2014 The Grid
//     imgflo may be freely distributed under the MIT license

#include <glib.h>
#include <gegl.h>

#include "lib/utils.c"
#include "lib/uuid.c"
#include "lib/processor.c"
#include "lib/library.c"
#include "lib/graph.c"
#include "lib/network.c"
#include "lib/video.c"

static gboolean process_video = FALSE;
static gchar *node_info = NULL;

static GOptionEntry entries[] = {
    { "video", 'v', 0, G_OPTION_ARG_NONE, &process_video, "Input should be processed as a video", NULL },
    { "nodeinfo", 'i', 0, G_OPTION_ARG_STRING, &node_info, "Show info from these (comma,separated) nodes", NULL },
    { NULL }
};

void video_progress(Network *net, gint frame, gint total, gpointer user_data) {
    g_debug("frame: %d / %d\n", frame, total);
}

void
show_node_info(Network *net, const gchar *info) {
    gchar **names = g_strsplit(info, ",", 0);
    for (int i = 0; names[i]; ++i) {
        const gchar *name = names[i];
        GeglRectangle bbox = network_get_bounding_box(net, name);
        g_print("NodeInfo: { \"name\":\"%s\", \"x\":%d, \"y\":%d, \"width\":%d, \"height\":%d }\n", name, bbox.x, bbox.y, bbox.width, bbox.height);
    }
    g_strfreev(names);
}

int main(int argc, char *argv[]) {

    // Parse options
    {
        GOptionContext *opts;
        GError *error = NULL;

        opts = g_option_context_new ("graph.json");
        g_option_context_add_main_entries (opts, entries, NULL);
        if (!g_option_context_parse (opts, &argc, &argv, &error)) {
            g_printerr("Could not parse arguments: %s\n", error->message);
            g_printerr("%s", g_option_context_get_help (opts, TRUE, NULL));
            return 1;
        }
        if (argc != 2) {
            g_printerr("Error: Expected 1 argument, got %d\n", argc-1);
            g_printerr("%s", g_option_context_get_help (opts, TRUE, NULL));
            return 1;
        }
        g_option_context_free (opts);
    }

    const char *path = argv[1];

    const double before_init = imgflo_get_time();
    gegl_init(0, NULL);
    const double after_init = imgflo_get_time();
    g_print("GeglInit: { \"duration\":%f }\n", (after_init-before_init)*1000.0);

    Library *lib = library_new();
    Graph *graph = graph_new("stdin", lib);
    Network *net = network_new(graph);

    if (g_strcmp0(path, "-") == 0) {
        if (!graph_load_stdin(graph, NULL)) {
            g_printerr("Error: Failed to load graph from stdin\n");
            return 2;
        }

    } else {
        if (!graph_load_json_file(graph, path, NULL)) {
            g_printerr("Failed to load file!\n");
            return 2;
        }
    }

    if (process_video) {
        const int frames_processed = video_process_network(net, video_progress, NULL);
        if (frames_processed <= 0) {
            g_printerr("Error: No video frames processed\n");
            return 1;
        } else {
            g_print("Processed %d video frames\n", frames_processed);
        }
    } else {
        network_process(net);

        if (node_info) {
            show_node_info(net, node_info);
        }
    }

    network_free(net);
    library_free(lib);

    gegl_exit();
	return 0;
}
