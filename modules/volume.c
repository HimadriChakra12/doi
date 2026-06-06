#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "module.h"

static int vol_get(void) {
        FILE *p = popen("pamixer --get-volume 2>/dev/null", "r");
        int v = 0;
        if (p) { fscanf(p, "%d", &v); pclose(p); }
        return v;
}

static int vol_muted(void) {
        FILE *p = popen("pamixer --get-mute 2>/dev/null", "r");
        char buf[8] = {0};
        if (p) { fscanf(p, "%7s", buf); pclose(p); }
        return strcmp(buf, "true") == 0;
}

static void notify(int pct, int muted) {
        char body[16];
        ModuleOpts o = {0};
        snprintf(body, sizeof(body), "%d%%", pct);
        o.summary      = muted ? "󰝟  Volume" : "󰕾  Volume";
        o.body         = body;
        o.bg           = VOL_BG;
        o.fg           = VOL_FG;
        o.border_color = VOL_BORDER_COLOR;
        o.border       = VOL_BORDER;
        o.timeout      = VOL_TIMEOUT * 1000;
        o.pos_x        = VOL_POS_X;
        o.pos_y        = VOL_POS_Y;
        o.min_width    = VOL_MIN_WIDTH;
        o.min_height   = VOL_MIN_HEIGHT;
        o.offset_x     = VOL_OFFSET_X;
        o.offset_y     = VOL_OFFSET_Y;
        o.show_bar     = !muted;
        o.bar_value    = pct;
        o.bar_width    = VOL_BAR_WIDTH;
        o.bar_height   = VOL_BAR_HEIGHT;
        o.bar_fg       = VOL_BAR_FG;
        o.bar_bg       = VOL_BAR_BG;
        doi_notify(&o);
}

int main(int argc, char **argv) {
        const char *cmd = argc >= 2 ? argv[1] : "get";
        if      (strcmp(cmd, "up")   == 0) system("pamixer -i 5");
        else if (strcmp(cmd, "down") == 0) system("pamixer -d 5");
        else if (strcmp(cmd, "mute") == 0) system("pamixer -t");
        notify(vol_get(), vol_muted());
        return EXIT_SUCCESS;
}
