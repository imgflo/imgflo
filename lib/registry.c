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
    gint port; // external
} RuntimeInfo;

// TODO: move back to HTTPs, when we've built libsoup with SSL support on Heroku
const gchar *api_endpoint = "http://api.flowhub.io";

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
    if (g_strcmp0(host, "") != 0) {
        self->hostname = g_strdup(host);
    } else {
        // TODO: autodetect external interface IP
        self->hostname = g_strdup("localhost");
    }

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

gchar *
runtime_info_liveurl(RuntimeInfo *self, gchar *ide) {
    gchar address[120];
    g_snprintf(address, sizeof(address), "ws://%s:%d", self->hostname, self->port);

    gchar *params = soup_form_encode(
        "protocol", "websocket",
        "address", address,
        "type", "imgflo",
        NULL
    );
    gchar *live_url = g_strdup_printf("%s#runtime/endpoint?%s", ide, params);
    g_free(params);
    return live_url;
}


// TODO: error handling
typedef struct _Registry {
    RuntimeInfo *info;
    SoupSession *session;
    guint ping_timeout;
} Registry;

Registry *
registry_new(RuntimeInfo *info) {
    Registry *self = g_new(Registry, 1);
    self->info = info;
    self->session = soup_session_new_with_options(SOUP_SESSION_MAX_CONNS, 10, NULL);
    self->ping_timeout = 0;
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

/* Pinging */
gboolean
registry_ping(Registry *self) {
    g_return_val_if_fail(self->info, FALSE);
    g_return_val_if_fail(self->info->user_id, FALSE);
    g_return_val_if_fail(self->info->id, FALSE);

    gchar url[200];
    g_snprintf(url, sizeof(url), "%s/runtimes/%s", api_endpoint, self->info->id);
    SoupMessage *msg = soup_message_new("POST", url);
    soup_message_set_request(msg, "application/json", SOUP_MEMORY_STATIC, "", 0);

    // XXX: syncronous HTTP call
    const guint status = soup_session_send_message(self->session, msg);
    gboolean success = FALSE;
    if (status == 201 || status == 200) {
        success = TRUE;
    } else {
        // Could be intermittent failure
    }
    g_object_unref(msg);
    return success;
}

static gboolean
ping_func(gpointer data) {
    Registry *r = (Registry *)data;
    registry_ping(r);
    return TRUE;
}

void
registry_start_pinging(Registry *self) {
    if (self->ping_timeout) {
        // already registered
        return;
    }
    const guint ping_interval_seconds = 10*60;
    registry_ping(self); // Fire initial ping right away
    self->ping_timeout = g_timeout_add_seconds(ping_interval_seconds,
                                               (GSourceFunc)ping_func, self);
}

void
registry_stop_pinging(Registry *self) {
    if (!self->ping_timeout) {
        // not registered
        return;
    }
    g_source_remove(self->ping_timeout);
}

/* Registration */
static gchar *
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
    json_object_set_string_member(root, "secret", "abracadacbra"); // XXX: let user specify
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

    RuntimeInfo *rt = self->info;

    if (!rt->id) {
        // Create new
        rt->id = imgflo_uuid_new_string();
    }

    gchar url[200];
    g_snprintf(url, sizeof(url), "%s/runtimes/%s", api_endpoint, rt->id);
    SoupMessage *msg = soup_message_new("PUT", url);

    gsize body_length = -1;
    gchar *body = register_msg_body(rt, &body_length);
    soup_message_set_request(msg, "application/json", SOUP_MEMORY_TAKE, body, body_length);

    // XXX: syncronous HTTP call
    const guint status = soup_session_send_message(self->session, msg);
    gboolean success = FALSE;
    if (status == 201 || status == 200) {
        imgflo_message("Registered runtime.\n"
                 "IMGFLO_RUNTIME_ID=%s\n", rt->id);
        success = TRUE;
    } else {
        imgflo_warning("Failed to register Flowhub runtime: status=%d, %s\n", status, msg->response_body->data);
    }
    g_object_unref(msg);
    return success;
}
