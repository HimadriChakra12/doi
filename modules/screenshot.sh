#!/bin/sh
# doi-screenshot: flameshot wrapper with notification
# Usage: doi-screenshot [full|region|window]
# Config: BND_SCREENSHOT_DIR in config.h (set here for shell use)

DIR="${BND_SCREENSHOT_DIR:-$HOME/Pictures/Screenshots}"
CMD=${1:-region}
ICON="📷"

mkdir -p "$DIR"

case "$CMD" in
        full)
                FILE="$DIR/$(date +%Y%m%d_%H%M%S).png"
                flameshot full -p "$FILE"
                ;;
        region)
                flameshot gui -p "$DIR"
                FILE=$(ls -t "$DIR"/*.png 2>/dev/null | head -1)
                ;;
        window)
                FILE="$DIR/$(date +%Y%m%d_%H%M%S).png"
                flameshot screen -p "$FILE"
                ;;
        *)
                echo "usage: doi-screenshot [full|region|window]"
                exit 1
                ;;
esac

if [ -n "$FILE" ] && [ -f "$FILE" ]; then
        FNAME=$(basename "$FILE")
        if command -v doi >/dev/null 2>&1; then
                doi -b "$FILE" "$ICON  Screenshot saved"
        else
                notify-send "$ICON  Screenshot saved" "$FILE"
        fi
else
        if command -v doi >/dev/null 2>&1; then
                doi "📷  Screenshot cancelled"
        else
                notify-send "📷  Screenshot cancelled"
        fi
fi
