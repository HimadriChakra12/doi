#ifndef NOTIFY_H
#define NOTIFY_H

typedef struct {
        char* summary;
        char* body;
        char* app_name;
        char* icon;
        char* bg;           /* NULL = use BND_BG */
        char* fg;           /* NULL = use BND_FG */
        char* border_color; /* NULL = use BND_BORDER_COLOR */
        int   border;       /* px, -1 = use BND_BORDER */
        int   timeout;      /* seconds, 0 = use BND_TIMEOUT */
        int   pos_x;        /* BND_LEFT / BND_CENTER / BND_RIGHT */
        int   pos_y;        /* BND_TOP  / BND_MIDDLE / BND_BOTTOM */
        int   show_icon;    /* -1 = use config, 0 = hide, 1 = show */
        int   show_body;    /* -1 = use config, 0 = hide, 1 = show */
        int   show_bar;
        int   bar_value;    /* 0-100 */
} Notification;

int notify(Notification* n);

#endif
