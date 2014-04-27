//     imgflo - Flowhub.io Image-processing runtime
//     (c) 2014 The Grid
//     imgflo may be freely distributed under the MIT license

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <libsoup/soup.h>
#include <gegl-plugin.h>

// FIXME: need proper GEGL API, https://bugzilla.gnome.org/show_bug.cgi?id=728085
GType gegl_operation_gtype_from_name(const gchar *name);

typedef struct {
	SoupServer *server;
    Graph *graph;
    Network *network;
    gchar *hostname;
    SoupWebsocketConnection *connection; // TODO: allow multiple clients
} UiConnection;

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

JsonArray *
inports_for_operation(const gchar *name)
{
    JsonArray *inports = json_array_new();

    // FIXME: find a better way than these heuristics for determining available pads
    GType type = gegl_operation_gtype_from_name(name);
    GType composer = g_type_from_name("GeglOperationComposer");
    GType composer3 = g_type_from_name("GeglOperationComposer3");
    GType source = g_type_from_name("GeglOperationSource");

    if (!g_type_is_a(type, source)) {
        JsonObject *input = json_object_new();
        json_object_set_string_member(input, "id", "input");
        json_object_set_string_member(input, "type", "buffer");
        json_array_add_object_element(inports, input);
    }
    if (g_type_is_a(type, composer) || g_type_is_a(type, composer3)) {
        JsonObject *aux = json_object_new();
        json_object_set_string_member(aux, "id", "aux");
        json_object_set_string_member(aux, "type", "buffer");
        json_array_add_object_element(inports, aux);
    }
    if (g_type_is_a(type, composer3)) {
        JsonObject *aux2 = json_object_new();
        json_object_set_string_member(aux2, "id", "aux2");
        json_object_set_string_member(aux2, "type", "buffer");
        json_array_add_object_element(inports, aux2);
    }

    guint n_properties = 0;
    GParamSpec** properties = gegl_operation_list_properties(name, &n_properties);
    for (int i=0; i<n_properties; i++) {
        GParamSpec *prop = properties[i];
        const gchar *id = g_param_spec_get_name(prop);
        const gchar *type = G_PARAM_SPEC_TYPE_NAME(prop);
        // TODO: map the GParam types to something more sane

        JsonObject *port = json_object_new();
        json_object_set_string_member(port, "id", id);
        json_object_set_string_member(port, "type", type);

        json_array_add_object_element(inports, port);
    }

    return inports;
}

static JsonObject *
get_processor_component(void)
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

static void
send_response(SoupWebsocketConnection *ws,
            const gchar *protocol, const gchar *command, JsonObject *payload)
{
    g_return_if_fail(ws);

    JsonObject *response = json_object_new();
    g_assert(response);

    json_object_set_string_member(response, "protocol", protocol);
    json_object_set_string_member(response, "command", command);
    json_object_set_object_member(response, "payload", payload);

    JsonGenerator *generator = json_generator_new();
    JsonNode *node = json_node_new(JSON_NODE_OBJECT);
    json_node_take_object(node, response);
    json_generator_set_root(generator, node);

    gsize len = 0;
    gchar *data = json_generator_to_data(generator, &len);
    GBytes *resp = g_bytes_new_take(data, len);
    g_print ("SEND: %.*s\n", (int)len, data);
    soup_websocket_connection_send(ws, SOUP_WEBSOCKET_DATA_TEXT, resp);

    g_object_unref(generator);
}

