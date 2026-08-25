#ifndef NUEVOOS_COMPOSITOR_H
#define NUEVOOS_COMPOSITOR_H

#include "types.h"

/* Own compositor: scene in RAM, damage rects to the LFB, cursor overlay.
 * fb.c stays the pixel driver. Screens and widgets talk to this. */

void ui_comp_init(void);
void ui_comp_scene_begin(void);
void ui_comp_damage(u32 x, u32 y, u32 w, u32 h);
void ui_comp_mark_full(void);
bool ui_comp_is_full(void);
bool ui_comp_has_damage(void);
/* Copy damage (or the whole scene) then stamp the cursor on the LFB. */
void ui_comp_present(void);
/* Heavy present for splash/crossfade. Cursor stamped after. */
void ui_comp_present_heavy(void);

#endif
