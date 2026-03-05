/*
 * doi-volume: volume control + notification
 * Usage: doi-volume [up|down|mute|get]
 * Config: BND_VOLUME_BACKEND, BND_VOLUME_CARD, BND_VOLUME_CHANNEL in config.h
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "module.h"

#if BND_VOLUME_BACKEND == BND_VOLUME_ALSA
#include <alsa/asoundlib.h>

static int get_volume(void) {
        snd_mixer_t* handle;
        snd_mixer_elem_t* elem;
        snd_mixer_selem_id_t* sid;
        long vol, min, max;

        snd_mixer_open(&handle, 0);
        snd_mixer_attach(handle, BND_VOLUME_CARD);
        snd_mixer_selem_register(handle, NULL, NULL);
        snd_mixer_load(handle);

        snd_mixer_selem_id_alloca(&sid);
        snd_mixer_selem_id_set_index(sid, 0);
        snd_mixer_selem_id_set_name(sid, BND_VOLUME_CHANNEL);
        elem = snd_mixer_find_selem(handle, sid);

        snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
        snd_mixer_selem_get_playback_volume(elem,
                SND_MIXER_SCHN_FRONT_LEFT, &vol);
        snd_mixer_close(handle);

        return (int)(((vol - min) * 100) / (max - min));
}

static void set_volume(int delta) {
        char cmd[128];
        snprintf(cmd, sizeof(cmd),
                "amixer -q set %s %d%%%s",
                BND_VOLUME_CHANNEL,
                abs(delta),
                delta >= 0 ? "+" : "-");
        system(cmd);
}

static void toggle_mute(void) {
        char cmd[128];
        snprintf(cmd, sizeof(cmd),
                "amixer -q set %s toggle", BND_VOLUME_CHANNEL);
        system(cmd);
}

#else /* PulseAudio */

static int get_volume(void) {
        FILE* p = popen("pactl get-sink-volume @DEFAULT_SINK@ "
                        "| grep -oP '\\d+(?=%)' | head -1", "r");
        int vol = 0;
        if (p) { fscanf(p, "%d", &vol); pclose(p); }
        return vol;
}

static void set_volume(int delta) {
        char cmd[128];
        snprintf(cmd, sizeof(cmd),
                "pactl set-sink-volume @DEFAULT_SINK@ %s%d%%",
                delta >= 0 ? "+" : "-", abs(delta));
        system(cmd);
}

static void toggle_mute(void) {
        system("pactl set-sink-mute @DEFAULT_SINK@ toggle");
}

#endif /* backend */

int main(int argc, char** argv) {
        int vol;
        char body[64];
        char summary[64];
        const char* icon = "🔊";
        BndNotifyOpts opts;

        if (argc < 2) {
                fprintf(stderr, "usage: doi-volume [up|down|mute|get]\n");
                return EXIT_FAILURE;
        }

        if (strcmp(argv[1], "up") == 0) {
                set_volume(5);
                icon = "🔊";
        } else if (strcmp(argv[1], "down") == 0) {
                set_volume(-5);
                icon = "🔉";
        } else if (strcmp(argv[1], "mute") == 0) {
                toggle_mute();
                icon = "🔇";
        } else if (strcmp(argv[1], "get") != 0) {
                fprintf(stderr, "usage: doi-volume [up|down|mute|get]\n");
                return EXIT_FAILURE;
        }

        vol = get_volume();
        snprintf(summary, sizeof(summary), "%s  Volume", icon);
        snprintf(body,    sizeof(body),    "%d%%", vol);

        memset(&opts, 0, sizeof(BndNotifyOpts));
        opts.summary      = summary;
        opts.body         = body;
        opts.icon         = "";
        opts.bg           = BND_VOLUME_BG;
        opts.fg           = BND_VOLUME_FG;
        opts.border       = BND_VOLUME_BORDER;
        opts.border_color = BND_VOLUME_BORDER_COLOR;
        opts.timeout      = BND_VOLUME_TIMEOUT * 1000;
        opts.pos_x        = BND_VOLUME_POS_X;
        opts.pos_y        = BND_VOLUME_POS_Y;
        opts.show_bar     = 1;
        opts.bar_value    = vol;

        return doi_notify_opts(&opts);
}
