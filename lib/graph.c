//     imgflo - Flowhub.io Image-processing runtime
//     (c) 2014 The Grid
//     imgflo may be freely distributed under the MIT license

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <glib.h>
#include <gio/gunixinputstream.h>
#include <gegl.h>
#include <json-glib/json-glib.h>

// GOAL: get JSON serialization support upstream, support building meta-operations with it

gboolean
gvalue_from_string(GValue *value, GType target_type, GValue *dest_value) {

    // Custom conversion from string
    if (!G_VALUE_HOLDS_STRING(value)) {
        return FALSE;
    }
    const gchar *iip = g_value_get_string(value);
    if (g_type_is_a(target_type, G_TYPE_DOUBLE)) {
        gdouble d = g_ascii_strtod(iip, NULL);
        g_value_set_double(dest_value, d);
    } else if (g_type_is_a(target_type, G_TYPE_INT)) {
        gint i = g_ascii_strtoll(iip, NULL, 10);
        g_value_set_int(dest_value, i);
    } else if (g_type_is_a(target_type, G_TYPE_INT64)) {
        gint64 i = g_ascii_strtoll(iip, NULL, 10);
        g_value_set_int64(dest_value, i);
    } else if (g_type_is_a(target_type, GEGL_TYPE_COLOR)) {
        GeglColor *color = gegl_color_new(iip);
        if (!color || !GEGL_IS_COLOR(color)) {
            return FALSE;
        }
        g_value_set_object(dest_value, color);
    } else if (g_type_is_a(target_type, G_TYPE_ENUM)) {
        GEnumClass *klass = (GEnumClass *)g_type_class_ref(target_type);
        GEnumValue *val = g_enum_get_value_by_nick(klass, iip);
        g_return_val_if_fail(val, FALSE);

        g_value_set_enum(dest_value, val->value);
        g_type_class_unref((gpointer)klass);
    } else if (g_type_is_a(target_type, G_TYPE_BOOLEAN)) {
        gboolean b = g_ascii_strcasecmp("true", iip) == 0;
        g_value_set_boolean(dest_value, b);
    } else {
        return FALSE;
    }
    return TRUE;
}

gboolean
set_property(GeglNode *t, const gchar *port, GParamSpec *paramspec, GValue *value)  {
    GType target_type = G_PARAM_SPEC_VALUE_TYPE(paramspec);
    GValue dest_value = {0,};
    g_value_init(&dest_value, target_type);

    gboolean success = g_param_value_convert(paramspec, value, &dest_value, FALSE);
    if (success) {
        gegl_node_set_property(t, port, &dest_value);
        return TRUE;
    }

    if (gvalue_from_string(value, target_type, &dest_value)) {
        g_param_value_validate(paramspec, &dest_value);
        gegl_node_set_property(t, port, &dest_value);
        return TRUE;
    }
    return FALSE;
}

gboolean
gh_value_equals_outkey(gpointer key, gpointer value, gpointer user_data) {
    const gboolean found = value == *(gpointer *)(user_data);
    if (found) {
        *(gpointer *)(user_data) = key;
    }
    return found;
}

struct _Graph;
struct _GraphEdge;

typedef void (* GraphEdgeVisitFunc)
    (struct _Graph *graph, const struct _GraphEdge *edge, gpointer user_data);

typedef void (* GraphNodeAdded) // Note: only one of @node and @proc are set
    (struct _Graph *graph, const gchar *name, GeglNode *node, Processor *proc, gpointer user_data);

typedef enum _GraphPortDirection {
    GraphOutPort = 0,
    GraphInPort = 1
} GraphPortDirection;

typedef struct _Graph {
    gchar *id;
    GeglNode *top;
    GHashTable *node_map;
    GHashTable *processor_map;
    GHashTable *inports;
    GHashTable *outports;
    Library *component_lib; // unowned

    // signals
    GraphNodeAdded on_node_added;
    gpointer on_node_added_data;
} Graph;

typedef struct _GraphEdge {
    const gchar *src_name;
    const gchar *src_port;
    const gchar *tgt_name;
    const gchar *tgt_port;
} GraphEdge;

