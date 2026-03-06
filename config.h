/* doi configuration - edit and recompile to apply changes */

/* --- log --- */
#define BND_LOG_FILE        ".doi/log"

/* --- font --- */
#define BND_FONT  "JetBrainsMono Nerd Font:size=11:antialias=true"

/* --- default notification appearance --- */
#define BND_BG              "#282828"
#define BND_FG              "wheat"
#define BND_MARGIN          15
#define BND_PADDING         8
#define BND_TIMEOUT         3       /* seconds, 0 = click to dismiss */
#define BND_SHOW_ICON       1       /* 1 = show icon, 0 = hide */
#define BND_SHOW_BODY       1       /* 1 = show body text below title */
#define BND_ICON_SIZE       32      /* pixels */

/* --- border (global) --- */
#define BND_BORDER          1       /* border width in px, 0 = no border */
#define BND_BORDER_COLOR    "wheat"

/* --- position ---
 * BND_POS_X: BND_LEFT / BND_CENTER / BND_RIGHT
 * BND_POS_Y: BND_TOP  / BND_MIDDLE / BND_BOTTOM
 */
#define BND_POS_X           BND_RIGHT
#define BND_POS_Y           BND_TOP
#define BND_OFFSET_X        20
#define BND_OFFSET_Y        20
#define BND_STACK_GAP       15
#define BND_STACK_HEIGHT    50  /* estimated notification height for stacking */

/* position constants - do not edit */
#define BND_LEFT            0
#define BND_CENTER          1
#define BND_RIGHT           2
#define BND_TOP             0
#define BND_MIDDLE          1
#define BND_BOTTOM          2

/* --- progress bar (volume/brightness) --- */
#define BND_BAR_WIDTH       250
#define BND_BAR_HEIGHT      3
#define BND_BAR_FG          "wheat"
#define BND_BAR_BG          "#444444"

/* --- volume module --- */
#define BND_VOLUME_ALSA     0
#define BND_VOLUME_PULSE    1
#define BND_VOLUME_BACKEND  BND_VOLUME_PULSE
#define BND_VOLUME_CARD     "default"
#define BND_VOLUME_CHANNEL  "Master"
#define BND_VOLUME_TIMEOUT  2
#define BND_VOLUME_BG       "#282828"
#define BND_VOLUME_FG       "wheat"
#define BND_VOLUME_BORDER        1
#define BND_VOLUME_BORDER_COLOR  "wheat"
#define BND_VOLUME_POS_X    BND_RIGHT
#define BND_VOLUME_POS_Y    BND_BOTTOM

/* --- brightness module --- */
#define BND_BRIGHT_SYSFS      0
#define BND_BRIGHT_XBACKLIGHT 1
#define BND_BRIGHT_BACKEND    BND_BRIGHT_SYSFS
#define BND_BRIGHT_SYSFS_PATH "/sys/class/backlight/intel_backlight"
#define BND_BRIGHT_TIMEOUT    2
#define BND_BRIGHT_BG         "#282828"
#define BND_BRIGHT_FG         "wheat"
#define BND_BRIGHT_BORDER        1
#define BND_BRIGHT_BORDER_COLOR  "wheat"
#define BND_BRIGHT_POS_X      BND_LEFT
#define BND_BRIGHT_POS_Y      BND_TOP

/* --- screenshot module --- */
#define BND_SCREENSHOT_TIMEOUT  3
#define BND_SCREENSHOT_BG       "#282828"
#define BND_SCREENSHOT_FG       "wheat"
#define BND_SCREENSHOT_BORDER        1
#define BND_SCREENSHOT_BORDER_COLOR  "wheat"
#define BND_SCREENSHOT_POS_X    BND_RIGHT
#define BND_SCREENSHOT_POS_Y    BND_TOP
#define BND_SCREENSHOT_DIR      "~/Pictures/Screenshots"

/* --- media module --- */
#define BND_MEDIA_TIMEOUT   4
#define BND_MEDIA_BG        "#282828"
#define BND_MEDIA_FG        "wheat"
#define BND_MEDIA_BORDER        1
#define BND_MEDIA_BORDER_COLOR  "wheat"
#define BND_MEDIA_POS_X     BND_LEFT
#define BND_MEDIA_POS_Y     BND_BOTTOM
#define BND_MEDIA_SHOW_ART  1

/* --- wifi module (stub - implement modules/wifi.c) --- */
#define BND_WIFI_TIMEOUT    3
#define BND_WIFI_BG         "#282828"
#define BND_WIFI_FG         "wheat"
#define BND_WIFI_BORDER        1
#define BND_WIFI_BORDER_COLOR  "wheat"
#define BND_WIFI_POS_X      BND_RIGHT
#define BND_WIFI_POS_Y      BND_TOP
