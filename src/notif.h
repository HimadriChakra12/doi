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
        int   border_radius;
        int   timeout;
        int   pos_x;
        int   pos_y;
        int   min_width;
        int   min_height;
        int   offset_x;
        int   offset_y;
        int   show_icon;
        int   show_body;
        int   show_bar;
        int   bar_value;
        int   bar_width;
        int   bar_height;
        char* bar_fg;
        char* bar_bg;
        int   stack_index;
        int   doi_hints;
        int   layout;         /* 0=brick (auto width), 1=block (fixed width) */
} Notif;

/* pipe message types */
#define DOI_MSG_UPDATE     1
#define DOI_MSG_REPOSITION 2

typedef struct {
        int  msg_type;
        char summary[256];
        char body[512];
        char icon[64];
        char bg[32];
        char fg[32];
        char border_color[32];
        char bar_fg[32];
        char bar_bg[32];
        int  border;
        int  border_radius;
        int  timeout;
        int  min_width;
        int  min_height;
        int  offset_x;
        int  offset_y;
        int  show_icon;
        int  show_body;
        int  show_bar;
        int  bar_value;
        int  bar_width;
        int  bar_height;
        int  layout;
        int  new_stack_idx;   /* used by DOI_MSG_REPOSITION */
} NotifUpdate;

void render_loop(int read_fd, Notif* initial);

#endif
