#ifndef CORDOS_FB_CONSOLE_H
#define CORDOS_FB_CONSOLE_H

#include "types.h"

/* Mirror VGA text output onto the linear framebuffer (for Terminal in gfx mode). */
void fb_console_enable(void);
void fb_console_disable(void);
bool fb_console_is_enabled(void);
void fb_console_put_cell(u8 row, u8 column, char character);

#endif