static void
ui_connection_handle_message(UiConnection *self,
                const gchar *protocol, const gchar *command, JsonObject *payload,
                SoupWebsocketConnection *ws)
{
    // TODO: move graph command handling code out to functions, taking graph instance, payload, ws

    if (g_strcmp0(protocol, "component") == 0 && g_strcmp0(command, "list") == 0) {

        // Our special Processor component
        JsonObject *processor = get_processor_component();
        send_response(ws, "component", "component", processor);

        // Components for all available GEGL operations
        guint no_ops = 0;
        gchar **operation_names = gegl_list_operations(&no_ops);
        for (int i=0; i<no_ops; i++) {
            const gchar *op = operation_names[i];
            gchar *name = geglop2component(op);
            const gchar *description = gegl_operation_get_key(op, "description");
            const gchar *categories = gegl_operation_get_key(op, "categories");

            JsonObject *component = json_object_new();
            json_object_set_string_member(component, "name", name);
            json_object_set_string_member(component, "description", description);
            json_object_set_string_member(component, "icon", icon_for_op(op, categories));

            JsonArray *inports = inports_for_operation(op);
            json_object_set_array_member(component, "inPorts", inports);

            // FIXME: find better way to get output pads
            GType type = gegl_operation_gtype_from_name(op);
            GType sink = g_type_from_name("GeglOperationSink");
            JsonArray *outports = json_array_new();

            JsonObject *out = json_object_new();
            json_object_set_string_member(out, "id", g_type_is_a(type, sink) ? "process" : "output");
            json_object_set_string_member(out, "type", g_type_is_a(type, sink) ? "N/A" : "buffer");
            json_array_add_object_element(outports, out);

            json_object_set_array_member(component, "outPorts", outports);

            send_response(ws, "component", "component", component);
        }

    } else if (g_strcmp0(protocol, "runtime") == 0 && g_strcmp0(command, "getruntime") == 0) {

        JsonObject *runtime = json_object_new();
        json_object_set_string_member(runtime, "version", "0.4"); // protocol version
        json_object_set_string_member(runtime, "type", "imgflo");

        JsonArray *capabilities = json_array_new();
        json_array_add_string_element(capabilities, "protocol:component");
        json_object_set_array_member(runtime, "capabilities", capabilities);

        send_response(ws, "runtime", "runtime", runtime);

    } else if (g_strcmp0(protocol, "graph") == 0 && g_strcmp0(command, "clear") == 0) {
        network_set_graph(self->network, NULL);
        if (self->graph) {
            graph_free(self->graph);
            self->graph = NULL;
        }
        // TODO: respect id/label
        self->graph = graph_new();
        network_set_graph(self->network, self->graph);
    } else if (g_strcmp0(protocol, "graph") == 0 && g_strcmp0(command, "addnode") == 0) {
        g_return_if_fail(self->graph);

        graph_add_node(self->graph,
            json_object_get_string_member(payload, "id"),
            json_object_get_string_member(payload, "component")
        );
    } else if (g_strcmp0(protocol, "graph") == 0 && g_strcmp0(command, "removenode") == 0) {
        g_return_if_fail(self->graph);

        graph_remove_node(self->graph,
            json_object_get_string_member(payload, "id")
        );
    } else if (g_strcmp0(protocol, "graph") == 0 && g_strcmp0(command, "addinitial") == 0) {
        g_return_if_fail(self->graph);

        JsonObject *tgt = json_object_get_object_member(payload, "tgt");
        JsonObject *src = json_object_get_object_member(payload, "src");
        GValue data = G_VALUE_INIT;
        json_node_get_value(json_object_get_member(src, "data"), &data);
        graph_add_iip(self->graph,
            json_object_get_string_member(tgt, "node"),
            json_object_get_string_member(tgt, "port"),
            &data
        );
        g_value_unset(&data);
    } else if (g_strcmp0(protocol, "graph") == 0 && g_strcmp0(command, "removeinitial") == 0) {
        g_return_if_fail(self->graph);

        JsonObject *tgt = json_object_get_object_member(payload, "tgt");
        graph_remove_iip(self->graph,
            json_object_get_string_member(tgt, "node"),
            json_object_get_string_member(tgt, "port")
        );
    } else if (g_strcmp0(protocol, "graph") == 0 && g_strcmp0(command, "addedge") == 0) {
        g_return_if_fail(self->graph);

        JsonObject *src = json_object_get_object_member(payload, "src");
        JsonObject *tgt = json_object_get_object_member(payload, "tgt");
        graph_add_edge(self->graph,
            json_object_get_string_member(src, "node"),
            json_object_get_string_member(src, "port"),
            json_object_get_string_member(tgt, "node"),
            json_object_get_string_member(tgt, "port")
        );
    } else if (g_strcmp0(protocol, "graph") == 0 && g_strcmp0(command, "removeedge") == 0) {
        g_return_if_fail(self->graph);

        JsonObject *src = json_object_get_object_member(payload, "src");
        JsonObject *tgt = json_object_get_object_member(payload, "tgt");
        graph_remove_edge(self->graph,
            json_object_get_string_member(src, "node"),
            json_object_get_string_member(src, "port"),
            json_object_get_string_member(tgt, "node"),
            json_object_get_string_member(tgt, "port")
        );
    } else if (g_strcmp0(protocol, "network") == 0 && g_strcmp0(command, "start") == 0) {
        g_return_if_fail(self->graph);

        // FIXME: should be done in callback monitoring network state changes
        // TODO: send timestamp, graph id
        JsonObject *info = json_object_new();
        send_response(ws, "network", "started", info);

        g_print("\tNetwork START\n");
        network_set_running(self->network, TRUE);

    } else if (g_strcmp0(protocol, "network") == 0 && g_strcmp0(command, "stop") == 0) {
        g_return_if_fail(self->graph);

        // FIXME: should be done in callback monitoring network state changes
        // TODO: send timestamp, graph id
        JsonObject *info = json_object_new();
        send_response(ws, "network", "stopped", info);

        g_print("\tNetwork STOP\n");
        network_set_running(self->network, FALSE);

    } else {
        g_printerr("Unhandled message: protocol='%s', command='%s'", protocol, command);
    }
}

