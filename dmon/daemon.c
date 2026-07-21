/* doid — doi notification daemon
 *
 * Owns org.freedesktop.Notifications on the session D-Bus.
 * Forks one render child per active notification; communicates via pipes.
 *
 * NotifMsg sentinel convention:
 *   Integer fields that have a config.h default are initialised to
 *   DOI_DEFAULT (-999) by send_update().  render.c resolves them:
 *     DOI_DEFAULT  →  use the compiled-in config.h value
 *     anything else → use that value verbatim (even 0)
 *   String fields use "" as the "use default" sentinel.
 *
 * This is the ONLY correct way to let config.h values like BORDER=0
 *   or BORDER_RADIUS=0 actually take effect — zero-initialising the
 *   struct with memset is not enough because 0 is a valid value.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dbus/dbus.h>
#include "../src/notif.h"
#include "../src/log.h"
#include "../config.h"

/* Must match the sentinel in render.c */
#define DOI_DEFAULT (-999)

#define MAX_SLOTS     32
#define NOTIF_SERVICE "org.freedesktop.Notifications"
#define NOTIF_PATH    "/org/freedesktop/Notifications"
#define NOTIF_IFACE   NOTIF_SERVICE

/* ── slot ────────────────────────────────────────────────────────────── */

typedef struct {
    char  key[128];
    pid_t pid;
    int   write_fd;
    int   stack_index;
    int   pos_x;
    int   pos_y;
    dbus_uint32_t notif_id;
} Slot;

static Slot           slots[MAX_SLOTS];
static int            slot_count     = 0;
static int            stack_depth[3][3];
static DBusConnection *g_conn        = NULL;

static void slide_slots_down(int px, int py, int removed_index);

/* ── signal handlers ─────────────────────────────────────────────────── */

static void sigchld_handler(int sig) {
    pid_t p;
    int   i;
    (void)sig;
    while ((p = waitpid(-1, NULL, WNOHANG)) > 0) {
        for (i = 0; i < slot_count; i++) {
            int px, py, sidx;
            if (slots[i].pid != p) continue;
            close(slots[i].write_fd);
            px   = slots[i].pos_x;
            py   = slots[i].pos_y;
            sidx = slots[i].stack_index;
            slots[i] = slots[--slot_count];
            if (stack_depth[px][py] > 0) stack_depth[px][py]--;
            slide_slots_down(px, py, sidx);
            break;
        }
    }
}

static void sighup_handler(int sig) {
    DBusError err;
    int ret;
    (void)sig;
    if (!g_conn) return;
    dbus_error_init(&err);
    ret = dbus_bus_request_name(g_conn, NOTIF_SERVICE,
        DBUS_NAME_FLAG_REPLACE_EXISTING | DBUS_NAME_FLAG_DO_NOT_QUEUE, &err);
    if (dbus_error_is_set(&err)) {
        log_write("sighup: re-assert failed: %s", err.message);
        dbus_error_free(&err);
    } else {
        log_write("sighup: re-asserted %s (reply=%d)", NOTIF_SERVICE, ret);
    }
}

/* ── slot management ─────────────────────────────────────────────────── */

static void slide_slots_down(int px, int py, int removed_index) {
    int i;
    for (i = 0; i < slot_count; i++) {
        NotifMsg reposition;
        if (slots[i].pos_x != px || slots[i].pos_y != py) continue;
        if (slots[i].stack_index <= removed_index)         continue;
        slots[i].stack_index--;
        memset(&reposition, 0, sizeof(reposition));
        reposition.msg_type        = MSG_REPOSITION;
        reposition.new_stack_index = slots[i].stack_index;
        /* all other fields irrelevant for MSG_REPOSITION */
        { ssize_t _r = write(slots[i].write_fd, &reposition,
                             sizeof(reposition)); (void)_r; }
    }
}

static Slot *find_slot_by_key(const char *key) {
    int i;
    for (i = 0; i < slot_count; i++)
        if (strcmp(slots[i].key, key) == 0)
            return &slots[i];
    return NULL;
}

static Slot *find_slot_by_id(dbus_uint32_t id) {
    int i;
    for (i = 0; i < slot_count; i++)
        if (slots[i].notif_id == id)
            return &slots[i];
    return NULL;
}

