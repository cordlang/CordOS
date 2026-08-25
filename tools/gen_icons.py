#!/usr/bin/env python3
"""Build CordOS icons.

App glyphs come from the Iconsax/Iconly Figma sheets in /icons.
Power (⏻) and battery states come from Solar (same visual family:
Linear / Bold / Broken / Bulk-duotone).

48px app icons: flat colored CIRCLE, no sheen.
24px chrome and battery: white glyph, tinted at draw time.
"""
from __future__ import annotations

import io
import re
import urllib.request
from pathlib import Path

from PIL import Image, ImageDraw

try:
    import cairosvg
except ImportError:
    cairosvg = None

ROOT = Path(__file__).resolve().parent.parent
SHEETS = ROOT / "assets" / "icon-library"
OUT_DIR = ROOT / "assets" / "icons"
STYLE_DIR = OUT_DIR / "iconsax"
VENDOR = OUT_DIR / "vendor" / "solar"
KBUILD = ROOT / "out"

SIZE = 48
SIZE_SM = 24
STYLES_IN_COL = 6
ICONIFY = "https://api.iconify.design"

# Desktop/app icons from the user's Figma sheets. Flat circle color.
SHEET_ICONS = {
    "files": ("Files.svg", 0, (245, 186, 74)),
    "terminal": ("Programing.svg", 16, (42, 50, 62)),
    "settings": ("Settings.svg", 3, (122, 138, 152)),
    "about": ("Essetional.svg", 15, (62, 186, 168)),
    "launcher": ("Grid.svg", 19, (92, 142, 232)),
    "logout": ("Arrow.svg", 53, (154, 162, 174)),
}

# Enum order in draw.h
PACK = (
    ("files", "sheet"),
    ("terminal", "sheet"),
    ("settings", "sheet"),
    ("about", "sheet"),
    ("power", "solar"),
    ("launcher", "sheet"),
    ("logout", "sheet"),
    ("bat_low", "solar"),
    ("bat_half", "solar"),
    ("bat_full", "solar"),
    ("bat_charge", "solar"),
)

SOLAR_NAME = {
    "power": "power",
    "bat_low": "battery-low",
    "bat_half": "battery-half",
    "bat_full": "battery-full",
    "bat_charge": "battery-charge",
}

PLATE_COLOR = {
    "power": (232, 92, 78),
}

# UI order: Linear, Bold, Broken, Bulk
STYLES = (
    ("linear", 0, "linear"),
    ("bold", 1, "bold"),
    ("broken", 2, "broken"),
    ("bulk", 4, "bold-duotone"),
)


def frames(svg: str) -> list[tuple[float, float, float, float]]:
    found = []
    for m in re.finditer(r"<rect\b([^>]*)>", svg):
        attrs = m.group(1)
        if "#7B61FF" not in attrs:
            continue

        def g(name: str) -> float:
            mm = re.search(rf'{name}="([^"]+)"', attrs)
            return float(mm.group(1)) if mm else 0.0

        found.append((g("x"), g("y"), g("width"), g("height")))
    found.sort(key=lambda t: (round(t[1]), t[0]))
    return found


def raster_svg_file(path: Path) -> Image.Image:
    if cairosvg is None:
        raise RuntimeError("cairosvg is required")
    png = cairosvg.svg2png(url=str(path), dpi=96)
    return Image.open(io.BytesIO(png)).convert("RGBA")


def raster_svg_bytes(data: bytes, px: int) -> Image.Image:
    if cairosvg is None:
        raise RuntimeError("cairosvg is required")
    png = cairosvg.svg2png(bytestring=data, output_width=px, output_height=px)
    return Image.open(io.BytesIO(png)).convert("RGBA")


