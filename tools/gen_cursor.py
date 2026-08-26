#!/usr/bin/env python3
"""Rasterize OS pointers (arrow + pointing hand) in light and dark polarities."""
from __future__ import annotations

import io
import os

import cairosvg
from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
OUT_DIR = os.path.join(ROOT, "assets", "cursors")
OUT_C = os.path.join(ROOT, "src", "ui", "gfx", "cursor_data.c")
OUT_H = os.path.join(ROOT, "src", "include", "ui", "cursor.h")

RENDER = 256
TARGET_H = 32

# Shape from ful1e5/apple_cursor (MIT). Tip = fill path origin.
ARROW_TIP = (84.1, 48.56)
ARROW = """\
<path fill-rule="evenodd" clip-rule="evenodd"
  d="M84.1001 48.5601V173.06L110.8 146.56L136.3 207.56L158.3 197.06L133.8 139.06H172.8L84.1001 48.5601Z"
  fill="{fill}"/>
<path
  d="M88.0281 44.7102L78.6001 35.0909V48.5601V173.06V186.268L87.9746 176.964L108.876 156.218L131.225 209.681L133.454 215.013L138.669 212.524L160.669 202.024L165.411 199.76L163.366 194.92L142.094 144.56H172.8H185.892L176.728 135.21L88.0281 44.7102Z"
  stroke="{stroke}" stroke-width="10"/>
"""

# Pointing hand (index up). Tip = top of index finger.
HAND_TIP = (96.0, 22.0)
HAND = """\
<path fill="{fill}" stroke="{stroke}" stroke-width="10" stroke-linejoin="round"
  d="M88 22
     c 12 0 22 10 22 22
     v 78
     c 8 -6 18 -10 28 -10
     c 16 0 26 12 28 24
     c 8 -6 18 -10 28 -10
     c 20 0 34 16 34 36
     v 52
     c 0 44 -34 78 -86 78
     h -28
     c -28 0 -50 -10 -68 -30
     L 18 196
     c -10 -10 -10 -26 0 -36
     c 10 -10 26 -10 36 0
     l 16 16
     V 44
     c 0 -12 10 -22 22 -22
     z"/>
"""

SHADOW = """\
<filter id="shadow" x="-20%" y="-20%" width="140%" height="140%"
  filterUnits="objectBoundingBox" color-interpolation-filters="sRGB">
  <feFlood flood-opacity="0" result="BackgroundImageFix"/>
  <feColorMatrix in="SourceAlpha" type="matrix"
    values="0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 127 0" result="hardAlpha"/>
  <feOffset dx="1" dy="3"/>
  <feGaussianBlur stdDeviation="2.2"/>
  <feColorMatrix type="matrix" values="0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0.35 0"/>
  <feBlend mode="normal" in2="BackgroundImageFix" result="drop"/>
  <feBlend mode="normal" in="SourceGraphic" in2="drop" result="shape"/>
</filter>
"""

POLAR = {
    "light": ("#FFFFFF", "#111111"),  # white fill, dark stroke — dark backgrounds
    "dark": ("#1A1A1C", "#F4F4F2"),   # dark fill, light stroke — light backgrounds
}


def svg_wrap(inner: str) -> str:
    return f"""<?xml version="1.0" encoding="UTF-8"?>
<svg width="256" height="256" viewBox="0 0 256 256" fill="none"
     xmlns="http://www.w3.org/2000/svg">
  <defs>{SHADOW}</defs>
  <g filter="url(#shadow)">{inner}</g>
</svg>
"""


def raster(inner: str, tip: tuple[float, float]) -> tuple[Image.Image, int, int]:
    png = cairosvg.svg2png(
        bytestring=svg_wrap(inner).encode("utf-8"),
        output_width=RENDER,
        output_height=RENDER,
    )
    img = Image.open(io.BytesIO(png)).convert("RGBA")
    bbox = img.getbbox()
    if bbox is None:
        raise RuntimeError("empty cursor")
    pad = 2
    l = max(0, bbox[0] - pad)
    t = max(0, bbox[1] - pad)
    r = min(img.width, bbox[2] + pad)
    b = min(img.height, bbox[3] + pad)
    crop = img.crop((l, t, r, b))
    scale = TARGET_H / crop.height
    w = max(8, int(round(crop.width * scale)))
    out = crop.resize((w, TARGET_H), Image.Resampling.LANCZOS)
    hot_x = int(round((tip[0] * RENDER / 256.0 - l) * scale))
    hot_y = int(round((tip[1] * RENDER / 256.0 - t) * scale))
    hot_x = max(0, min(w - 1, hot_x))
    hot_y = max(0, min(TARGET_H - 1, hot_y))
    return out, hot_x, hot_y