void
send_preview_invalidated(Network *network, Processor *processor, GeglRectangle rect, gpointer user_data) {
    UiConnection *ui = (UiConnection *)user_data;

    const gchar *node = graph_find_processor_name(network->graph, processor);

    gchar url[1024];
    g_snprintf(url, 1024, "http://%s:%d/process?node=%s",
               ui->hostname, soup_server_get_port(ui->server), node);

    JsonObject *payload = json_object_new();
    json_object_set_string_member(payload, "type", "previewurl");
    json_object_set_string_member(payload, "url", url);
    send_response(ui->connection, "network", "output", payload);
}

static void
on_web_socket_open(SoupWebsocketConnection *ws, gpointer user_data)
{
	gchar *url = soup_uri_to_string(soup_websocket_connection_get_uri (ws), FALSE);
	g_print("WebSocket: client opened %s with %s\n", soup_websocket_connection_get_protocol(ws), url);

    UiConnection *self = (UiConnection *)user_data;
    g_assert(self);
    self->connection = ws;
    self->network->on_processor_invalidated_data = (gpointer)self;
    self->network->on_processor_invalidated = send_preview_invalidated;

	g_free(url);
}

static void
on_web_socket_message(SoupWebsocketConnection *ws,
                      SoupWebsocketDataType type,
                      GBytes *message,
                      void *user_data)
{
	const gchar *data;
	gsize len;

    //g_print ("%s: %p", __PRETTY_FUNCTION__, user_data);

	data = g_bytes_get_data (message, &len);
	g_print ("RECV: %.*s\n", (int)len, data);

    JsonParser *parser = json_parser_new();
    gboolean success = json_parser_load_from_data(parser, data, len, NULL);
    if (success) {
        JsonNode *r = json_parser_get_root(parser);
        g_assert(JSON_NODE_HOLDS_OBJECT(r));
        JsonObject *root = json_node_get_object(r);

        const gchar *protocol = json_object_get_string_member(root, "protocol");
        const gchar *command = json_object_get_string_member(root, "command");

        JsonNode *pnode = json_object_get_member(root, "payload");
        JsonObject *payload = JSON_NODE_HOLDS_OBJECT(pnode) ? json_object_get_object_member(root, "payload") : NULL;

        UiConnection *ui = (UiConnection *)user_data;
        ui_connection_handle_message(ui, protocol, command, payload, ws);

    } else {
        g_error("Unable to parse WebSocket message as JSON");
    }

    g_object_unref(parser);
}

static void
on_web_socket_error(SoupWebsocketConnection *ws, GError *error, gpointer user_data)
{
    UiConnection *ui = (UiConnection *)user_data;
    ui->connection = NULL;
    g_printerr("WebSocket: error: %s\n", error->message);
}

static void
on_web_socket_close(SoupWebsocketConnection *ws, gpointer user_data)
{
    UiConnection *ui = (UiConnection *)user_data;
    ui->connection = NULL;

	gushort code = soup_websocket_connection_get_close_code(ws);
	if (code != 0) {
		g_printerr("WebSocket: close: %d %s\n", code,
			    soup_websocket_connection_get_close_data(ws));
	} else {
		g_printerr("WebSocket: close\n");
    }
}