typedef struct _GraphNodePort {
    gchar *node;
    gchar *port;
} GraphNodePort;

GraphNodePort *
graph_node_port_new(const gchar *node, const gchar *port)
{
    GraphNodePort *self = g_new(GraphNodePort, 1);
    self->node = g_strdup(node);
    self->port = g_strdup(port);
    return self;
}
void
graph_node_port_free(GraphNodePort *self)
{
    if (!self) {
        return;
    }
    g_free(self->node);
    g_free(self->port);
}

Graph *
graph_new(const gchar *id, Library *lib) {
    g_return_val_if_fail(id, NULL);
    g_return_val_if_fail(lib, NULL);

    Graph *self = g_new(Graph, 1);
    self->component_lib = lib;
    self->id = g_strdup(id);
    self->top = gegl_node_new();
    self->node_map = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    self->processor_map = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

    self->inports = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)graph_node_port_free);
    self->outports = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)graph_node_port_free);

    self->on_node_added = NULL;
    self->on_node_added_data = NULL;

    return self;
}

void
graph_free(Graph *self) {
    if (!self) {
        return;
    }

    g_free(self->id);
    g_object_unref(self->top);
    // FIXME: leaks memeory. Go through all nodes and processors and free
    g_hash_table_destroy(self->node_map);
    g_hash_table_destroy(self->processor_map);

    g_hash_table_destroy(self->inports);
    g_hash_table_destroy(self->outports);

    g_free(self);
}

void
graph_add_port(Graph *self, GraphPortDirection dir, const gchar *exported,
                 const gchar *node, const gchar *port)
{
    g_return_if_fail(self);
    g_return_if_fail(dir==GraphInPort || dir==GraphOutPort);
    GHashTable *ports = (dir == GraphInPort) ? self->inports : self->outports;
    g_hash_table_replace(ports, g_strdup(exported), graph_node_port_new(node, port));
}


void
graph_remove_port(Graph *self, GraphPortDirection dir, const gchar *exported)
{
    g_return_if_fail(self);
    GHashTable *ports = (dir == GraphInPort) ? self->inports : self->outports;
    g_hash_table_remove(ports, exported);
}

void
graph_add_iip(Graph *self, const gchar *node, const gchar *port, GValue *value)
{
    g_return_if_fail(self);
    g_return_if_fail(node);
    g_return_if_fail(port);
    g_return_if_fail(value);

    const gchar *iip = G_VALUE_HOLDS_STRING(value) ? g_value_get_string(value) : "IIP";

    GeglNode *t = g_hash_table_lookup(self->node_map, node);
    g_return_if_fail(t);

    GParamSpec *paramspec = gegl_node_find_property(t, port);
    if (paramspec) {
        const gboolean success = set_property(t, port, paramspec, value);
        if (success) {
            g_info("\t'%s' -> %s %s\n", iip, port, node);
        } else {
            GType value_type = G_VALUE_TYPE(value);
            GType target_type = G_PARAM_SPEC_VALUE_TYPE(paramspec);
            g_debug("target_type=%s value_type=%s\n",
                    g_type_name(target_type), g_type_name(value_type));
            g_warning("Unable to convert value for property '%s' of node '%s'\n",
                    port, node);
        }
    } else {
        g_warning("Node '%s' has no property '%s'\n", node, port);
    }
}

void
graph_remove_iip(Graph *self, const gchar *node, const gchar *port)
{
    g_return_if_fail(self);
    g_return_if_fail(node);
    g_return_if_fail(port);

    g_info("\tDEL '%s' -> %s %s\n", "IIP", node, port);

    GeglNode *t = g_hash_table_lookup(self->node_map, node);
    g_return_if_fail(t);
    GParamSpec *paramspec = gegl_node_find_property(t, port);
    if (paramspec) {
        const GValue *def = g_param_spec_get_default_value(paramspec);
        gegl_node_set_property(t, port, def);
    } else {
        g_warning("Node '%s' has no property '%s'\n", node, port);
    }
}

