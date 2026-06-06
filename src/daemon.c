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

#define MAX_SLOTS 32

typedef struct {
        char  key[128];
        pid_t pid;
        int   write_fd;
        int   stack_index;
        int   pos_x;
        int   pos_y;
} Slot;

static Slot slots[MAX_SLOTS];
static int  slot_count = 0;
static int  stack_depth[3][3];  /* [pos_x][pos_y] */

static void slide_slots_down(int px, int py, int removed_index);

static void sigchld_handler(int sig) {
        pid_t p;
        int i;
        (void)sig;
        while ((p = waitpid(-1, NULL, WNOHANG)) > 0) {
                for (i = 0; i < slot_count; i++) {
                        int px, py, sidx;
                        if (slots[i].pid != p) continue;
                        close(slots[i].write_fd);
                        px   = slots[i].pos_x;
                        py   = slots[i].pos_y;
                        sidx = slots[i].stack_index;
                        slots[i] = slots[slot_count - 1];
                        slot_count--;
                        if (stack_depth[px][py] > 0) stack_depth[px][py]--;
                        slide_slots_down(px, py, sidx);
                        break;
                }
        }
}

static void slide_slots_down(int px, int py, int removed_index) {
        int i;
        for (i = 0; i < slot_count; i++) {
                NotifMsg reposition;
                if (slots[i].pos_x != px || slots[i].pos_y != py) continue;
                if (slots[i].stack_index <= removed_index)         continue;
                slots[i].stack_index--;
                memset(&reposition, 0, sizeof(reposition));
                reposition.msg_type       = MSG_REPOSITION;
                reposition.new_stack_index = slots[i].stack_index;
                write(slots[i].write_fd, &reposition, sizeof(reposition));
        }
}

static Slot *find_slot(const char *key) {
        int i;
        for (i = 0; i < slot_count; i++)
                if (strcmp(slots[i].key, key) == 0)
                        return &slots[i];
        return NULL;
}

static Slot *new_slot(const char *key, int px, int py, Notif *initial) {
        int   fds[2];
        pid_t pid;
        Slot *s;

        if (slot_count >= MAX_SLOTS) return NULL;
        if (pipe(fds) < 0) return NULL;

        initial->stack_index = stack_depth[px][py]++;

        pid = fork();
        if (pid < 0) {
                close(fds[0]); close(fds[1]);
                stack_depth[px][py]--;
                return NULL;
        }
        if (pid == 0) {
                close(fds[1]);
                render_loop(fds[0], initial);
                close(fds[0]);
                exit(EXIT_SUCCESS);
        }

        close(fds[0]);
        s = &slots[slot_count++];
        strncpy(s->key, key, sizeof(s->key) - 1);
        s->pid         = pid;
        s->write_fd    = fds[1];
        s->stack_index = initial->stack_index;
        s->pos_x       = px;
        s->pos_y       = py;
        return s;
}

static void send_to_slot(Slot *s, const Notif *n) {
        NotifMsg m;
        memset(&m, 0, sizeof(m));
        m.msg_type = MSG_UPDATE;

        if (n->summary)      strncpy(m.summary,      n->summary,      sizeof(m.summary)      - 1);
        if (n->body)         strncpy(m.body,          n->body,         sizeof(m.body)         - 1);
        if (n->icon)         strncpy(m.icon,          n->icon,         sizeof(m.icon)         - 1);
        if (n->bg)           strncpy(m.bg,            n->bg,           sizeof(m.bg)           - 1);
        if (n->fg)           strncpy(m.fg,            n->fg,           sizeof(m.fg)           - 1);
        if (n->border_color) strncpy(m.border_color,  n->border_color, sizeof(m.border_color) - 1);
        if (n->bar_fg)       strncpy(m.bar_fg,        n->bar_fg,       sizeof(m.bar_fg)       - 1);
        if (n->bar_bg)       strncpy(m.bar_bg,        n->bar_bg,       sizeof(m.bar_bg)       - 1);

        m.border        = n->border;
        m.border_radius = n->border_radius;
        m.timeout       = n->timeout;
        m.min_width     = n->min_width;
        m.min_height    = n->min_height;
        m.offset_x      = n->offset_x;
        m.offset_y      = n->offset_y;
        m.show_icon     = n->show_icon;
        m.show_body     = n->show_body;
        m.show_bar      = n->show_bar;
        m.bar_value     = n->bar_value;
        m.bar_width     = n->bar_width;
        m.bar_height    = n->bar_height;
        m.layout        = n->layout;

        write(s->write_fd, &m, sizeof(m));
}

