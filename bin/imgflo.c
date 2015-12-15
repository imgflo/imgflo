//     imgflo - Flowhub.io Image-processing runtime
//     (c) 2014 The Grid
//     imgflo may be freely distributed under the MIT license

#include <glib.h>
#include <gegl.h>

#include "lib/utils.c"
#include "lib/processor.c"
#include "lib/library.c"
#include "lib/graph.c"
#include "lib/network.c"
#include "lib/video.c"

static gboolean process_video = FALSE;

static GOptionEntry entries[] = {
    { "video", 'v', 0, G_OPTION_ARG_NONE, &process_video, "Input should be processed as a video", NULL },
    { NULL }
};

void video_progress(Network *net, gint frame, gint total, gpointer user_data) {
    g_debug("frame: %d / %d\n", frame, total);
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

    gegl_init(0, NULL);

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
    }

    network_free(net);
    library_free(lib);

    gegl_exit();
	return 0;
}
