#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dbus/dbus.h>
#include "notif.h"
#include "log.h"
#include "../config.h"

/* ── persistent render children ──────────────────────────────────────── */
/* Each unique (app_name + pos_x + pos_y) gets one persistent child.
 * New notifications for the same slot are piped into the existing child — 
 * no kill, no respawn, no flicker. */

#define MAX_SLOTS 32

typedef struct {
        char  key[128];   /* "app|pos_x|pos_y" */
        pid_t pid;
        int   write_fd;   /* write end of pipe to child */
        int   stack_idx;
        int   pos_x;
        int   pos_y;
} Slot;

static Slot   slots[MAX_SLOTS];
static int    slot_count = 0;

/* track stack depth per position for stacking */
static int    stack_depth[3][3];

static void sigchld_handler(int sig) {
        pid_t p; int i;
        (void)sig;
        while ((p = waitpid(-1, NULL, WNOHANG)) > 0) {
                for (i = 0; i < slot_count; i++) {
                        int px, py;
                        if (slots[i].pid != p) continue;
                        w_log("slot %s exited", slots[i].key);
                        close(slots[i].write_fd);
                        /* free stack position */
                        px = slots[i].pos_x;
                        py = slots[i].pos_y;
                        if (stack_depth[px][py] > 0) stack_depth[px][py]--;
                        /* compact */
                        slots[i] = slots[slot_count-1];
                        slot_count--;
                        break;
                }
        }
}

static void make_key(char* out, size_t sz,
                const char* app, int px, int py) {
        snprintf(out, sz, "%s|%d|%d", app ? app : "", px, py);
}

static Slot* find_slot(const char* key) {
        int i;
        for (i = 0; i < slot_count; i++)
                if (strcmp(slots[i].key, key) == 0)
                        return &slots[i];
        return NULL;
}

static Slot* new_slot(const char* key, int px, int py,
                Notif* initial) {
        int fds[2];
        pid_t pid;

        if (slot_count >= MAX_SLOTS) return NULL;
        if (pipe(fds) < 0) return NULL;

        /* assign stack index BEFORE fork so child sees correct value */
        initial->stack_index = stack_depth[px][py]++;

        pid = fork();
        if (pid < 0) {
                close(fds[0]); close(fds[1]);
                stack_depth[px][py]--;   /* undo on failure */
                return NULL;
        }

        if (pid == 0) {
                /* child: close write end, render from read end */
                close(fds[1]);
                render_loop(fds[0], initial);
                close(fds[0]);
                exit(EXIT_SUCCESS);
        }

        /* parent: close read end */
        close(fds[0]);

        Slot* s = &slots[slot_count++];
        strncpy(s->key, key, sizeof(s->key)-1);
        s->pid      = pid;
        s->write_fd = fds[1];
        s->stack_idx = initial->stack_index;
        s->pos_x     = px;
        s->pos_y     = py;
        w_log("new slot key=%s pid=%d sidx=%d", key, (int)pid, s->stack_idx);
        return s;
}

static void send_update(Slot* s, const Notif* n) {
        NotifUpdate u;
        memset(&u, 0, sizeof(u));
        if (n->summary) strncpy(u.summary, n->summary, sizeof(u.summary)-1);
        if (n->body)    strncpy(u.body,    n->body,    sizeof(u.body)-1);
        if (n->icon)    strncpy(u.icon,    n->icon,    sizeof(u.icon)-1);
        if (n->bg)      strncpy(u.bg,      n->bg,      sizeof(u.bg)-1);
        if (n->fg)      strncpy(u.fg,      n->fg,      sizeof(u.fg)-1);
        if (n->border_color)
                        strncpy(u.border_color, n->border_color,
                                sizeof(u.border_color)-1);
        if (n->bar_fg)  strncpy(u.bar_fg, n->bar_fg,  sizeof(u.bar_fg)-1);
        if (n->bar_bg)  strncpy(u.bar_bg, n->bar_bg,  sizeof(u.bar_bg)-1);
        u.border     = n->border;
        u.timeout    = n->timeout;
        u.min_width  = n->min_width;
        u.show_icon  = n->show_icon;
        u.show_body  = n->show_body;
        u.show_bar   = n->show_bar;
        u.bar_value  = n->bar_value;
        u.bar_width  = n->bar_width;
        u.bar_height = n->bar_height;
        u.min_height = n->min_height;
        u.offset_x   = n->offset_x;
        u.offset_y   = n->offset_y;
        write(s->write_fd, &u, sizeof(u));
}

