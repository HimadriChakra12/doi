#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dbus/dbus.h>
#include "notify.h"
#include "log.h"
#include "../config.h"

/* slot tracker: counts active notifications per (pos_x, pos_y) pair */
#define BND_MAX_CHILDREN 64
static int  slot_count[3][3] = {{0}};
static pid_t child_pid[BND_MAX_CHILDREN];
static int   child_px[BND_MAX_CHILDREN];
static int   child_py[BND_MAX_CHILDREN];
static int   child_slot[BND_MAX_CHILDREN];
static int   child_count = 0;

static void sigchld_handler(int a) {
        pid_t p;
        int i;
        (void)a;
        while ((p = waitpid(-1, NULL, WNOHANG)) > 0) {
                for (i = 0; i < child_count; ++i) {
                        if (child_pid[i] == p) {
                                int px = child_px[i];
                                int py = child_py[i];
                                /* mark slot as free by shifting higher slots down */
                                int freed = child_slot[i];
                                int j;
                                for (j = 0; j < child_count; ++j) {
                                        if (child_px[j] == px
                                                && child_py[j] == py
                                                && child_slot[j] > freed) {
                                                child_slot[j]--;
                                        }
                                }
                                if (slot_count[px][py] > 0)
                                        slot_count[px][py]--;
                                /* compact array */
                                child_pid[i]  = child_pid[child_count-1];
                                child_px[i]   = child_px[child_count-1];
                                child_py[i]   = child_py[child_count-1];
                                child_slot[i] = child_slot[child_count-1];
                                child_count--;
                                break;
                        }
                }
        }
}

static int acquire_slot(int pos_x, int pos_y) {
        int s = slot_count[pos_x][pos_y]++;
        return s;
}

static void register_child(pid_t p, int pos_x, int pos_y, int slot) {
        if (child_count < BND_MAX_CHILDREN) {
                child_pid[child_count]  = p;
                child_px[child_count]   = pos_x;
                child_py[child_count]   = pos_y;
                child_slot[child_count] = slot;
                child_count++;
        }
}

/* replace tracking: keyed by notif_id OR by app+summary string */
#define BND_MAX_IDS 64

static dbus_uint32_t id_map_id[BND_MAX_IDS];
static int           id_map_idx[BND_MAX_IDS];
static char          id_map_key[BND_MAX_IDS][128]; /* "app\0summary" */
static int           id_map_count = 0;

static void make_key(char* out, size_t sz,
                const char* app, const char* summary) {
        snprintf(out, sz, "%s|%s", app ? app : "", summary ? summary : "");
}

static void id_map_add(dbus_uint32_t id, int child_idx,
                const char* app, const char* summary) {
        if (id_map_count < BND_MAX_IDS) {
                id_map_id[id_map_count]  = id;
                id_map_idx[id_map_count] = child_idx;
                make_key(id_map_key[id_map_count],
                        sizeof(id_map_key[0]), app, summary);
                id_map_count++;
        }
}

static int do_replace(int i, int* out_px, int* out_py) {
        int idx = id_map_idx[i];
        if (idx < child_count) {
                int slot = child_slot[idx];
                *out_px  = child_px[idx];
                *out_py  = child_py[idx];
                kill(child_pid[idx], SIGTERM);
                child_pid[idx]  = child_pid[child_count-1];
                child_px[idx]   = child_px[child_count-1];
                child_py[idx]   = child_py[child_count-1];
                child_slot[idx] = child_slot[child_count-1];
                child_count--;
                id_map_id[i]  = id_map_id[id_map_count-1];
                id_map_idx[i] = id_map_idx[id_map_count-1];
                memcpy(id_map_key[i], id_map_key[id_map_count-1],
                        sizeof(id_map_key[0]));
                id_map_count--;
                return slot;
        }
        return -1;
}

