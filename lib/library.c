//     imgflo - Flowhub.io Image-processing runtime
//     (c) 2014 The Grid
//     imgflo may be freely distributed under the MIT license

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <gegl-plugin.h>

static const gchar *
icon_for_op(const gchar *op, const gchar *categories) {

    // TODO: go through all GEGL categories and operations, find more appropriate icons
    // http://gegl.org/operations.html
    // http://fortawesome.github.io/Font-Awesome/icons/
    // PERFORMANCE: build up a hashmap once, lookup on-demand afterwards
    if (g_strcmp0(op, "gegl:crop") == 0) {
        return "crop";
    } else if (g_strstr_len(op, -1, "save") != NULL) {
        return "save";
    } else if (g_strstr_len(op, -1, "load") != NULL) {
        return "file-o";
    } else if (g_strstr_len(op, -1, "text") != NULL) {
        return "font";
    } else {
        return "picture-o";
    }
}


static JsonNode *
json_from_gvalue(const GValue *val) {
    GType type = G_VALUE_TYPE(val);
    JsonNode *ret = json_node_alloc();
    json_node_init(ret, JSON_NODE_VALUE);

    if (G_VALUE_HOLDS_STRING(val)) {
        json_node_set_string(ret, g_value_get_string(val));
    } else if (G_VALUE_HOLDS(val, G_TYPE_BOOLEAN)) {
        json_node_set_boolean(ret, g_value_get_boolean(val));
    } else if (G_VALUE_HOLDS_DOUBLE(val)) {
        json_node_set_double(ret, g_value_get_double(val));
    } else if (G_VALUE_HOLDS_FLOAT(val)) {
        json_node_set_double(ret, g_value_get_float(val));
    } else if (G_VALUE_HOLDS_INT(val)) {
        json_node_set_int(ret, g_value_get_int(val));
    } else if (G_VALUE_HOLDS_UINT(val)) {
        json_node_set_int(ret, g_value_get_uint(val));
    } else if (G_VALUE_HOLDS_INT64(val)) {
        json_node_set_int(ret, g_value_get_int64(val));
    } else if (G_VALUE_HOLDS_UINT64(val)) {
        json_node_set_int(ret, g_value_get_uint64(val));
    } else if (g_type_is_a(type, GEGL_TYPE_COLOR)) {
        // TODO: support GeglColor
        json_node_init_null(ret);
    } else if (g_type_is_a(type, GEGL_TYPE_PATH) || g_type_is_a(type, GEGL_TYPE_CURVE)) {
        // TODO: support GeglPath / GeglCurve
        json_node_init_null(ret);
    } else if (G_VALUE_HOLDS_ENUM(val)) {
        // TODO: support enums
        json_node_init_null(ret);
    } else if (G_VALUE_HOLDS_POINTER(val) || G_VALUE_HOLDS_OBJECT(val)) {
        // TODO: go over operations which report this generic
        json_node_init_null(ret);
    } else {
        json_node_init_null(ret);
        g_warning("Cannot map GValue '%s' to JSON", g_type_name(type));
    }

    return ret;
}

static JsonArray *
json_for_enum(GType type) {
    JsonArray *array = json_array_new();
    GEnumClass *klass = (GEnumClass *)g_type_class_ref(type);

    for (int i=klass->minimum; i<klass->maximum; i++) {
        GEnumValue *val = g_enum_get_value(klass, i);
        json_array_add_string_element(array, val->value_nick);
        // TODO: verify that we should use nick for enums. Strange uses of nick and name in GEGL...
    }
    g_type_class_unref((gpointer)klass);

    return array;
}

