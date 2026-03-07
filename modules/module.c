#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dbus/dbus.h>
#include "module.h"

/* ── hint helpers ─────────────────────────────────────────────────────── */

static void hint_str(DBusMessageIter* d, const char* k, const char* v) {
        DBusMessageIter e, var;
        dbus_message_iter_open_container(d, DBUS_TYPE_DICT_ENTRY, NULL, &e);
        dbus_message_iter_append_basic(&e, DBUS_TYPE_STRING, &k);
        dbus_message_iter_open_container(&e, DBUS_TYPE_VARIANT,
                DBUS_TYPE_STRING_AS_STRING, &var);
        dbus_message_iter_append_basic(&var, DBUS_TYPE_STRING, &v);
        dbus_message_iter_close_container(&e, &var);
        dbus_message_iter_close_container(d, &e);
}

static void hint_int(DBusMessageIter* d, const char* k, int val) {
        DBusMessageIter e, var;
        dbus_int32_t v = (dbus_int32_t)val;
        dbus_message_iter_open_container(d, DBUS_TYPE_DICT_ENTRY, NULL, &e);
        dbus_message_iter_append_basic(&e, DBUS_TYPE_STRING, &k);
        dbus_message_iter_open_container(&e, DBUS_TYPE_VARIANT,
                DBUS_TYPE_INT32_AS_STRING, &var);
        dbus_message_iter_append_basic(&var, DBUS_TYPE_INT32, &v);
        dbus_message_iter_close_container(&e, &var);
        dbus_message_iter_close_container(d, &e);
}

/* ── ID persistence ───────────────────────────────────────────────────── */

static dbus_uint32_t read_id(const char* key) {
        char path[512], line[256];
        const char* home = getenv("HOME");
        size_t klen;
        FILE* f;
        if (!home) return 0;
        klen = strlen(key);
        snprintf(path, sizeof(path), "%s/.doi/ids", home);
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

static void write_id(const char* key, dbus_uint32_t id) {
        char path[512], tmp[512], line[256];
        const char* home = getenv("HOME");
        FILE *fin, *fout;
        int found = 0;
        size_t klen;
        if (!home) return;
        klen = strlen(key);
        snprintf(path, sizeof(path), "%s/.doi/ids",     home);
        snprintf(tmp,  sizeof(tmp),  "%s/.doi/ids.tmp", home);
        fin  = fopen(path, "r");
        fout = fopen(tmp,  "w");
        if (!fout) { if (fin) fclose(fin); return; }
        if (fin) {
                while (fgets(line, sizeof(line), fin)) {
                        if (strncmp(line, key, klen)==0 && line[klen]=='\t') {
                                fprintf(fout, "%s\t%u\n", key, id);
                                found = 1;
                        } else {
                                fputs(line, fout);
                        }
                }
                fclose(fin);
        }
        if (!found) fprintf(fout, "%s\t%u\n", key, id);
        fclose(fout);
        rename(tmp, path);
}

/* ── main entry point ─────────────────────────────────────────────────── */

int doi_notify(const DoiOpts* opts) {
        DBusConnection* conn;
        DBusError       err;
        DBusMessage*    msg;
        DBusMessage*    reply;
        DBusMessageIter args, arr, dict;
        const char* app      = "doi";
        const char* sum      = opts->summary ? opts->summary : "";
        const char* bod      = opts->body    ? opts->body    : "";
        const char* ico      = opts->icon    ? opts->icon    : "";
        const char* id_key   = sum[0] ? sum : "default";
        dbus_uint32_t rep_id = read_id(id_key);
        dbus_uint32_t out_id = 0;
        dbus_int32_t  tms    = (dbus_int32_t)(opts->timeout > 0
                                ? opts->timeout : 5000);

        dbus_error_init(&err);
        conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
        if (dbus_error_is_set(&err)) {
                fprintf(stderr, "doi_notify: %s\n", err.message);
                dbus_error_free(&err);
                return 1;
        }

        msg = dbus_message_new_method_call(
                "org.freedesktop.Notifications",
                "/org/freedesktop/Notifications",
                "org.freedesktop.Notifications", "Notify");
        if (!msg) return 1;

        dbus_message_iter_init_append(msg, &args);
        dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &app);
        dbus_message_iter_append_basic(&args, DBUS_TYPE_UINT32, &rep_id);
        dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &ico);
        dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &sum);
        dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &bod);

        /* actions: empty */
        dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY,
                DBUS_TYPE_STRING_AS_STRING, &arr);
        dbus_message_iter_close_container(&args, &arr);

        /* hints */
        dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "{sv}", &dict);
        if (opts->bg)           hint_str(&dict, "x-doi-bg",           opts->bg);
        if (opts->fg)           hint_str(&dict, "x-doi-fg",           opts->fg);
        if (opts->border_color) hint_str(&dict, "x-doi-border-color", opts->border_color);
        if (opts->bar_fg)       hint_str(&dict, "x-doi-bar-fg",       opts->bar_fg);
        if (opts->bar_bg)       hint_str(&dict, "x-doi-bar-bg",       opts->bar_bg);
        if (opts->border >= 0)  hint_int(&dict, "x-doi-border",       opts->border);
        if (opts->min_width > 0) hint_int(&dict, "x-doi-min-width",   opts->min_width);
        hint_int(&dict, "x-doi-pos-x",      opts->pos_x);
        hint_int(&dict, "x-doi-pos-y",      opts->pos_y);
        hint_int(&dict, "x-doi-show-bar",   opts->show_bar);
        hint_int(&dict, "x-doi-bar-value",  opts->bar_value);
        if (opts->bar_width  > 0) hint_int(&dict, "x-doi-bar-width",   opts->bar_width);
        if (opts->bar_height > 0) hint_int(&dict, "x-doi-bar-height",  opts->bar_height);
        if (opts->min_height > 0) hint_int(&dict, "x-doi-min-height",  opts->min_height);
        if (opts->offset_x   >= 0) hint_int(&dict, "x-doi-offset-x",   opts->offset_x);
        if (opts->offset_y   >= 0) hint_int(&dict, "x-doi-offset-y",   opts->offset_y);
        dbus_message_iter_close_container(&args, &dict);

        dbus_message_iter_append_basic(&args, DBUS_TYPE_INT32, &tms);

        reply = dbus_connection_send_with_reply_and_block(conn, msg, 2000, &err);
        if (dbus_error_is_set(&err)) {
                fprintf(stderr, "doi_notify: send: %s\n", err.message);
                dbus_error_free(&err);
                dbus_message_unref(msg);
                return 1;
        }

        dbus_message_get_args(reply, &err,
                DBUS_TYPE_UINT32, &out_id, DBUS_TYPE_INVALID);
        write_id(id_key, out_id);
        dbus_message_unref(reply);
        dbus_message_unref(msg);
        return 0;
}
