#!/usr/bin/env python3
"""Install the Japanese street photo as NuevoOS default wallpaper."""
from __future__ import annotations

import os
import shutil
import sys

sys.path.insert(0, "/tmp/nv/lib")
try:
    from PIL import Image
except ImportError:
    print("Need Pillow: pip install Pillow", file=sys.stderr)
    sys.exit(1)

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
BG_DIR = os.path.join(ROOT, "assets", "backgrounds")
BUILD = os.path.join(ROOT, "build")
TARGET_W = 1920
TARGET_H = 1080

CANDIDATES = [
    os.path.join(BG_DIR, "japan_street.jpg"),
    os.path.join(BG_DIR, "japan_street.png"),
]


def find_src() -> str:
    for path in CANDIDATES:
        if os.path.isfile(path):
            return path
    raise SystemExit("source image not found")


def cover_resize(im: Image.Image, tw: int, th: int) -> Image.Image:
    im = im.convert("RGB")
    sw, sh = im.size
    scale = max(tw / sw, th / sh)
    nw = max(1, int(sw * scale + 0.5))
    nh = max(1, int(sh * scale + 0.5))
    im = im.resize((nw, nh), Image.Resampling.LANCZOS)
    left = (nw - tw) // 2
    top = (nh - th) // 2
    return im.crop((left, top, left + tw, top + th))


def main() -> None:
    src = find_src()
    os.makedirs(BG_DIR, exist_ok=True)
    os.makedirs(BUILD, exist_ok=True)

    im = Image.open(src)
    print("source", src, im.size, im.mode)

    covered = cover_resize(im, TARGET_W, TARGET_H)
    out_png = os.path.join(BG_DIR, "default.png")
    out_jpg = os.path.join(BG_DIR, "default.jpg")
    covered.save(out_png, optimize=True)
    covered.save(out_jpg, quality=90, optimize=True)

    rgb_path = os.path.join(BUILD, "wallpaper.rgb")
    with open(rgb_path, "wb") as f:
        f.write(covered.tobytes())

    print("wrote", out_png)
    print("wrote", out_jpg)
    print("wrote", rgb_path, os.path.getsize(rgb_path), "bytes", f"{TARGET_W}x{TARGET_H}")


if __name__ == "__main__":
    main()