void
graph_add_node(Graph *self, const gchar *name, const gchar *component)
{
    g_return_if_fail(self);
    g_return_if_fail(name);
    g_return_if_fail(component);

    if (g_strcmp0(component, "Processor") == 0) {
        Processor *proc = processor_new();
        g_hash_table_insert(self->processor_map, (gpointer)g_strdup(name), (gpointer)proc);
        g_info("\tAdding Processor: %s\n", name);
        if (self->on_node_added) {
            self->on_node_added(self, name, NULL, proc, self->on_node_added_data);
        }
        return;
    }


    gchar *op = library_get_operation_name(self->component_lib, component);
    // FIXME: check that operation is correct
    GeglNode *n = gegl_node_new_child(self->top, "operation", op, NULL);

    g_return_if_fail(n);

    g_info("\t%s(%s)\n", name, op);
    g_hash_table_insert(self->node_map, (gpointer)g_strdup(name), (gpointer)n);
    if (self->on_node_added) {
        self->on_node_added(self, name, n, NULL, self->on_node_added_data);
    }

    g_free(op);
}

void
graph_remove_node(Graph *self, const gchar *name)
{
    g_return_if_fail(self);
    g_return_if_fail(name);

    Processor *p = g_hash_table_lookup(self->processor_map, name);
    if (p) {
        g_info("\tDeleting Processor '%s'\n", name);
        g_hash_table_remove(self->processor_map, name);
        processor_free(p);
        return;
    }

    GeglNode *n = g_hash_table_lookup(self->node_map, name);
    g_return_if_fail(n);

    g_info("\t DEL %s()\n", name);
    g_hash_table_remove(self->node_map, name);
    gegl_node_remove_child(self->top, n);
}

GeglNode *
graph_get_gegl_node(Graph *self, const gchar *name) {
    g_return_val_if_fail(self, NULL);
    g_return_val_if_fail(name, NULL);
    return g_hash_table_lookup(self->node_map, name);
}

gchar *
graph_get_node_component(Graph *self, const gchar *name) {
    g_return_val_if_fail(self, NULL);
    g_return_val_if_fail(name, NULL);

    Processor *p = g_hash_table_lookup(self->processor_map, name);
    if (p) {
        return g_strdup("Processor");
    }

    GeglNode *n = g_hash_table_lookup(self->node_map, name);
    g_return_val_if_fail(n, NULL);
    gchar *operation = NULL;
    gegl_node_get(n, "operation", &operation, NULL);
    g_assert(operation);
    gchar *comp = library_get_component_name(self->component_lib, operation);
    return comp;
}

gchar **
graph_list_nodes(Graph *self, gint *no_nodes_out) {
    GList *processors = g_hash_table_get_keys(self->processor_map);
    GList *nodes = g_hash_table_get_keys(self->node_map);
    const gint total_length = g_list_length(processors)+g_list_length(nodes);

    gchar **names = g_new0(gchar *, total_length+1);
    for(int i=0; i<g_list_length(processors); i++) {
        names[i] = g_strdup((gchar *)g_list_nth_data(processors, i));
    }
    for(int i=0; i<g_list_length(nodes); i++) {
        names[i+g_list_length(processors)] = g_strdup((gchar *)g_list_nth_data(nodes, i));
    }

    g_list_free(processors);
    g_list_free(nodes);
    if (no_nodes_out) {
        *no_nodes_out = total_length;
    }
    return names;
}

void
graph_add_edge(Graph *self,
        const gchar *src, const gchar *srcport,
        const gchar *tgt, const gchar *tgtport)
{
    g_return_if_fail(self);
    g_return_if_fail(src);
    g_return_if_fail(tgt);
    g_return_if_fail(srcport);
    g_return_if_fail(tgtport);

    Processor *p = g_hash_table_lookup(self->processor_map, tgt);
    if (p) {
        GeglNode *s = g_hash_table_lookup(self->node_map, src);
        g_return_if_fail(s);
        g_info("\tConnecting Processor '%s' to node '%s'\n", tgt, src);
        processor_set_target(p, s);
        return;
    }

    GeglNode *t = g_hash_table_lookup(self->node_map, tgt);
    GeglNode *s = g_hash_table_lookup(self->node_map, src);

    g_return_if_fail(t);
    g_return_if_fail(s);

    g_info("\t%s %s -> %s %s\n",
           src, srcport, tgtport, tgt);
    gegl_node_connect_to(s, srcport, t, tgtport);
}

