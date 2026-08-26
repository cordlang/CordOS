#ifndef CORDOS_ICONS_H
#define CORDOS_ICONS_H

#include "types.h"

#define ICON_PX     48u
#define ICON_PX_SM  24u
#define ICON_PACK   11u

#define ICON_STYLE_LINEAR  0u
#define ICON_STYLE_BOLD    1u
#define ICON_STYLE_BROKEN  2u
#define ICON_STYLE_BULK    3u
#define ICON_STYLE_COUNT   4u

extern const u8 icon48_rgba[ICON_STYLE_COUNT][ICON_PACK][ICON_PX * ICON_PX * 4u];
extern const u8 icon24_rgba[ICON_STYLE_COUNT][ICON_PACK][ICON_PX_SM * ICON_PX_SM * 4u];

void icon_set_style(u32 id);
u32 icon_style(void);

#endif
