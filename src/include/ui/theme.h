#ifndef NUEVOOS_THEME_H
#define NUEVOOS_THEME_H

#include "types.h"

/* Night instrument on glass (THEME_FG). Wallpaper chrome is THEME_INK.
 * Static — never sampled from the background. */
struct rgb {
    u8 r;
    u8 g;
    u8 b;
};

static const struct rgb THEME_BG0     = { 0x0E, 0x14, 0x1B };
static const struct rgb THEME_BG1     = { 0x18, 0x22, 0x2D };
static const struct rgb THEME_BG2     = { 0x12, 0x1B, 0x25 };
static const struct rgb THEME_FG      = { 0xEE, 0xF5, 0xF7 };
static const struct rgb THEME_FG_DIM  = { 0xA9, 0xB9, 0xC4 };
static const struct rgb THEME_INK     = { 0x1A, 0x1A, 0x1C };
static const struct rgb THEME_INK_DIM = { 0x4A, 0x4A, 0x52 };
static const struct rgb THEME_ACCENT  = { 0x55, 0xDE, 0xB5 };
static const struct rgb THEME_DANGER  = { 0xEE, 0x77, 0x78 };
static const struct rgb THEME_BORDER  = { 0x54, 0x6A, 0x7A };
static const struct rgb THEME_FIELD   = { 0x0A, 0x12, 0x1A };
static const struct rgb THEME_SHADOW  = { 0x02, 0x05, 0x08 };
static const struct rgb THEME_TITLE   = { 0x1B, 0x2A, 0x36 };
static const struct rgb THEME_HOVER   = { 0x2E, 0x46, 0x54 };
static const struct rgb THEME_GRID    = { 0x20, 0x31, 0x3D };
static const struct rgb THEME_GLASS   = { 0x16, 0x21, 0x2C };

#define THEME_RAD_WIN   14u
#define THEME_RAD_CARD  18u
#define THEME_RAD_BTN   12u
#define THEME_RAD_FIELD 10u
#define THEME_RAD_DOCK  32u
#define THEME_RAD_TILE  14u
#define THEME_GLASS_A   218u
#define THEME_SHADOW_A  90u

#endif