static const gchar *
noflo_type_for_gtype(GType type) {

    const char *n = g_type_name(type);
    const gboolean is_integer = g_type_is_a(type, G_TYPE_INT) || g_type_is_a(type, G_TYPE_INT64)
            || g_type_is_a(type, G_TYPE_UINT) || g_type_is_a(type, G_TYPE_UINT64);
    if (is_integer) {
        return "int";
    } else if (g_type_is_a(type, G_TYPE_FLOAT) || g_type_is_a(type, G_TYPE_DOUBLE)) {
        return "number";
    } else if (g_type_is_a(type, G_TYPE_BOOLEAN)) {
        return "boolean";
    } else if (g_type_is_a(type, G_TYPE_STRING)) {
        return "string";
    } else if (g_type_is_a(type, GEGL_TYPE_COLOR)) {
        return "color";
    } else if (g_type_is_a(type, GEGL_TYPE_PATH) || g_type_is_a(type, GEGL_TYPE_CURVE)) {
        // TODO: support GeglPaths and GeglCurve
        return n;
    } else if (g_type_is_a(type, G_TYPE_ENUM)) {
        // FIXME: make sure "enum" is the standardized value
        return "enum";
    } else if (g_type_is_a(type, G_TYPE_POINTER) || g_type_is_a(type, G_TYPE_OBJECT)) {
        // TODO: go through the operation with this type, try to make more specific, and serializable
        return n;
    } else {
        g_warning("Could not map GType '%s' to a NoFlo FBP type", n);
        return n;
    }
}

static void
add_pad_ports(JsonArray *ports, gchar **pads) {
    if (pads == NULL) {
        return;
    }
    gchar *pad_name = pads[0];
    while (pad_name) {
        // TODO: look up type of pad from its paramspec. Requires GeglPad public API in GEGL
        const gchar *type = "buffer";
        JsonObject *port = json_object_new();
        json_object_set_string_member(port, "id", pad_name);
        json_object_set_string_member(port, "type", type);
        json_array_add_object_element(ports, port);
        pad_name = *(++pads);
    }
}

static JsonArray *
outports_for_operation(const gchar *name)
{
    JsonArray *outports = json_array_new();

    // Pads
    GeglNode *node = gegl_node_new();
    gegl_node_set(node, "operation", name, NULL);

    gchar **pads = gegl_node_list_output_pads(node);
    add_pad_ports(outports, pads);
    g_strfreev(pads);

    if (json_array_get_length(outports) == 0) {
        // Add dummy "process" port for sinks, used to connect to a Processor node
        JsonObject *port = json_object_new();
        json_object_set_string_member(port, "id", "process");
        json_object_set_string_member(port, "type", "N/A");
        json_array_add_object_element(outports, port);
    }

    g_object_unref(node);

    return outports;
}

static JsonArray *
inports_for_operation(const gchar *name)
{
    JsonArray *inports = json_array_new();

    // Pads
    GeglNode *node = gegl_node_new();
    gegl_node_set(node, "operation", name, NULL);
    gchar **pads = gegl_node_list_input_pads(node);
    add_pad_ports(inports, pads);
    g_strfreev(pads);

    // Properties
    guint n_properties = 0;
    GParamSpec** properties = gegl_operation_list_properties(name, &n_properties);
    for (int i=0; i<n_properties; i++) {
        GParamSpec *prop = properties[i];
        const gchar *id = g_param_spec_get_name(prop);
        GType type = G_PARAM_SPEC_VALUE_TYPE(prop);
        const gchar *type_name = noflo_type_for_gtype(type);
        const GValue *def = g_param_spec_get_default_value(prop);
        JsonNode *def_json = json_from_gvalue(def);
        const gchar *blurb = g_param_spec_get_blurb(prop);
        const gchar *nick = g_param_spec_get_nick(prop);
        const gchar *description = (blurb) ? blurb : nick;

        JsonObject *port = json_object_new();
        json_object_set_string_member(port, "id", id);
        json_object_set_string_member(port, "type", type_name);
        json_object_set_string_member(port, "description", description);
        if (!json_node_is_null(def_json)) {
            json_object_set_member(port, "default", def_json);
        }
        if (G_TYPE_IS_ENUM(type)) {
            JsonArray *values = json_for_enum(type);
            json_object_set_array_member(port, "values", values);
        }

        json_array_add_object_element(inports, port);
    }

    return inports;
}

static JsonObject *
processor_component(void)
{
    JsonObject *component = json_object_new();
    json_object_set_string_member(component, "name", "Processor");
    json_object_set_string_member(component, "description", "Process a connected node");
    json_object_set_string_member(component, "icon", "spinner");

    // Inports
    JsonArray *inports = json_array_new();
    JsonObject *input = json_object_new();
    json_object_set_string_member(input, "id", "input");
    json_object_set_string_member(input, "type", "buffer");
    json_array_add_object_element(inports, input);
    json_object_set_array_member(component, "inPorts", inports);

    // Outports
    JsonArray *outports = json_array_new();
    json_object_set_array_member(component, "outPorts", outports);

    return component;
}