void
graph_remove_edge(Graph *self,
        const gchar *src, const gchar *srcport,
        const gchar *tgt, const gchar *tgtport)
{
    g_return_if_fail(self);
    g_return_if_fail(src);
    g_return_if_fail(tgt);
    g_return_if_fail(srcport);
    g_return_if_fail(tgtport);

    Processor *p = g_hash_table_lookup(self->processor_map, tgt);
    if (p) {
        GeglNode *s = g_hash_table_lookup(self->node_map, src);
        g_return_if_fail(s);
        g_info("\tDisconnecting Processor '%s' from node '%s'\n", tgt, src);
        processor_set_target(p, NULL);
        return;
    }

    GeglNode *t = g_hash_table_lookup(self->node_map, tgt);
    GeglNode *s = g_hash_table_lookup(self->node_map, src);
    g_return_if_fail(t);
    g_return_if_fail(s);

    g_info("\tDEL %s %s -> %s %s\n",
            src, srcport, tgtport, tgt);
    gegl_node_disconnect(t, tgtport);
}

void
graph_load_json(Graph *self, JsonParser *parser) {

    JsonNode *rootnode = json_parser_get_root(parser);
    g_assert(JSON_NODE_HOLDS_OBJECT(rootnode));
    JsonObject *root = json_node_get_object(rootnode);

    // Processes
    JsonObject *processes = json_object_get_object_member(root, "processes");
    g_assert(processes);

    GList *process_names = json_object_get_members(processes);
    for (int i=0; i<g_list_length(process_names); i++) {
        const gchar *name = g_list_nth_data(process_names, i);
        JsonObject *proc = json_object_get_object_member(processes, name);
        const gchar *component = json_object_get_string_member(proc, "component");
        graph_add_node(self, name, component);
    }

    //g_free(process_names); crashes??

    // Connections
    JsonArray *connections = json_object_get_array_member(root, "connections");
    g_assert(connections);
    for (int i=0; i<json_array_get_length(connections); i++) {
        JsonObject *conn = json_array_get_object_element(connections, i);

        JsonObject *tgt = json_object_get_object_member(conn, "tgt");
        const gchar *tgt_proc = json_object_get_string_member(tgt, "process");
        const gchar *tgt_port = json_object_get_string_member(tgt, "port");

        JsonNode *srcnode = json_object_get_member(conn, "src");
        if (srcnode) {
            // Connection
            JsonObject *src = json_object_get_object_member(conn, "src");
            const gchar *src_proc = json_object_get_string_member(src, "process");
            const gchar *src_port = json_object_get_string_member(src, "port");

            graph_add_edge(self, src_proc, src_port, tgt_proc, tgt_port);
        } else {
            // IIP
            JsonNode *datanode = json_object_get_member(conn, "data");
            GValue value = G_VALUE_INIT;
            g_assert(JSON_NODE_HOLDS_VALUE(datanode));
            json_node_get_value(datanode, &value);

            graph_add_iip(self, tgt_proc, tgt_port, &value);
            g_value_unset(&value);
        }
    }

    // Exported ports
    JsonObject *inports = json_object_get_object_member(root, "inports");
    GList *inport_names = json_object_get_members(inports);
    for (int i=0; i<g_list_length(inport_names); i++) {
        const gchar *name = g_list_nth_data(inport_names, i);
        JsonObject *conn = json_object_get_object_member(inports, name);
        const gchar *proc = json_object_get_string_member(conn, "process");
        const gchar *port = json_object_get_string_member(conn, "port");
        graph_add_port(self, GraphInPort, name, proc, port);
    }
    JsonObject *outports = json_object_get_object_member(root, "outports");
    GList *outport_names = json_object_get_members(outports);
    for (int i=0; i<g_list_length(outport_names); i++) {
        const gchar *name = g_list_nth_data(outport_names, i);
        JsonObject *conn = json_object_get_object_member(outports, name);
        const gchar *proc = json_object_get_string_member(conn, "process");
        const gchar *port = json_object_get_string_member(conn, "port");
        graph_add_port(self, GraphOutPort, name, proc, port);
    }
}

