#ifndef DOI_MODULE_H
#define DOI_MODULE_H

#include "../config.h"

typedef struct {
        const char *summary;
        const char *body;
        const char *icon;
        const char *bg;
        const char *fg;
        const char *border_color;
        const char *bar_fg;
        const char *bar_bg;
        int   border;        /* -1 = use config default */
        int   timeout;       /* milliseconds */
        int   pos_x;
        int   pos_y;
        int   min_width;
        int   min_height;
        int   offset_x;
        int   offset_y;
        int   show_bar;
        int   bar_value;     /* 0–100 */
        int   bar_width;
        int   bar_height;
} ModuleOpts;

int doi_notify(const ModuleOpts *opts);

#endif
