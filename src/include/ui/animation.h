#ifndef CORDOS_ANIMATION_H
#define CORDOS_ANIMATION_H

#include "types.h"

void ui_fade_in(void);
void ui_fade_out(void);
/* old_front is a full framebuffer capture; new scene must already be in the
 * compose backbuffer. Blends old → new onto the screen. */
void ui_crossfade_from(const u8 *old_front);

#endif
