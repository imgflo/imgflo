
#ifdef HAVE_UUID

#include <uuid/uuid.h>
// xxxxxxxx-xxxx-Mxxx-Nxxx-xxxxxxxxxxxx
gchar *
imgflo_uuid_new_string() {
    gchar *ret = g_new(gchar, 40);
    uuid_t u;
    uuid_generate(u);

    g_snprintf(ret, 40,
           "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
           u[0],u[1],u[2],u[3],
           u[4],u[5],
           u[6],u[7],
           u[8],u[9],
           u[10],u[11],u[12],u[13],u[14],u[15]);
    return ret;
}

#else
#error "No UUID library available"
#endif //HAVE_UUID
