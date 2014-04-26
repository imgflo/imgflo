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

// Convert between the lib/Foo used by NoFlo and lib:foo used by GEGL
gchar *
component2geglop(const gchar *name) {
    gchar *dup = g_strdup(name);
    gchar *sep = g_strstr_len(dup, -1, "/");
    if (sep) {
        *sep = ':';
    }
    g_ascii_strdown(dup, -1);
    return dup;
}

gchar *
geglop2component(const gchar *name) {
    gchar *dup = g_strdup(name);
    gchar *sep = g_strstr_len(dup, -1, ":");
    if (sep) {
        *sep = '/';
    }
    g_ascii_strdown(dup, -1);
    return dup;
}


typedef struct {
    GeglNode *top;
    GHashTable *node_map;
    GHashTable *processor_map;
} Graph;


Graph *
graph_new() {
    Graph *self = g_new(Graph, 1);
    self->top = gegl_node_new();
    self->node_map = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    self->processor_map = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    return self;
}

void
graph_free(Graph *self) {

    g_object_unref(self->top);
    // FIXME: leaks memeory. Go through all nodes and processors and free
    g_hash_table_destroy(self->node_map);
    g_hash_table_destroy(self->processor_map);

    g_free(self);
}

void
graph_add_iip(Graph *self, const gchar *node, const gchar *port, GValue *value)
{
    g_return_if_fail(self);
    g_return_if_fail(node);
    g_return_if_fail(port);
    g_return_if_fail(value);

    const gchar *iip = G_VALUE_HOLDS_STRING(value) ? g_value_get_string(value) : "IIP";
    g_print("\t'%s' -> %s %s\n", iip, port, node);

    GeglNode *t = g_hash_table_lookup(self->node_map, node);

    GParamSpec *paramspec = gegl_node_find_property(t, port);
    if (paramspec) {
        GType value_type = G_VALUE_TYPE(value);
        GType target_type = G_PARAM_SPEC_VALUE_TYPE(paramspec);

        if (g_type_is_a(value_type, target_type)) {
            // No conversion needed
            gegl_node_set_property(t, port, value);
        } else {
            // TODO: attempt generic GValue transforms here?
            if (G_IS_PARAM_SPEC_DOUBLE(paramspec)) {
                gdouble d = g_ascii_strtod (iip, NULL);
                if (d != 0.0) {
                    gegl_node_set(t, port, d, NULL);
                }
            } else {
                g_printerr("target_type=%s value_type=%s\n",
                        g_type_name(target_type), g_type_name(value_type));
                g_printerr("Unable to convert value for property '%s' of node '%s'\n",
                        port, node);
            }
        }
    } else {
        g_printerr("Node '%s' has no property '%s'\n", node, port);
    }
}

void
graph_remove_iip(Graph *self, const gchar *node, const gchar *port)
{
    g_return_if_fail(self);
    g_return_if_fail(node);
    g_return_if_fail(port);

    g_print("\tDEL '%s' -> %s %s\n", "IIP", node, port);

    GeglNode *t = g_hash_table_lookup(self->node_map, node);
    g_return_if_fail(t);
    GParamSpec *paramspec = gegl_node_find_property(t, port);
    if (paramspec) {
        const GValue *def = g_param_spec_get_default_value(paramspec);
        gegl_node_set_property(t, port, def);
    } else {
        g_printerr("Node '%s' has no property '%s'\n", node, port);
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
        g_print("\tAdding Processor: %s\n", name);
        return;
    }

    gchar *op = component2geglop(component);
    // FIXME: check that operation is correct
    GeglNode *n = gegl_node_new_child(self->top, "operation", op, NULL);

    g_return_if_fail(n);

    g_print("\t%s(%s)\n", name, op);
    g_hash_table_insert(self->node_map, (gpointer)g_strdup(name), (gpointer)n);

    g_free(op);
}

void
graph_remove_node(Graph *self, const gchar *name)
{
    g_return_if_fail(self);
    g_return_if_fail(name);

    Processor *p = g_hash_table_lookup(self->processor_map, name);
    if (p) {
        g_print("\tDeleting Processor '%s'\n", name);
        g_hash_table_remove(self->processor_map, name);
        processor_free(p);
        return;
    }

    GeglNode *n = g_hash_table_lookup(self->node_map, name);
    g_return_if_fail(n);

    g_print("\t DEL %s()\n", name);
    g_hash_table_remove(self->node_map, name);
    gegl_node_remove_child(self->top, n);
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
        g_print("\tConnecting Processor '%s' to node '%s'\n", tgt, src);
        processor_set_target(p, s);
        return;
    }

    GeglNode *t = g_hash_table_lookup(self->node_map, tgt);
    GeglNode *s = g_hash_table_lookup(self->node_map, src);

    g_return_if_fail(t);
    g_return_if_fail(s);

    g_print("\t%s %s -> %s %s\n", src, srcport,
                                        tgtport, tgt);
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
        g_print("\tDisconnecting Processor '%s' from node '%s'\n", tgt, src);
        processor_set_target(p, NULL);
        return;
    }

    GeglNode *t = g_hash_table_lookup(self->node_map, tgt);
    GeglNode *s = g_hash_table_lookup(self->node_map, src);
    g_return_if_fail(t);
    g_return_if_fail(s);

    g_print("\tDEL %s %s -> %s %s\n",
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
