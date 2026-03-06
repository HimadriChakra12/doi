/*
 * doi-bright: brightness notification
 * Usage:
 *   doi-bright get          - read brightness and notify
 *   doi-bright set PERCENT  - notify with given percent (no sysfs read)
 *   doi-bright up           - increase by 5% then notify
 *   doi-bright down         - decrease by 5% then notify
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
#if DOI_BRIGHT_BACKEND == DOI_BRIGHT_SYSFS
        char cur_path[256], max_path[256];
        int cur, max;
        snprintf(cur_path, sizeof(cur_path),
                "%s/brightness", DOI_BRIGHT_SYSFS_PATH);
        snprintf(max_path, sizeof(max_path),
                "%s/max_brightness", DOI_BRIGHT_SYSFS_PATH);
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

static void adjust_brightness(int delta) {
#if DOI_BRIGHT_BACKEND == DOI_BRIGHT_SYSFS
        char cur_path[256], max_path[256];
        int cur, max, next;
        snprintf(cur_path, sizeof(cur_path),
                "%s/brightness", DOI_BRIGHT_SYSFS_PATH);
        snprintf(max_path, sizeof(max_path),
                "%s/max_brightness", DOI_BRIGHT_SYSFS_PATH);
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

static void send_notify(int val) {
        char body[64];
        BndNotifyOpts opts;

        snprintf(body, sizeof(body), "%d%%", val);

        memset(&opts, 0, sizeof(BndNotifyOpts));
        opts.summary      = "🔆  Brightness";
        opts.body         = body;
        opts.icon         = "";
        opts.bg           = DOI_BRIGHT_BG;
        opts.fg           = DOI_BRIGHT_FG;
        opts.border       = DOI_BRIGHT_BORDER;
        opts.border_color = DOI_BRIGHT_BORDER_COLOR;
        opts.timeout      = DOI_BRIGHT_TIMEOUT * 1000;
        opts.pos_x        = DOI_BRIGHT_POS_X;
        opts.pos_y        = DOI_BRIGHT_POS_Y;
        opts.show_bar     = 1;
        opts.bar_value    = val;

        doi_notify_opts(&opts);
}

int main(int argc, char** argv) {
        if (argc < 2) {
                fprintf(stderr,
                        "usage: doi-bright [get|set PERCENT|up|down]\n");
                return EXIT_FAILURE;
        }

        if (strcmp(argv[1], "set") == 0) {
                /* fast path: percent provided by caller, no sysfs read */
                int val = (argc >= 3) ? atoi(argv[2]) : 0;
                if (val < 0)   val = 0;
                if (val > 100) val = 100;
                send_notify(val);
        } else if (strcmp(argv[1], "get") == 0) {
                send_notify(get_brightness());
        } else if (strcmp(argv[1], "up") == 0) {
                adjust_brightness(5);
                send_notify(get_brightness());
        } else if (strcmp(argv[1], "down") == 0) {
                adjust_brightness(-5);
                send_notify(get_brightness());
        } else {
                fprintf(stderr,
                        "usage: doi-bright [get|set PERCENT|up|down]\n");
                return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
}