static void parse_hints(DBusMessageIter *iter, Notif *n) {
        DBusMessageIter dict;

        if (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_ARRAY) return;
        dbus_message_iter_recurse(iter, &dict);

        while (dbus_message_iter_get_arg_type(&dict) == DBUS_TYPE_DICT_ENTRY) {
                DBusMessageIter entry, var;
                const char *key;
                int type;

                dbus_message_iter_recurse(&dict, &entry);
                dbus_message_iter_get_basic(&entry, &key);
                dbus_message_iter_next(&entry);
                if (dbus_message_iter_get_arg_type(&entry) != DBUS_TYPE_VARIANT) goto next;

                dbus_message_iter_recurse(&entry, &var);
                type = dbus_message_iter_get_arg_type(&var);

#define READ_STR(hkey, field) \
        if (strcmp(key, hkey) == 0 && type == DBUS_TYPE_STRING) { \
                const char *v; dbus_message_iter_get_basic(&var, &v); \
                free(n->field); n->field = strdup(v); n->from_module = 1; goto next; }

#define READ_INT(hkey, field) \
        if (strcmp(key, hkey) == 0 && type == DBUS_TYPE_INT32) { \
                dbus_int32_t v; dbus_message_iter_get_basic(&var, &v); \
                n->field = (int)v; n->from_module = 1; goto next; }

                READ_STR("x-doi-bg",           bg)
                READ_STR("x-doi-fg",           fg)
                READ_STR("x-doi-border-color", border_color)
                READ_STR("x-doi-bar-fg",       bar_fg)
                READ_STR("x-doi-bar-bg",       bar_bg)
                READ_INT("x-doi-border",       border)
                READ_INT("x-doi-pos-x",        pos_x)
                READ_INT("x-doi-pos-y",        pos_y)
                READ_INT("x-doi-show-bar",     show_bar)
                READ_INT("x-doi-bar-value",    bar_value)
                READ_INT("x-doi-bar-width",    bar_width)
                READ_INT("x-doi-bar-height",   bar_height)
                READ_INT("x-doi-min-width",    min_width)
                READ_INT("x-doi-min-height",   min_height)
                READ_INT("x-doi-offset-x",     offset_x)
                READ_INT("x-doi-offset-y",     offset_y)
                READ_INT("x-doi-show-icon",    show_icon)
                READ_INT("x-doi-show-body",    show_body)
                READ_INT("x-doi-border-radius",border_radius)
                READ_INT("x-doi-layout",       layout)

                /* standard progress bar hint (e.g. from notify-send --hint) */
                if (strcmp(key, "value") == 0 && type == DBUS_TYPE_INT32) {
                        dbus_int32_t v;
                        dbus_message_iter_get_basic(&var, &v);
                        n->bar_value = (int)v;
                        n->show_bar  = 1;
                }

next:
                dbus_message_iter_next(&dict);
        }

#undef READ_STR
#undef READ_INT
}

static int is_ignored(const char *app) {
        char buf[] = IGNORE_APPS;
        char *tok  = strtok(buf, ",");
        while (tok) {
                while (*tok == ' ') tok++;
                if (strcmp(tok, app) == 0) return 1;
                tok = strtok(NULL, ",");
        }
        return 0;
}

