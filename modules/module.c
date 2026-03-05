#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dbus/dbus.h>
#include "module.h"

static void append_hint_str(DBusMessageIter* dict,
                const char* key, const char* val) {
        DBusMessageIter entry, var;
        dbus_message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY,
                NULL, &entry);
        dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
        dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT,
                DBUS_TYPE_STRING_AS_STRING, &var);
        dbus_message_iter_append_basic(&var, DBUS_TYPE_STRING, &val);
        dbus_message_iter_close_container(&entry, &var);
        dbus_message_iter_close_container(dict, &entry);
}

static void append_hint_int(DBusMessageIter* dict,
                const char* key, int val) {
        DBusMessageIter entry, var;
        dbus_int32_t v = (dbus_int32_t)val;
        dbus_message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY,
                NULL, &entry);
        dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
        dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT,
                DBUS_TYPE_INT32_AS_STRING, &var);
        dbus_message_iter_append_basic(&var, DBUS_TYPE_INT32, &v);
        dbus_message_iter_close_container(&entry, &var);
        dbus_message_iter_close_container(dict, &entry);
}

/* persist last notification id to ~/.doi/notif_ids
 * format: one entry per line: "<key>\t<id>\n"
 * tab separator allows full unicode+spaces in key */

static dbus_uint32_t read_last_id(const char* key) {
        char path[512], line[256];
        const char* home = getenv("HOME");
        size_t klen;
        FILE* f;
        if (!home) return 0;
        klen = strlen(key);
        snprintf(path, sizeof(path), "%s/.doi/notif_ids", home);
        f = fopen(path, "r");
        if (!f) return 0;
        while (fgets(line, sizeof(line), f)) {
                if (strncmp(line, key, klen) == 0 && line[klen] == '\t') {
                        unsigned int v = (unsigned int)atoi(line + klen + 1);
                        fclose(f);
                        return (dbus_uint32_t)v;
                }
        }
        fclose(f);
        return 0;
}

static void write_last_id(const char* key, dbus_uint32_t id) {
        char path[512], tmp[512], line[256];
        const char* home = getenv("HOME");
        FILE* fin;
        FILE* fout;
        int found = 0;
        size_t klen;
        if (!home) return;
        klen = strlen(key);
        snprintf(path, sizeof(path), "%s/.doi/notif_ids", home);
        snprintf(tmp,  sizeof(tmp),  "%s/.doi/notif_ids.tmp", home);
        fin  = fopen(path, "r");
        fout = fopen(tmp,  "w");
        if (!fout) return;
        if (fin) {
                while (fgets(line, sizeof(line), fin)) {
                        if (strncmp(line, key, klen) == 0
                                        && line[klen] == '\t') {
                                fprintf(fout, "%s\t%u\n", key, id);
                                found = 1;
                        } else {
                                fputs(line, fout);
                        }
                }
                fclose(fin);
        }
        if (!found)
                fprintf(fout, "%s\t%u\n", key, id);
        fclose(fout);
        rename(tmp, path);
}

int doi_notify_opts(const BndNotifyOpts* opts) {
        DBusConnection*  conn;
        DBusError        err;
        DBusMessage*     msg;
        DBusMessage*     reply;
        DBusMessageIter  args, arr, dict;
        char*  app_name   = "doi";
        char*  app_icon   = (char*)(opts->icon    ? opts->icon    : "");
        char*  sum        = (char*)(opts->summary ? opts->summary : "");
        char*  bod        = (char*)(opts->body    ? opts->body    : "");
        const char*   id_key     = sum[0] ? sum : "default";
        dbus_uint32_t replace_id = read_last_id(id_key);
        dbus_uint32_t notif_id   = 0;
        dbus_int32_t  tms        = (dbus_int32_t)opts->timeout;

        dbus_error_init(&err);
        conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
        if (dbus_error_is_set(&err)) {
                fprintf(stderr, "doi_notify: dbus: %s\n", err.message);
                dbus_error_free(&err);
                return 1;
        }

        msg = dbus_message_new_method_call(
                "org.freedesktop.Notifications",
                "/org/freedesktop/Notifications",
                "org.freedesktop.Notifications",
                "Notify");
        if (!msg) return 1;

        dbus_message_iter_init_append(msg, &args);
        dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &app_name);
        dbus_message_iter_append_basic(&args, DBUS_TYPE_UINT32, &replace_id);
        dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &app_icon);
        dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &sum);
        dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &bod);

        /* actions: empty */
        dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY,
                DBUS_TYPE_STRING_AS_STRING, &arr);
        dbus_message_iter_close_container(&args, &arr);

        /* hints */
        dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY,
                "{sv}", &dict);
        if (opts->bg)
                append_hint_str(&dict, "x-doi-bg", opts->bg);
        if (opts->fg)
                append_hint_str(&dict, "x-doi-fg", opts->fg);
        if (opts->border_color)
                append_hint_str(&dict, "x-doi-border-color", opts->border_color);
        if (opts->border >= 0)
                append_hint_int(&dict, "x-doi-border", opts->border);
        append_hint_int(&dict, "x-doi-pos-x",    opts->pos_x);
        append_hint_int(&dict, "x-doi-pos-y",    opts->pos_y);
        append_hint_int(&dict, "x-doi-show-bar",  opts->show_bar);
        append_hint_int(&dict, "x-doi-bar-value", opts->bar_value);
        dbus_message_iter_close_container(&args, &dict);

        dbus_message_iter_append_basic(&args, DBUS_TYPE_INT32, &tms);

        reply = dbus_connection_send_with_reply_and_block(conn, msg, 1000, &err);
        if (dbus_error_is_set(&err)) {
                fprintf(stderr, "doi_notify: failed: %s\n", err.message);
                dbus_error_free(&err);
                dbus_message_unref(msg);
                return 1;
        }

        dbus_message_get_args(reply, &err,
                DBUS_TYPE_UINT32, &notif_id, DBUS_TYPE_INVALID);
        write_last_id(id_key, notif_id);
        dbus_message_unref(reply);
        dbus_message_unref(msg);
        return 0;
}

int doi_notify(const char* summary, const char* body,
               const char* icon, int timeout_ms) {
        BndNotifyOpts opts;
        memset(&opts, 0, sizeof(BndNotifyOpts));
        opts.summary  = summary;
        opts.body     = body;
        opts.icon     = icon;
        opts.timeout  = timeout_ms;
        opts.border   = -1;
        opts.pos_x    = BND_POS_X;
        opts.pos_y    = BND_POS_Y;
        return doi_notify_opts(&opts);
}
