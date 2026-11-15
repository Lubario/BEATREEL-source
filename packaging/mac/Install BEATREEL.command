#!/bin/bash
# BEATREEL installer for macOS - double-click to run.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
PLUG="$HOME/Library/Audio/Plug-Ins"
DATA="$HOME/Library/Application Support/BEATREEL"

echo ""
echo "  Installing BEATREEL..."
echo ""

mkdir -p "$PLUG/VST3" "$PLUG/Components" "$DATA" "$HOME/Applications"

rm -rf "$PLUG/VST3/BEATREEL.vst3" "$PLUG/Components/BEATREEL.component" "$HOME/Applications/BEATREEL.app"
ditto "$HERE/BEATREEL.vst3"      "$PLUG/VST3/BEATREEL.vst3"           && echo "  [ok] VST3 -> $PLUG/VST3"
ditto "$HERE/BEATREEL.component" "$PLUG/Components/BEATREEL.component" && echo "  [ok] AU   -> $PLUG/Components"
ditto "$HERE/BEATREEL.app"       "$HOME/Applications/BEATREEL.app"    && echo "  [ok] App  -> ~/Applications"

if [ -f "$HERE/ffmpeg" ]; then
    cp -f "$HERE/ffmpeg" "$DATA/ffmpeg" && chmod +x "$DATA/ffmpeg"
    echo "  [ok] ffmpeg -> $DATA"
elif ! command -v ffmpeg >/dev/null 2>&1 && [ ! -x /opt/homebrew/bin/ffmpeg ] && [ ! -x /usr/local/bin/ffmpeg ]; then
    echo "  [!] ffmpeg not found. Install it with:  brew install ffmpeg"
fi

# downloaded files are quarantined by macOS; clear that so the DAW can load them
xattr -dr com.apple.quarantine "$PLUG/VST3/BEATREEL.vst3" "$PLUG/Components/BEATREEL.component" \
    "$HOME/Applications/BEATREEL.app" "$DATA" 2>/dev/null || true

# make Logic / GarageBand rescan Audio Units
killall -9 AudioComponentRegistrar 2>/dev/null || true

echo ""
echo "  Done. Restart your DAW and put BEATREEL on your master track."
echo ""
read -n 1 -s -r -p "  Press any key to close."