def pad_to(im: Image.Image, width: int, hot_x: int) -> tuple[Image.Image, int]:
    if im.width == width:
        return im, hot_x
    canvas = Image.new("RGBA", (width, im.height), (0, 0, 0, 0))
    canvas.paste(im, (0, 0))
    return canvas, hot_x


def c_array(name: str, data: list[int]) -> list[str]:
    lines = [f"const u8 {name}[{len(data)}] = {{"]
    for i in range(0, len(data), 16):
        chunk = ", ".join(f"0x{v:02X}" for v in data[i : i + 16])
        lines.append(f"    {chunk},")
    lines.append("};")
    return lines


def main() -> None:
    jobs = [
        ("arrow", ARROW, ARROW_TIP),
        ("pointer", HAND, HAND_TIP),
    ]
    sprites = {}
    max_w = 0
    for kind, tmpl, tip in jobs:
        for polar, (fill, stroke) in POLAR.items():
            im, hx, hy = raster(tmpl.format(fill=fill, stroke=stroke), tip)
            sprites[(kind, polar)] = (im, hx, hy)
            max_w = max(max_w, im.width)

    os.makedirs(OUT_DIR, exist_ok=True)
    packed = {}
    hots = {}
    for key, (im, hx, hy) in sprites.items():
        im, hx = pad_to(im, max_w, hx)
        packed[key] = list(im.tobytes())
        hots[key] = (hx, hy)
        im.save(os.path.join(OUT_DIR, f"{key[0]}_{key[1]}.png"), "PNG", optimize=True)

    # Keep a canonical pointer.png for the default arrow (light).
    sprites[("arrow", "light")][0].save(
        os.path.join(OUT_DIR, "pointer.png"), "PNG", optimize=True
    )

    nbytes = max_w * TARGET_H * 4
    ax, ay = hots[("arrow", "light")]
    px, py = hots[("pointer", "light")]

    with open(OUT_H, "w", encoding="utf-8") as f:
        f.write(
            "#ifndef CORDOS_CURSOR_H\n"
            "#define CORDOS_CURSOR_H\n\n"
            "#include \"types.h\"\n\n"
            f"#define CURSOR_W     {max_w}u\n"
            f"#define CURSOR_H     {TARGET_H}u\n"
            f"#define CURSOR_HOT_X {ax}u\n"
            f"#define CURSOR_HOT_Y {ay}u\n"
            f"#define CURSOR_PTR_HOT_X {px}u\n"
            f"#define CURSOR_PTR_HOT_Y {py}u\n\n"
            f"extern const u8 cursor_rgba[{nbytes}];\n"
            f"extern const u8 cursor_arrow_dark_rgba[{nbytes}];\n"
            f"extern const u8 cursor_pointer_rgba[{nbytes}];\n"
            f"extern const u8 cursor_pointer_dark_rgba[{nbytes}];\n\n"
            "#endif\n"
        )

    lines = [
        '#include "cursor.h"',
        "",
        "/* macOS-style arrow + pointing hand, light and dark polarities. */",
    ]
    mapping = [
        ("cursor_rgba", ("arrow", "light")),
        ("cursor_arrow_dark_rgba", ("arrow", "dark")),
        ("cursor_pointer_rgba", ("pointer", "light")),
        ("cursor_pointer_dark_rgba", ("pointer", "dark")),
    ]
    for name, key in mapping:
        lines.append("")
        lines.extend(c_array(name, packed[key]))
    lines.append("")
    with open(OUT_C, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))
    print(
        "cursors", max_w, "x", TARGET_H,
        "arrow hot", ax, ay, "pointer hot", px, py,
    )


if __name__ == "__main__":
    main()
