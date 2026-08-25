#ifndef NUEVOOS_CURSOR_H
#define NUEVOOS_CURSOR_H

#include "types.h"

#define CURSOR_W     31u
#define CURSOR_H     32u
#define CURSOR_HOT_X 2u
#define CURSOR_HOT_Y 4u
#define CURSOR_PTR_HOT_X 12u
#define CURSOR_PTR_HOT_Y 1u

extern const u8 cursor_rgba[3968];
extern const u8 cursor_arrow_dark_rgba[3968];
extern const u8 cursor_pointer_rgba[3968];
extern const u8 cursor_pointer_dark_rgba[3968];

#endif
