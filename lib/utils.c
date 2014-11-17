//     imgflo - Flowhub.io Image-processing runtime
//     (c) 2014 The Grid
//     imgflo may be freely distributed under the MIT license

#include <json-glib/json-glib.h>

// Debug handlers. These follow the same semantics as glib handlers
#define imgflo_error(format...)    G_STMT_START {                 \
                                imgflo_log (G_LOG_DOMAIN,         \
                                       G_LOG_LEVEL_ERROR,    \
                                       format);              \
                                for (;;) ;                   \
                              } G_STMT_END
#define imgflo_message(format...)    imgflo_log (G_LOG_DOMAIN,         \
                                       G_LOG_LEVEL_MESSAGE,  \
                                       format)
#define imgflo_critical(format...)   imgflo_log (G_LOG_DOMAIN,         \
                                       G_LOG_LEVEL_CRITICAL, \
                                       format)
#define imgflo_warning(format...)    imgflo_log (G_LOG_DOMAIN,         \
                                       G_LOG_LEVEL_WARNING,  \
                                       format)
#define imgflo_info(format...)       imgflo_log (G_LOG_DOMAIN,         \
                                       G_LOG_LEVEL_INFO,     \
                                       format)
#define imgflo_debug(format...)      imgflo_log (G_LOG_DOMAIN,         \
                                       G_LOG_LEVEL_DEBUG,    \
                                       format)

// Note: must not be defined in any externally visible headers!
#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "imgflo"


void imgflo_log_handler_print (const gchar *log_domain, GLogLevelFlags log_level,
                                    const gchar *message, gpointer user_data) {

    // Just forward to glib
    g_log(log_domain, log_level, "%s", message);
}

// Note: we only support one log handler,
// and we do not care about domain nor level
static GLogFunc global_imgflo_log_handler =  NULL;
static gpointer global_imgflo_log_handler_data = NULL;

guint
imgflo_log_set_handler (const gchar *log_domain,
                   GLogLevelFlags log_levels,
                   GLogFunc log_func,
                   gpointer user_data)
{
    global_imgflo_log_handler = log_func;
    global_imgflo_log_handler_data  = user_data;
    return 0;
}

void
imgflo_logv (const gchar   *log_domain,
	GLogLevelFlags log_level,
	const gchar   *format,
	va_list	       args)
{
    gchar *message = g_strdup_vprintf(format, args);

    // TODO: allow to disable?
    imgflo_log_handler_print(log_domain, log_level, message, NULL);

    if (global_imgflo_log_handler) {
        global_imgflo_log_handler(log_domain, log_level, message,
                                  global_imgflo_log_handler_data);
    }
    g_free(message);
}

void
imgflo_log (const gchar   *log_domain,
       GLogLevelFlags log_level,
       const gchar   *format,
       ...)
{
  va_list args;

  va_start (args, format);
  imgflo_logv (log_domain, log_level, format, args);
  va_end (args);
}

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

