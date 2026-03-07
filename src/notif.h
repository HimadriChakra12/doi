#ifndef DOI_NOTIF_H
#define DOI_NOTIF_H

typedef struct {
        char* summary;
        char* body;
        char* icon;
        char* bg;
        char* fg;
        char* border_color;
        int   border;
        int   timeout;
        int   pos_x;
        int   pos_y;
        int   min_width;
        int   min_height;   /* -1 = no minimum                             */
        int   offset_x;     /* -1 = use DOI_OFFSET_X                       */
        int   offset_y;     /* -1 = use DOI_OFFSET_Y                       */
        int   show_icon;
        int   show_body;
        int   show_bar;
        int   bar_value;
        int   bar_width;
        int   bar_height;
        char* bar_fg;
        char* bar_bg;
        int   stack_index;
} Notif;

/* Written to the render child's stdin pipe to trigger show/update */
typedef struct {
        char summary[256];
        char body[512];
        char icon[64];
        char bg[32];
        char fg[32];
        char border_color[32];
        char bar_fg[32];
        char bar_bg[32];
        int  border;
        int  timeout;
        int  min_width;
        int  min_height;    /* -1 = no minimum                             */
        int  offset_x;      /* -1 = use DOI_OFFSET_X                       */
        int  offset_y;      /* -1 = use DOI_OFFSET_Y                       */
        int  show_icon;
        int  show_body;
        int  show_bar;
        int  bar_value;
        int  bar_width;
        int  bar_height;
} NotifUpdate;

void render_loop(int read_fd, Notif* initial);

#endif
