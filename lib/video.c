//     imgflo - Flowhub.io Image-processing runtime
//     (c) 2015 The Grid
//     imgflo may be freely distributed under the MIT license

typedef void (* VideoProcessingProgress)
    (struct _Network *network, gint frame, gint total_frames, gpointer user_data);

// @network must have expected encoder, decoder and load/store buf nodes already set up
gint
video_process_network(Network *network, VideoProcessingProgress progress_callback, gpointer callback_user_data) {

    GeglBuffer *buffer = NULL;
    GeglNode *decode = graph_get_gegl_node(network->graph, "load");
    GeglNode *encode = graph_get_gegl_node(network->graph, "save");
    GeglNode *load_buf = graph_get_gegl_node(network->graph, "_load_buf");
    GeglNode *store_buf = graph_get_gegl_node(network->graph, "_store_buf");
    gegl_node_set(store_buf, "buffer", &buffer, NULL);

    g_return_val_if_fail(encode && decode, 0);
    g_return_val_if_fail(load_buf, 0);
    g_return_val_if_fail(store_buf, 0);

    gint frame_count = 100; // non-zero to enter for loop first time
    gint frame_no = 0;
    for (frame_no = 0; frame_no < frame_count; frame_no++) {
        GeglAudioFragment *audio_fragment;

        // decode next frame
        gegl_node_set(decode, "frame", frame_no, NULL);
        if (buffer) {
            g_object_unref(buffer);
            buffer = NULL;
        }
        gegl_node_process(store_buf);

        if (frame_no == 0 && buffer) {
            // load file metadata
            gdouble fps = -1;
            gegl_node_get(decode, "frame-rate", &fps, "frames", &frame_count, NULL);
            gegl_node_set (encode, "frame-rate", fps, NULL);
        }
        if (buffer) {
            // encode frame
            gegl_node_get (decode, "audio", &audio_fragment, NULL);
            gegl_node_set (encode, "audio", audio_fragment, NULL);
            gegl_node_set (load_buf, "buffer", buffer, NULL);
            gegl_node_process (encode);
        }

        if (progress_callback) {
          progress_callback(network, frame_no, frame_count, callback_user_data);
        }
    }

    if (buffer) {
        g_object_unref (buffer);
    }
    if (frame_count != frame_no) {
        return -1;
    }
    return frame_count;
}
