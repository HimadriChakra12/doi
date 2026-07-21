#ifndef DOI_NOTIF_H
#define DOI_NOTIF_H

/* Sentinel value meaning "use the compiled-in config.h default".
 * Used in both Notif (daemon side) and NotifMsg (pipe protocol).
 * -999 is chosen to be visually obvious and never a valid geometry value. */
#define DOI_DEFAULT (-999)

/* Notif: heap-allocated notification parsed from a D-Bus Notify call.
 * Integer fields use DOI_DEFAULT to mean "no override; use config.h".
 * String fields use NULL or "" to mean the same.                      */
typedef struct {
    char *summary;
    char *body;
    char *icon;
    char *bg;
    char *fg;
    char *border_color;
    char *bar_fg;
    char *bar_bg;
    int   border;        /* DOI_DEFAULT or px thickness                */
    int   border_radius; /* DOI_DEFAULT or px radius                   */
    int   timeout;       /* DOI_DEFAULT or seconds (0 = manual)        */
    int   pos_x;         /* LEFT / CENTER / RIGHT  (never DOI_DEFAULT) */
    int   pos_y;         /* TOP / MIDDLE / BOTTOM  (never DOI_DEFAULT) */
    int   min_width;     /* DOI_DEFAULT or px                          */
    int   min_height;    /* DOI_DEFAULT or px                          */
    int   offset_x;      /* DOI_DEFAULT or px from screen edge         */
    int   offset_y;      /* DOI_DEFAULT or px from screen edge         */
    int   show_icon;     /* DOI_DEFAULT, 0, or 1                       */
    int   show_body;     /* DOI_DEFAULT, 0, or 1                       */
    int   show_bar;      /* 0 or 1 (no DOI_DEFAULT; default is no bar) */
    int   bar_value;     /* 0-100                                      */
    int   bar_width;     /* DOI_DEFAULT or px                          */
    int   bar_height;    /* DOI_DEFAULT or px                          */
    int   stack_index;   /* set by daemon when forking render child     */
    int   from_module;   /* 1 = x-doi-* hints present                  */
    int   layout;        /* DOI_DEFAULT, 0 = brick, 1 = block          */
} Notif;

/* IPC message types sent daemon → render child */
#define MSG_UPDATE     1  /* new content; triggers a full repaint       */
#define MSG_REPOSITION 2  /* stack slot changed; move window            */
#define MSG_CLOSE      3  /* close and exit immediately                 */

/* NotifMsg: fixed-size pipe packet.  One write = one read = one event.
 *
 * Integer fields: DOI_DEFAULT means "use config.h default in render.c".
 * String fields:  ""          means "use config.h default in render.c".
 *
 * This matters because 0 is a valid value for BORDER, BORDER_RADIUS,
 * TIMEOUT, etc. — memset to zero is NOT a safe initialiser.           */
typedef struct {
    int  msg_type;        /* MSG_UPDATE / MSG_REPOSITION / MSG_CLOSE    */

    /* content (MSG_UPDATE) */
    char summary[256];
    char body[512];
    char icon[64];

    /* colours ("" = use config.h default) */
    char bg[32];
    char fg[32];
    char border_color[32];
    char bar_fg[32];
    char bar_bg[32];

    /* geometry (DOI_DEFAULT = use config.h default) */
    int  border;
    int  border_radius;
    int  timeout;
    int  min_width;
    int  min_height;
    int  offset_x;
    int  offset_y;
    int  pos_x;          /* DOI_DEFAULT = keep current, else LEFT/CENTER/RIGHT */
    int  pos_y;          /* DOI_DEFAULT = keep current, else TOP/MIDDLE/BOTTOM */

    /* display flags (DOI_DEFAULT = use config.h default) */
    int  show_icon;
    int  show_body;
    int  layout;

    /* progress bar */
    int  show_bar;       /* 0 = hidden (safe default, not config-driven) */
    int  bar_value;      /* 0-100                                        */
    int  bar_width;
    int  bar_height;

    /* MSG_REPOSITION only */
    int  new_stack_index;
} NotifMsg;

void render_loop(int read_fd, Notif *initial);

#endif /* DOI_NOTIF_H */
