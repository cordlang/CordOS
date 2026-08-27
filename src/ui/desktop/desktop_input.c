#include "desktop_priv.h"

bool is_double_click(u32 hit)
{
    const u32 dbl_ms = 350u;
    u32 now = time_uptime_ms();
    bool dbl = (hit == s_last_click_hit) && (now - s_last_click_ms) <= dbl_ms;

    s_last_click_hit = hit;
    s_last_click_ms = now;
    return dbl;
}

static void wallpaper_apply(u32 id)
{
    wallpaper_set_login(id);
    wallpaper_set_desk(id);
}

void desktop_cursor_update(bool scene_is_back)
{
    i32 mx = mouse_x();
    i32 my = mouse_y();
    bool moved = !s_cursor_valid || mx != s_cursor_x || my != s_cursor_y;
    u32 hit;
    enum cursor_kind kind;

    if (!scene_is_back && !moved) {
        return;
    }
    if (scene_is_back) {
        cursor_invalidate();
    } else {
        cursor_hide();
    }
    if (mx < 0) {
        mx = 0;
    }
    if (my < 0) {
        my = 0;
    }
    hit = hit_test(mx, my);
    kind = desktop_cursor_kind(hit);
    cursor_set_kind(kind);
    s_cursor_x = mx;
    s_cursor_y = my;
    s_cursor_valid = true;
}

