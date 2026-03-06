/* bnd configuration - edit and recompile to apply changes */

/* --- log --- */
#define DOI_LOG_FILE        ".doi/log"

/* --- font --- */
#define DOI_FONT  "JetBrains Mono:size=11:antialias=true"

/* --- default notification appearance --- */
#define DOI_BG              "#282828"
#define DOI_FG              "wheat"
#define DOI_MARGIN          15
#define DOI_PADDING         8
#define DOI_TIMEOUT         3       /* seconds, 0 = click to dismiss */
#define DOI_SHOW_ICON       1       /* 1 = show icon, 0 = hide */
#define DOI_SHOW_BODY       1       /* 1 = show body text below title */
#define DOI_ICON_SIZE       32      /* pixels */

/* --- border (global) --- */
#define DOI_BORDER          1       /* border width in px, 0 = no border */
#define DOI_BORDER_COLOR    "wheat"

/* --- position ---
 * DOI_POS_X: DOI_LEFT / DOI_CENTER / DOI_RIGHT
 * DOI_POS_Y: DOI_TOP  / DOI_MIDDLE / DOI_BOTTOM
 */
#define DOI_POS_X           DOI_RIGHT
#define DOI_POS_Y           DOI_TOP
#define DOI_OFFSET_X        20
#define DOI_OFFSET_Y        20

/* --- stacking --- */
#define DOI_STACK_GAP       15
#define DOI_STACK_HEIGHT    70  /* estimated notification height for stacking */
#define DOI_STACK_LIMIT     5   /* max visible notifications per position, 0 = unlimited */
#define DOI_STACK_OVERFLOW  "... and %d more"

/* position constants - do not edit */
#define DOI_LEFT            0
#define DOI_CENTER          1
#define DOI_RIGHT           2
#define DOI_TOP             0
#define DOI_MIDDLE          1
#define DOI_BOTTOM          2

/* --- progress bar (volume/brightness) --- */
#define DOI_BAR_WIDTH       200
#define DOI_BAR_HEIGHT      8
#define DOI_BAR_FG          "wheat"
#define DOI_BAR_BG          "#444444"

/* --- volume module --- */
#define DOI_VOLUME_ALSA     0
#define DOI_VOLUME_PULSE    1
#define DOI_VOLUME_BACKEND  DOI_VOLUME_PULSE
#define DOI_VOLUME_CARD     "default"
#define DOI_VOLUME_CHANNEL  "Master"
#define DOI_VOLUME_TIMEOUT  2
#define DOI_VOLUME_BG       "#282828"
#define DOI_VOLUME_FG       "wheat"
#define DOI_VOLUME_BORDER        1
#define DOI_VOLUME_BORDER_COLOR  "wheat"
#define DOI_VOLUME_POS_X    DOI_RIGHT
#define DOI_VOLUME_POS_Y    DOI_BOTTOM

/* --- brightness module --- */
#define DOI_BRIGHT_SYSFS      0
#define DOI_BRIGHT_XBACKLIGHT 1
#define DOI_BRIGHT_BACKEND    DOI_BRIGHT_SYSFS
#define DOI_BRIGHT_SYSFS_PATH "/sys/class/backlight/intel_backlight"
#define DOI_BRIGHT_TIMEOUT    2
#define DOI_BRIGHT_BG         "#282828"
#define DOI_BRIGHT_FG         "wheat"
#define DOI_BRIGHT_BORDER        1
#define DOI_BRIGHT_BORDER_COLOR  "wheat"
#define DOI_BRIGHT_POS_X      DOI_LEFT
#define DOI_BRIGHT_POS_Y      DOI_TOP

/* --- screenshot module --- */
#define DOI_SCREENSHOT_TIMEOUT  3
#define DOI_SCREENSHOT_BG       "#282828"
#define DOI_SCREENSHOT_FG       "wheat"
#define DOI_SCREENSHOT_BORDER        1
#define DOI_SCREENSHOT_BORDER_COLOR  "wheat"
#define DOI_SCREENSHOT_POS_X    DOI_RIGHT
#define DOI_SCREENSHOT_POS_Y    DOI_TOP
#define DOI_SCREENSHOT_DIR      "~/Pictures/Screenshots"

/* --- media module --- */
#define DOI_MEDIA_TIMEOUT   4
#define DOI_MEDIA_BG        "#282828"
#define DOI_MEDIA_FG        "wheat"
#define DOI_MEDIA_BORDER        1
#define DOI_MEDIA_BORDER_COLOR  "wheat"
#define DOI_MEDIA_POS_X     DOI_LEFT
#define DOI_MEDIA_POS_Y     DOI_BOTTOM
#define DOI_MEDIA_SHOW_ART  1

/* --- wifi module (stub - implement modules/wifi.c) --- */
#define DOI_WIFI_TIMEOUT    3
#define DOI_WIFI_BG         "#282828"
#define DOI_WIFI_FG         "wheat"
#define DOI_WIFI_BORDER        1
#define DOI_WIFI_BORDER_COLOR  "wheat"
#define DOI_WIFI_POS_X      DOI_RIGHT
#define DOI_WIFI_POS_Y      DOI_TOP