static Slot *new_slot(const char *key, int px, int py,
                      Notif *initial, dbus_uint32_t notif_id) {
    int   fds[2];
    pid_t pid;
    Slot *s;

    if (slot_count >= MAX_SLOTS) {
        log_write("new_slot: MAX_SLOTS (%d) reached", MAX_SLOTS);
        return NULL;
    }
    if (pipe(fds) < 0) return NULL;

    initial->stack_index = stack_depth[px][py]++;
    initial->pos_x       = px;
    initial->pos_y       = py;

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
    s->key[sizeof(s->key) - 1] = '\0';
    s->pid         = pid;
    s->write_fd    = fds[1];
    s->stack_index = initial->stack_index;
    s->pos_x       = px;
    s->pos_y       = py;
    s->notif_id    = notif_id;
    return s;
}

/* Pack a Notif into a NotifMsg using DOI_DEFAULT for "use config value".
 * This is the ONLY place NotifMsg is constructed for MSG_UPDATE.      */
static void send_update(Slot *s, const Notif *n) {
    NotifMsg m;

    /* Start with DOI_DEFAULT in every integer field so render.c falls
     * back to config.h for anything the daemon hasn't overridden.     */
    memset(&m, 0, sizeof(m));      /* zero all strings (= "use default") */
    m.msg_type = MSG_UPDATE;

    /* integers: init ALL to DOI_DEFAULT first */
    m.border        = DOI_DEFAULT;
    m.border_radius = DOI_DEFAULT;
    m.timeout       = DOI_DEFAULT;
    m.min_width     = DOI_DEFAULT;
    m.min_height    = DOI_DEFAULT;
    m.offset_x      = DOI_DEFAULT;
    m.offset_y      = DOI_DEFAULT;
    m.show_icon     = DOI_DEFAULT;
    m.show_body     = DOI_DEFAULT;
    m.layout        = DOI_DEFAULT;
    m.show_bar      = 0;           /* 0 = no bar (safe default, not config) */
    m.bar_value     = 0;
    m.bar_width     = DOI_DEFAULT;
    m.bar_height    = DOI_DEFAULT;
    m.pos_x         = DOI_DEFAULT;
    m.pos_y         = DOI_DEFAULT;

    /* copy strings only when they are set */
#define COPY_STR(dst, src) \
    if (n->src && n->src[0]) \
        strncpy(m.dst, n->src, sizeof(m.dst) - 1)

    COPY_STR(summary,      summary);
    COPY_STR(body,         body);
    COPY_STR(icon,         icon);
    COPY_STR(bg,           bg);
    COPY_STR(fg,           fg);
    COPY_STR(border_color, border_color);
    COPY_STR(bar_fg,       bar_fg);
    COPY_STR(bar_bg,       bar_bg);

#undef COPY_STR

    /* copy integers only when the Notif has a real override
     * (sentinel in Notif is also DOI_DEFAULT, set in handle()) */
#define COPY_I(field) \
    if (n->field != DOI_DEFAULT) m.field = n->field

    COPY_I(border);
    COPY_I(border_radius);
    COPY_I(timeout);
    COPY_I(min_width);
    COPY_I(min_height);
    COPY_I(offset_x);
    COPY_I(offset_y);
    COPY_I(show_icon);
    COPY_I(show_body);
    COPY_I(layout);
    COPY_I(bar_width);
    COPY_I(bar_height);
    COPY_I(pos_x);
    COPY_I(pos_y);

#undef COPY_I

    /* bar is off unless explicitly requested */
    if (n->show_bar) {
        m.show_bar  = 1;
        m.bar_value = n->bar_value;
    }

    { ssize_t _r = write(s->write_fd, &m, sizeof(m)); (void)_r; }
}

static void close_slot(Slot *s) {
    NotifMsg m;
    memset(&m, 0, sizeof(m));
    m.msg_type = MSG_CLOSE;
    { ssize_t _r = write(s->write_fd, &m, sizeof(m)); (void)_r; }
}

/* ── D-Bus hint parsing ───────────────────────────────────────────────── */

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
        if (dbus_message_iter_get_arg_type(&entry) != DBUS_TYPE_VARIANT)
            goto next;

        dbus_message_iter_recurse(&entry, &var);
        type = dbus_message_iter_get_arg_type(&var);

#define READ_STR(hkey, field) \
    if (strcmp(key, hkey) == 0 && type == DBUS_TYPE_STRING) { \
        const char *v; dbus_message_iter_get_basic(&var, &v); \
        free(n->field); n->field = strdup(v); \
        n->from_module = 1; goto next; }

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
        READ_INT("x-doi-border-radius",border_radius)
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
        READ_INT("x-doi-layout",       layout)
        READ_INT("x-doi-timeout",      timeout)

        /* standard freedesktop progress hint */
        if (strcmp(key, "value") == 0 && type == DBUS_TYPE_INT32) {
            dbus_int32_t v;
            dbus_message_iter_get_basic(&var, &v);
            n->bar_value = (int)v;
            n->show_bar  = 1;
        }

