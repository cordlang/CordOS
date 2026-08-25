#ifndef NUEVOOS_KEYCODES_H
#define NUEVOOS_KEYCODES_H

#include "types.h"

/* Beyond Unicode scalar values; keyboard pushes these for UI navigation. */
#define KEY_UP    0x110001u
#define KEY_DOWN  0x110002u
#define KEY_LEFT  0x110003u
#define KEY_RIGHT 0x110004u
#define KEY_F1    0x110010u
#define KEY_HOME  0x110020u
#define KEY_END   0x110021u

static inline bool key_is_special(u32 code)
{
    return code >= 0x110000u;
}

#endif
