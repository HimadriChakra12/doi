#!/bin/sh
# doi-screenshot: flameshot wrapper with notification
# Usage: doi-screenshot [full|region|window]

DIR="${DOI_SCREENSHOT_DIR:-$HOME/Pictures/Screenshots}"
CMD=${1:-region}
mkdir -p "$DIR"

case "$CMD" in
        full)
                FILE="$DIR/$(date +%Y-%m-%d_%H-%M-%S).png"
                flameshot full -p "$FILE"
                ;;
        region)
                flameshot gui -p "$DIR"
                FILE=$(ls -t "$DIR"/*.png 2>/dev/null | head -1)
                ;;
        window)
                FILE="$DIR/$(date +%Y-%m-%d_%H-%M-%S).png"
                flameshot screen -p "$FILE"
                ;;
        *)
                echo "usage: doi-screenshot [full|region|window]"
                exit 1
                ;;
esac

_notify() {
        if command -v doi >/dev/null 2>&1; then
                doi -b "$2" "$1"
        else
                notify-send "$1" "$2"
        fi
}

if [ -n "$FILE" ] && [ -f "$FILE" ]; then
        _notify "Screenshot saved" "$FILE"
else
        _notify "Screenshot cancelled" ""
fi