u32 hit_test(i32 px, i32 py)
{
    u32 i;
    u32 x;
    u32 y;
    u32 iw;
    u32 ih;

    if (s_spot) {
        u32 sx;
        u32 sy;
        u32 sw;
        u32 sh;

        spot_geom(&sx, &sy, &sw, &sh);
        if (in_rect(px, py, (i32)sx, (i32)sy, (i32)sw, (i32)sh)) {
            u32 row;
            u32 ry;

            if (py < (i32)(sy + SPOT_BAR_H)) {
                return HIT_SPOT;
            }
            for (row = 0; row < s_spot_n && row < SPOT_MAX; ++row) {
                ry = sy + SPOT_BAR_H + row * SPOT_ROW_H;
                if (in_rect(px, py, (i32)sx, (i32)ry, (i32)sw,
                            (i32)SPOT_ROW_H)) {
                    return HIT_SPOT_ROW + row;
                }
            }
            return HIT_SPOT;
        }
    }

    if (s_ctx) {
        u32 cx;
        u32 cy;
        u32 cw;
        u32 ch;
        ctx_geom(&cx, &cy, &cw, &ch);
        if (in_rect(px, py, (i32)cx, (i32)cy, (i32)cw, (i32)ch)) {
            u32 i;
            u32 rows_y = cy + MENU_INSET + CTX_HEADER;
            if (py < (i32)rows_y) {
                return HIT_BAR;
            }
            for (i = 0; i < CTX_COUNT; ++i) {
                u32 extra = (i >= 2u) ? CTX_SEP : 0;
                u32 ry = rows_y + extra + i * MENU_ROW;
                if (in_rect(px, py, (i32)cx, (i32)ry, (i32)cw, (i32)MENU_ROW)) {
                    return HIT_CTX + i;
                }
            }
            return HIT_BAR;
        }
    }

    if (s_menu) {
        u32 mx;
        u32 my;
        u32 mw;
        u32 mh;
        menu_geom(&mx, &my, &mw, &mh);
        if (in_rect(px, py, (i32)mx, (i32)my, (i32)mw, (i32)mh)) {
            i32 rel = py - (i32)my - (i32)MENU_INSET;
            u32 row;
            if (rel < 0) {
                return HIT_BAR;
            }
            row = (u32)rel / MENU_ROW;
            if (row < MENU_COUNT) {
                return HIT_MENU + row;
            }
            return HIT_BAR;
        }
    }

    for (i = s_zcount; i > 0; --i) {
        u32 id = s_z[i - 1u];
        struct window *w = &s_win[id];
        i32 wx = w->x;
        i32 wy = w->y;
        i32 ww = (i32)w->w;
        i32 wh = (i32)w->h;

        if (!w->used) {
            continue;
        }
        if (!in_rect(px, py, wx, wy, ww, wh)) {
            continue;
        }
        {
            u32 cx;
            u32 cy;
            u32 cs;

            draw_window_close_rect((u32)wx, (u32)wy, w->w, &cx, &cy, &cs);
            if (in_rect(px, py, (i32)cx, (i32)cy, (i32)cs, (i32)cs)) {
                return HIT_CLOSE + id;
            }
        }
        if (in_rect(px, py, wx, wy, ww, (i32)TITLE_H)) {
            return HIT_TITLE + id;
        }
        if (w->kind == WIN_FILES) {
            u32 f;
            for (f = 0; f < s_files.count; ++f) {
                if (in_rect(px, py, wx + 12,
                            wy + (i32)TITLE_H + 12 + (i32)FILE_ROW +
                                (i32)f * (i32)FILE_ROW,
                            188, (i32)FILE_ROW)) {
                    return HIT_FILE + f;
                }
            }
        }
        if (w->kind == WIN_SETTINGS) {
            u32 lang_y;
            u32 wp_y;
            u32 ic_y;
            u32 s;

            settings_layout(w, &lang_y, &wp_y, &ic_y);
            {
                u32 li;
                u32 n = i18n_lang_count();

                if (n > HIT_LANG_MAX) {
                    n = HIT_LANG_MAX;
                }
                for (li = 0; li < n; ++li) {
                    u32 lx;
                    u32 ly;

                    settings_lang_btn((u32)wx, lang_y, li, &lx, &ly);
                    if (in_rect(px, py, (i32)lx, (i32)ly, 140, 40)) {
                        return HIT_LANG + li;
                    }
                }
            }
            if (in_rect(px, py, wx + 24, (i32)wp_y, (i32)WP_THUMB_W,
                        (i32)WP_THUMB_H)) {
                return HIT_WP_0;
            }
            if (in_rect(px, py, wx + 24 + (i32)WP_THUMB_W + (i32)WP_THUMB_GAP,
                        (i32)wp_y, (i32)WP_THUMB_W, (i32)WP_THUMB_H)) {
                return HIT_WP_1;
            }
            for (s = 0; s < ICON_STYLE_COUNT; ++s) {
                i32 ix = wx + 24 + (i32)s * (i32)(IC_THUMB + IC_THUMB_GAP);
                if (in_rect(px, py, ix, (i32)ic_y, (i32)IC_THUMB,
                            (i32)IC_THUMB)) {
                    return HIT_IC_0 + s;
                }
            }
        }
        if (w->kind == WIN_ACTIVITY) {
            u32 lx;
            u32 ly;
            u32 lw;
            u32 rh;
            u32 rgap;
            u32 dx;
            u32 dy;
            u32 dw;
            u32 dh;
            u32 r;

            act_layout(w, &lx, &ly, &lw, &rh, &rgap, &dx, &dy, &dw, &dh);
            for (r = 0; r < ACT_N; r++) {
                u32 ry = ly + r * (rh + rgap);

                if (in_rect(px, py, (i32)lx, (i32)ry, (i32)lw, (i32)rh)) {
                    return HIT_ACT + r;
                }
            }
        }
        if (w->kind == WIN_POWER) {
            i32 by = wy + (i32)TITLE_H + 16 + (i32)FONT_LINE + 12;
            if (in_rect(px, py, wx + 24, by, 140, 40)) {
                return HIT_POWER_OK;
            }
            if (in_rect(px, py, wx + 180, by, 140, 40)) {
                return HIT_POWER_NO;
            }
        }
        return HIT_BODY + id;
    }

    {
        u32 dx;
        u32 dy;
        u32 dw;
        u32 dh;
        dock_geom(&dx, &dy, &dw, &dh);
        if (in_rect(px, py, (i32)dx, (i32)dy, (i32)dw, (i32)dh)) {
            u32 s;
            for (s = 0; s < DOCK_APPS; ++s) {
                u32 sx = dock_slot_x(dx, s);
                if (in_rect(px, py, (i32)sx, (i32)dy, (i32)DOCK_SLOT, (i32)dh)) {
                    if (s == 0u) {
                        return HIT_LAUNCHER;
                    }
                    return HIT_DOCK + (s - 1u);
                }
            }
            return HIT_BAR;
        }
    }

    {
        u32 sx;
        u32 sy;
        u32 sw;
        u32 sh;
        status_geom(&sx, &sy, &sw, &sh);
        if (in_rect(px, py, (i32)sx, (i32)sy, (i32)sw, (i32)sh)) {
            return HIT_STATUS;
        }
    }

    if (s_show_icons) {
        for (i = 0; i < ICON_COUNT; ++i) {
            icon_geom(i, &x, &y, &iw, &ih);
            if (in_rect(px, py, (i32)x, (i32)y, (i32)iw, (i32)ih)) {
                return HIT_ICON + i;
            }
        }
    }
    return HIT_DESKTOP;
}