def fetch_solar(icon: str, suffix: str) -> bytes:
    VENDOR.mkdir(parents=True, exist_ok=True)
    path = VENDOR / f"{icon}-{suffix}.svg"
    if path.exists() and path.stat().st_size > 80:
        return path.read_bytes()
    url = f"{ICONIFY}/solar:{icon}-{suffix}.svg?height=256"
    req = urllib.request.Request(url, headers={"User-Agent": "CordOS-icon-gen/2.0"})
    with urllib.request.urlopen(req, timeout=30) as resp:
        data = resp.read()
    if b"<svg" not in data[:80]:
        raise RuntimeError(f"not svg: {url} -> {data[:80]!r}")
    path.write_bytes(data)
    return data


def is_bg(r: int, g: int, b: int, a: int) -> bool:
    if a < 10:
        return True
    if abs(r - 0x7B) + abs(g - 0x61) + abs(b - 0xFF) < 48:
        return True
    if r >= 236 and g >= 236 and b >= 236:
        return True
    return False


def to_white_glyph(img: Image.Image) -> Image.Image:
    px = img.load()
    w, h = img.size
    minx, miny, maxx, maxy = w, h, 0, 0
    ink = 0
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            if is_bg(r, g, b, a):
                continue
            ink += 1
            if x < minx:
                minx = x
            if y < miny:
                miny = y
            if x > maxx:
                maxx = x
            if y > maxy:
                maxy = y
    if ink < 8:
        raise RuntimeError(f"empty glyph {img.size} ink={ink}")
    pad = 2
    minx = max(0, minx - pad)
    miny = max(0, miny - pad)
    maxx = min(w - 1, maxx + pad)
    maxy = min(h - 1, maxy + pad)
    crop = img.crop((minx, miny, maxx + 1, maxy + 1))
    cw, ch = crop.size
    side = max(cw, ch)
    square = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    square.paste(crop, ((side - cw) // 2, (side - ch) // 2))
    sp = square.load()
    out = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    op = out.load()
    for y in range(side):
        for x in range(side):
            r, g, b, a = sp[x, y]
            if is_bg(r, g, b, a):
                continue
            luma = (r * 54 + g * 183 + b * 19) >> 8
            alpha = 255 - luma
            if a < 255:
                alpha = (alpha * a) // 255
            if alpha < 8:
                continue
            op[x, y] = (255, 255, 255, alpha)
    return out


def crop_style_cell(
    img: Image.Image, col: tuple[float, float, float, float], style_row: int
) -> Image.Image:
    x, y, w, h = col
    inset = 4.0
    cell_h = h / float(STYLES_IN_COL)
    x0 = x + inset
    x1 = x + w - inset
    y0 = y + style_row * cell_h + 2.0
    y1 = y + (style_row + 1) * cell_h - 2.0
    return img.crop((int(round(x0)), int(round(y0)), int(round(x1)), int(round(y1))))


def circle_plate(size: int, color) -> Image.Image:
    """Flat disk, 4x oversampled. No gradient, no sheen."""
    hi = size * 4
    img = Image.new("RGBA", (hi, hi), (0, 0, 0, 0))
    ImageDraw.Draw(img).ellipse(
        [1, 1, hi - 2, hi - 2], fill=(color[0], color[1], color[2], 255)
    )
    return img.resize((size, size), Image.Resampling.LANCZOS)


def compose_app_icon(glyph: Image.Image, color) -> Image.Image:
    plate = circle_plate(SIZE, color)
    g = glyph.resize((26, 26), Image.Resampling.LANCZOS)
    out = plate.copy()
    ox = (SIZE - 26) // 2
    oy = (SIZE - 26) // 2
    out.alpha_composite(g, (ox, oy))
    return out


def compose_chrome(glyph: Image.Image) -> Image.Image:
    ui = Image.new("RGBA", (SIZE_SM, SIZE_SM), (0, 0, 0, 0))
    g = glyph.resize((20, 20), Image.Resampling.LANCZOS)
    ui.alpha_composite(g, (2, 2))
    return ui


def compose_plain48(glyph: Image.Image) -> Image.Image:
    ui = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    g = glyph.resize((36, 36), Image.Resampling.LANCZOS)
    ui.alpha_composite(g, ((SIZE - 36) // 2, (SIZE - 36) // 2))
    return ui


def rgba_bytes(img: Image.Image) -> bytes:
    return img.convert("RGBA").tobytes()


def main() -> None:
    if cairosvg is None:
        raise SystemExit("cairosvg is required")
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    STYLE_DIR.mkdir(parents=True, exist_ok=True)
    KBUILD.mkdir(parents=True, exist_ok=True)

    needed_sheets = sorted({SHEET_ICONS[n][0] for n, kind in PACK if kind == "sheet"})
    rasters: dict[str, Image.Image] = {}
    cols: dict[str, list] = {}
    for name in needed_sheets:
        path = SHEETS / name
        print("raster", name)
        svg = path.read_text(encoding="utf-8")
        rasters[name] = raster_svg_file(path)
        cols[name] = frames(svg)
        print(" ", name, "size", rasters[name].size, "cols", len(cols[name]))

    blob48 = bytearray()
    blob24 = bytearray()
    preview = Image.new(
        "RGBA",
        (8 + (SIZE + 12) * len(PACK), 8 + (SIZE + 28) * len(STYLES)),
        (16, 18, 22, 255),
    )

    for si, (style_name, style_row, solar_sfx) in enumerate(STYLES):
        dest = STYLE_DIR / style_name
        dest.mkdir(parents=True, exist_ok=True)
        for pi, (name, kind) in enumerate(PACK):
            if kind == "sheet":
                sheet, col_i, color = SHEET_ICONS[name]
                if col_i >= len(cols[sheet]):
                    raise RuntimeError(
                        f"{sheet}: column {col_i} missing (n={len(cols[sheet])})"
                    )
                cell = crop_style_cell(rasters[sheet], cols[sheet][col_i], style_row)
                glyph = to_white_glyph(cell)
                app = compose_app_icon(glyph, color)
            else:
                solar = SOLAR_NAME[name]
                print("  fetch", solar, solar_sfx)
                data = fetch_solar(solar, solar_sfx)
                raw = raster_svg_bytes(data, 160)
                glyph = to_white_glyph(raw)
                color = PLATE_COLOR.get(name)
                if color is not None:
                    app = compose_app_icon(glyph, color)
                else:
                    app = compose_plain48(glyph)
            chrome = compose_chrome(glyph)
            blob48.extend(rgba_bytes(app))
            blob24.extend(rgba_bytes(chrome))
            glyph.save(dest / f"{name}-glyph.png")
            app.save(dest / f"{name}.png")
            chrome.save(dest / f"{name}-24.png")
            if style_name == "bold":
                app.save(OUT_DIR / f"{name}.png")
                chrome.save(OUT_DIR / f"{name}-24.png")
            preview.paste(app, (8 + pi * (SIZE + 12), 8 + si * (SIZE + 28)), app)
            print(f"  {style_name:8} {name:12} glyph={glyph.size}")

    (KBUILD / "icon48.rgba").write_bytes(bytes(blob48))
    (KBUILD / "icon24.rgba").write_bytes(bytes(blob24))
    preview.save(STYLE_DIR / "preview.png")

    expect48 = len(STYLES) * len(PACK) * SIZE * SIZE * 4
    expect24 = len(STYLES) * len(PACK) * SIZE_SM * SIZE_SM * 4
    if len(blob48) != expect48 or len(blob24) != expect24:
        raise RuntimeError(
            f"blob size {len(blob48)}/{len(blob24)} != {expect48}/{expect24}"
        )

    (OUT_DIR / "README.md").write_text(
        "# CordOS icons\n\n"
        "App icons: Iconsax/Iconly sheets, flat round plates (no sheen).\n"
        "Power is the ⏻ mark, not a battery. Battery glyphs are status-only\n"
        "(dock charge: low / half / full / charging).\n"
        "Four styles in Settings: Linear, Bold, Broken, Bulk.\n",
        encoding="utf-8",
    )
    print("wrote", KBUILD / "icon48.rgba", len(blob48), "and", KBUILD / "icon24.rgba", len(blob24))
    print("preview", STYLE_DIR / "preview.png")


if __name__ == "__main__":
    main()
