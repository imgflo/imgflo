//     imgflo - Flowhub.io Image-processing runtime
//     (c) 2014 The Grid
//     imgflo may be freely distributed under the MIT license

struct _Network;

typedef void (* NetworkProcessorInvalidatedCallback)
    (struct _Network *network, struct _Processor *processor, GeglRectangle rect, gpointer user_data);

typedef struct _Network {
    Graph *graph; // owned
    gboolean running;
    NetworkProcessorInvalidatedCallback on_processor_invalidated;
    gpointer on_processor_invalidated_data;
} Network;

Network *
network_new(Graph *graph)
{
    Network *self = g_new(Network, 1);
    self->graph = graph;
    self->running = FALSE;
    self->on_processor_invalidated = NULL;
    self->on_processor_invalidated_data = NULL;
    return self;
}

void
network_free(Network *self)
{
    if (self->graph) {
        graph_free(self->graph);
    }
    g_free(self);
}

void
emit_invalidated(Processor *processor, GeglRectangle rect, gpointer user_data) {
    Network *network = (Network *)user_data;
    if (network->on_processor_invalidated) {
        network->on_processor_invalidated(network, processor, rect,
                                          network->on_processor_invalidated_data);
    }
}

void 
set_running_state_func(gpointer key, Processor *value, Network *network)
{
    value->on_invalidated_data = (gpointer)network;
    value->on_invalidated = emit_invalidated;
    processor_set_running(value, network->running);
}

void
network_set_running(Network *self, gboolean running)
{
    if (!self->graph) {
        return;
    }
    self->running = running;
    g_hash_table_foreach(self->graph->processor_map, (GHFunc)set_running_state_func, self);
}

void 
process_func(gpointer key, Processor *value, gpointer unused)
{
    g_assert(value);
    if (value->node) {
        gegl_node_process(value->node);
    }
}

void
network_process(Network *self) {
    if (!self->graph) {
        return;
    }
    g_hash_table_foreach(self->graph->processor_map, (GHFunc)process_func, NULL);
}

Processor *
network_processor(Network *self, const gchar *node_name) {
    g_return_val_if_fail(self, NULL);
    g_return_val_if_fail(self->graph, NULL);
    g_return_val_if_fail(node_name, NULL);

    return g_hash_table_lookup(self->graph->processor_map, node_name);
}