/* Kill existing child: first by replace_id, then by app+summary key */
static int replace_existing(dbus_uint32_t replace_id,
                const char* app, const char* summary,
                int* out_px, int* out_py) {
        char key[128];
        int i;
        /* try by id first */
        if (replace_id > 0) {
                for (i = 0; i < id_map_count; ++i) {
                        if (id_map_id[i] == replace_id)
                                return do_replace(i, out_px, out_py);
                }
        }
        /* fall back to app+summary key */
        make_key(key, sizeof(key), app, summary);
        for (i = 0; i < id_map_count; ++i) {
                if (strcmp(id_map_key[i], key) == 0)
                        return do_replace(i, out_px, out_py);
        }
        return -1;
}

static void send_notification_closed(DBusConnection* conn,
                dbus_uint32_t id, dbus_uint32_t reason) {
        DBusMessage* sig;
        sig = dbus_message_new_signal("/org/freedesktop/Notifications",
                "org.freedesktop.Notifications", "NotificationClosed");
        dbus_message_append_args(sig,
                DBUS_TYPE_UINT32, &id,
                DBUS_TYPE_UINT32, &reason,
                DBUS_TYPE_INVALID);
        dbus_connection_send(conn, sig, NULL);
        dbus_message_unref(sig);
}

static void read_hints(DBusMessageIter* hints_iter, Notification* n) {
        DBusMessageIter dict;

        if (dbus_message_iter_get_arg_type(hints_iter) != DBUS_TYPE_ARRAY)
                return;

        dbus_message_iter_recurse(hints_iter, &dict);

        while (dbus_message_iter_get_arg_type(&dict) == DBUS_TYPE_DICT_ENTRY) {
                DBusMessageIter entry, var;
                const char* key;

                dbus_message_iter_recurse(&dict, &entry);
                dbus_message_iter_get_basic(&entry, &key);
                dbus_message_iter_next(&entry);

                if (dbus_message_iter_get_arg_type(&entry) != DBUS_TYPE_VARIANT)
                        goto next;

                dbus_message_iter_recurse(&entry, &var);

                if (strcmp(key, "x-doi-bg") == 0
                                && dbus_message_iter_get_arg_type(&var)
                                        == DBUS_TYPE_STRING) {
                        const char* v;
                        dbus_message_iter_get_basic(&var, &v);
                        n->bg = strdup(v);

                } else if (strcmp(key, "x-doi-fg") == 0
                                && dbus_message_iter_get_arg_type(&var)
                                        == DBUS_TYPE_STRING) {
                        const char* v;
                        dbus_message_iter_get_basic(&var, &v);
                        n->fg = strdup(v);

                } else if (strcmp(key, "x-doi-border-color") == 0
                                && dbus_message_iter_get_arg_type(&var)
                                        == DBUS_TYPE_STRING) {
                        const char* v;
                        dbus_message_iter_get_basic(&var, &v);
                        n->border_color = strdup(v);

                } else if (strcmp(key, "x-doi-border") == 0
                                && dbus_message_iter_get_arg_type(&var)
                                        == DBUS_TYPE_INT32) {
                        dbus_int32_t v;
                        dbus_message_iter_get_basic(&var, &v);
                        n->border = (int)v;

                } else if (strcmp(key, "x-doi-pos-x") == 0
                                && dbus_message_iter_get_arg_type(&var)
                                        == DBUS_TYPE_INT32) {
                        dbus_int32_t v;
                        dbus_message_iter_get_basic(&var, &v);
                        n->pos_x = (int)v;

                } else if (strcmp(key, "x-doi-pos-y") == 0
                                && dbus_message_iter_get_arg_type(&var)
                                        == DBUS_TYPE_INT32) {
                        dbus_int32_t v;
                        dbus_message_iter_get_basic(&var, &v);
                        n->pos_y = (int)v;

                } else if (strcmp(key, "x-doi-show-bar") == 0
                                && dbus_message_iter_get_arg_type(&var)
                                        == DBUS_TYPE_INT32) {
                        dbus_int32_t v;
                        dbus_message_iter_get_basic(&var, &v);
                        n->show_bar = (int)v;

                } else if (strcmp(key, "x-doi-bar-value") == 0
                                && dbus_message_iter_get_arg_type(&var)
                                        == DBUS_TYPE_INT32) {
                        dbus_int32_t v;
                        dbus_message_iter_get_basic(&var, &v);
                        n->bar_value = (int)v;

                /* standard freedesktop progress bar hint */
                } else if (strcmp(key, "value") == 0
                                && dbus_message_iter_get_arg_type(&var)
                                        == DBUS_TYPE_INT32) {
                        dbus_int32_t v;
                        dbus_message_iter_get_basic(&var, &v);
                        n->bar_value = (int)v;
                        n->show_bar  = 1;
                }

next:
                dbus_message_iter_next(&dict);
        }
}

