
typedef struct Tracer_ {
    Graph *graph; // borrowed
    JsonObject *root;
} Tracer;

Tracer *
tracer_new() {
    Tracer *self = g_new0(Tracer, 1);
    tracer_init(self);
}

static void
tracer_init(Tracer *self) {
    JsonObject *root = json_object_new();

    // Header
    JsonObject *header = json_object_new();
    json_object_set_object_member(root, "header", header);

    JsonObject *graphs = json_object_new();
    json_object_set_object_member(header, "graphs", graphs);

    // Only one graph, add it
    json_object_set_object_member(root, graph->id, graph_save_json(graph));

    // Events
    JsonArray *events = json_array_new();
    json_object_set_array_member(root, "events", events);

    self->root = root;
}

void
trace_add(Tracer *self) {
   // TODO: add into events.
   // Should be a circular buffer, with size (and maybe time limit)
   // When hitting buffer size, first prune out heavy info like buffers for the oldest events.
   // Maybe first reduce to lowres?
   // Keep fact that event happened, maybe dimensions.
   // Image/buffers should be lazily encoded/serialized, to avoid
   // Should images be serialized in data:// urls?
   // Need to be fairly small, say 10kB max. Maybe ~256px JPG? Gives 100 images/MB
}

JsonObject *
tracer_save_json(Tracer *self) {
    return self->root;
}

gchar *
tracer_save_file(Tracer *self) {

    self->root;
}
