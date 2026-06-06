#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dbus/dbus.h>

static void usage(FILE *out) {
        fprintf(out,
                "usage: doi [-t SECONDS] [-i ICON] [-b BODY] SUMMARY\n"
                "  -t SECONDS   timeout (0 = click to dismiss)\n"
                "  -i ICON      icon (emoji or text glyph)\n"
                "  -b BODY      body text\n"
        );
}

int main(int argc, char **argv) {
        int i;
        const char *summary    = NULL;
        const char *body       = "";
        const char *icon       = "";
        const char *app_name   = "doi";
        int         timeout_ms = 0;

        DBusConnection *conn;
        DBusError       err;
        DBusMessage    *msg, *reply;
        DBusMessageIter args;
        dbus_uint32_t   replace_id = 0;
        dbus_uint32_t   notif_id;
        dbus_int32_t    timeout_dbus;

        for (i = 1; i < argc; i++) {
                if ((strcmp(argv[i], "-t") == 0) && i + 1 < argc)
                        timeout_ms = atoi(argv[++i]) * 1000;
                else if ((strcmp(argv[i], "-i") == 0) && i + 1 < argc)
                        icon = argv[++i];
                else if ((strcmp(argv[i], "-b") == 0) && i + 1 < argc)
                        body = argv[++i];
                else if (argv[i][0] != '-' && !summary)
                        summary = argv[i];
                else { usage(stderr); return EXIT_FAILURE; }
        }
        if (!summary) { usage(stderr); return EXIT_FAILURE; }

        timeout_dbus = (dbus_int32_t)timeout_ms;

        dbus_error_init(&err);
        conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
        if (dbus_error_is_set(&err)) {
                fprintf(stderr, "dbus: %s\n", err.message);
                dbus_error_free(&err);
                return EXIT_FAILURE;
        }

        msg = dbus_message_new_method_call(
                "org.freedesktop.Notifications",
                "/org/freedesktop/Notifications",
                "org.freedesktop.Notifications",
                "Notify");
        if (!msg) return EXIT_FAILURE;

        dbus_message_iter_init_append(msg, &args);
        dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &app_name);
        dbus_message_iter_append_basic(&args, DBUS_TYPE_UINT32, &replace_id);
        dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &icon);
        dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &summary);
        dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &body);

        { DBusMessageIter a;
          dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY,
                DBUS_TYPE_STRING_AS_STRING, &a);
          dbus_message_iter_close_container(&args, &a); }

        { DBusMessageIter d;
          dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "{sv}", &d);
          dbus_message_iter_close_container(&args, &d); }

        dbus_message_iter_append_basic(&args, DBUS_TYPE_INT32, &timeout_dbus);

        reply = dbus_connection_send_with_reply_and_block(conn, msg, 1000, &err);
        if (dbus_error_is_set(&err)) {
                fprintf(stderr, "send failed: %s\n", err.message);
                dbus_error_free(&err);
                dbus_message_unref(msg);
                return EXIT_FAILURE;
        }
        dbus_message_get_args(reply, &err,
                DBUS_TYPE_UINT32, &notif_id, DBUS_TYPE_INVALID);
        dbus_message_unref(reply);
        dbus_message_unref(msg);
        return EXIT_SUCCESS;
}
