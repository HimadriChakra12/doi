/*
 * doi module interface
 *
 * Each module fills a BndNotifyOpts struct and calls doi_notify_opts().
 * Options are packed into DBus hints and read by doid, which passes
 * them into the Notification struct for notify().
 */

#ifndef MODULE_H
#define MODULE_H

#include <dbus/dbus.h>
#include "../config.h"

typedef struct {
        const char* summary;
        const char* body;
        const char* icon;
        const char* bg;
        const char* fg;
        const char* border_color;
        int border;     /* px, -1 = use global */
        int timeout;    /* ms */
        int pos_x;      /* BND_LEFT / BND_CENTER / BND_RIGHT */
        int pos_y;      /* BND_TOP  / BND_MIDDLE / BND_BOTTOM */
        int show_bar;
        int bar_value;  /* 0-100 */
} BndNotifyOpts;

int doi_notify_opts(const BndNotifyOpts* opts);

/* convenience wrapper - uses global defaults for pos/color */
int doi_notify(const char* summary, const char* body,
               const char* icon, int timeout_ms);

#endif
