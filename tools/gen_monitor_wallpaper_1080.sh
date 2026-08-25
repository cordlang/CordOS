#!/usr/bin/env bash
# Build exact 1920x1080 wallpaper from the monitor art (same 16:9, LANCZOS).
set -euo pipefail
cd /mnt/d/os
mkdir -p assets/backgrounds build

SRC_CANDIDATES=(
  "assets/backgrounds/terminal_monitor.png"
  "/mnt/c/Users/burge/.cursor/projects/d-os/assets/c__Users_burge_AppData_Roaming_Cursor_User_workspaceStorage_300f80478b195d75fe217481f5e4c249_images_image-5f16d332-6b4c-48ca-b595-8df28584cd75.png"
)

python3 <<'PY'
from PIL import Image, ImageFilter
import os

W, H = 1920, 1080
root = "/mnt/d/os"
candidates = [
    os.path.join(root, "assets/backgrounds/terminal_monitor.png"),
    "/mnt/c/Users/burge/.cursor/projects/d-os/assets/"
    "c__Users_burge_AppData_Roaming_Cursor_User_workspaceStorage_"
    "300f80478b195d75fe217481f5e4c249_images_image-5f16d332-6b4c-48ca-b595-8df28584cd75.png",
]
src = next(p for p in candidates if os.path.isfile(p))
im = Image.open(src).convert("RGB")
print("source", src, im.size)

# Exact 16:9 → uniform scale to 1920x1080 (no crop, no aspect warp).
out = im.resize((W, H), Image.Resampling.LANCZOS)
# Light sharpen so flat vector edges stay crisp after upscale.
out = out.filter(ImageFilter.UnsharpMask(radius=1.2, percent=120, threshold=2))

bg = os.path.join(root, "assets/backgrounds")
out.save(os.path.join(bg, "default.png"), optimize=True)
out.save(os.path.join(bg, "default.jpg"), quality=95, optimize=True)
out.save(os.path.join(bg, "terminal_monitor_1080.png"), optimize=True)

rgb = os.path.join(root, "build/wallpaper.rgb")
with open(rgb, "wb") as f:
    f.write(out.tobytes("raw", "RGB"))
assert os.path.getsize(rgb) == W * H * 3
print("OK", W, "x", H, os.path.getsize(rgb))
PY

touch build/wallpaper.rgb
make -j4
