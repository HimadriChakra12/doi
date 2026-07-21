/* doi — user configuration
 * Edit, then recompile: make && sudo make install
 *
 * All colours accept X11 named colours ("wheat", "red") or hex ("#282828").
 * Positions use LEFT / CENTER / RIGHT and TOP / MIDDLE / BOTTOM constants.
 */
#ifndef DOI_CONFIG_H
#define DOI_CONFIG_H

/* ── position constants — do not change ──────────────────────────────── */
#define LEFT   0
#define CENTER 1
#define RIGHT  2
#define TOP    0
#define MIDDLE 1
#define BOTTOM 2

/* ── font ────────────────────────────────────────────────────────────── */
/* Any Xft font pattern; antialias=true recommended for sub-pixel text. */
#define FONT  "JetBrainsMono Nerd Font:size=9:antialias=true"

/* ── palette ─────────────────────────────────────────────────────────── */
#define BG            "#282828"
#define FG            "wheat"
#define BORDER_COLOR  "wheat"

/* ── geometry ────────────────────────────────────────────────────────── */
#define BORDER        1     /* border thickness in px; 0 = no border     */
#define BORDER_RADIUS 0     /* corner radius in px; 0 = sharp corners    */
#define PADDING       8     /* horizontal padding inside content area     */
#define MARGIN        6     /* gap between border edge and content        */
#define MIN_WIDTH     260   /* minimum notification width in px           */
#define MAX_WIDTH_PCT 40    /* maximum width as % of screen width         */
#define NOTIF_HEIGHT  56    /* fixed notification height in px            */

/* ── position ────────────────────────────────────────────────────────── */
#define POS_X     LEFT  /* LEFT | CENTER | RIGHT                         */
#define POS_Y     TOP    /* TOP  | MIDDLE | BOTTOM                        */
#define OFFSET_X  20     /* px from screen edge horizontally              */
#define OFFSET_Y  20     /* px from screen edge vertically                */

/* ── behaviour ───────────────────────────────────────────────────────── */
#define TIMEOUT   5      /* auto-dismiss after N seconds; 0 = manual      */
#define SHOW_ICON 0      /* 1 = show icon glyph before summary            */
#define SHOW_BODY 1      /* 1 = show body text below summary              */

/* layout:
 *   0 = brick — width grows to fit content
 *   1 = block — always exactly MIN_WIDTH wide                            */
#define LAYOUT    0

/* ── progress bar ────────────────────────────────────────────────────── */
#define BAR_HEIGHT 6
#define BAR_WIDTH  220
#define BAR_BG     "#1d2021"
#define BAR_FG     "wheat"

/* ── stacking ────────────────────────────────────────────────────────── */
#define STACK_GAP   8   /* vertical gap between stacked notifications     */
#define STACK_LIMIT 5   /* max simultaneous notifications; 0 = no limit   */

/* ── filtering ───────────────────────────────────────────────────────── */
/* Comma-separated app names whose notifications are silently dropped.   */
#define IGNORE_APPS "flameshot"

/* ── logging ─────────────────────────────────────────────────────────── */
/* Log path relative to $HOME.                                           */
#define LOG_FILE ".doi/doi.log"

/* ════════════════════════════════════════════════════════════════════════
 * Built-in module overrides
 *
 * doi ships three modules activated via:  doi -m MODULE [VALUE]
 *
 *   doi -m vol    PERCENT      volume indicator    (bottom-right by default)
 *   doi -m bright PERCENT      brightness bar      (top-left by default)
 *   doi -m media  "ARTIST - TITLE"  now-playing    (bottom-left by default)
 *
 * Each module gets its own corner, colours, and geometry so it never
 * conflicts with regular desktop notifications.  Every macro below that
 * shares its name with a global default (e.g. VOL_BG vs BG) will use
 * that global when left equal — change only what you want to differ.
 * ════════════════════════════════════════════════════════════════════ */

/* ── volume module ───────────────────────────────────────────────────── */
#define VOL_BG           BG
#define VOL_FG           FG
#define VOL_BORDER_COLOR BORDER_COLOR
#define VOL_BORDER       BORDER
#define VOL_TIMEOUT      3
#define VOL_POS_X        RIGHT
#define VOL_POS_Y        BOTTOM
#define VOL_OFFSET_X     20
#define VOL_OFFSET_Y     20
#define VOL_MIN_WIDTH    200
#define VOL_MIN_HEIGHT   NOTIF_HEIGHT
#define VOL_BAR_WIDTH    180
#define VOL_BAR_HEIGHT   BAR_HEIGHT
#define VOL_BAR_BG       BAR_BG
#define VOL_BAR_FG       BAR_FG

/* ── brightness module ───────────────────────────────────────────────── */
#define BRIGHT_BG           BG
#define BRIGHT_FG           FG
#define BRIGHT_BORDER_COLOR BORDER_COLOR
#define BRIGHT_BORDER       BORDER
#define BRIGHT_TIMEOUT      3
#define BRIGHT_POS_X        LEFT
#define BRIGHT_POS_Y        TOP
#define BRIGHT_OFFSET_X     20
#define BRIGHT_OFFSET_Y     20
#define BRIGHT_MIN_WIDTH    250
#define BRIGHT_MIN_HEIGHT   NOTIF_HEIGHT
#define BRIGHT_BAR_WIDTH    230
#define BRIGHT_BAR_HEIGHT   4   /* thinner than default bar               */
#define BRIGHT_BAR_BG       BAR_BG
#define BRIGHT_BAR_FG       BAR_FG
/* backend: 0 = read /sys/class/backlight sysfs, 1 = call xbacklight     */
#define BRIGHT_BACKEND    0
#define BRIGHT_SYSFS_PATH "/sys/class/backlight/intel_backlight"

/* ── media module ────────────────────────────────────────────────────── */
#define MEDIA_BG           BG
#define MEDIA_FG           FG
#define MEDIA_BORDER_COLOR BORDER_COLOR
#define MEDIA_BORDER       BORDER
#define MEDIA_TIMEOUT      4
#define MEDIA_POS_X        LEFT
#define MEDIA_POS_Y        BOTTOM
#define MEDIA_OFFSET_X     20
#define MEDIA_OFFSET_Y     20
#define MEDIA_MIN_WIDTH    240
#define MEDIA_MIN_HEIGHT   NOTIF_HEIGHT

#endif /* DOI_CONFIG_H */
