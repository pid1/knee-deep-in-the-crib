#!/bin/bash
# Grab the Photo Booth live preview region (screen points) and un-mirror it.
# Photo Booth pauses the preview when idle; nudge posts a mouse-move to keep it live.
# The -R region is screen points for where the Photo Booth window sits; adjust to taste.
# usage: tools/look.sh out.png
out=${1:-captures/look.png}
"$(dirname "$0")/nudge/nudge"; sleep 0.3

screencapture -x -R396,246,720,480 "$out" && sips -f horizontal "$out" >/dev/null && echo "$out"