enum cursor_kind desktop_cursor_kind(u32 hit)
{
    if (hit >= HIT_ICON && hit < HIT_ICON + ICON_COUNT) {
        return CURSOR_KIND_POINTER;
    }
    if (hit == HIT_LAUNCHER) {
        return CURSOR_KIND_POINTER;
    }
    if (hit >= HIT_MENU && hit < HIT_MENU + MENU_COUNT) {
        return CURSOR_KIND_POINTER;
    }
    if (hit >= HIT_TASK && hit < HIT_TASK + MAX_WIN) {
        return CURSOR_KIND_POINTER;
    }
    if (hit >= HIT_DOCK && hit < HIT_DOCK + ICON_COUNT) {
        return CURSOR_KIND_POINTER;
    }
    if (hit >= HIT_CTX && hit < HIT_CTX + CTX_COUNT) {
        return CURSOR_KIND_POINTER;
    }
    if (hit == HIT_SPOT ||
        (hit >= HIT_SPOT_ROW && hit < HIT_SPOT_ROW + SPOT_MAX)) {
        return CURSOR_KIND_POINTER;
    }
    if (hit >= HIT_CLOSE && hit < HIT_CLOSE + MAX_WIN) {
        return CURSOR_KIND_POINTER;
    }
    if (hit >= HIT_FILE && hit < HIT_FILE + MAX_FILES) {
        return CURSOR_KIND_POINTER;
    }
    if ((hit >= HIT_LANG && hit < HIT_LANG + HIT_LANG_MAX) ||
        hit == HIT_POWER_OK || hit == HIT_POWER_NO ||
        hit == HIT_WP_0 || hit == HIT_WP_1 ||
        (hit >= HIT_IC_0 && hit < HIT_IC_0 + ICON_STYLE_COUNT) ||
        (hit >= HIT_ACT && hit < HIT_ACT + ACT_N)) {
        return CURSOR_KIND_POINTER;
    }
    return CURSOR_KIND_ARROW;
}

void desktop_drag(u32 id, i32 old_x, i32 old_y)
{
    (void)id;
    (void)old_x;
    (void)old_y;

    /* The partial path repainted complete overlapping windows outside the
     * restored damage rect. Keep drag rendering coherent until it has true
     * per-window clipping. */
    desktop_redraw();
}

static void power_halt(void)
{
    cursor_hide();
    fb_compose_begin();
    draw_bg_atmosphere();
    draw_text(48, 120, name_os, THEME_INK, 2);
    draw_text(48, 180, i18n(MSG_POWER_MSG), THEME_INK, 1);
    cursor_invalidate();
    fb_compose_present();
    machine_power_off();
}