void websocket_callback(SoupServer *server,
					    const char *path,
					    SoupWebsocketConnection *connection,
					    SoupClientContext *client,
					    gpointer user_data)
{
	g_signal_connect(connection, "open", G_CALLBACK(on_web_socket_open), user_data);
	g_signal_connect(connection, "message", G_CALLBACK(on_web_socket_message), user_data);
	g_signal_connect(connection, "error", G_CALLBACK(on_web_socket_error), user_data);
	g_signal_connect(connection, "close", G_CALLBACK(on_web_socket_close), user_data);
}

static void
process_image_callback (SoupServer *server, SoupMessage *msg,
		 const char *path, GHashTable *query,
		 SoupClientContext *context, gpointer user_data) {

    UiConnection *self = (UiConnection *)user_data;
    PngEncoder *encoder = png_encoder_new();

    const gchar *node_name = g_hash_table_lookup(query, "node");

    // TODO: move code into backend somewhere
    Processor *processor = NULL;
    if (self->graph && node_name) {
        processor = g_hash_table_lookup(self->graph->processor_map, node_name);
    }

    if (processor) {
        const Babl *format = babl_format("R'G'B'A u8");
        // FIXME: take region-of-interest as parameter
        GeglRectangle roi = gegl_node_get_bounding_box(processor->node);
        // FIXME: take size as parameter, set scale to give approx that
        const double scale = 1.0;
        gchar *buffer = g_malloc(roi.width*roi.height*babl_format_get_bytes_per_pixel(format));
        // XXX: maybe use GEGL_BLIT_DIRTY?
        gegl_node_blit(processor->node, scale, &roi, format, buffer,
                       GEGL_AUTO_ROWSTRIDE, GEGL_BLIT_DEFAULT);
        png_encoder_encode_rgba(encoder, roi.width, roi.height, buffer);

        g_free(buffer);
    } else {
        g_warning("Rendering fallback image");
        png_encoder_encode_rgba(encoder, 100, 100, NULL);
    }

    const gchar *mime_type = "image/png";
    char *buf = encoder->buffer;
    const size_t len = encoder->size;

    soup_message_set_status(msg, SOUP_STATUS_OK);
    soup_message_set_response(msg, mime_type, SOUP_MEMORY_COPY, buf, len);

    //png_encoder_free(encoder);
}

static void
server_callback (SoupServer *server, SoupMessage *msg,
		 const char *path, GHashTable *query,
		 SoupClientContext *context, gpointer data)
{

    SoupMessageHeadersIter iter;
    const char *name, *value;

    g_print("%s %s HTTP/1.%d\n", msg->method, path,
         soup_message_get_http_version(msg));

    if (g_strcmp0(path, "/process") == 0 && msg->method == SOUP_METHOD_GET) {
        process_image_callback(server, msg, path, query, context, data);
    } else {
        soup_message_headers_iter_init(&iter, msg->request_headers);
        while (soup_message_headers_iter_next(&iter, &name, &value))
            g_print("%s: %s\n", name, value);
        if (msg->request_body->length)
            g_print("%s\n", msg->request_body->data);

        g_print("  -> %d %s\n\n", msg->status_code, msg->reason_phrase);

        soup_message_set_status(msg, SOUP_STATUS_NOT_IMPLEMENTED);
    }
}


UiConnection *
ui_connection_new(const gchar *hostname, int port) {
    UiConnection *self = g_new(UiConnection, 1);

    self->graph = NULL;
    self->network = network_new();
    self->hostname = g_strdup(hostname);

	self->server = soup_server_new(SOUP_SERVER_PORT, port,
        SOUP_SERVER_SERVER_HEADER, "imgflo-runtime", NULL);
    if (!self->server) {
        g_free(self);
        return NULL;
    }

    soup_server_add_websocket_handler(self->server, NULL, NULL, NULL,
        websocket_callback, self, NULL);
	soup_server_add_handler(self->server, NULL,
        server_callback, self, NULL);

	soup_server_run_async (self->server);

    return self;
}

void
ui_connection_free(UiConnection *self) {

    network_free(self->network);
    g_free(self->hostname);

    if (self->graph) {
        graph_free(self->graph);
        self->graph = NULL;
    }
    g_object_unref(self->server);

    g_free(self);
}
