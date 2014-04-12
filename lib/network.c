

typedef struct {
    Graph *graph; // unowned
} Network;

Network *
network_new(void)
{
    Network *self = g_new(Network, 1);
    return self;
}

void
network_free(Network *self)
{
    g_free(self);
}

void
network_set_graph(Network *self, Graph *graph)
{
    self->graph = graph;
}

void 
set_running_state_func(gpointer key, Processor *value, gboolean *running)
{
    processor_set_running(value, *running);
}

void
network_set_running(Network *self, gboolean running)
{
    if (!self->graph) {
        return;
    }
    g_hash_table_foreach(self->graph->processor_map, (GHFunc)set_running_state_func, &running);
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