/* ── hint parsing ─────────────────────────────────────────────────────── */

static void read_hints(DBusMessageIter* it, Notif* n) {
        DBusMessageIter dict;
        if (dbus_message_iter_get_arg_type(it) != DBUS_TYPE_ARRAY) return;
        dbus_message_iter_recurse(it, &dict);
        while (dbus_message_iter_get_arg_type(&dict) == DBUS_TYPE_DICT_ENTRY) {
                DBusMessageIter entry, var;
                const char* key;
                dbus_message_iter_recurse(&dict, &entry);
                dbus_message_iter_get_basic(&entry, &key);
                dbus_message_iter_next(&entry);
                if (dbus_message_iter_get_arg_type(&entry) != DBUS_TYPE_VARIANT)
                        goto next;
                dbus_message_iter_recurse(&entry, &var);
                int type = dbus_message_iter_get_arg_type(&var);

#define STR_HINT(k, field) \
        if (strcmp(key,k)==0 && type==DBUS_TYPE_STRING) { \
                const char* v; dbus_message_iter_get_basic(&var,&v); \
                free(n->field); n->field=strdup(v); n->doi_hints=1; goto next; }
#define INT_HINT(k, field) \
        if (strcmp(key,k)==0 && type==DBUS_TYPE_INT32) { \
                dbus_int32_t v; dbus_message_iter_get_basic(&var,&v); \
                n->field=(int)v; n->doi_hints=1; goto next; }

                STR_HINT("x-doi-bg",           bg)
                STR_HINT("x-doi-fg",           fg)
                STR_HINT("x-doi-border-color", border_color)
                STR_HINT("x-doi-bar-fg",       bar_fg)
                STR_HINT("x-doi-bar-bg",       bar_bg)
                INT_HINT("x-doi-border",       border)
                INT_HINT("x-doi-pos-x",        pos_x)
                INT_HINT("x-doi-pos-y",        pos_y)
                INT_HINT("x-doi-show-bar",     show_bar)
                INT_HINT("x-doi-bar-value",    bar_value)
                INT_HINT("x-doi-bar-width",    bar_width)
                INT_HINT("x-doi-bar-height",   bar_height)
                INT_HINT("x-doi-min-width",    min_width)
                INT_HINT("x-doi-min-height",   min_height)
                INT_HINT("x-doi-offset-x",     offset_x)
                INT_HINT("x-doi-offset-y",     offset_y)
                INT_HINT("x-doi-show-icon",    show_icon)
                INT_HINT("x-doi-show-body",    show_body)

                if (strcmp(key,"value")==0 && type==DBUS_TYPE_INT32) {
                        dbus_int32_t v;
                        dbus_message_iter_get_basic(&var, &v);
                        n->bar_value = (int)v;
                        n->show_bar  = 1;
                }
next:
                dbus_message_iter_next(&dict);
        }
}

/* ── ignore list ──────────────────────────────────────────────────────── */

static int is_ignored(const char* app) {
        char buf[] = DOI_IGNORE_APPS;
        char* tok = strtok(buf, ",");
        while (tok) {
                while (*tok == ' ') tok++;
                if (strcmp(tok, app) == 0) return 1;
                tok = strtok(NULL, ",");
        }
        return 0;
}

/* ── DBus message handler ─────────────────────────────────────────────── */

