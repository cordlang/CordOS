#ifndef CORDOS_KEYBOARD_H
#define CORDOS_KEYBOARD_H

#include "types.h"

void keyboard_init(void);
bool keyboard_has_char(void);
u32 keyboard_get_codepoint(void);
/* ASCII compatibility helper; non-ASCII codepoints become '?'. */
char keyboard_getchar(void);

#endif
