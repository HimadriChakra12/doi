#ifndef NOTIFY_H
#define NOTIFY_H

typedef struct {
        char* summary;
        char* body;
        char* app_name;
        char* icon;
        char* bg;           /* NULL = use DOI_BG */
        char* fg;           /* NULL = use DOI_FG */
        char* border_color; /* NULL = use DOI_BORDER_COLOR */
        int   border;       /* px, -1 = use DOI_BORDER */
        int   timeout;      /* seconds, 0 = use DOI_TIMEOUT */
        int   pos_x;
        int   pos_y;
        int   show_icon;    /* -1 = use config, 0 = hide, 1 = show */
        int   show_body;    /* -1 = use config, 0 = hide, 1 = show */
        int   show_bar;
        int   bar_value;    /* 0-100 */
        int   stack_index;
} Notification;

/* update file format: written by daemon, read by child via inotify/poll */
typedef struct {
        char    summary[256];
        char    body[512];
        int     bar_value;
        int     show_bar;
} NotifUpdate;

int notify(Notification* n);

#endif