static DBusHandlerResult handle(DBusConnection* conn,
                DBusMessage* msg, void* data) {
        (void)data;

        if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_METHOD_CALL)
                return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

        if (dbus_message_is_method_call(msg,
                        "org.freedesktop.Notifications", "Notify")) {

                static dbus_uint32_t next_id = 1;
                DBusMessageIter args;
                DBusMessage* reply;
                char*  app_name  = "";
                dbus_uint32_t replace_id = 0;
                char*  app_icon  = "";
                char*  summary   = "";
                char*  body      = "";
                dbus_int32_t expire = -1;
                Notif n;

                if (!dbus_message_iter_init(msg, &args)) goto reply;

#define NEXT_STR(f) \
        if (dbus_message_iter_get_arg_type(&args)==DBUS_TYPE_STRING) \
                dbus_message_iter_get_basic(&args,&f); \
        dbus_message_iter_next(&args);
#define NEXT_U32(f) \
        if (dbus_message_iter_get_arg_type(&args)==DBUS_TYPE_UINT32) \
                dbus_message_iter_get_basic(&args,&f); \
        dbus_message_iter_next(&args);

                NEXT_STR(app_name)
                NEXT_U32(replace_id)
                NEXT_STR(app_icon)
                NEXT_STR(summary)
                NEXT_STR(body)
                dbus_message_iter_next(&args); /* skip actions */

                memset(&n, 0, sizeof(n));
                n.summary   = strdup(summary);
                n.body      = strdup(body);
                
                n.icon      = (app_icon[0] && app_icon[0]!='/'
                                && strncmp(app_icon,"file:",5)!=0)
                                ? strdup(app_icon) : strdup("");
                n.border     = -1;
                n.show_icon  = -1;
                n.show_body  = -1;
                n.bar_width  = -1;
                n.bar_height = -1;
                n.min_width  = -1;
                n.min_height = -1;
                n.offset_x   = -1;
                n.offset_y   = -1;
                n.pos_x      = DOI_POS_X;
                n.pos_y      = DOI_POS_Y;
                n.timeout    = DOI_TIMEOUT;
                n.doi_hints  = 0;

                read_hints(&args, &n);
                dbus_message_iter_next(&args);

                if (dbus_message_iter_get_arg_type(&args)==DBUS_TYPE_INT32) {
                        dbus_message_iter_get_basic(&args, &expire);
                        if (expire > 0) n.timeout = expire / 1000;
                }



                if (is_ignored(app_name)) {
                        free(n.summary); free(n.body);
                         free(n.icon);
                        goto reply;
                }

                w_log("Notify app=%s sum=%s pos=%d,%d bar=%d/%d",
                        app_name, summary,
                        n.pos_x, n.pos_y, n.show_bar, n.bar_value);

                {
                        char key[128];
                        Slot* s = NULL;

                        if (n.doi_hints) {
                                /* doi module: one persistent slot per app+position */
                                make_key(key, sizeof(key), app_name,
                                        n.pos_x, n.pos_y);
                                s = find_slot(key);
                                if (s) {
                                        send_update(s, &n);
                                        w_log("updated slot key=%s", key);
                                } else {
                                        s = new_slot(key, n.pos_x, n.pos_y, &n);
                                        if (s) send_update(s, &n);
                                }
                        } else {
                                /* external app (Firefox, Edge, etc.):
                                 * replace_id > 0 → reuse existing slot by id
                                 * replace_id == 0 → always new slot            */
                                if (replace_id > 0) {
                                        snprintf(key, sizeof(key), "ext|%u",
                                                replace_id);
                                        s = find_slot(key);
                                }
                                if (s) {
                                        send_update(s, &n);
                                        /* rename to new id for future replaces */
                                        snprintf(s->key, sizeof(s->key),
                                                "ext|%u", next_id);
                                        w_log("ext replace slot id=%u->%u",
                                                replace_id, next_id);
                                } else {
                                        snprintf(key, sizeof(key), "ext|%u",
                                                next_id);
                                        s = new_slot(key, n.pos_x, n.pos_y, &n);
                                        if (s) send_update(s, &n);
                                        w_log("ext new slot key=%s app=%s",
                                                key, app_name);
                                }
                        }
                }

                free(n.summary); free(n.body);  free(n.icon);
                free(n.bg); free(n.fg); free(n.border_color);
                free(n.bar_fg); free(n.bar_bg);

reply:
                reply = dbus_message_new_method_return(msg);
                dbus_message_append_args(reply,
                        DBUS_TYPE_UINT32, &next_id, DBUS_TYPE_INVALID);
                dbus_connection_send(conn, reply, NULL);
                dbus_message_unref(reply);
                next_id++;
                return DBUS_HANDLER_RESULT_HANDLED;

        } else if (dbus_message_is_method_call(msg,
                        "org.freedesktop.Notifications", "GetCapabilities")) {
                const char* caps[] = {
                        "actions","body","body-markup",
                        "icon-static","persistence","x-doi-hints"
                };
                DBusMessage* r = dbus_message_new_method_return(msg);
                DBusMessageIter a, arr; int i;
                dbus_message_iter_init_append(r, &a);
                dbus_message_iter_open_container(&a,DBUS_TYPE_ARRAY,
                        DBUS_TYPE_STRING_AS_STRING,&arr);
                for (i=0;i<6;i++)
                        dbus_message_iter_append_basic(&arr,
                                DBUS_TYPE_STRING,&caps[i]);
                dbus_message_iter_close_container(&a,&arr);
                dbus_connection_send(conn, r, NULL);
                dbus_message_unref(r);
                return DBUS_HANDLER_RESULT_HANDLED;

        } else if (dbus_message_is_method_call(msg,
                        "org.freedesktop.Notifications",
                        "GetServerInformation")) {
                const char *name="doid",*vendor="doi",*ver="2.0",*spec="1.2";
                DBusMessage* r = dbus_message_new_method_return(msg);
                dbus_message_append_args(r,
                        DBUS_TYPE_STRING,&name, DBUS_TYPE_STRING,&vendor,
                        DBUS_TYPE_STRING,&ver,  DBUS_TYPE_STRING,&spec,
                        DBUS_TYPE_INVALID);
                dbus_connection_send(conn, r, NULL);
                dbus_message_unref(r);
                return DBUS_HANDLER_RESULT_HANDLED;

        } else if (dbus_message_is_method_call(msg,
                        "org.freedesktop.Notifications",
                        "CloseNotification")) {
                dbus_uint32_t id = 0;
                DBusError err;
                DBusMessage* r;
                dbus_error_init(&err);
                dbus_message_get_args(msg,&err,
                        DBUS_TYPE_UINT32,&id,DBUS_TYPE_INVALID);
                dbus_error_free(&err);
                r = dbus_message_new_method_return(msg);
                dbus_connection_send(conn, r, NULL);
                dbus_message_unref(r);
                return DBUS_HANDLER_RESULT_HANDLED;
        }

        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

