//     imgflo - Flowhub.io Image-processing runtime
//     (c) 2014 The Grid
//     imgflo may be freely distributed under the MIT license

#include <json-glib/json-glib.h>

gchar *
json_stringify_node(JsonNode *node, gsize *length_out) {
    JsonGenerator *generator = json_generator_new();
    json_generator_set_root(generator, node);

    gsize len = 0;
    gchar *data = json_generator_to_data(generator, &len);
    g_object_unref(generator);

    if (length_out) {
        *length_out = len;
    }
    return data;
}

gchar *
json_stringify(JsonObject *root, gsize *length_out) {
    JsonNode *node = json_node_new(JSON_NODE_OBJECT);
    json_node_take_object(node, root);

    return json_stringify_node(node, length_out);
}

