#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dbus/dbus.h>

static void print_help(FILE* out) {
        fprintf(out,
                "usage: doi [-h] [-t SECONDS] [-i ICON] [-b BODY] SUMMARY\n"
                "  -t SECONDS   timeout (0 = click to dismiss)\n"
                "  -i ICON      icon path or unicode glyph\n"
                "  -b BODY      body text shown below summary\n"
        );
}

int main(int argc, char** argv) {
        int i;
        char* timeout_str = "0";
        char* summary     = NULL;
        char* body        = "";
        char* icon        = "";
        int   timeout_ms;

        DBusConnection*  conn;
        DBusError        err;
        DBusMessage*     msg;
        DBusMessage*     reply;
        DBusMessageIter  args;
        char*            app_name   = "doi";
        char*            app_icon;
        dbus_uint32_t    replace_id = 0;
        dbus_uint32_t    notif_id;

        for (i = 1; i < argc; ++i) {
                if (strcmp(argv[i], "-h") == 0
                                || strcmp(argv[i], "--help") == 0) {
                        print_help(stdout);
                        return EXIT_SUCCESS;
                } else if (strcmp(argv[i], "-t") == 0) {
                        if (++i < argc) timeout_str = argv[i];
                        else { print_help(stderr); return EXIT_FAILURE; }
                } else if (strcmp(argv[i], "-i") == 0) {
                        if (++i < argc) icon = argv[i];
                        else { print_help(stderr); return EXIT_FAILURE; }
                } else if (strcmp(argv[i], "-b") == 0) {
                        if (++i < argc) body = argv[i];
                        else { print_help(stderr); return EXIT_FAILURE; }
                } else if (!summary) {
                        summary = argv[i];
                } else {
                        print_help(stderr);
                        return EXIT_FAILURE;
                }
        }

        if (!summary) { print_help(stderr); return EXIT_FAILURE; }

        timeout_ms = atoi(timeout_str) * 1000;
        app_icon   = icon;

        dbus_error_init(&err);
        conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
        if (dbus_error_is_set(&err)) {
                fprintf(stderr, "dbus error: %s\n", err.message);
                dbus_error_free(&err);
                return EXIT_FAILURE;
        }

        msg = dbus_message_new_method_call(
                "org.freedesktop.Notifications",
                "/org/freedesktop/Notifications",
                "org.freedesktop.Notifications",
                "Notify");
        if (!msg) {
                fprintf(stderr, "failed to create message.\n");
                return EXIT_FAILURE;
        }

        dbus_message_iter_init_append(msg, &args);
        dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &app_name);
        dbus_message_iter_append_basic(&args, DBUS_TYPE_UINT32, &replace_id);
        dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &app_icon);
        dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &summary);
        dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &body);

        { /* actions: empty array */
                DBusMessageIter arr;
                dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY,
                        DBUS_TYPE_STRING_AS_STRING, &arr);
                dbus_message_iter_close_container(&args, &arr);
        }
        { /* hints: empty dict */
                DBusMessageIter dict;
                dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY,
                        "{sv}", &dict);
                dbus_message_iter_close_container(&args, &dict);
        }

        dbus_message_iter_append_basic(&args, DBUS_TYPE_INT32, &timeout_ms);

        reply = dbus_connection_send_with_reply_and_block(conn, msg, 1000, &err);
        if (dbus_error_is_set(&err)) {
                fprintf(stderr, "failed: %s\n", err.message);
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
