#!/bin/bash
# Installs BoomTown Beats Pump 96 for the current user and clears the "downloaded" flag so Logic will load it.
# Run from Terminal:  bash "/Volumes/Pump 96/install.sh"
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
mkdir -p "$AU_DIR" "$VST3_DIR"

rm -rf "$AU_DIR/Pump 96.component" "$VST3_DIR/Pump 96.vst3"
cp -R "$HERE/Pump 96.component" "$AU_DIR/"
cp -R "$HERE/Pump 96.vst3" "$VST3_DIR/"
xattr -dr com.apple.quarantine "$AU_DIR/Pump 96.component" 2>/dev/null || true
xattr -dr com.apple.quarantine "$VST3_DIR/Pump 96.vst3" 2>/dev/null || true

# make macOS rescan Audio Units
killall -9 AudioComponentRegistrar 2>/dev/null || true

echo ""
echo "Pump 96 is geinstalleerd:"
echo "  AU:   $AU_DIR/Pump 96.component"
echo "  VST3: $VST3_DIR/Pump 96.vst3"
echo ""
echo "Start Logic Pro opnieuw. Je vindt de plugin bij Audio FX > Audio Units > BoomTown Beats > Pump 96."
