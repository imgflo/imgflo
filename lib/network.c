//     imgflo - Flowhub.io Image-processing runtime
//     (c) 2014 The Grid
//     imgflo may be freely distributed under the MIT license

struct _Network;
void net_node_added(Graph *graph, const gchar *name, GeglNode *node, Processor *proc, gpointer user_data);


typedef void (* NetworkProcessorInvalidatedCallback)
    (struct _Network *network, struct _Processor *processor, GeglRectangle rect, gpointer user_data);
typedef void (* NetworkStateChanged)
    (struct _Network *network, gboolean running, gboolean processing, gpointer user_data);
typedef void (* NetworkEdgeChanged)
    (struct _Network *network, const GraphEdge *edge, gpointer user_data);

typedef struct _Network {
    Graph *graph; // owned
    gboolean running;
    NetworkProcessorInvalidatedCallback on_processor_invalidated;
    gpointer on_processor_invalidated_data;
    NetworkStateChanged on_state_changed;
    gpointer on_state_changed_data;
    NetworkEdgeChanged on_edge_changed; // data along the edge changed
    gpointer on_edge_changed_data;
} Network;

Network *
network_new(Graph *graph)
{
    g_return_val_if_fail(graph, NULL);

    Network *self = g_new(Network, 1);
    self->graph = graph;
    self->running = FALSE;
    self->on_processor_invalidated = NULL;
    self->on_processor_invalidated_data = NULL;
    self->on_state_changed = NULL;
    self->on_state_changed_data = NULL;
    self->on_edge_changed = NULL;
    self->on_edge_changed_data = NULL;

    self->graph->on_node_added = net_node_added;
    self->graph->on_node_added_data = self;

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

gboolean
network_is_processing(Network *self) {
    gboolean is_processing = FALSE;
    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init(&iter, self->graph->processor_map);
    while (g_hash_table_iter_next(&iter, &key, &value))
    {
        if (processor_is_processing((Processor *)value)) {
            is_processing = TRUE;
            break;
        }
    }
    return is_processing;
}

void
net_emit_edge_changed_func(Graph *graph, const GraphEdge *edge, gpointer user_data) {
    g_assert(user_data);
    Network *self = (Network *)user_data;
    if (self->on_edge_changed) {
        self->on_edge_changed(self, edge, self->on_edge_changed_data);
    }
}

void
net_emit_state_changed(Network *self) {
    const gboolean is_processing = network_is_processing(self);
    //g_printerr("NET processing=%d running=%d\n", is_processing, self->running);
    if (self->on_state_changed) {
        self->on_state_changed(self, self->running, is_processing, self->on_state_changed_data);
    }

    if (self->running && !is_processing) {
        // We assume that every edge changed
        graph_visit_edges(self->graph, net_emit_edge_changed_func, self);
    }
}

void
net_proc_state_changed(Processor *processor, gboolean running,
                       gboolean processing, gpointer user_data) {
    Network *self = (Network *)user_data;
    g_assert(self);

    //g_printerr("proc: running=%d processing=%d\n", running, processing);
    net_emit_state_changed(self);
}

void
net_node_added(Graph *graph, const gchar *name,
               GeglNode *node, Processor *proc, gpointer user_data) {
    g_assert(graph);
    g_assert(user_data);
    Network *self = (Network *)user_data;
    if (proc) {
        proc->on_state_changed = net_proc_state_changed;
        proc->on_state_changed_data = self;
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
    net_emit_state_changed(self);
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