/* ── main ─────────────────────────────────────────────────────────────── */

int main(void) {
        DBusConnection* conn;
        DBusError err;
        int ret;
        struct sigaction sa;
        pid_t sid, pid = fork();

        if (pid < 0) return EXIT_FAILURE;
        if (pid > 0) return EXIT_SUCCESS;

        sid = setsid();
        if (sid < 0) return EXIT_FAILURE;
        chdir("/");

        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = sigchld_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART;
        sigaction(SIGCHLD, &sa, NULL);

        {
                const char* home = getenv("HOME");
                if (home) {
                        char dir[512];
                        snprintf(dir, sizeof(dir), "%s/.doi", home);
                        mkdir(dir, 0755);
                }
        }

        dbus_error_init(&err);
        conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
        if (dbus_error_is_set(&err)) {
                w_log("dbus: %s", err.message);
                dbus_error_free(&err);
                return EXIT_FAILURE;
        }

        ret = dbus_bus_request_name(conn,
                "org.freedesktop.Notifications",
                DBUS_NAME_FLAG_REPLACE_EXISTING, &err);
        if (dbus_error_is_set(&err) ||
                        ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
                w_log("failed to own org.freedesktop.Notifications");
                return EXIT_FAILURE;
        }

        if (!dbus_connection_add_filter(conn, handle, NULL, NULL))
                return EXIT_FAILURE;

        w_log("doid started");

        while (dbus_connection_read_write_dispatch(conn, -1))
                ;

        return EXIT_SUCCESS;
}
