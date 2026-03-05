/*
 * doi-bright: brightness control + notification
 * Usage: doi-bright [up|down|get]
 * Config: BND_BRIGHT_BACKEND, BND_BRIGHT_SYSFS_PATH in config.h
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "module.h"

static int sysfs_read_int(const char* path) {
        FILE* f = fopen(path, "r");
        int v = 0;
        if (f) { fscanf(f, "%d", &v); fclose(f); }
        return v;
}

static void sysfs_write_int(const char* path, int v) {
        FILE* f = fopen(path, "w");
        if (f) { fprintf(f, "%d\n", v); fclose(f); }
}

static int get_brightness(void) {
#if BND_BRIGHT_BACKEND == BND_BRIGHT_SYSFS
        char cur_path[256], max_path[256];
        int cur, max;
        snprintf(cur_path, sizeof(cur_path),
                "%s/brightness", BND_BRIGHT_SYSFS_PATH);
        snprintf(max_path, sizeof(max_path),
                "%s/max_brightness", BND_BRIGHT_SYSFS_PATH);
        cur = sysfs_read_int(cur_path);
        max = sysfs_read_int(max_path);
        if (max == 0) return 0;
        return (cur * 100) / max;
#else
        FILE* p = popen("xbacklight -get", "r");
        int v = 0;
        if (p) { fscanf(p, "%d", &v); pclose(p); }
        return v;
#endif
}

static void set_brightness(int delta) {
#if BND_BRIGHT_BACKEND == BND_BRIGHT_SYSFS
        char cur_path[256], max_path[256];
        int cur, max, next;
        snprintf(cur_path, sizeof(cur_path),
                "%s/brightness", BND_BRIGHT_SYSFS_PATH);
        snprintf(max_path, sizeof(max_path),
                "%s/max_brightness", BND_BRIGHT_SYSFS_PATH);
        cur = sysfs_read_int(cur_path);
        max = sysfs_read_int(max_path);
        next = cur + (delta * max / 100);
        if (next < 1)   next = 1;
        if (next > max) next = max;
        sysfs_write_int(cur_path, next);
#else
        char cmd[64];
        snprintf(cmd, sizeof(cmd), "xbacklight %s%d",
                delta >= 0 ? "+" : "", delta);
        system(cmd);
#endif
}

int main(int argc, char** argv) {
        int val;
        char body[64];
        BndNotifyOpts opts;

        if (argc < 2) {
                fprintf(stderr, "usage: doi-bright [up|down|get]\n");
                return EXIT_FAILURE;
        }

        if (strcmp(argv[1], "up") == 0) {
                set_brightness(5);
        } else if (strcmp(argv[1], "down") == 0) {
                set_brightness(-5);
        } else if (strcmp(argv[1], "get") != 0) {
                fprintf(stderr, "usage: doi-bright [up|down|get]\n");
                return EXIT_FAILURE;
        }

        val = get_brightness();
        snprintf(body, sizeof(body), "%d%%", val);

        memset(&opts, 0, sizeof(BndNotifyOpts));
        opts.summary      = "🔆  Brightness";
        opts.body         = body;
        opts.icon         = "";
        opts.bg           = BND_BRIGHT_BG;
        opts.fg           = BND_BRIGHT_FG;
        opts.border       = BND_BRIGHT_BORDER;
        opts.border_color = BND_BRIGHT_BORDER_COLOR;
        opts.timeout      = BND_BRIGHT_TIMEOUT * 1000;
        opts.pos_x        = BND_BRIGHT_POS_X;
        opts.pos_y        = BND_BRIGHT_POS_Y;
        opts.show_bar     = 1;
        opts.bar_value    = val;

        return doi_notify_opts(&opts);
}
