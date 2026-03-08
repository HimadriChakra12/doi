/* doi notification daemon — user configuration
 * Edit this file to customise doi's appearance and behaviour.
 *
 * Position constants:  DOI_LEFT / DOI_CENTER / DOI_RIGHT
 *                      DOI_TOP  / DOI_MIDDLE / DOI_BOTTOM
 */
#ifndef DOI_CONFIG_H
#define DOI_CONFIG_H
#define DOI_LOG_FILE      ".doi/log"  /* relative to $HOME */
/* ── position constants (do not change these) ─────────────────────────── */
#define DOI_LEFT   0
#define DOI_CENTER 1
#define DOI_RIGHT  2
#define DOI_TOP    0
#define DOI_MIDDLE 1
#define DOI_BOTTOM 2
/* ── global defaults ───────────────────────────────────────────────────── */
#define DOI_FONT          "JetBrainsMono Nerd Font:size=9:antialias=true"
#define DOI_BG            "#282828"
#define DOI_FG            "wheat"
#define DOI_BORDER_COLOR  "wheat"
#define DOI_BORDER        1
#define DOI_PADDING       10         /* nearest int to 9.5 — must be integer */
#define DOI_MARGIN        3
#define DOI_MIN_WIDTH     260
#define DOI_MAX_WIDTH_PCT 40
#define DOI_POS_X         DOI_RIGHT
#define DOI_POS_Y         DOI_TOP
#define DOI_OFFSET_X      20
#define DOI_OFFSET_Y      20
#define DOI_TIMEOUT       5
#define DOI_SHOW_ICON     0
#define DOI_SHOW_BODY     1
/* ── progress bar ─────────────────────────────────────────────────────── */
#define DOI_BAR_HEIGHT    6
#define DOI_BAR_WIDTH     220
#define DOI_BAR_BG        "#1d2021"
#define DOI_BAR_FG        "wheat"
#define DOI_BAR_RADIUS    3
/* ── stacking ─────────────────────────────────────────────────────────── */
#define DOI_STACK_GAP      9
#define DOI_NOTIF_HEIGHT   80
#define DOI_STACK_HEIGHT   70         /* step between stacked slots (notif + gap)     */
#define DOI_STACK_LIMIT    5
#define DOI_STACK_OVERFLOW "… and %d more"
/* ── ignored apps ─────────────────────────────────────────────────────── */
#define DOI_IGNORE_APPS   ""
/* ═══════════════════════════════════════════════════════════════════════ */
/*  MODULE OVERRIDES                                                       */
/* ═══════════════════════════════════════════════════════════════════════ */
/* ── brightness ───────────────────────────────────────────────────────── */
#define DOI_BRIGHT_BG           "#282828"
#define DOI_BRIGHT_FG           "wheat"
#define DOI_BRIGHT_BORDER_COLOR "wheat"
#define DOI_BRIGHT_BORDER       2
#define DOI_BRIGHT_TIMEOUT      3
#define DOI_BRIGHT_POS_X        DOI_CENTER
#define DOI_BRIGHT_POS_Y        DOI_BOTTOM
#define DOI_BRIGHT_MIN_WIDTH    250
#define DOI_BRIGHT_BAR_WIDTH    250
#define DOI_BRIGHT_BAR_HEIGHT   2
#define DOI_BRIGHT_BAR_BG       "#1d2021"
#define DOI_BRIGHT_BAR_FG       "wheat"
#define DOI_BRIGHT_MIN_HEIGHT   6
#define DOI_BRIGHT_OFFSET_X     20
#define DOI_BRIGHT_OFFSET_Y     30
#define DOI_BRIGHT_SYSFS        0
#define DOI_BRIGHT_XBACKLIGHT   1
#define DOI_BRIGHT_BACKEND      DOI_BRIGHT_SYSFS
#define DOI_BRIGHT_SYSFS_PATH   "/sys/class/backlight/intel_backlight"
/* ── volume ───────────────────────────────────────────────────────────── */
#define DOI_VOLUME_BG           "#282828"
#define DOI_VOLUME_FG           "wheat"
#define DOI_VOLUME_BORDER_COLOR "wheat"
#define DOI_VOLUME_BORDER       2
#define DOI_VOLUME_TIMEOUT      3
#define DOI_VOLUME_POS_X        DOI_RIGHT
#define DOI_VOLUME_POS_Y        DOI_BOTTOM
#define DOI_VOLUME_MIN_WIDTH    200
#define DOI_VOLUME_BAR_WIDTH    180
#define DOI_VOLUME_BAR_HEIGHT   6
#define DOI_VOLUME_BAR_BG       "#1d2021"
#define DOI_VOLUME_BAR_FG       "wheat"
#define DOI_VOLUME_MIN_HEIGHT   60
#define DOI_VOLUME_OFFSET_X     20
#define DOI_VOLUME_OFFSET_Y     20
/* ── media ────────────────────────────────────────────────────────────── */
#define DOI_MEDIA_BG            "#282828"
#define DOI_MEDIA_FG            "wheat"
#define DOI_MEDIA_BORDER_COLOR  "wheat"
#define DOI_MEDIA_BORDER        2
#define DOI_MEDIA_TIMEOUT       4
#define DOI_MEDIA_POS_X         DOI_LEFT
#define DOI_MEDIA_POS_Y         DOI_BOTTOM
#define DOI_MEDIA_MIN_WIDTH     240
#define DOI_MEDIA_MIN_HEIGHT    60
#define DOI_MEDIA_OFFSET_X      20
#define DOI_MEDIA_OFFSET_Y      20
/* ── screenshot ───────────────────────────────────────────────────────── */
#define DOI_SCREENSHOT_BG           "#282828"
#define DOI_SCREENSHOT_FG           "wheat"
#define DOI_SCREENSHOT_BORDER_COLOR "wheat"
#define DOI_SCREENSHOT_BORDER       2
#define DOI_SCREENSHOT_TIMEOUT      4
#define DOI_SCREENSHOT_POS_X        DOI_RIGHT
#define DOI_SCREENSHOT_POS_Y        DOI_TOP
#define DOI_SCREENSHOT_DIR          "~/Pictures/Screenshots"
#define DOI_SCREENSHOT_MIN_HEIGHT   50
#define DOI_SCREENSHOT_OFFSET_X     20
#define DOI_SCREENSHOT_OFFSET_Y     20
/* ── wifi ─────────────────────────────────────────────────────────────── */
#define DOI_WIFI_BG             "#282828"
#define DOI_WIFI_FG             "wheat"
#define DOI_WIFI_BORDER_COLOR   "wheat"
#define DOI_WIFI_BORDER         2
#define DOI_WIFI_TIMEOUT        4
#define DOI_WIFI_POS_X          DOI_RIGHT
#define DOI_WIFI_POS_Y          DOI_TOP
#define DOI_WIFI_MIN_HEIGHT     50
#define DOI_WIFI_OFFSET_X       20
#define DOI_WIFI_OFFSET_Y       20
#endif /* DOI_CONFIG_H */
