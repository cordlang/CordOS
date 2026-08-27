#ifndef CORDOS_DRAW_H
#define CORDOS_DRAW_H

#include "theme.h"
#include "types.h"

enum ui_icon {
    UI_ICON_FILES = 0,
    UI_ICON_TERM,
    UI_ICON_SETTINGS,
    UI_ICON_ABOUT,
    UI_ICON_POWER,
    UI_ICON_LAUNCHER,
    UI_ICON_LOGOUT,
    UI_ICON_BAT_LOW,
    UI_ICON_BAT_HALF,
    UI_ICON_BAT_FULL,
    UI_ICON_BAT_CHARGE
};

void draw_quality_init(void);
void draw_bg_atmosphere(void);
void draw_bg_login(void);
void draw_bg_login_frosted(void);
void draw_bg_frosted(void);
void draw_wallpaper_thumb(u32 x, u32 y, u32 w, u32 h, u32 wp_id, bool selected);
void draw_icon_style_thumb(u32 x, u32 y, u32 w, u32 h, u32 style, bool selected);
void draw_round_fill(u32 x, u32 y, u32 w, u32 h, u32 rad, struct rgb color,
                     u8 alpha);
/* Same fill, but coverage is snapped so dark chrome does not pick up a
 * 1px light halo from the wallpaper. */
void draw_round_fill_hard(u32 x, u32 y, u32 w, u32 h, u32 rad, struct rgb color,
                          u8 alpha);
void draw_glass(u32 x, u32 y, u32 w, u32 h, u32 rad, struct rgb tint, u8 alpha);
void draw_text(u32 x, u32 y, const char *text, struct rgb color, u32 scale);
void draw_text_clip(u32 x, u32 y, u32 max_x, const char *text, struct rgb color,
                    u32 scale);
void draw_text_h(u32 x, u32 y, const char *text, struct rgb color, u32 height);
u32 draw_text_width(const char *text, u32 scale);
u32 draw_text_width_h(const char *text, u32 height);
void draw_text_centered(u32 cx, u32 y, const char *text, struct rgb color,
                        u32 scale);
void draw_panel(u32 x, u32 y, u32 w, u32 h, bool focused);
void draw_field(u32 x, u32 y, u32 w, u32 h, const char *text, bool password,
                bool focused);
void draw_button(u32 x, u32 y, u32 w, u32 h, const char *label, bool focused);
void draw_icon(u32 x, u32 y, u32 size, enum ui_icon icon, struct rgb color);
void draw_icon_styled(u32 x, u32 y, u32 size, enum ui_icon icon, struct rgb color,
                      u32 style);
void draw_window_frame(u32 x, u32 y, u32 w, u32 h, const char *title,
                       bool focused, bool close_hot);
enum cursor_kind {
    CURSOR_KIND_ARROW = 0,
    CURSOR_KIND_POINTER = 1
};

void cursor_invalidate(void);
void cursor_hide(void);
void cursor_set_kind(enum cursor_kind kind);
void cursor_draw(u32 x, u32 y);
/* Present the composed scene, then stamp the cursor on the live LFB. */
void cursor_flip(u32 x, u32 y);

#endif
