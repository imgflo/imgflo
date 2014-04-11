
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <libsoup/soup.h>


JsonArray *
inports_for_operation(const gchar *name)
{
    JsonArray *inports = json_array_new();

    // Hardcode 'input' buffer as the only inport
    // FIXME: programatically determine if 'aux' or other ports exists, and add them
    JsonObject *in = json_object_new();
    json_object_set_string_member(in, "id", "input");
    json_object_set_string_member(in, "type", "buffer");
    json_array_add_object_element(inports, in);

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


static void
send_response(SoupWebsocketConnection *ws,
            const gchar *protocol, const gchar *command, JsonObject *payload)
{
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
    //g_print ("SEND: %.*s\n", (int)len, data);
    soup_websocket_connection_send(ws, SOUP_WEBSOCKET_DATA_TEXT, resp);

    g_object_unref(generator);
}

static gboolean
handle_message(const gchar *protocol, const gchar *command,
                JsonObject *payload, SoupWebsocketConnection *ws)
{
    if (g_strcmp0(protocol, "component") == 0 && g_strcmp0(command, "list") == 0) {

        guint no_ops = 0;
        gchar **operation_names = gegl_list_operations(&no_ops);

        for (int i=0; i<no_ops; i++) {
            // FIXME: normalize to NoFlo convention
            const gchar *op = operation_names[i];
            gchar *name = geglop2component(op);

            JsonObject *component = json_object_new();
            json_object_set_string_member(component, "name", name);
            json_object_set_string_member(component, "description", "");

            JsonArray *inports = inports_for_operation(op);
            json_object_set_array_member(component, "inPorts", inports);

            // Hardcode 'output' buffer as the only outport. Should be the case for all current GEGL ops
            JsonObject *out = json_object_new();
            json_object_set_string_member(out, "id", "output");
            json_object_set_string_member(out, "type", "buffer");

            JsonArray *outports = json_array_new();
            json_array_add_object_element(outports, out);
            json_object_set_array_member(component, "outPorts", outports);

            send_response(ws, "component", "component", component);
        }
        return TRUE;
    } else {
        return FALSE;
    }
}

static void
on_web_socket_open(SoupWebsocketConnection *ws, gpointer user_data)
{
	gchar *url = soup_uri_to_string(soup_websocket_connection_get_uri (ws), FALSE);
	g_print("WebSocket: opened %s with %s\n", soup_websocket_connection_get_protocol(ws), url);
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
	//g_print ("RECV: %.*s\n", (int)len, data);

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

        handle_message(protocol, command, payload, ws);

    } else {
        g_error("Unable to parse WebSocket message as JSON");
    }

    g_object_unref(parser);
}

static void
on_web_socket_error(SoupWebsocketConnection *ws, GError *error, gpointer user_data)
{
	g_printerr("WebSocket: error: %s\n", error->message);
}

static void
on_web_socket_close(SoupWebsocketConnection *ws, gpointer user_data)
{
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
	g_signal_connect (connection, "open", G_CALLBACK (on_web_socket_open), user_data);
	g_signal_connect (connection, "message", G_CALLBACK (on_web_socket_message), user_data);
	g_signal_connect (connection, "error", G_CALLBACK (on_web_socket_error), user_data);
	g_signal_connect (connection, "close", G_CALLBACK (on_web_socket_close), user_data);
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
	soup_message_headers_iter_init(&iter, msg->request_headers);
	while (soup_message_headers_iter_next(&iter, &name, &value))
		g_print("%s: %s\n", name, value);
	if (msg->request_body->length)
		g_print("%s\n", msg->request_body->data);

	char *file_path = g_strdup_printf(".%s", path);

	if (msg->method == SOUP_METHOD_GET || msg->method == SOUP_METHOD_HEAD) {
		g_print("GET REQUEST!!");
    } else {
		soup_message_set_status(msg, SOUP_STATUS_NOT_IMPLEMENTED);
    }
	g_free(file_path);
	g_print("  -> %d %s\n\n", msg->status_code, msg->reason_phrase);
}


typedef struct {
	SoupServer *server;
} UiConnection;

UiConnection *
ui_connection_new(int port) {
    UiConnection *self = g_new(UiConnection, 1);

	self->server = soup_server_new(SOUP_SERVER_PORT, port,
        SOUP_SERVER_SERVER_HEADER, "noflo-gegl-runtime", NULL);
    if (!self->server) {
        g_free(self);
        return NULL;
    }

    soup_server_add_websocket_handler(self->server, NULL, NULL, NULL,
        websocket_callback, (void *)0x1000, NULL);
	soup_server_add_handler(self->server, NULL,
        server_callback, NULL, NULL);

	soup_server_run_async (self->server);

    return self;
}

void
ui_connection_free(UiConnection *self) {

    g_object_unref(self->server);

    g_free(self);
}
