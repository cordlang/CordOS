#include "desktop_priv.h"

struct window s_win[MAX_WIN];
u32 s_z[MAX_WIN];
u32 s_zcount;
bool s_menu;
bool s_spot;
bool s_ctx;
i32 s_ctx_x;
i32 s_ctx_y;
bool s_show_icons;
i32 s_drag;
i32 s_dx;
i32 s_dy;
u32 s_hover;
bool s_logout;
char s_clock[8];
char s_date[12];
u32 s_last_min = 0xFFFFFFFFu;
u8 s_last_day = 0xFFu;
u8 s_last_month = 0xFFu;
u16 s_last_year = 0xFFFFu;
u32 s_widget_last_hover = HIT_NONE;
bool s_widget_repaint_base;
u32 s_last_click_hit = HIT_NONE;
u32 s_last_click_ms;
char s_spot_q[SPOT_QMAX];
u32 s_spot_len;
struct spot_item s_spot_hits[SPOT_MAX];
u32 s_spot_n;
u32 s_spot_sel;
u8 *s_spot_fade_from;
bool s_spot_need_xfade;
struct file_acc s_files;
bool s_cursor_valid;
i32 s_cursor_x;
i32 s_cursor_y;

void desktop_run(void)
{
    u32 i;
    bool dirty = true;
    i32 last_x = -1;
    i32 last_y = -1;
    u8 *from = NULL;

    for (i = 0; i < MAX_WIN; ++i) {
        s_win[i].used = false;
    }
    s_zcount = 0;
    s_menu = false;
    s_spot = false;
    s_spot_q[0] = '\0';
    s_spot_len = 0;
    s_spot_n = 0;
    s_spot_sel = 0;
    s_spot_need_xfade = false;
    s_ctx = false;
    s_ctx_x = 0;
    s_ctx_y = 0;
    s_show_icons = true;
    s_drag = -1;
    s_hover = HIT_NONE;
    s_logout = false;
    s_last_min = 0xFFFFFFFFu;
    s_last_day = 0xFFu;
    s_last_month = 0xFFu;
    s_last_year = 0xFFFFu;
    s_widget_last_hover = HIT_NONE;
    s_widget_repaint_base = false;
    s_cursor_valid = false;
    cursor_hide();
    if (fb_compose_ready()) {
        from = fb_layer_alloc();
        if (from != NULL) {
            fb_copy_front(from);
        }
    }
    files_reload();
    clock_refresh();
    mouse_set_bounds(fb_width(), fb_height());

    while (keyboard_has_char()) {
        (void)keyboard_get_codepoint();
    }
    while (mouse_has_event()) {
        (void)mouse_get_event();
    }

    fb_compose_begin();
    paint_desktop_base();
    paint_windows();
    draw_taskbar();
    if (from != NULL) {
        ui_crossfade_from(from);
        s_spot_fade_from = from;
    }
    desktop_cursor_update(true);
    ui_comp_mark_full();
    ui_comp_present();
    dirty = false;
    last_x = mouse_x();
    last_y = mouse_y();

    for (;;) {
        if (s_logout) {
            return;
        }

        if (keyboard_has_char()) {
            handle_key(keyboard_get_codepoint());
            dirty = true;
        } else if (mouse_has_event()) {
            struct mouse_event ev = mouse_get_event();
            u32 hit = hit_test(ev.x, ev.y);

            if (ev.kind == MOUSE_EV_MOVE) {
                if (s_drag >= 0 && s_win[s_drag].used) {
                    i32 ox = s_win[s_drag].x;
                    i32 oy = s_win[s_drag].y;
                    s_win[s_drag].x = ev.x - s_dx;
                    s_win[s_drag].y = ev.y - s_dy;
                    clamp_win(&s_win[s_drag]);
                    if (ox != s_win[s_drag].x || oy != s_win[s_drag].y) {
                        desktop_drag((u32)s_drag, ox, oy);
                    }
                    last_x = mouse_x();
                    last_y = mouse_y();
                } else if (hit != s_hover) {
                    bool was = desk_chrome_hit(s_hover);
                    bool now = desk_chrome_hit(hit);

                    s_hover = hit;
                    if (s_spot && hit >= HIT_SPOT_ROW &&
                        hit < HIT_SPOT_ROW + SPOT_MAX) {
                        s_spot_sel = hit - HIT_SPOT_ROW;
                        dirty = true;
                    } else if (!was || !now || s_spot) {
                        dirty = true;
                    }
                }
            } else if (ev.kind == MOUSE_EV_DOWN && ev.button == MOUSE_LEFT) {
                s_hover = hit;
                if (!desk_chrome_hit(hit)) {
                    handle_click(hit, is_double_click(hit));
                    dirty = true;
                }
            } else if (ev.kind == MOUSE_EV_DOWN && ev.button == MOUSE_RIGHT) {
                handle_right_click(hit);
                dirty = true;
            } else if (ev.kind == MOUSE_EV_UP && ev.button == MOUSE_LEFT) {
                s_drag = -1;
            }
        } else if (clock_changed()) {
            dirty = true;
        }

        if (!dirty && (mouse_x() != last_x || mouse_y() != last_y)) {
            u32 live = hit_test(mouse_x(), mouse_y());

            if (live != s_hover) {
                bool was = desk_chrome_hit(s_hover);
                bool now = desk_chrome_hit(live);

                s_hover = live;
                if (s_spot && live >= HIT_SPOT_ROW &&
                    live < HIT_SPOT_ROW + SPOT_MAX) {
                    s_spot_sel = live - HIT_SPOT_ROW;
                    dirty = true;
                } else if (!was || !now || s_spot) {
                    dirty = true;
                }
            }
        }

        if (dirty) {
            desktop_redraw();
            ui_invalidate();
            dirty = false;
        }
        if (desktop_widgets()) {
            dirty = true;
        }
        last_x = mouse_x();
        last_y = mouse_y();
        if (!dirty && !ui_busy() && !mouse_has_event()) {
            __asm__ volatile("hlt");
        }
    }
}