#undef READ_STR
#undef READ_INT

next:
        dbus_message_iter_next(&dict);
    }
}

/* ── ignore list ─────────────────────────────────────────────────────── */

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

/* ── emit NotificationClosed signal ──────────────────────────────────── */

static void emit_closed(DBusConnection *conn,
                        dbus_uint32_t id, dbus_uint32_t reason) {
    DBusMessage *sig = dbus_message_new_signal(NOTIF_PATH, NOTIF_IFACE,
                                               "NotificationClosed");
    if (!sig) return;
    dbus_message_append_args(sig,
        DBUS_TYPE_UINT32, &id,
        DBUS_TYPE_UINT32, &reason,
        DBUS_TYPE_INVALID);
    dbus_connection_send(conn, sig, NULL);
    dbus_message_unref(sig);
}

/* ── D-Bus handler ───────────────────────────────────────────────────── */

static DBusHandlerResult handle(DBusConnection *conn,
                                DBusMessage *msg, void *data) {
    (void)data;
    if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_METHOD_CALL)
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    /* ── Notify ───────────────────────────────────────────────────── */
    if (dbus_message_is_method_call(msg, NOTIF_IFACE, "Notify")) {
        static dbus_uint32_t next_id = 1;
        DBusMessageIter args;
        DBusMessage    *reply;
        const char     *app_name = "";
        const char     *app_icon = "";
        const char     *summary  = "";
        const char     *body     = "";
        dbus_uint32_t   replace_id = 0;
        dbus_int32_t    expire     = -1;
        Notif n;

        if (!dbus_message_iter_init(msg, &args)) goto reply;

#define NEXT(T, V) \
    if (dbus_message_iter_get_arg_type(&args) == T) \
        dbus_message_iter_get_basic(&args, &V); \
    dbus_message_iter_next(&args);

        NEXT(DBUS_TYPE_STRING, app_name)
        NEXT(DBUS_TYPE_UINT32, replace_id)
        NEXT(DBUS_TYPE_STRING, app_icon)
        NEXT(DBUS_TYPE_STRING, summary)
        NEXT(DBUS_TYPE_STRING, body)
        dbus_message_iter_next(&args); /* skip actions array */

#undef NEXT

        /* Initialise ALL integer fields to DOI_DEFAULT.
         * parse_hints() will override only what the client sent.
         * send_update() will then propagate DOI_DEFAULT fields to
         * render.c, which resolves them against config.h.           */
        memset(&n, 0, sizeof(n));
        n.summary       = strdup(summary);
        n.body          = strdup(body);
        n.icon          = (app_icon[0] && app_icon[0] != '/' &&
                           strncmp(app_icon, "file:", 5) != 0)
                          ? strdup(app_icon) : strdup("");
        n.border        = DOI_DEFAULT;
        n.border_radius = DOI_DEFAULT;
        n.timeout       = DOI_DEFAULT;
        n.min_width     = DOI_DEFAULT;
        n.min_height    = DOI_DEFAULT;
        n.offset_x      = DOI_DEFAULT;
        n.offset_y      = DOI_DEFAULT;
        n.show_icon     = DOI_DEFAULT;
        n.show_body     = DOI_DEFAULT;
        n.layout        = DOI_DEFAULT;
        n.bar_width     = DOI_DEFAULT;
        n.bar_height    = DOI_DEFAULT;
        /* pos_x/pos_y: default to the global config position */
        n.pos_x         = POS_X;
        n.pos_y         = POS_Y;
        n.show_bar      = 0;
        n.bar_value     = 0;
        n.from_module   = 0;

        parse_hints(&args, &n);
        dbus_message_iter_next(&args);

        if (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_INT32) {
            dbus_message_iter_get_basic(&args, &expire);
            /* only use dbus expire if no module timeout hint was set */
            if (expire > 0 && n.timeout == DOI_DEFAULT)
                n.timeout = expire / 1000;
        }

        if (is_ignored(app_name)) {
            free(n.summary); free(n.body); free(n.icon);
            goto reply;
        }

        {
            char  key[128];
            Slot *s = NULL;
            int   px = n.pos_x;
            int   py = n.pos_y;

            if (n.from_module) {
                snprintf(key, sizeof(key), "%s|%d|%d",
                         app_name, px, py);
                s = find_slot_by_key(key);
                if (s) {
                    s->notif_id = next_id;
                    send_update(s, &n);
                } else {
                    s = new_slot(key, px, py, &n, next_id);
                    if (s) send_update(s, &n);
                }
            } else {
                if (replace_id > 0)
                    s = find_slot_by_id(replace_id);
                if (s) {
                    s->notif_id = next_id;
                    send_update(s, &n);
                } else if (STACK_LIMIT <= 0 ||
                           stack_depth[px][py] < STACK_LIMIT) {
                    snprintf(key, sizeof(key), "ext|%u", next_id);
                    s = new_slot(key, px, py, &n, next_id);
                    if (s) send_update(s, &n);
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

    /* ── GetCapabilities ──────────────────────────────────────────── */
    } else if (dbus_message_is_method_call(msg, NOTIF_IFACE,
                                           "GetCapabilities")) {
        const char *caps[] = {
            "body", "icon-static", "persistence", "x-doi-hints"
        };
        DBusMessage    *r = dbus_message_new_method_return(msg);
        DBusMessageIter a, arr;
        int i;
        dbus_message_iter_init_append(r, &a);
        dbus_message_iter_open_container(&a, DBUS_TYPE_ARRAY,
            DBUS_TYPE_STRING_AS_STRING, &arr);
        for (i = 0; i < 4; i++)
            dbus_message_iter_append_basic(&arr,
                DBUS_TYPE_STRING, &caps[i]);
        dbus_message_iter_close_container(&a, &arr);
        dbus_connection_send(conn, r, NULL);
        dbus_message_unref(r);
        return DBUS_HANDLER_RESULT_HANDLED;

    /* ── GetServerInformation ─────────────────────────────────────── */
    } else if (dbus_message_is_method_call(msg, NOTIF_IFACE,
                                           "GetServerInformation")) {
        const char *name = "doid", *vendor = "doi",
                   *ver  = "2.1", *spec   = "1.2";
        DBusMessage *r = dbus_message_new_method_return(msg);
        dbus_message_append_args(r,
            DBUS_TYPE_STRING, &name,   DBUS_TYPE_STRING, &vendor,
            DBUS_TYPE_STRING, &ver,    DBUS_TYPE_STRING, &spec,
            DBUS_TYPE_INVALID);
        dbus_connection_send(conn, r, NULL);
        dbus_message_unref(r);
        return DBUS_HANDLER_RESULT_HANDLED;

    /* ── CloseNotification ────────────────────────────────────────── */
    } else if (dbus_message_is_method_call(msg, NOTIF_IFACE,
                                           "CloseNotification")) {
        dbus_uint32_t id = 0;
        DBusError     err;
        DBusMessage  *r;
        Slot         *s;

        dbus_error_init(&err);
        dbus_message_get_args(msg, &err,
            DBUS_TYPE_UINT32, &id, DBUS_TYPE_INVALID);
        dbus_error_free(&err);

        s = find_slot_by_id(id);
        if (s) {
            close_slot(s);
            emit_closed(conn, id, 3);
        }

        r = dbus_message_new_method_return(msg);
        dbus_connection_send(conn, r, NULL);
        dbus_message_unref(r);
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

/* ── main ────────────────────────────────────────────────────────────── */

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

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sighup_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGHUP, &sa, NULL);

    dbus_error_init(&err);
    conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if (dbus_error_is_set(&err)) {
        log_write("dbus connect: %s", err.message);
        dbus_error_free(&err);
        return EXIT_FAILURE;
    }
    g_conn = conn;

    ret = dbus_bus_request_name(conn, NOTIF_SERVICE,
        DBUS_NAME_FLAG_REPLACE_EXISTING | DBUS_NAME_FLAG_DO_NOT_QUEUE,
        &err);
    if (dbus_error_is_set(&err)) {
        log_write("dbus request_name: %s", err.message);
        dbus_error_free(&err);
        return EXIT_FAILURE;
    }
    if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        log_write("failed to own %s (reply=%d)", NOTIF_SERVICE, ret);
        return EXIT_FAILURE;
    }

    if (!dbus_connection_add_filter(conn, handle, NULL, NULL))
        return EXIT_FAILURE;

    log_write("doid started, owning %s", NOTIF_SERVICE);

    while (dbus_connection_read_write_dispatch(conn, -1))
        ;

    return EXIT_SUCCESS;
}