void handle_click(u32 hit, bool dbl)
{
    if (s_spot) {
        if (hit >= HIT_SPOT_ROW && hit < HIT_SPOT_ROW + SPOT_MAX) {
            spot_activate(hit - HIT_SPOT_ROW);
            return;
        }
        if (hit == HIT_SPOT) {
            return;
        }
        spot_close();
        if (hit == HIT_DESKTOP || hit == HIT_BAR || hit == HIT_STATUS) {
            return;
        }
    }
    if (hit >= HIT_CTX && hit < HIT_CTX + CTX_COUNT) {
        u32 item = hit - HIT_CTX;
        s_ctx = false;
        if (item == 0u) {
            wallpaper_apply(DESK_WP_DEFAULT);
        } else if (item == 1u) {
            wallpaper_apply(DESK_WP_ABSTRACT);
        } else if (item == 2u) {
            (void)win_open(WIN_SETTINGS);
        } else if (item == 3u) {
            (void)win_open(WIN_ABOUT);
        } else if (item == 4u) {
            s_show_icons = !s_show_icons;
        }
        return;
    }
    s_ctx = false;
    if (hit >= HIT_MENU && hit < HIT_MENU + MENU_COUNT) {
        action_open(hit - HIT_MENU);
        return;
    }
    if (hit >= HIT_DOCK && hit < HIT_DOCK + ICON_COUNT) {
        if (dbl) {
            action_open(hit - HIT_DOCK);
        }
        return;
    }
    if (hit >= HIT_ICON && hit < HIT_ICON + ICON_COUNT) {
        if (dbl) {
            action_open(hit - HIT_ICON);
        }
        return;
    }
    if (hit >= HIT_CLOSE && hit < HIT_CLOSE + MAX_WIN) {
        win_close(hit - HIT_CLOSE);
        return;
    }
    if (hit >= HIT_TITLE && hit < HIT_TITLE + MAX_WIN) {
        u32 id = hit - HIT_TITLE;
        z_raise(id);
        s_drag = (i32)id;
        s_dx = mouse_x() - s_win[id].x;
        s_dy = mouse_y() - s_win[id].y;
        return;
    }
    if (hit >= HIT_TASK && hit < HIT_TASK + MAX_WIN) {
        z_raise(hit - HIT_TASK);
        s_menu = false;
        return;
    }
    if (hit >= HIT_ACT && hit < HIT_ACT + ACT_N) {
        u32 wi;

        for (wi = 0; wi < MAX_WIN; ++wi) {
            if (s_win[wi].used && s_win[wi].kind == WIN_ACTIVITY) {
                s_win[wi].file_sel = hit - HIT_ACT;
                z_raise(wi);
                break;
            }
        }
        return;
    }
    if (hit >= HIT_BODY && hit < HIT_BODY + MAX_WIN) {
        z_raise(hit - HIT_BODY);
        return;
    }
    if (hit >= HIT_FILE && hit < HIT_FILE + MAX_FILES) {
        i32 top = z_top();
        if (top >= 0 && s_win[top].kind == WIN_FILES) {
            files_preview(&s_win[top], hit - HIT_FILE);
        }
        return;
    }
    if (hit >= HIT_LANG && hit < HIT_LANG + HIT_LANG_MAX) {
        u32 li = hit - HIT_LANG;

        if (li < i18n_lang_count()) {
            i18n_set_lang((enum lang_id)li);
            (void)persist_set_u32("lang", li);
            clock_refresh();
        }
        return;
    }
    if (hit == HIT_WP_0) {
        wallpaper_apply(DESK_WP_DEFAULT);
        return;
    }
    if (hit == HIT_WP_1) {
        wallpaper_apply(DESK_WP_ABSTRACT);
        return;
    }
    if (hit >= HIT_IC_0 && hit < HIT_IC_0 + ICON_STYLE_COUNT) {
        icon_set_style(hit - HIT_IC_0);
        return;
    }
    if (hit == HIT_POWER_OK) {
        power_halt();
    }
    if (hit == HIT_POWER_NO) {
        i32 top = z_top();
        if (top >= 0 && s_win[top].kind == WIN_POWER) {
            win_close((u32)top);
        }
        return;
    }
    if (hit == HIT_LAUNCHER) {
        s_ctx = false;
        spot_close();
        s_menu = !s_menu;
        return;
    }
    if (hit == HIT_DESKTOP || hit == HIT_BAR || hit == HIT_STATUS) {
        s_menu = false;
        s_ctx = false;
        spot_close();
        return;
    }
    s_ctx = false;
}

void handle_right_click(u32 hit)
{
    if (s_spot) {
        spot_close();
        return;
    }
    if (hit == HIT_DESKTOP) {
        s_menu = false;
        s_ctx = true;
        s_ctx_x = mouse_x();
        s_ctx_y = mouse_y();
        return;
    }
    s_ctx = false;
}

void handle_key(u32 key)
{
    i32 top = z_top();
    struct window *w;

    if (key == KEY_SPOTLIGHT) {
        if (s_spot) {
            spot_close();
        } else {
            spot_open();
        }
        return;
    }
    if (s_spot) {
        if (key == 27u) {
            spot_close();
            return;
        }
        if (key == KEY_DOWN) {
            if (s_spot_n > 0u) {
                s_spot_sel = (s_spot_sel + 1u) % s_spot_n;
            }
            return;
        }
        if (key == KEY_UP) {
            if (s_spot_n > 0u) {
                s_spot_sel = (s_spot_sel == 0u) ? (s_spot_n - 1u) : (s_spot_sel - 1u);
            }
            return;
        }
        if (key == '\n' || key == '\r') {
            spot_activate(s_spot_sel);
            return;
        }
        if (key == '\b' || key == 127u) {
            spot_backspace();
            return;
        }
        spot_append(key);
        return;
    }
    if (key == KEY_F1) {
        shell_run();
        return;
    }
    if (key == 27u) {
        if (s_ctx) {
            s_ctx = false;
            return;
        }
        if (s_menu) {
            s_menu = false;
            return;
        }
        if (top >= 0) {
            win_close((u32)top);
        }
        return;
    }
    if (top < 0) {
        return;
    }
    w = &s_win[top];
    if (w->kind != WIN_TERM) {
        return;
    }
    if (key == '\n' || key == '\r') {
        term_exec(w);
        if (w->term_len == 0xFFFFu) {
            win_close((u32)top);
        }
        return;
    }
    if (key == '\b' || key == 127u) {
        if (w->term_len > 0 && w->term_len < 0xFFFFu) {
            w->term_len--;
            w->term_input[w->term_len] = '\0';
        }
        return;
    }
    if (key < 32u || key_is_special(key) || key > 0x7Fu) {
        return;
    }
    if (w->term_len + 1u < TERM_COLS) {
        w->term_input[w->term_len] = (char)key;
        w->term_len++;
        w->term_input[w->term_len] = '\0';
    }
}