static DBusHandlerResult handle_message(DBusConnection* conn,
                DBusMessage* msg, void* data) {
        (void)data;

        if (dbus_message_is_method_call(msg,
                        "org.freedesktop.Notifications", "Notify")) {
                char* app_name = "";
                dbus_uint32_t replace_id = 0;
                char* app_icon = "";
                char* summary  = "";
                char* body     = "";
                dbus_int32_t expire_timeout = 0;
                DBusError err;
                DBusMessage* reply;
                static dbus_uint32_t notif_id = 1;
                DBusMessageIter args;
                pid_t pid;
                Notification n;

                dbus_error_init(&err);
                if (!dbus_message_iter_init(msg, &args)) goto send_reply;

                /* arg 0: app_name (string) */
                if (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_STRING)
                        dbus_message_iter_get_basic(&args, &app_name);
                dbus_message_iter_next(&args);

                /* arg 1: replace_id (uint32) */
                if (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_UINT32)
                        dbus_message_iter_get_basic(&args, &replace_id);
                dbus_message_iter_next(&args);

                /* arg 2: app_icon (string) */
                if (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_STRING)
                        dbus_message_iter_get_basic(&args, &app_icon);
                dbus_message_iter_next(&args);

                /* arg 3: summary (string) */
                if (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_STRING)
                        dbus_message_iter_get_basic(&args, &summary);
                dbus_message_iter_next(&args);

                /* arg 4: body (string) */
                if (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_STRING)
                        dbus_message_iter_get_basic(&args, &body);
                dbus_message_iter_next(&args);

                /* arg 5: actions (array of string) — skip */
                dbus_message_iter_next(&args);

                /* arg 6: hints (array of dict entries) — READ THIS */
                memset(&n, 0, sizeof(Notification));
                n.summary      = summary;
                n.body         = body;
                n.app_name     = app_name;
                /* skip file:// paths unless Imlib2 is available
                 * to avoid rendering raw paths as text */
#ifdef BND_USE_IMLIB2
                n.icon = app_icon;
#else
                n.icon = (app_icon[0] && app_icon[0] != '/'
                        && strncmp(app_icon, "file:", 5) != 0)
                        ? app_icon : "";
#endif
                n.bg           = NULL;
                n.fg           = NULL;
                n.border_color = NULL;
                n.border       = -1;
                n.timeout      = BND_TIMEOUT;
                n.pos_x        = BND_POS_X;
                n.pos_y        = BND_POS_Y;
                n.show_icon    = -1;
                n.show_body    = -1;
                n.show_bar     = 0;
                n.bar_value    = 0;

                read_hints(&args, &n);  /* args is pointing at hints dict */
                dbus_message_iter_next(&args);

                /* arg 7: expire_timeout (int32) */
                if (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_INT32) {
                        dbus_message_iter_get_basic(&args, &expire_timeout);
                        if (expire_timeout > 0)
                                n.timeout = expire_timeout / 1000;
                }

                /* ignore noisy system notifications */
                if (strcmp(app_name, "flameshot") == 0)
                        goto send_reply;

                w_log("Notify: app=%s summary=%s pos=%d,%d bg=%s bar=%d val=%d replace=%u",
                        app_name, summary, n.pos_x, n.pos_y,
                        n.bg ? n.bg : "(default)",
                        n.show_bar, n.bar_value, replace_id);

                {
                        int rpx = n.pos_x, rpy = n.pos_y;
                        int rslot = replace_existing(replace_id,
                                app_name, summary, &rpx, &rpy);
                        if (rslot >= 0) {
                                n.pos_x       = rpx;
                                n.pos_y       = rpy;
                                n.stack_index = rslot;
                                w_log("replacing slot %d pos=%d,%d", rslot, rpx, rpy);
                        } else {
#if BND_STACK_LIMIT > 0
                                /* check if we are at the limit */
                                if (slot_count[n.pos_x][n.pos_y] >= BND_STACK_LIMIT) {
                                        /* update the overflow indicator in the last slot */
                                        int overflow = slot_count[n.pos_x][n.pos_y]
                                                        - BND_STACK_LIMIT + 1;
                                        char obody[64];
                                        Notification ov;
                                        snprintf(obody, sizeof(obody),
                                                BND_STACK_OVERFLOW, overflow);
                                        memset(&ov, 0, sizeof(Notification));
                                        ov.summary      = obody;
                                        ov.body         = "";
                                        ov.bg           = n.bg;
                                        ov.fg           = n.fg;
                                        ov.border_color = n.border_color;
                                        ov.border       = n.border;
                                        ov.timeout      = n.timeout;
                                        ov.pos_x        = n.pos_x;
                                        ov.pos_y        = n.pos_y;
                                        ov.show_icon    = 0;
                                        ov.show_body    = 0;
                                        ov.show_bar     = 0;
                                        ov.stack_index  = BND_STACK_LIMIT - 1;
                                        /* replace existing overflow indicator if present */
                                        {
                                                int ox, oy;
                                                replace_existing(0,
                                                        "doi-overflow", "overflow",
                                                        &ox, &oy);
                                        }
                                        pid = fork();
                                        if (pid == 0) {
                                                notify(&ov);
                                                exit(EXIT_SUCCESS);
                                        }
                                        register_child(pid, n.pos_x, n.pos_y,
                                                BND_STACK_LIMIT - 1);
                                        id_map_add(notif_id, child_count - 1,
                                                "doi-overflow", "overflow");
                                        slot_count[n.pos_x][n.pos_y]++;
                                        goto send_reply;
                                }
#endif
                                n.stack_index = acquire_slot(n.pos_x, n.pos_y);
                                w_log("slot acquired: index=%d pos=%d,%d", n.stack_index, n.pos_x, n.pos_y);
                        }
                }

                pid = fork();
                if (pid == 0) {
                        notify(&n);
                        free(n.bg);
                        free(n.fg);
                        free(n.border_color);
                        exit(EXIT_SUCCESS);
                }

                /* release slot when child finishes (via sigchld_handler) */
                /* we track release by monitoring child exit in a wrapper */
                register_child(pid, n.pos_x, n.pos_y, n.stack_index);
                id_map_add(notif_id, child_count - 1, app_name, summary);
                free(n.bg);
                free(n.fg);
                free(n.border_color);

send_reply:
                reply = dbus_message_new_method_return(msg);
                dbus_message_append_args(reply,
                        DBUS_TYPE_UINT32, &notif_id,
                        DBUS_TYPE_INVALID);
                dbus_connection_send(conn, reply, NULL);
                dbus_message_unref(reply);
                send_notification_closed(conn, notif_id, 1);
                notif_id++;
                return DBUS_HANDLER_RESULT_HANDLED;

        } else if (dbus_message_is_method_call(msg,
                        "org.freedesktop.Notifications",
                        "GetCapabilities")) {
                DBusMessage* reply;
                DBusMessageIter args, arr;
                const char* caps[] = {
                        "actions", "body", "body-markup",
                        "icon-static", "persistence",
                        "x-doi-hints"
                };
                int i;
                reply = dbus_message_new_method_return(msg);
                dbus_message_iter_init_append(reply, &args);
                dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY,
                        DBUS_TYPE_STRING_AS_STRING, &arr);
                for (i = 0; i < 6; ++i)
                        dbus_message_iter_append_basic(&arr,
                                DBUS_TYPE_STRING, &caps[i]);
                dbus_message_iter_close_container(&args, &arr);
                dbus_connection_send(conn, reply, NULL);
                dbus_message_unref(reply);
                return DBUS_HANDLER_RESULT_HANDLED;

        } else if (dbus_message_is_method_call(msg,
                        "org.freedesktop.Notifications",
                        "GetServerInformation")) {
                DBusMessage* reply;
                const char* name    = "bndd";
                const char* vendor  = "bnd";
                const char* version = "1.0";
                const char* spec    = "1.2";
                reply = dbus_message_new_method_return(msg);
                dbus_message_append_args(reply,
                        DBUS_TYPE_STRING, &name,
                        DBUS_TYPE_STRING, &vendor,
                        DBUS_TYPE_STRING, &version,
                        DBUS_TYPE_STRING, &spec,
                        DBUS_TYPE_INVALID);
                dbus_connection_send(conn, reply, NULL);
                dbus_message_unref(reply);
                return DBUS_HANDLER_RESULT_HANDLED;

        } else if (dbus_message_is_method_call(msg,
                        "org.freedesktop.Notifications",
                        "CloseNotification")) {
                dbus_uint32_t id = 0;
                DBusError err;
                DBusMessage* reply;
                dbus_error_init(&err);
                dbus_message_get_args(msg, &err,
                        DBUS_TYPE_UINT32, &id, DBUS_TYPE_INVALID);
                dbus_error_free(&err);
                reply = dbus_message_new_method_return(msg);
                dbus_connection_send(conn, reply, NULL);
                dbus_message_unref(reply);
                send_notification_closed(conn, id, 3);
                return DBUS_HANDLER_RESULT_HANDLED;
        }

        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