JsonObject *
library_component(const gchar *op)
{
    g_return_if_fail(op);

    if (g_strcmp0(op, "Processor") == 0) {
        return processor_component();
    }

    gchar *name = geglop2component(op);
    const gchar *description = gegl_operation_get_key(op, "description");
    const gchar *categories = gegl_operation_get_key(op, "categories");

    JsonObject *component = json_object_new();
    json_object_set_string_member(component, "name", name);
    json_object_set_string_member(component, "description", description);
    json_object_set_string_member(component, "icon", icon_for_op(op, categories));

    JsonArray *inports = inports_for_operation(op);
    json_object_set_array_member(component, "inPorts", inports);

    JsonArray *outports = outports_for_operation(op);
    json_object_set_array_member(component, "outPorts", outports);

    return component;
}

void
try_print_error(GError *err) {
    if (!err) {
        return;
    }
    g_printerr("GERROR: %s\n", err->message);
}

static GFile *
get_source_file(const gchar *op) {
    // TEMP: use g_dir_make_tmp, store in Library instance
    const gchar *components_path = "components/dynamic";
    gboolean exists = g_mkdir_with_parents(components_path, 0755) == 0;
    g_assert(exists);
    gchar *opfile = g_strdup_printf("%s.c", op);
    gchar *operation_path = g_strjoin("/", components_path, opfile, NULL);
    g_free(opfile);
    GFile *file = g_file_new_for_path(operation_path);
    g_free(operation_path);
    return file;
}

static void
compile_plugins() {
    // FIXME: nasty hacky system()
    system("make components COMPONENTDIR=components/dynamic");
}

static void
reload_plugins() {
    const gchar *p = "/home/jon/contrib/code/imgflo-server/runtime/install/lib/imgflo/operations";
    gegl_load_module_directory(p);
}

gboolean
library_set_source(const gchar *op, const gchar *source) {

    GFile *file = get_source_file(op);
    GError *err = NULL;
    GOutputStream *stream = (GOutputStream *)g_file_replace(file, NULL, FALSE, G_FILE_CREATE_PRIVATE, NULL, &err);
    try_print_error(err);
    g_assert(stream);

    gsize bytes = strlen(source);
    gsize bytes_written = 0;
    const gboolean success = g_output_stream_write_all(stream, source, bytes, &bytes_written, NULL, &err);

    try_print_error(err);
    const gboolean closed = g_output_stream_close(stream, NULL, &err);
    try_print_error(err);

    compile_plugins();
    reload_plugins();

    g_clear_error(&err);
    g_object_unref(file);
    return success && closed;
}

gchar *
library_get_source(const gchar *op) {
    GFile *file = get_source_file(op);
    GError *err = NULL;
    GInputStream *stream = (GInputStream *)g_file_read(file, NULL, &err);
    try_print_error(err);
    g_assert(stream);

    gsize bytes = 1024*1000;
    gchar* buffer = g_new(gchar, bytes);
    gsize bytes_read = 0;
    gboolean success = g_input_stream_read_all(stream, buffer, bytes, &bytes_read, NULL, &err);
    try_print_error(err);

    g_object_unref(stream);
    g_object_unref(file);
    g_clear_error(&err);
    if (!success) {
        g_free(buffer);
        return NULL;
    }
    return buffer;
}

gchar **
library_list_components(gint *len) {
    // Our special "operations"
    const gchar *special_ops[] = {
        "Processor"
    };
    const gint no_special_ops = G_N_ELEMENTS(special_ops);

    // GEGL operations
    guint no_ops = 0;
    gchar **operation_names = gegl_list_operations(&no_ops);
    if (no_ops == 0) {
        g_warning("No GEGL operations found");
    }

    // Concatenate both
    gchar **ret = (gchar **)g_new0(gchar*, no_special_ops+no_ops+1); // leave NULL at end
    for (int i=0; i<no_special_ops; i++) {
        ret[i] = g_strdup(special_ops[i]);
    }
    for (int i=0; i<no_ops; i++) {
        const gchar *op = operation_names[i];
        if (g_strcmp0(op, "gegl:seamless-clone-compose") == 0) {
            // FIXME: reported by GEGL but cannot be instantiated...
            op = NULL;
        }
        ret[no_special_ops+i] = g_strdup(op);
    }

    if (len) {
        *len = no_special_ops+no_ops;
    }
    return ret;
}
