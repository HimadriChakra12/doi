#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dbus/dbus.h>
#include "module.h"

#define MPRIS_IFACE  "org.mpris.MediaPlayer2.Player"
#define MPRIS_PATH   "/org/mpris/MediaPlayer2"
#define MPRIS_PREFIX "org.mpris.MediaPlayer2."
#define DBUS_PROPS   "org.freedesktop.DBus.Properties"

static char *find_player(DBusConnection *conn) {
        DBusMessage *msg, *reply;
        DBusError err;
        DBusMessageIter iter, arr;
        char *found = NULL;

        dbus_error_init(&err);
        msg = dbus_message_new_method_call("org.freedesktop.DBus",
                "/org/freedesktop/DBus", "org.freedesktop.DBus", "ListNames");
        reply = dbus_connection_send_with_reply_and_block(conn, msg, 1000, &err);
        dbus_message_unref(msg);
        if (!reply) return NULL;

        dbus_message_iter_init(reply, &iter);
        dbus_message_iter_recurse(&iter, &arr);
        while (dbus_message_iter_get_arg_type(&arr) == DBUS_TYPE_STRING) {
                const char *name;
                dbus_message_iter_get_basic(&arr, &name);
                if (!found && strncmp(name, MPRIS_PREFIX, strlen(MPRIS_PREFIX)) == 0)
                        found = strdup(name);
                dbus_message_iter_next(&arr);
        }
        dbus_message_unref(reply);
        return found;
}

static void player_cmd(DBusConnection *conn, const char *player, const char *method) {
        DBusMessage *msg;
        DBusError err;
        dbus_error_init(&err);
        msg = dbus_message_new_method_call(player, MPRIS_PATH, MPRIS_IFACE, method);
        if (!msg) return;
        dbus_connection_send_with_reply_and_block(conn, msg, 1000, &err);
        dbus_error_free(&err);
        dbus_message_unref(msg);
}

static char *metadata_str(DBusConnection *conn, const char *player, const char *key) {
        DBusMessage *msg, *reply;
        DBusError err;
        DBusMessageIter iter, var, dict, entry, val;
        char *result = NULL;
        const char *iface = MPRIS_IFACE;
        const char *prop  = "Metadata";

        dbus_error_init(&err);
        msg = dbus_message_new_method_call(player, MPRIS_PATH, DBUS_PROPS, "Get");
        dbus_message_append_args(msg,
                DBUS_TYPE_STRING, &iface, DBUS_TYPE_STRING, &prop, DBUS_TYPE_INVALID);
        reply = dbus_connection_send_with_reply_and_block(conn, msg, 1000, &err);
        dbus_message_unref(msg);
        if (!reply || dbus_error_is_set(&err)) { dbus_error_free(&err); return NULL; }

        dbus_message_iter_init(reply, &iter);
        if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_VARIANT) goto done;
        dbus_message_iter_recurse(&iter, &var);
        if (dbus_message_iter_get_arg_type(&var) != DBUS_TYPE_ARRAY) goto done;
        dbus_message_iter_recurse(&var, &dict);

        while (dbus_message_iter_get_arg_type(&dict) == DBUS_TYPE_DICT_ENTRY) {
                const char *k;
                dbus_message_iter_recurse(&dict, &entry);
                dbus_message_iter_get_basic(&entry, &k);
                if (strcmp(k, key) == 0) {
                        dbus_message_iter_next(&entry);
                        if (dbus_message_iter_get_arg_type(&entry) == DBUS_TYPE_VARIANT) {
                                dbus_message_iter_recurse(&entry, &val);
                                if (dbus_message_iter_get_arg_type(&val) == DBUS_TYPE_ARRAY) {
                                        DBusMessageIter sub;
                                        dbus_message_iter_recurse(&val, &sub);
                                        if (dbus_message_iter_get_arg_type(&sub) == DBUS_TYPE_STRING) {
                                                const char *s;
                                                dbus_message_iter_get_basic(&sub, &s);
                                                result = strdup(s);
                                        }
                                } else if (dbus_message_iter_get_arg_type(&val) == DBUS_TYPE_STRING) {
                                        const char *s;
                                        dbus_message_iter_get_basic(&val, &s);
                                        result = strdup(s);
                                }
                        }
                        break;
                }
                dbus_message_iter_next(&dict);
        }

done:
        dbus_message_unref(reply);
        return result;
}

int main(int argc, char **argv) {
        DBusConnection *conn;
        DBusError err;
        char *player;
        char summary[256], body[256];
        const char *icon = "🎵";
        char *title, *artist, *album;
        ModuleOpts o;

        if (argc < 2) {
                fprintf(stderr, "usage: doi-media [play|pause|toggle|next|prev|stop|info]\n");
                return EXIT_FAILURE;
        }

        dbus_error_init(&err);
        conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
        if (dbus_error_is_set(&err)) {
                fprintf(stderr, "dbus: %s\n", err.message);
                dbus_error_free(&err);
                return EXIT_FAILURE;
        }

        player = find_player(conn);
        if (!player) { fprintf(stderr, "no MPRIS player\n"); return EXIT_FAILURE; }

        if      (strcmp(argv[1], "play")   == 0) player_cmd(conn, player, "Play");
        else if (strcmp(argv[1], "pause")  == 0) player_cmd(conn, player, "Pause");
        else if (strcmp(argv[1], "toggle") == 0) player_cmd(conn, player, "PlayPause");
        else if (strcmp(argv[1], "next")   == 0) { player_cmd(conn, player, "Next");     icon = "⏭"; }
        else if (strcmp(argv[1], "prev")   == 0) { player_cmd(conn, player, "Previous"); icon = "⏮"; }
        else if (strcmp(argv[1], "stop")   == 0) { player_cmd(conn, player, "Stop");     icon = "⏹"; }
        else if (strcmp(argv[1], "info")   != 0) {
                fprintf(stderr, "unknown: %s\n", argv[1]);
                free(player);
                return EXIT_FAILURE;
        }

        title  = metadata_str(conn, player, "xesam:title");
        artist = metadata_str(conn, player, "xesam:artist");
        album  = metadata_str(conn, player, "xesam:album");
        free(player);

        snprintf(summary, sizeof(summary), "%s  %s", icon, title ? title : "Unknown");
        snprintf(body,    sizeof(body),    "%s%s%s",
                artist ? artist : "",
                (artist && album) ? " — " : "",
                album  ? album  : "");

        free(title); free(artist); free(album);

        memset(&o, 0, sizeof(o));
        o.summary      = summary;
        o.body         = body;
        o.bg           = MEDIA_BG;
        o.fg           = MEDIA_FG;
        o.border       = MEDIA_BORDER;
        o.border_color = MEDIA_BORDER_COLOR;
        o.timeout      = MEDIA_TIMEOUT * 1000;
        o.pos_x        = MEDIA_POS_X;
        o.pos_y        = MEDIA_POS_Y;
        o.min_width    = MEDIA_MIN_WIDTH;
        o.min_height   = MEDIA_MIN_HEIGHT;
        o.offset_x     = MEDIA_OFFSET_X;
        o.offset_y     = MEDIA_OFFSET_Y;
        return doi_notify(&o);
}