int main(void) {
        DBusConnection* conn;
        DBusError err;
        int ret;
        struct sigaction newaction;
        pid_t sid;
        pid_t pid = fork();

        if (pid < 0) { w_log("ERROR: fork failed."); return EXIT_FAILURE; }
        if (pid > 0) return EXIT_SUCCESS;

        sid = setsid();
        if (sid < 0) { w_log("ERROR: setsid failed."); return EXIT_FAILURE; }
        chdir("/");

        newaction.sa_handler = sigchld_handler;
        sigemptyset(&newaction.sa_mask);
        newaction.sa_flags = 0;
        sigaction(SIGCHLD, &newaction, NULL);

        /* ensure log directory exists */
        {
                const char* home = getenv("HOME");
                if (home) {
                        char logdir[512];
                        snprintf(logdir, sizeof(logdir), "%s/.bnd", home);
                        mkdir(logdir, 0755);
                }
        }

        dbus_error_init(&err);
        conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
        if (dbus_error_is_set(&err)) {
                w_log("ERROR: dbus: %s", err.message);
                dbus_error_free(&err);
                return EXIT_FAILURE;
        }

        ret = dbus_bus_request_name(conn, "org.freedesktop.Notifications",
                DBUS_NAME_FLAG_REPLACE_EXISTING, &err);
        if (dbus_error_is_set(&err)) {
                w_log("ERROR: request name: %s", err.message);
                dbus_error_free(&err);
                return EXIT_FAILURE;
        }
        if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
                w_log("ERROR: not primary owner.");
                return EXIT_FAILURE;
        }

        if (!dbus_connection_add_filter(conn, handle_message, NULL, NULL)) {
                w_log("ERROR: add filter failed.");
                return EXIT_FAILURE;
        }

        w_log("bndd started");

        while (dbus_connection_read_write_dispatch(conn, -1))
                ;

        return EXIT_SUCCESS;
}