gboolean
iip_equivalent(const GValue *a, const GValue *b) {
    // TEMP: consider equivalent if their serialization is
    gboolean aconv, bconv;
    JsonNode *an = json_from_gvalue(a, &aconv);
    JsonNode *bn = json_from_gvalue(b, &bconv);
    gchar *as = json_stringify_node(an, NULL);
    gchar *bs = json_stringify_node(bn, NULL);
    return aconv && bconv && (g_strcmp0(as, bs) == 0);
}

void
graph_visit_edges_for_nodes(Graph *self, GraphEdgeVisitFunc visit_func, gpointer user_data,
                            gchar **nodes, gint no_nodes) {

    // Connections
    // In GEGL the connection order is unimportant,
    // so we just loop through all nodes and their input connections
    // implicitly we'll then also get the outputs, from the edge
    for (int i=0; i<no_nodes; i++) {
        const gchar *tgt_name = nodes[i];
        GeglNode *tgt_node = g_hash_table_lookup(self->node_map, tgt_name);

        if (tgt_node) {
            gchar **pads = gegl_node_list_input_pads(tgt_node);
            gchar *tgt_pad = pads ? pads[0] : NULL;
            gint i = 0;
            while (tgt_pad) {
                gchar *src_pad = NULL;
                GeglNode *src_node = gegl_node_get_producer(tgt_node, tgt_pad, &src_pad);
                if (src_node) {
                    // has connection
                    g_assert(src_pad);
                    gpointer src_name = src_node;
                    g_hash_table_find(self->node_map, gh_value_equals_outkey, &src_name);

                    GraphEdge edge = { src_name, src_pad, tgt_name, tgt_pad };
                    visit_func(self, &edge, user_data);

                    g_free(src_pad);
                }
                tgt_pad = pads[++i];
            }
        } else {
            Processor *processor = g_hash_table_lookup(self->processor_map, tgt_name);
            g_assert(processor);
            GeglNode *src_node = processor->node;
            if (src_node) {
                // has connection
                gpointer src_name = src_node;
                g_hash_table_find(self->node_map, gh_value_equals_outkey, &src_name);

                // XXX: will break if GEGL gets more output pads
                GraphEdge edge = { src_name, "output", tgt_name, "input" };
                visit_func(self, &edge, user_data);
            }
        }
    }

}

void
graph_visit_edges(Graph *self, GraphEdgeVisitFunc visit_func, gpointer user_data) {
    gint no_nodes = 0;
    gchar **nodes = graph_list_nodes(self, &no_nodes);
    graph_visit_edges_for_nodes(self, visit_func, user_data, nodes, no_nodes);
    g_strfreev(nodes);
}

JsonObject *
graph_edge_to_json(const GraphEdge *edge) {
    g_return_val_if_fail(edge, NULL);

    JsonObject *conn = json_object_new();

    JsonObject *tgt = json_object_new();
    json_object_set_object_member(conn, "tgt", tgt);
    json_object_set_string_member(tgt, "process", edge->tgt_name);
    json_object_set_string_member(tgt, "port", edge->tgt_port);

    JsonObject *src = json_object_new();
    json_object_set_object_member(conn, "src", src);
    json_object_set_string_member(src, "process", edge->src_name);
    json_object_set_string_member(src, "port", edge->src_port);

    return conn;
}

void add_edge_json_func(Graph *graph, const GraphEdge *edge, gpointer user_data) {
    g_assert(user_data);
    JsonArray *connections = (JsonArray *)user_data;

    JsonObject *conn = graph_edge_to_json(edge);
    json_array_add_object_element(connections, conn);
}

