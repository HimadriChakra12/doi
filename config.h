/* doi — user configuration
 * Recompile after editing: make && sudo make install
 */
#ifndef DOI_CONFIG_H
#define DOI_CONFIG_H

/* position constants (do not change) */
#define LEFT   0
#define CENTER 1
#define RIGHT  2
#define TOP    0
#define MIDDLE 1
#define BOTTOM 2

/* font */
#define FONT          "JetBrainsMono Nerd Font:size=9:antialias=true"

/* default colours */
#define BG            "#282828"
#define FG            "wheat"
#define BORDER_COLOR  "wheat"

/* geometry */
#define BORDER        2
#define BORDER_RADIUS 6
#define PADDING       8
#define MARGIN        6
#define MIN_WIDTH     260
#define MAX_WIDTH_PCT 40
#define NOTIF_HEIGHT  56

/* position */
#define POS_X         RIGHT
#define POS_Y         TOP
#define OFFSET_X      20
#define OFFSET_Y      20

/* behaviour */
#define TIMEOUT       5      /* seconds; 0 = click to dismiss */
#define SHOW_ICON     0
#define SHOW_BODY     1

/* layout: 0 = brick (width from content), 1 = block (fixed MIN_WIDTH) */
#define LAYOUT        0

/* progress bar */
#define BAR_HEIGHT    6
#define BAR_WIDTH     220
#define BAR_BG        "#1d2021"
#define BAR_FG        "wheat"

/* stacking */
#define STACK_GAP     8
#define STACK_LIMIT   5

/* comma-separated list of app names to ignore */
#define IGNORE_APPS   "flameshot"

/* log file, relative to $HOME */
#define LOG_FILE      ".doi/doi.log"

/* ── module overrides ──────────────────────────────────────────────────── */

/* volume */
#define VOL_BG           BG
#define VOL_FG           FG
#define VOL_BORDER_COLOR BORDER_COLOR
#define VOL_BORDER       BORDER
#define VOL_TIMEOUT      3
#define VOL_POS_X        RIGHT
#define VOL_POS_Y        BOTTOM
#define VOL_MIN_WIDTH    200
#define VOL_BAR_WIDTH    180
#define VOL_BAR_HEIGHT   6
#define VOL_BAR_BG       BAR_BG
#define VOL_BAR_FG       BAR_FG
#define VOL_MIN_HEIGHT   NOTIF_HEIGHT
#define VOL_OFFSET_X     20
#define VOL_OFFSET_Y     20

/* brightness */
#define BRIGHT_BG           BG
#define BRIGHT_FG           FG
#define BRIGHT_BORDER_COLOR BORDER_COLOR
#define BRIGHT_BORDER       BORDER
#define BRIGHT_TIMEOUT      3
#define BRIGHT_POS_X        LEFT
#define BRIGHT_POS_Y        TOP
#define BRIGHT_MIN_WIDTH    250
#define BRIGHT_BAR_WIDTH    230
#define BRIGHT_BAR_HEIGHT   4
#define BRIGHT_BAR_BG       BAR_BG
#define BRIGHT_BAR_FG       BAR_FG
#define BRIGHT_MIN_HEIGHT   NOTIF_HEIGHT
#define BRIGHT_OFFSET_X     20
#define BRIGHT_OFFSET_Y     20
/* backend: 0 = sysfs, 1 = xbacklight */
#define BRIGHT_BACKEND      0
#define BRIGHT_SYSFS_PATH   "/sys/class/backlight/intel_backlight"

/* media */
#define MEDIA_BG           BG
#define MEDIA_FG           FG
#define MEDIA_BORDER_COLOR BORDER_COLOR
#define MEDIA_BORDER       BORDER
#define MEDIA_TIMEOUT      4
#define MEDIA_POS_X        LEFT
#define MEDIA_POS_Y        BOTTOM
#define MEDIA_MIN_WIDTH    240
#define MEDIA_MIN_HEIGHT   NOTIF_HEIGHT
#define MEDIA_OFFSET_X     20
#define MEDIA_OFFSET_Y     20

#endif /* DOI_CONFIG_H */
