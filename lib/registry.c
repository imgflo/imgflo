//     imgflo - Flowhub.io Image-processing runtime
//     (c) 2014 The Grid
//     imgflo may be freely distributed under the MIT license

#include <libsoup/soup.h>
#include <glib.h>

typedef struct _RuntimeInfo {
    gchar *id;
    gchar *user_id;
    gchar *label;
    gchar *hostname;
    gint port;
} RuntimeInfo;

RuntimeInfo *
runtime_info_new_from_env(const gchar *host, gint port) {
    RuntimeInfo *self = g_new(RuntimeInfo, 1);

    const gchar *id = g_getenv("IMGFLO_RUNTIME_ID");
    const gchar *label = g_getenv("IMGFLO_RUNTIME_LABEL");
    const gchar *user_id = g_getenv("FLOWHUB_USER_ID");

    if (!label) {
        label = "imgflo";
    }
    self->label = g_strdup(label);
    self->id = g_strdup(id);
    self->user_id = g_strdup(user_id);
    self->hostname = g_strdup(host);
    self->port = port;

    return self;
}
void
runtime_info_free(RuntimeInfo *self) {
    if (!self) {
        return;
    }

    g_free(self->id);
    g_free(self->label);
    g_free(self->user_id);
    g_free(self->hostname);
}


// TODO: error handling
typedef struct _Registry {
    RuntimeInfo *info;
    SoupSession *session;
} Registry;

Registry *
registry_new(RuntimeInfo *info) {
    Registry *self = g_new(Registry, 1);
    self->info = info;
    self->session = soup_session_new_with_options(SOUP_SESSION_MAX_CONNS, 10, NULL);
    return self;
}

void
registry_free(Registry *self) {
    if (self->info) {
        runtime_info_free(self->info);
    }
    g_free(self->info);
    g_object_unref(self->session);
}

gchar *
register_msg_body(RuntimeInfo *rt, gsize *body_length_out) {
    gchar address[100];
    g_snprintf(address, sizeof(address), "ws://%s:%d", rt->hostname, rt->port);

    JsonNode *node = json_node_new(JSON_NODE_OBJECT);
    JsonObject *root = json_object_new();
    json_node_init_object(node, root);
    json_object_set_string_member(root, "type", "imgflo");
    json_object_set_string_member(root, "protocol", "websocket");
    json_object_set_string_member(root, "user", rt->user_id);
    json_object_set_string_member(root, "id", rt->id);
    json_object_set_string_member(root, "label", rt->label);
    json_object_set_string_member(root, "address", address);

    JsonGenerator *gen = json_generator_new();
    json_generator_set_root(gen, node);
    gchar *body = json_generator_to_data(gen, body_length_out);
    g_object_unref(gen);
    return body;
}

gboolean
registry_register(Registry *self) {
    g_return_val_if_fail(self->info, FALSE);
    g_return_val_if_fail(self->info->user_id, FALSE);

    const gchar *endpoint = "https://api.flowhub.io";
    RuntimeInfo *rt = self->info;

    if (!rt->id) {
        // Create new
        g_print("No runtime id found, generating new\n");
        rt->id = imgflo_uuid_new_string();
    }

    gchar url[200];
    g_snprintf(url, sizeof(url), "%s/runtimes/%s", endpoint, rt->id);
    SoupMessage *msg = soup_message_new("PUT", url);

    gsize body_length = -1;
    gchar *body = register_msg_body(rt, &body_length);
    soup_message_set_request(msg, "application/json", SOUP_MEMORY_TAKE, body, body_length);

    // XXX: syncronous HTTP call
    const guint status = soup_session_send_message(self->session, msg);
    gboolean success = FALSE;
    if (status == 201 || status == 200) {
        g_print("Registered runtime.\n"
                 "IMGFLO_RUNTIME_ID=%s\n", rt->id);
        success = TRUE;
    } else {
        g_warning("Failed to register Flowhub runtime: status=%d, %s\n", status, msg->response_body->data);
    }
    g_object_unref(msg);
    return success;
}