JsonObject *
graph_save_json(Graph *self) {

    JsonObject *root = json_object_new();

    // Properties
    JsonObject *properties = json_object_new();
    json_object_set_object_member(root, "properties", properties);

    // Exported ports
    JsonObject *inports = json_object_new();
    json_object_set_object_member(root, "inports", inports);
    JsonObject *outports = json_object_new();
    json_object_set_object_member(root, "outports", outports);

    // Processes
    JsonObject *processes = json_object_new();
    json_object_set_object_member(root, "processes", processes);
    gint no_nodes = 0;
    gchar **nodes = graph_list_nodes(self, &no_nodes);
    for (int i=0; i<no_nodes; i++) {
        const gchar *name = nodes[i];
        JsonObject *proc = json_object_new();
        const gchar *component = graph_get_node_component(self, name);
        json_object_set_string_member(proc, "component", component);
        json_object_set_object_member(processes, name, proc);
    }


    // Connections
    // In GEGL the connection order is unimportant,
    // so we just loop through all nodes and their input connections
    JsonArray *connections = json_array_new();
    json_object_set_array_member(root, "connections", connections);
    graph_visit_edges(self, add_edge_json_func, connections);

    // IIPs
    for (int i=0; i<no_nodes; i++) {
        const gchar *node_name = nodes[i];
        GeglNode *node = g_hash_table_lookup(self->node_map, node_name);
        if (!node) {
            // Processor
            continue;
        }

        gchar *operation = NULL;
        gegl_node_get(node, "operation", &operation, NULL);
        g_assert(operation);
        guint n_properties = 0;
        GParamSpec** properties = gegl_operation_list_properties(operation, &n_properties);
        for (int i=0; i<n_properties; i++) {
            GParamSpec *prop = properties[i];
            const gchar *id = g_param_spec_get_name(prop);
            const GValue *def = g_param_spec_get_default_value(prop);
            GValue value = G_VALUE_INIT;
            gegl_node_get_property(node, id, &value);

            // Don't send defaults
            if (!iip_equivalent(&value, def)) {
                JsonNode *value_json = json_from_gvalue(&value, NULL);
                JsonObject *conn = json_object_new();
                json_array_add_object_element(connections, conn);

                JsonObject *tgt = json_object_new();
                json_object_set_string_member(tgt, "process", node_name);
                json_object_set_string_member(tgt, "port", id);
                json_object_set_object_member(conn, "tgt", tgt);

                json_object_set_member(conn, "data", value_json);
            }

            g_value_unset(&value);
        }

    }
    g_strfreev(nodes);

    return root;
}

gboolean
graph_load_json_file(Graph *self, const gchar *path, GError **error) {

    JsonParser *parser = json_parser_new();

    gboolean success = json_parser_load_from_file(parser, path, error);
    if (success) {
        graph_load_json(self, parser);
    }

    g_object_unref(parser);
    return success;
}

gboolean
graph_load_json_data(Graph *self, const gchar *data, ssize_t length, GError **error) {

    JsonParser *parser = json_parser_new();

    gboolean success = json_parser_load_from_data(parser, data, length, error);
    if (success) {
        graph_load_json(self, parser);
    }

    g_object_unref(parser);
    return success;
}

gboolean
graph_load_stdin(Graph *self, GError **error) {
    size_t max_bytes = 1e6;
    void * buffer = g_malloc(max_bytes);
    size_t bytes_read = 0;
    GInputStream *stdin_stream = g_unix_input_stream_new(STDIN_FILENO, FALSE);

    gboolean stdin_read = g_input_stream_read_all(stdin_stream, buffer, max_bytes, &bytes_read, NULL, error);
    gboolean json_loaded = FALSE;
    if (stdin_read) {
        json_loaded = graph_load_json_data(self, buffer, bytes_read, error);
    }

    g_object_unref(stdin_stream);
    return stdin_read && json_loaded;
}

const gchar *
graph_find_processor_name(Graph *self, Processor *processor) {
    g_return_val_if_fail(self, NULL);
    g_return_val_if_fail(processor, NULL);
    g_return_val_if_fail(self->processor_map, NULL);

    GHashTableIter iter;
    gpointer key, value;
    gchar *processor_name = NULL;

    g_hash_table_iter_init(&iter, self->processor_map);
    while (g_hash_table_iter_next(&iter, &key, &value))
    {
        if (value == processor) {
            processor_name = (gchar *)key;
        }
    }
    return processor_name;
}
