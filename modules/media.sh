#!/bin/sh
# doi-media.sh: MPRIS media control shell fallback
# Usage: doi-media.sh [play|pause|toggle|next|prev|stop|info]
# Requires: playerctl, doi (or notify-send)

CMD=${1:-info}
ICON="🎵"

case "$CMD" in
        play)    playerctl play   ;;
        pause)   playerctl pause  ;;
        toggle)  playerctl play-pause ;;
        next)    playerctl next;  ICON="⏭" ;;
        prev)    playerctl previous; ICON="⏮" ;;
        stop)    playerctl stop;  ICON="⏹" ;;
        info)    ;;
        *) echo "usage: doi-media.sh [play|pause|toggle|next|prev|stop|info]"; exit 1 ;;
esac

TITLE=$(playerctl metadata xesam:title  2>/dev/null || echo "Unknown")
ARTIST=$(playerctl metadata xesam:artist 2>/dev/null || echo "")
ALBUM=$(playerctl metadata xesam:album  2>/dev/null || echo "")

SUMMARY="$ICON  $TITLE"
BODY=""
[ -n "$ARTIST" ] && BODY="$ARTIST"
[ -n "$ARTIST" ] && [ -n "$ALBUM" ] && BODY="$BODY — $ALBUM"

# try doi first, fall back to notify-send
if command -v doi >/dev/null 2>&1; then
        doi -b "$BODY" "$SUMMARY"
else
        notify-send "$SUMMARY" "$BODY"
fi
