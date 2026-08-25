#!/usr/bin/env python3
"""Rasterize CordOS brand marks (white-on-black) for the boot splash."""
from __future__ import annotations

import os

import cairosvg
from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
LOGO_DIR = os.path.join(ROOT, "assets", "logo")
BUILD = os.path.join(ROOT, "out")
BRAND_H = os.path.join(ROOT, "src", "include", "ui", "brand.h")
ASSETS = os.path.join(ROOT, "assets", "brand")

# Native SVG is 420×420. Resize with ONE scale so the circle stays a circle.
LOGO_PX = 180
LOGIN_PX = 96
NAME_SRC_W, NAME_SRC_H = 300, 79
NAME_W = 320
SPLASH_W, SPLASH_H = 1920, 1080


def svg_png(path: str, width: int, height: int) -> Image.Image:
    from io import BytesIO

    png = cairosvg.svg2png(url=path, output_width=width, output_height=height)
    im = Image.open(BytesIO(png)).convert("RGBA")
    if im.size != (width, height):
        im = im.resize((width, height), Image.Resampling.LANCZOS)
    return im


def write_rgba(path: str, im: Image.Image) -> None:
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(im.tobytes("raw", "RGBA"))


def main() -> None:
    os.makedirs(BUILD, exist_ok=True)
    os.makedirs(ASSETS, exist_ok=True)

    logo_svg = os.path.join(LOGO_DIR, "cordos_logo_white.svg")
    logo = svg_png(logo_svg, LOGO_PX, LOGO_PX)
    login = svg_png(logo_svg, LOGIN_PX, LOGIN_PX)
    name_h = max(1, round(NAME_SRC_H * NAME_W / NAME_SRC_W))
    name = svg_png(
        os.path.join(LOGO_DIR, "cordos_name_white.svg"), NAME_W, name_h
    )
    if logo.width != logo.height:
        raise SystemExit(f"logo must stay square, got {logo.size}")
    if login.width != login.height:
        raise SystemExit(f"login mark must stay square, got {login.size}")

    write_rgba(os.path.join(BUILD, "brand_logo.rgba"), logo)
    write_rgba(os.path.join(BUILD, "brand_login.rgba"), login)
    write_rgba(os.path.join(BUILD, "brand_name.rgba"), name)
    logo.save(os.path.join(ASSETS, "logo.png"), "PNG")
    login.save(os.path.join(ASSETS, "login.png"), "PNG")
    name.save(os.path.join(ASSETS, "name.png"), "PNG")

    splash = Image.new("RGB", (SPLASH_W, SPLASH_H), (0, 0, 0))
    lx = (SPLASH_W - logo.width) // 2
    ly = SPLASH_H // 2 - logo.height // 2 - 48
    nx = (SPLASH_W - name.width) // 2
    ny = ly + logo.height + 28
    splash.paste(logo, (lx, ly), logo)
    splash.paste(name, (nx, ny), name)
    splash_path = os.path.join(BUILD, "splash.png")
    splash.save(splash_path, "PNG", optimize=True)

    with open(BRAND_H, "w", encoding="utf-8") as f:
        f.write(
            "#ifndef NUEVOOS_BRAND_H\n"
            "#define NUEVOOS_BRAND_H\n\n"
            "#include \"types.h\"\n\n"
            f"#define BRAND_LOGO_W {logo.width}u\n"
            f"#define BRAND_LOGO_H {logo.height}u\n"
            f"#define BRAND_LOGIN_W {login.width}u\n"
            f"#define BRAND_LOGIN_H {login.height}u\n"
            f"#define BRAND_NAME_W {name.width}u\n"
            f"#define BRAND_NAME_H {name.height}u\n\n"
            "extern const u8 brand_logo_rgba[];\n"
            "extern const u8 brand_login_rgba[];\n"
            "extern const u8 brand_name_rgba[];\n\n"
            "void draw_boot_splash(u8 progress);\n"
            "void draw_boot_splash_ex(u8 progress, u8 mark_alpha, i32 y_shift);\n"
            "void draw_boot_splash_to_back(u8 progress, u8 mark_alpha, i32 y_shift);\n\n"
            "#endif\n"
        )
    print(
        "brand logo", logo.size, "login", login.size, "name", name.size,
        "splash", os.path.getsize(splash_path),
    )


if __name__ == "__main__":
    main()
