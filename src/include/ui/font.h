#ifndef CORDOS_FONT_H
#define CORDOS_FONT_H

#include "types.h"

#define FONT_WIDTH  22
#define FONT_HEIGHT 28
#define FONT_LINE   (FONT_HEIGHT + 8u)
#define FONT_TITLE_W  38
#define FONT_TITLE_H  48
#define FONT_GLYPHS 198

/* 8-bit coverage + per-glyph advance. Varela Round (proportional). */
extern const u32 font_codepoints[198];
extern const u8 font_advance[198];
extern const u8 font_title_advance[198];
extern const u8 font_alpha[198][616];
extern const u8 font_title_alpha[198][1824];

#endif
