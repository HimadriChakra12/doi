/*
 * doi-bright — brightness notification module
 * Usage:
 *   doi-bright get
 *   doi-bright set PERCENT
 *   doi-bright up
 *   doi-bright down
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "module.h"

static int sysfs_read(const char* path) {
        FILE* f = fopen(path, "r");
        int v = 0;
        if (f) { fscanf(f, "%d", &v); fclose(f); }
        return v;
}

static void sysfs_write(const char* path, int v) {
        FILE* f = fopen(path, "w");
        if (f) { fprintf(f, "%d\n", v); fclose(f); }
}

static int get_pct(void) {
#if DOI_BRIGHT_BACKEND == DOI_BRIGHT_SYSFS
        char cur[256], max[256];
        snprintf(cur, sizeof(cur), "%s/brightness",     DOI_BRIGHT_SYSFS_PATH);
        snprintf(max, sizeof(max), "%s/max_brightness", DOI_BRIGHT_SYSFS_PATH);
        int c = sysfs_read(cur), m = sysfs_read(max);
        return m ? c * 100 / m : 0;
#else
        FILE* p = popen("xbacklight -get", "r");
        int v = 0;
        if (p) { fscanf(p, "%d", &v); pclose(p); }
        return v;
#endif
}

static void adjust(int delta) {
#if DOI_BRIGHT_BACKEND == DOI_BRIGHT_SYSFS
        char cur[256], max[256];
        snprintf(cur, sizeof(cur), "%s/brightness",     DOI_BRIGHT_SYSFS_PATH);
        snprintf(max, sizeof(max), "%s/max_brightness", DOI_BRIGHT_SYSFS_PATH);
        int c = sysfs_read(cur), m = sysfs_read(max);
        int next = c + delta * m / 100;
        if (next < 1)   next = 1;
        if (next > m)   next = m;
        sysfs_write(cur, next);
#else
        char cmd[64];
        snprintf(cmd, sizeof(cmd), "xbacklight %s%d",
                delta >= 0 ? "+" : "", delta);
        system(cmd);
#endif
}

static void notify(int pct) {
        char body[32];
        snprintf(body, sizeof(body), "%d%%", pct);
        DoiOpts o = {0};
        o.summary      = "󰃞  Brightness";
        o.body         = body;
        o.bg           = DOI_BRIGHT_BG;
        o.fg           = DOI_BRIGHT_FG;
        o.border_color = DOI_BRIGHT_BORDER_COLOR;
        o.border       = DOI_BRIGHT_BORDER;
        o.timeout      = DOI_BRIGHT_TIMEOUT * 1000;
        o.pos_x        = DOI_BRIGHT_POS_X;
        o.pos_y        = DOI_BRIGHT_POS_Y;
        o.min_width    = DOI_BRIGHT_MIN_WIDTH;
        o.show_bar     = 1;
        o.bar_value    = pct;
        o.bar_width    = DOI_BRIGHT_BAR_WIDTH;
        o.bar_height   = DOI_BRIGHT_BAR_HEIGHT;
        o.min_height   = DOI_BRIGHT_MIN_HEIGHT;
        o.offset_x     = DOI_BRIGHT_OFFSET_X;
        o.offset_y     = DOI_BRIGHT_OFFSET_Y;
        o.bar_fg       = DOI_BRIGHT_BAR_FG;
        o.bar_bg       = DOI_BRIGHT_BAR_BG;
        doi_notify(&o);
}

int main(int argc, char** argv) {
        if (argc < 2) {
                fprintf(stderr, "usage: doi-bright [get|set PCT|up|down]\n");
                return EXIT_FAILURE;
        }
        if (strcmp(argv[1], "set") == 0) {
                int v = argc >= 3 ? atoi(argv[2]) : 0;
                if (v < 0) v = 0;
                if (v > 100) v = 100;
                notify(v);
        } else if (strcmp(argv[1], "get") == 0) {
                notify(get_pct());
        } else if (strcmp(argv[1], "up") == 0) {
                adjust(5); notify(get_pct());
        } else if (strcmp(argv[1], "down") == 0) {
                adjust(-5); notify(get_pct());
        } else {
                fprintf(stderr, "usage: doi-bright [get|set PCT|up|down]\n");
                return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
}
