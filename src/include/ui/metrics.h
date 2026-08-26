#ifndef CORDOS_METRICS_H
#define CORDOS_METRICS_H

#include "types.h"

/* Design is authored at 1080p. Scale to the live framebuffer. */

u32 ui_px(u32 at_1080);
u32 ui_margin(void);
u32 ui_content_w(void);
u32 ui_text_scale(void);
u32 ui_gap(void);

#endif
