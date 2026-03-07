/*
 * doi-volume — volume notification module
 * Usage: doi-volume [up|down|mute|get]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "module.h"

static int get_vol(void) {
        FILE* p = popen("pamixer --get-volume 2>/dev/null", "r");
        int v = 0;
        if (p) { fscanf(p, "%d", &v); pclose(p); }
        return v;
}

static int get_muted(void) {
        FILE* p = popen("pamixer --get-mute 2>/dev/null", "r");
        char buf[8] = {0};
        if (p) { fscanf(p, "%7s", buf); pclose(p); }
        return strcmp(buf, "true") == 0;
}

static void notify(int pct, int muted) {
        char body[32];
        snprintf(body, sizeof(body), "%d%%", pct);
        DoiOpts o = {0};
        o.summary      = muted ? "󰝟  Volume" : "󰕾  Volume";
        o.body         = body;
        o.bg           = DOI_VOLUME_BG;
        o.fg           = DOI_VOLUME_FG;
        o.border_color = DOI_VOLUME_BORDER_COLOR;
        o.border       = DOI_VOLUME_BORDER;
        o.timeout      = DOI_VOLUME_TIMEOUT * 1000;
        o.pos_x        = DOI_VOLUME_POS_X;
        o.pos_y        = DOI_VOLUME_POS_Y;
        o.min_width    = DOI_VOLUME_MIN_WIDTH;
        o.show_bar     = !muted;
        o.bar_value    = pct;
        o.bar_width    = DOI_VOLUME_BAR_WIDTH;
        o.bar_height   = DOI_VOLUME_BAR_HEIGHT;
        o.min_height   = DOI_VOLUME_MIN_HEIGHT;
        o.offset_x     = DOI_VOLUME_OFFSET_X;
        o.offset_y     = DOI_VOLUME_OFFSET_Y;
        o.bar_fg       = DOI_VOLUME_BAR_FG;
        o.bar_bg       = DOI_VOLUME_BAR_BG;
        doi_notify(&o);
}

int main(int argc, char** argv) {
        const char* cmd = argc >= 2 ? argv[1] : "get";
        if (strcmp(cmd, "up") == 0)
                system("pamixer -i 5");
        else if (strcmp(cmd, "down") == 0)
                system("pamixer -d 5");
        else if (strcmp(cmd, "mute") == 0)
                system("pamixer -t");
        notify(get_vol(), get_muted());
        return EXIT_SUCCESS;
}
