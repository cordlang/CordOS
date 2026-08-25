#ifndef NUEVOOS_MOUSE_H
#define NUEVOOS_MOUSE_H

#include "types.h"

#define MOUSE_LEFT   0x01u
#define MOUSE_RIGHT  0x02u
#define MOUSE_MIDDLE 0x04u

enum mouse_ev_kind {
    MOUSE_EV_MOVE = 1,
    MOUSE_EV_DOWN = 2,
    MOUSE_EV_UP = 3
};

struct mouse_event {
    u8 kind;
    u8 button;
    u8 buttons;
    i32 x;
    i32 y;
};

void mouse_init(void);
bool mouse_ready(void);
i32 mouse_x(void);
i32 mouse_y(void);
u8 mouse_buttons(void);
void mouse_set_bounds(u32 width, u32 height);
bool mouse_has_event(void);
struct mouse_event mouse_get_event(void);

/* Shared 8042: keyboard IRQ may see aux bytes. */
void mouse_handle_byte(u8 data);

#endif
