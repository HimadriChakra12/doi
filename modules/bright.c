#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "module.h"

static int sysfs_read(const char *path) {
        FILE *f = fopen(path, "r");
        int v = 0;
        if (f) { fscanf(f, "%d", &v); fclose(f); }
        return v;
}

static void sysfs_write(const char *path, int v) {
        FILE *f = fopen(path, "w");
        if (f) { fprintf(f, "%d\n", v); fclose(f); }
}

static int bright_get(void) {
#if BRIGHT_BACKEND == 0
        char cur_path[256], max_path[256];
        int cur, max;
        snprintf(cur_path, sizeof(cur_path), "%s/brightness",     BRIGHT_SYSFS_PATH);
        snprintf(max_path, sizeof(max_path), "%s/max_brightness", BRIGHT_SYSFS_PATH);
        cur = sysfs_read(cur_path);
        max = sysfs_read(max_path);
        return max ? cur * 100 / max : 0;
#else
        FILE *p = popen("xbacklight -get", "r");
        int v = 0;
        if (p) { fscanf(p, "%d", &v); pclose(p); }
        return v;
#endif
}

static void bright_adjust(int delta) {
#if BRIGHT_BACKEND == 0
        char cur_path[256], max_path[256];
        int cur, max, next;
        snprintf(cur_path, sizeof(cur_path), "%s/brightness",     BRIGHT_SYSFS_PATH);
        snprintf(max_path, sizeof(max_path), "%s/max_brightness", BRIGHT_SYSFS_PATH);
        cur  = sysfs_read(cur_path);
        max  = sysfs_read(max_path);
        next = cur + delta * max / 100;
        if (next < 1)   next = 1;
        if (next > max) next = max;
        sysfs_write(cur_path, next);
#else
        char cmd[64];
        snprintf(cmd, sizeof(cmd), "xbacklight %s%d", delta >= 0 ? "+" : "", delta);
        system(cmd);
#endif
}

static void notify(int pct) {
        char body[16];
        ModuleOpts o = {0};
        snprintf(body, sizeof(body), "%d%%", pct);
        o.summary      = "󰃞  Brightness";
        o.body         = body;
        o.bg           = BRIGHT_BG;
        o.fg           = BRIGHT_FG;
        o.border_color = BRIGHT_BORDER_COLOR;
        o.border       = BRIGHT_BORDER;
        o.timeout      = BRIGHT_TIMEOUT * 1000;
        o.pos_x        = BRIGHT_POS_X;
        o.pos_y        = BRIGHT_POS_Y;
        o.min_width    = BRIGHT_MIN_WIDTH;
        o.min_height   = BRIGHT_MIN_HEIGHT;
        o.offset_x     = BRIGHT_OFFSET_X;
        o.offset_y     = BRIGHT_OFFSET_Y;
        o.show_bar     = 1;
        o.bar_value    = pct;
        o.bar_width    = BRIGHT_BAR_WIDTH;
        o.bar_height   = BRIGHT_BAR_HEIGHT;
        o.bar_fg       = BRIGHT_BAR_FG;
        o.bar_bg       = BRIGHT_BAR_BG;
        doi_notify(&o);
}

int main(int argc, char **argv) {
        if (argc < 2) { fprintf(stderr, "usage: doi-bright [get|up|down]\n"); return EXIT_FAILURE; }
        if      (strcmp(argv[1], "up")   == 0) bright_adjust(5);
        else if (strcmp(argv[1], "down") == 0) bright_adjust(-5);
        else if (strcmp(argv[1], "get")  != 0) {
                fprintf(stderr, "usage: doi-bright [get|up|down]\n");
                return EXIT_FAILURE;
        }
        notify(bright_get());
        return EXIT_SUCCESS;
}
