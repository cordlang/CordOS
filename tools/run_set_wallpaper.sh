#!/usr/bin/env bash
set -euo pipefail
cd /mnt/d/os
python3 -m pip install --user --break-system-packages Pillow >/tmp/pil-install.log 2>&1 || true
python3 tools/set_japan_wallpaper.py
ls -la build/wallpaper.rgb assets/backgrounds/default.png assets/backgrounds/japan_street.jpg
