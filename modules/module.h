#ifndef DOI_MODULE_H
#define DOI_MODULE_H

#include "../config.h"

/* Options passed by a module when sending a notification.
 * Unset string fields = NULL, unset int fields = -1 (use config default). */
typedef struct {
        const char* summary;
        const char* body;
        const char* icon;        /* emoji/text drawn before summary         */
        const char* bg;
        const char* fg;
        const char* border_color;
        const char* bar_fg;
        const char* bar_bg;
        int   border;            /* -1 = use config                         */
        int   timeout;           /* ms                                      */
        int   pos_x;
        int   pos_y;
        int   min_width;         /* -1 = use config                         */
        int   show_bar;
        int   bar_value;         /* 0–100                                   */
        int   bar_width;         /* -1 = use config                         */
        int   bar_height;        /* -1 = use config                         */
        int   min_height;        /* -1 = no minimum                         */
        int   offset_x;          /* -1 = use DOI_OFFSET_X                   */
        int   offset_y;          /* -1 = use DOI_OFFSET_Y                   */
} DoiOpts;

/* Send a notification via DBus. Returns 0 on success. */
int doi_notify(const DoiOpts* opts);

#endif
