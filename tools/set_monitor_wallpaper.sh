#!/usr/bin/env bash
set -euo pipefail
cd /mnt/d/os

SRC="/mnt/c/Users/burge/.cursor/projects/d-os/assets/c__Users_burge_AppData_Roaming_Cursor_User_workspaceStorage_300f80478b195d75fe217481f5e4c249_images_image-5f16d332-6b4c-48ca-b595-8df28584cd75.png"
mkdir -p assets/backgrounds build
cp -f "$SRC" assets/backgrounds/terminal_monitor.png

python3 <<'PY'
from PIL import Image
import os

root = "/mnt/d/os"
src = os.path.join(root, "assets/backgrounds/terminal_monitor.png")
bg = os.path.join(root, "assets/backgrounds")
build = os.path.join(root, "build")
W, H = 1920, 1080

im = Image.open(src).convert("RGB")
print("source", im.size)
# Exact 16:9 → clean scale to 1920x1080 (no crop, no aspect warp)
out = im.resize((W, H), Image.Resampling.LANCZOS)
out.save(os.path.join(bg, "default.png"), optimize=True)
out.save(os.path.join(bg, "default.jpg"), quality=95, optimize=True)
rgb = os.path.join(build, "wallpaper.rgb")
with open(rgb, "wb") as f:
    f.write(out.tobytes("raw", "RGB"))
print("wrote", rgb, os.path.getsize(rgb), "expected", W * H * 3)
assert os.path.getsize(rgb) == W * H * 3
assert out.size == (W, H)
print("OK 1920x1080")
PY

# Point japan default generator at this asset next time too
touch build/wallpaper.rgb
make -j4
ls -la build/wallpaper.rgb assets/backgrounds/default.png build/nuevoos64.iso
