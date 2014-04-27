//     imgflo - Flowhub.io Image-processing runtime
//     (c) 2014 The Grid
//     imgflo may be freely distributed under the MIT license

struct _Network;

typedef void (* NetworkProcessorInvalidatedCallback)
    (struct _Network *network, struct _Processor *processor, GeglRectangle rect, gpointer user_data);

typedef struct _Network {
    Graph *graph; // unowned
    gboolean running;
    NetworkProcessorInvalidatedCallback on_processor_invalidated;
    gpointer on_processor_invalidated_data;
} Network;

Network *
network_new(void)
{
    Network *self = g_new(Network, 1);
    self->running = FALSE;
    self->on_processor_invalidated = NULL;
    self->on_processor_invalidated_data = NULL;
    return self;
}

void
network_free(Network *self)
{
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
network_set_graph(Network *self, Graph *graph)
{
    self->graph = graph;
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

