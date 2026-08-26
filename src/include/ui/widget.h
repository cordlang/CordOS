#ifndef NUEVOOS_WIDGET_H
#define NUEVOOS_WIDGET_H

#include "draw.h"
#include "types.h"

void ui_begin(i32 mx, i32 my, u8 buttons, u32 now_ms);
void ui_end(void);
bool ui_busy(void);
bool ui_took_click(void);
void ui_want_full(void);
/* Chrome was redrawn; widgets must stamp again before present. */
void ui_invalidate(void);
/* Override the default arrow/hand choice for non-widget hit targets. */
void ui_set_cursor_kind(enum cursor_kind kind);

bool ui_button(u32 id, u32 x, u32 y, u32 w, u32 h, const char *label,
               bool selected, bool enabled);
bool ui_pill(u32 id, u32 x, u32 y, u32 w, u32 h, const char *label,
             bool selected);
bool ui_field(u32 id, u32 x, u32 y, u32 w, u32 h, const char *text,
              bool password, bool focused, const char *placeholder);
bool ui_icon_btn(u32 id, u32 x, u32 y, u32 w, u32 h, enum ui_icon icon,
                 bool accent, bool running);

#endif