bool desk_chrome_hit(u32 hit)
{
    if (hit == HIT_LAUNCHER) {
        return true;
    }
    if (hit >= HIT_DOCK && hit < HIT_DOCK + ICON_COUNT) {
        return true;
    }
    if (hit >= HIT_MENU && hit < HIT_MENU + MENU_COUNT) {
        return true;
    }
    if (hit == HIT_SPOT) {
        return true;
    }
    if (hit >= HIT_SPOT_ROW && hit < HIT_SPOT_ROW + SPOT_MAX) {
        return true;
    }
    return false;
}

bool desktop_widgets(void)
{
    u32 dx;
    u32 dy;
    u32 dw;
    u32 dh;
    u32 i;
    bool need_full = false;
    bool full = ui_comp_is_full();
    bool chrome_changed = s_widget_last_hover != s_hover &&
                          (desk_chrome_hit(s_widget_last_hover) ||
                           desk_chrome_hit(s_hover));
    bool repaint_base = full || s_widget_repaint_base || chrome_changed;

    /* Partial dock paint is undimmed and punches a hole through Spotlight. */
    if (s_spot) {
        repaint_base = false;
    }

    dock_geom(&dx, &dy, &dw, &dh);
    if (repaint_base) {
        fb_compose_begin();
        draw_dock();
        ui_comp_damage(dx, dy, dw, dh);
        if (s_menu) {
            u32 mx;
            u32 my;
            u32 mw;
            u32 mh;

            menu_geom(&mx, &my, &mw, &mh);
            draw_menu();
            ui_comp_damage(mx, my, mw, mh);
        }
    } else {
        desktop_cursor_update(false);
    }
    ui_begin(mouse_x(), mouse_y(), mouse_buttons(), time_uptime_ms());
    ui_set_cursor_kind(desktop_cursor_kind(hit_test(mouse_x(), mouse_y())));
    for (i = 0; i < DOCK_APPS; ++i) {
        u32 sx = dock_slot_x(dx, i);

        if (ui_clicked(0xD00u + i, sx, dy, DOCK_SLOT, dh)) {
            if (i == 0u) {
                s_menu = !s_menu;
                s_ctx = false;
                if (s_menu) {
                    spot_close();
                }
                need_full = true;
            } else {
                s_menu = false;
                s_ctx = false;
                if (is_double_click(HIT_DOCK + (i - 1u))) {
                    action_open(i - 1u);
                    need_full = true;
                }
            }
        }
    }
    if (s_menu) {
        u32 mx;
        u32 my;
        u32 mw;
        u32 mh;

        menu_geom(&mx, &my, &mw, &mh);
        for (i = 0; i < MENU_COUNT; ++i) {
            u32 ry = my + MENU_INSET + i * MENU_ROW;

            if (ui_clicked(0xE00u + i, mx + MENU_INSET, ry + 2u,
                           mw > MENU_INSET * 2u ? mw - MENU_INSET * 2u : mw,
                           MENU_ROW > 4u ? MENU_ROW - 4u : MENU_ROW)) {
                action_open(i);
                need_full = true;
            }
        }
    }
    if (s_spot) {
        u32 sx;
        u32 sy;
        u32 sw;
        u32 sh;

        spot_geom(&sx, &sy, &sw, &sh);
        for (i = 0; i < s_spot_n; ++i) {
            u32 ry = sy + SPOT_BAR_H + i * SPOT_ROW_H;

            if (ui_clicked(0xF20u + i, sx + 10u, ry + 4u,
                           sw > 20u ? sw - 20u : sw,
                           SPOT_ROW_H > 8u ? SPOT_ROW_H - 8u : SPOT_ROW_H)) {
                spot_activate(i);
                need_full = true;
            }
        }
    }
    if (repaint_base) {
        desktop_cursor_update(true);
    }
    ui_end();
    s_widget_last_hover = s_hover;
    s_widget_repaint_base = ui_busy();
    return need_full;
}
