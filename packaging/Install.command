#!/bin/bash
# Serkan Duck Pro installer — copies the plugins to your user plug-in folders.
cd "$(dirname "$0")"

VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$VST3_DIR" "$AU_DIR"

rm -rf "$VST3_DIR/Serkan Duck Pro.vst3" "$AU_DIR/Serkan Duck Pro.component"
cp -R "Serkan Duck Pro.vst3" "$VST3_DIR/"
cp -R "Serkan Duck Pro.component" "$AU_DIR/"

xattr -dr com.apple.quarantine "$VST3_DIR/Serkan Duck Pro.vst3" "$AU_DIR/Serkan Duck Pro.component" 2>/dev/null
killall -9 AudioComponentRegistrar 2>/dev/null

echo ""
echo "Serkan Duck Pro kuruldu:"
echo "  VST3 -> $VST3_DIR"
echo "  AU   -> $AU_DIR"
echo "DAW'ini yeniden başlat ve eklentileri yeniden tara."