static DBusHandlerResult handle(DBusConnection *conn,
                                DBusMessage *msg, void *data) {
        (void)data;
        if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_METHOD_CALL)
                return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

        if (dbus_message_is_method_call(msg, "org.freedesktop.Notifications", "Notify")) {
                static dbus_uint32_t next_id = 1;
                DBusMessageIter args;
                DBusMessage *reply;
                const char *app_name = "";
                const char *app_icon = "";
                const char *summary  = "";
                const char *body     = "";
                dbus_uint32_t replace_id = 0;
                dbus_int32_t  expire     = -1;
                Notif n;

                if (!dbus_message_iter_init(msg, &args)) goto reply;

#define NEXT_STR(f) \
        if (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_STRING) \
                dbus_message_iter_get_basic(&args, &f); \
        dbus_message_iter_next(&args);
#define NEXT_U32(f) \
        if (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_UINT32) \
                dbus_message_iter_get_basic(&args, &f); \
        dbus_message_iter_next(&args);

                NEXT_STR(app_name)
                NEXT_U32(replace_id)
                NEXT_STR(app_icon)
                NEXT_STR(summary)
                NEXT_STR(body)
                dbus_message_iter_next(&args);  /* skip actions array */

                memset(&n, 0, sizeof(n));
                n.summary       = strdup(summary);
                n.body          = strdup(body);
                n.icon          = (app_icon[0] && app_icon[0] != '/'
                                    && strncmp(app_icon, "file:", 5) != 0)
                                   ? strdup(app_icon) : strdup("");
                n.border        = -1;
                n.border_radius = -1;
                n.show_icon     = -1;
                n.show_body     = -1;
                n.bar_width     = -1;
                n.bar_height    = -1;
                n.min_width     = -1;
                n.min_height    = -1;
                n.offset_x      = -1;
                n.offset_y      = -1;
                n.layout        = LAYOUT;
                n.pos_x         = POS_X;
                n.pos_y         = POS_Y;
                n.timeout       = TIMEOUT;
                n.from_module   = 0;

                parse_hints(&args, &n);
                dbus_message_iter_next(&args);

                if (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_INT32) {
                        dbus_message_iter_get_basic(&args, &expire);
                        if (expire > 0) n.timeout = expire / 1000;
                }

                if (is_ignored(app_name)) {
                        free(n.summary); free(n.body); free(n.icon);
                        goto reply;
                }

                {
                        char key[128];
                        Slot *s = NULL;

                        if (n.from_module) {
                                snprintf(key, sizeof(key), "%s|%d|%d",
                                        app_name, n.pos_x, n.pos_y);
                                s = find_slot(key);
                                if (s) {
                                        send_to_slot(s, &n);
                                } else {
                                        s = new_slot(key, n.pos_x, n.pos_y, &n);
                                        if (s) send_to_slot(s, &n);
                                }
                        } else {
                                if (replace_id > 0) {
                                        snprintf(key, sizeof(key), "ext|%u", replace_id);
                                        s = find_slot(key);
                                }
                                if (s) {
                                        send_to_slot(s, &n);
                                        snprintf(s->key, sizeof(s->key), "ext|%u", next_id);
                                } else if (STACK_LIMIT <= 0 ||
                                           stack_depth[n.pos_x][n.pos_y] < STACK_LIMIT) {
                                        snprintf(key, sizeof(key), "ext|%u", next_id);
                                        s = new_slot(key, n.pos_x, n.pos_y, &n);
                                        if (s) send_to_slot(s, &n);
                                }
                        }
                }

                free(n.summary); free(n.body);  free(n.icon);
                free(n.bg);      free(n.fg);    free(n.border_color);
                free(n.bar_fg);  free(n.bar_bg);

reply:
                reply = dbus_message_new_method_return(msg);
                dbus_message_append_args(reply,
                        DBUS_TYPE_UINT32, &next_id, DBUS_TYPE_INVALID);
                dbus_connection_send(conn, reply, NULL);
                dbus_message_unref(reply);
                next_id++;
                return DBUS_HANDLER_RESULT_HANDLED;

        } else if (dbus_message_is_method_call(msg, "org.freedesktop.Notifications", "GetCapabilities")) {
                const char *caps[] = { "body", "icon-static", "persistence", "x-doi-hints" };
                DBusMessage *r = dbus_message_new_method_return(msg);
                DBusMessageIter a, arr;
                int i;
                dbus_message_iter_init_append(r, &a);
                dbus_message_iter_open_container(&a, DBUS_TYPE_ARRAY,
                        DBUS_TYPE_STRING_AS_STRING, &arr);
                for (i = 0; i < 4; i++)
                        dbus_message_iter_append_basic(&arr, DBUS_TYPE_STRING, &caps[i]);
                dbus_message_iter_close_container(&a, &arr);
                dbus_connection_send(conn, r, NULL);
                dbus_message_unref(r);
                return DBUS_HANDLER_RESULT_HANDLED;

        } else if (dbus_message_is_method_call(msg, "org.freedesktop.Notifications", "GetServerInformation")) {
                const char *name = "doid", *vendor = "doi", *ver = "2.0", *spec = "1.2";
                DBusMessage *r = dbus_message_new_method_return(msg);
                dbus_message_append_args(r,
                        DBUS_TYPE_STRING, &name,   DBUS_TYPE_STRING, &vendor,
                        DBUS_TYPE_STRING, &ver,    DBUS_TYPE_STRING, &spec,
                        DBUS_TYPE_INVALID);
                dbus_connection_send(conn, r, NULL);
                dbus_message_unref(r);
                return DBUS_HANDLER_RESULT_HANDLED;

        } else if (dbus_message_is_method_call(msg, "org.freedesktop.Notifications", "CloseNotification")) {
                dbus_uint32_t id = 0;
                DBusError err;
                DBusMessage *r;
                dbus_error_init(&err);
                dbus_message_get_args(msg, &err, DBUS_TYPE_UINT32, &id, DBUS_TYPE_INVALID);
                dbus_error_free(&err);
                r = dbus_message_new_method_return(msg);
                dbus_connection_send(conn, r, NULL);
                dbus_message_unref(r);
                return DBUS_HANDLER_RESULT_HANDLED;
        }

        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

int main(void) {
        DBusConnection *conn;
        DBusError err;
        struct sigaction sa;
        int ret;

        {
                const char *home = getenv("HOME");
                if (home) {
                        char dir[512];
                        snprintf(dir, sizeof(dir), "%s/.doi", home);
                        mkdir(dir, 0755);
                }
        }

        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = sigchld_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART;
        sigaction(SIGCHLD, &sa, NULL);

        dbus_error_init(&err);
        conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
        if (dbus_error_is_set(&err)) {
                log_write("dbus: %s", err.message);
                dbus_error_free(&err);
                return EXIT_FAILURE;
        }

        ret = dbus_bus_request_name(conn, "org.freedesktop.Notifications",
                DBUS_NAME_FLAG_REPLACE_EXISTING, &err);
        if (dbus_error_is_set(&err) || ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
                log_write("failed to own org.freedesktop.Notifications");
                return EXIT_FAILURE;
        }

        if (!dbus_connection_add_filter(conn, handle, NULL, NULL))
                return EXIT_FAILURE;

        log_write("doid started");

        while (dbus_connection_read_write_dispatch(conn, -1));

        return EXIT_SUCCESS;
}
