//     imgflo - Flowhub.io Image-processing runtime
//     (c) 2014 The Grid
//     imgflo may be freely distributed under the MIT license

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <json-glib/json-glib.h>
#include <gegl-plugin.h>

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
json_from_gvalue(const GValue *val, gboolean *supported_out) {
    GType type = G_VALUE_TYPE(val);
    JsonNode *ret = json_node_alloc();
    json_node_init(ret, JSON_NODE_VALUE);

    gboolean supported = FALSE;
    if (G_VALUE_HOLDS_STRING(val)) {
        json_node_set_string(ret, g_value_get_string(val));
        supported = TRUE;
    } else if (G_VALUE_HOLDS(val, G_TYPE_BOOLEAN)) {
        json_node_set_boolean(ret, g_value_get_boolean(val));
        supported = TRUE;
    } else if (G_VALUE_HOLDS_DOUBLE(val)) {
        json_node_set_double(ret, g_value_get_double(val));
        supported = TRUE;
    } else if (G_VALUE_HOLDS_FLOAT(val)) {
        json_node_set_double(ret, g_value_get_float(val));
        supported = TRUE;
    } else if (G_VALUE_HOLDS_INT(val)) {
        json_node_set_int(ret, g_value_get_int(val));
        supported = TRUE;
    } else if (G_VALUE_HOLDS_UINT(val)) {
        json_node_set_int(ret, g_value_get_uint(val));
        supported = TRUE;
    } else if (G_VALUE_HOLDS_INT64(val)) {
        json_node_set_int(ret, g_value_get_int64(val));
        supported = TRUE;
    } else if (G_VALUE_HOLDS_UINT64(val)) {
        json_node_set_int(ret, g_value_get_uint64(val));
        supported = TRUE;
    } else if (g_type_is_a(type, GEGL_TYPE_COLOR)) {
        guint8 *hex = g_new0(guint8, 4);
        GeglColor *color = g_value_get_object(val);
        g_assert(color);
        gegl_color_get_pixel(color, babl_format("R'G'B'A u8"), hex);
        gchar *rgba_string = g_strdup_printf("#%02x%02x%02x%02x",
                                             hex[0], hex[1], hex[2], hex[3]);
        json_node_set_string(ret, rgba_string);
        g_free(rgba_string);
        g_free(hex);
        supported = TRUE;
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
        imgflo_warning("Cannot map GValue '%s' to JSON", g_type_name(type));
    }

    if (supported_out) {
        *supported_out = supported;
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
fbp_type_for_param(GParamSpec *prop) {
    GType type = G_PARAM_SPEC_VALUE_TYPE(prop);
    const char *n = g_type_name(type);
    const gboolean is_integer = g_type_is_a(type, G_TYPE_INT) || g_type_is_a(type, G_TYPE_INT64)
            || g_type_is_a(type, G_TYPE_UINT) || g_type_is_a(type, G_TYPE_UINT64);
    if (is_integer) {
        return "int";
    } else if (g_type_is_a(G_PARAM_SPEC_TYPE(prop), GEGL_TYPE_PARAM_URI)) {
        return "uri";
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
        imgflo_warning("Could not map GType '%s' to a NoFlo FBP type", n);
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

JsonArray *
library_outports_for_operation(const gchar *name)
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

JsonArray *
library_inports_for_operation(const gchar *name)
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
        const gchar *type_name = fbp_type_for_param(prop);
        const GValue *def = g_param_spec_get_default_value(prop);
        JsonNode *def_json = json_from_gvalue(def, NULL);
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

struct _Library;

static const gchar * const SETSOURCE_COMP_PREFIX = "imgflo-setsource-";
static const gchar * const SETSOURCE_COMP_FORMAT = "imgflo-setsource-%s-%d"; // basename, rev

typedef void (* LibraryComponentChangedCallback) (struct _Library *library, gchar *op, gpointer user_data);
LibraryComponentChangedCallback on_component_changed;
gpointer on_component_changed_data;

typedef struct _Library {
    gchar *source_path;
    gchar *build_path;
    GHashTable *setsource_components;
} Library;

Library *
library_new() {
    Library *self = g_new(Library, 1);

    gchar *cwd = g_get_current_dir();

    self->source_path = g_strdup_printf("%s/spec/out/components2", cwd);
    self->build_path = g_strdup_printf("%s/spec/out/build2", cwd);
    g_assert(g_mkdir_with_parents(self->build_path, 0755) == 0);
    g_assert(g_mkdir_with_parents(self->source_path, 0755) == 0);
    self->setsource_components = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                        g_free, NULL);
    g_free(cwd);

    return self;
}

void
library_free(Library *self) {

    g_hash_table_destroy(self->setsource_components);
    g_free(self->build_path);
    g_free(self->source_path);

    g_free(self);
}

void
try_print_error(GError *err) {
    if (!err) {
        return;
    }
    imgflo_warning("GERROR: %s\n", err->message);
}

static GFile *
get_source_file(const gchar *components_path, const gchar *op) {
    gchar *opfile = g_strdup_printf("%s.c", op);
    gchar *operation_path = g_strjoin("/", components_path, opfile, NULL);
    g_free(opfile);
    GFile *file = g_file_new_for_path(operation_path);
    g_free(operation_path);
    return file;
}

static void
compile_plugin(GFile *file, const gchar *build_dir, gint rev) {
    gchar *component = g_file_get_basename(file);
    GFile* dir = g_file_get_parent(file);
    gchar* dir_name = g_file_get_path(dir);

    gchar *stdout = NULL;
    gchar *stderr = NULL;
    gint exitcode = 1;
    GError *err = NULL;
    gchar **argv = g_new0(gchar *, 10);
    argv[0] = g_strdup("/usr/bin/env");
    argv[1] = g_strdup("make");
    argv[2] = g_strdup("component");
    argv[3] = g_strdup_printf("COMPONENT=%s", component);
    argv[4] = g_strdup_printf("COMPONENTDIR=%s", dir_name);
    argv[5] = g_strdup_printf("COMPONENTINSTALLDIR=%s", build_dir);
    argv[6] = g_strdup_printf("COMPONENT_NAME_PREFIX=\"%s\"", SETSOURCE_COMP_PREFIX);
    argv[7] = g_strdup_printf("COMPONENT_NAME_SUFFIX=\"-%d\"", rev);
 
    const gboolean success = g_spawn_sync(NULL, argv, NULL,
                              G_SPAWN_DEFAULT, NULL, NULL,
                              &stdout, &stderr, &exitcode, &err);
    try_print_error(err);
    if (!success) {
        imgflo_error("Failed to compile operation: %s", stderr);
    }
    g_free(stdout);
    g_free(stderr);
    g_strfreev(argv);

    g_free(component);
    g_object_unref(dir);
    g_free(dir_name);
}

static void
reload_plugins(const gchar *path) {
    gegl_load_module_directory(path);
}

gboolean
is_setsource_comp(const gchar *name) {
    return g_str_has_prefix(name, SETSOURCE_COMP_PREFIX);
}

gint
find_op_revision(Library *self, const gchar *base, gchar **name_out) {
    g_return_val_if_fail(base, -1);

    // imgflo_debug("%s: %s\n", __PRETTY_FUNCTION__, base);

    gint current_rev = -1;
    gpointer val = NULL;
    const gboolean found = g_hash_table_lookup_extended(self->setsource_components, base, NULL, &val);
    if (found) {
        current_rev = GPOINTER_TO_INT(val);
        if (name_out) {
            *name_out = g_strdup_printf(SETSOURCE_COMP_FORMAT, base, current_rev);
        }
    }
    return current_rev;
}

// Return the component name for a GEGL operation
gchar *
library_get_component_name(Library *self, const gchar *op) {
    g_return_val_if_fail(self, NULL);
    g_return_val_if_fail(op, NULL);
    // FIXME: take into account dynamic setsource ops
    return geglop2component(op);
}

// Return the GEGL operation name for a given component
gchar *
library_get_operation_name(Library *self, const gchar *comp) {
    g_return_val_if_fail(self, NULL);
    g_return_val_if_fail(comp, NULL);

    gchar *opname = NULL;
    const gint rev = find_op_revision(self, comp, &opname);
    if (rev < 0) {
        // normal op
        opname = g_strdup(comp);
    }

    g_assert(opname);
    gchar *geglname = component2geglop(opname);
    g_free(opname);
    return geglname;
}

// Returns operation name
gchar *
library_set_source(Library *self, const gchar *op, const gchar *source) {
    const gint next_rev = find_op_revision(self, op, NULL)+1;
    gchar *opname =  g_strdup_printf(SETSOURCE_COMP_FORMAT, op, next_rev);

    GFile *file = get_source_file(self->source_path, opname);
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

    compile_plugin(file, self->build_path, next_rev);
    reload_plugins(self->build_path);

    // imgflo_debug("%s: %s, %s, %d\n", __PRETTY_FUNCTION__, op, opname, next_rev);
    g_hash_table_replace(self->setsource_components, (gpointer)g_strdup(op), GINT_TO_POINTER(next_rev));

    g_clear_error(&err);
    g_object_unref(file);
    return (success && closed) ? opname : NULL;
}

void
print_kv(gpointer key, gpointer value, gpointer user_data) {
    imgflo_debug("%s: %d\n", (const gchar *)key, GPOINTER_TO_INT(value));
}

void
print_setsource_comps(GHashTable *table) {
    g_hash_table_foreach(table, print_kv, NULL);
}

gchar *
library_get_source(Library *self, const gchar *op) {

    gchar *opname = NULL;
    const gint op_rev = find_op_revision(self, op, &opname);
    if (op_rev < 0) {
        // Normal op
        gchar *name = component2geglop(op);
        const gchar *source = gegl_operation_get_key(name, "source");
        g_free(name);
        return source ? g_strdup(source) : NULL;
    }

    // imgflo_debug("%s: %s, %s\n", __PRETTY_FUNCTION__, op, opname);
    // print_setsource_comps(self->setsource_components);

    GFile *file = get_source_file(self->source_path, opname);
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
    g_free(opname);
    g_clear_error(&err);
    if (!success) {
        g_free(buffer);
        return NULL;
    }
    return buffer;
}

JsonObject *
library_get_component(Library *self, const gchar *comp)
{
    g_return_val_if_fail(comp, NULL);

    // Special ops
    if (g_strcmp0(comp, "Processor") == 0) {
        return processor_component();
    }

    // setsource dynamic ops
    gchar *op = NULL;
    gint setsource_rev = find_op_revision(self, comp, &op);
    if (setsource_rev == -1) {
        // normal component
        g_assert(!op);
        op = g_strdup(comp);
    }

    gchar *name = geglop2component(comp);
    const gchar *description = gegl_operation_get_key(op, "description");
    const gchar *categories = gegl_operation_get_key(op, "categories");

    JsonObject *component = json_object_new();
    json_object_set_string_member(component, "name", name);
    json_object_set_string_member(component, "description", description);
    json_object_set_string_member(component, "icon", icon_for_op(op, categories));

    JsonArray *inports = library_inports_for_operation(op);
    json_object_set_array_member(component, "inPorts", inports);

    JsonArray *outports = library_outports_for_operation(op);
    json_object_set_array_member(component, "outPorts", outports);

    g_free(op);
    return component;
}

gchar **
library_list_components(Library *self, gint *len) {
    // Our special "operations"
    const gchar *special_ops[] = {
        "Processor"
    };
    const gint no_special_ops = G_N_ELEMENTS(special_ops);

    // GEGL operations
    guint no_ops = 0;
    gchar **operation_names = gegl_list_operations(&no_ops);
    if (no_ops == 0) {
        imgflo_warning("No GEGL operations found");
    }

    // FIXME: list setsource ops
    const gint no_setsource_ops = g_hash_table_size(self->setsource_components);


    const gint total_ops = no_special_ops+no_ops+no_setsource_ops;

    // Concatenate all
    gchar **ret = (gchar **)g_new0(gchar*, total_ops+1); // leave NULL at end
    for (int i=0; i<no_special_ops; i++) {
        ret[i] = g_strdup(special_ops[i]);
    }
    for (int i=0; i<no_ops; i++) {
        const gchar *op = operation_names[i];
        if (g_strcmp0(op, "gegl:seamless-clone-compose") == 0) {
            // FIXME: reported by GEGL but cannot be instantiated...
            op = NULL;
        } else if (is_setsource_comp(op)) {
            op = NULL;
        }

        ret[no_special_ops+i] = g_strdup(op);
    }

    if (len) {
        *len = total_ops;
    }
    return ret;
}
