#include "desktop_priv.h"

static const struct rgb DESK_WHITE = { 0xF7, 0xF8, 0xFA };
static const struct rgb DESK_INK   = { 0x1A, 0x1A, 0x1C };
static const struct rgb DESK_MUTED = { 0x4A, 0x4A, 0x52 };

static void date_refresh(u8 day, u8 mon)
{
    const char *mn;

    if (day < 1u || day > 31u) {
        day = 1;
    }
    if (mon < 1u || mon > 12u) {
        mon = 1;
    }
    mn = i18n_month_abbr(mon);
    if (i18n_date_day_first()) {
        s_date[0] = (char)('0' + (day / 10u));
        s_date[1] = (char)('0' + (day % 10u));
        s_date[2] = ' ';
        s_date[3] = mn[0];
        s_date[4] = mn[1];
        s_date[5] = mn[2];
        s_date[6] = '\0';
    } else {
        s_date[0] = mn[0];
        s_date[1] = mn[1];
        s_date[2] = mn[2];
        s_date[3] = ' ';
        s_date[4] = (char)('0' + (day / 10u));
        s_date[5] = (char)('0' + (day % 10u));
        s_date[6] = '\0';
    }
}

void clock_refresh(void)
{
    struct rtc_time now;

    if (!rtc_read(&now)) {
        s_clock[0] = '0';
        s_clock[1] = '0';
        s_clock[2] = ':';
        s_clock[3] = '0';
        s_clock[4] = '0';
        s_clock[5] = '\0';
        date_refresh(1u, 1u);
        return;
    }
    s_clock[0] = (char)('0' + (now.hour / 10u));
    s_clock[1] = (char)('0' + (now.hour % 10u));
    s_clock[2] = ':';
    s_clock[3] = (char)('0' + (now.minute / 10u));
    s_clock[4] = (char)('0' + (now.minute % 10u));
    s_clock[5] = '\0';
    date_refresh(now.day, now.month);
}

static enum ui_icon battery_icon(void)
{
    u32 pct = power_battery_percent();

    if (power_on_ac() && pct < 95u) {
        return UI_ICON_BAT_CHARGE;
    }
    if (pct <= 20u) {
        return UI_ICON_BAT_LOW;
    }
    if (pct <= 65u) {
        return UI_ICON_BAT_HALF;
    }
    return UI_ICON_BAT_FULL;
}

bool clock_changed(void)
{
    struct rtc_time now;

    if (!rtc_read(&now)) {
        return false;
    }
    if (now.minute == (u8)s_last_min && now.day == s_last_day &&
        now.month == s_last_month && now.year == s_last_year) {
        return false;
    }
    s_last_min = now.minute;
    s_last_day = now.day;
    s_last_month = now.month;
    s_last_year = now.year;
    s_clock[0] = (char)('0' + (now.hour / 10u));
    s_clock[1] = (char)('0' + (now.hour % 10u));
    s_clock[2] = ':';
    s_clock[3] = (char)('0' + (now.minute / 10u));
    s_clock[4] = (char)('0' + (now.minute % 10u));
    s_clock[5] = '\0';
    date_refresh(now.day, now.month);
    return true;
}

void dock_geom(u32 *x, u32 *y, u32 *w, u32 *h)
{
    u32 screen_w = fb_width();
    u32 dock_w = DOCK_PADX * 2u + DOCK_APPS * DOCK_SLOT;

    if (dock_w + 16u > screen_w) {
        dock_w = screen_w > 16u ? screen_w - 16u : screen_w;
    }
    *w = dock_w;
    *h = DOCK_H;
    *x = screen_w > dock_w ? (screen_w - dock_w) / 2u : 0;
    *y = fb_height() > (DOCK_M + DOCK_H) ? fb_height() - DOCK_M - DOCK_H : 0;
}

void status_geom(u32 *x, u32 *y, u32 *w, u32 *h)
{
    u32 bat = 22u;

    *h = STAT_H;
    *w = 12u + bat + 12u;
    *x = fb_width() > *w + 22u ? fb_width() - *w - 22u : 8u;
    *y = 18u;
}

void icon_geom(u32 i, u32 *x, u32 *y, u32 *w, u32 *h)
{
    u32 sw = fb_width();
    u32 sh = fb_height();
    u32 cell_w = ui_px(128u);
    u32 cell_h = ui_px(108u);
    u32 gap = ui_px(28u);
    u32 cols = (sw >= ui_px(780u)) ? ICON_COUNT : 2u;
    u32 rows = (ICON_COUNT + cols - 1u) / cols;
    u32 grid_w = cols * cell_w + (cols - 1u) * gap;
    u32 grid_h = rows * cell_h + (rows - 1u) * gap;
    u32 usable = (sh > DOCK_H + DOCK_M + DESK_TOP)
                     ? (sh - DOCK_H - DOCK_M - DESK_TOP) : sh;
    u32 x0 = (sw > grid_w) ? (sw - grid_w) / 2u : 16u;
    u32 y0 = DESK_TOP + ((usable > grid_h) ? (usable - grid_h) / 2u : 8u);
    u32 col = i % cols;
    u32 row = i / cols;

    *x = x0 + col * (cell_w + gap);
    *y = y0 + row * (cell_h + gap);
    *w = cell_w;
    *h = cell_h;
}

void menu_geom(u32 *x, u32 *y, u32 *w, u32 *h)
{
    u32 dx;
    u32 dy;
    u32 dw;
    u32 dh;
    dock_geom(&dx, &dy, &dw, &dh);
    *w = 268u;
    *h = MENU_INSET * 2u + MENU_COUNT * MENU_ROW;
    *x = dock_slot_x(dx, 0);
    if (*x + *w + 8u > fb_width()) {
        *x = fb_width() > *w + 8u ? fb_width() - *w - 8u : 0;
    }
    *y = dy > *h + 12u ? dy - *h - 12u : 8u;
}

void ctx_geom(u32 *x, u32 *y, u32 *w, u32 *h)
{
    u32 sw = fb_width();
    u32 sh = fb_height();

    *w = 272u;
    *h = MENU_INSET * 2u + CTX_HEADER + CTX_COUNT * MENU_ROW + CTX_SEP;
    *x = (s_ctx_x > 0) ? (u32)s_ctx_x : 0;
    *y = (s_ctx_y > 0) ? (u32)s_ctx_y : 0;
    if (*x + *w + 8u > sw) {
        *x = sw > *w + 8u ? sw - *w - 8u : 0;
    }
    if (*y + *h + 8u > sh) {
        if ((u32)s_ctx_y > *h + 8u) {
            *y = (u32)s_ctx_y - *h;
        } else {
            *y = 8u;
        }
    }
}

static void draw_desktop_icons(void)
{
    const enum msg_id labels[ICON_COUNT] = {
        MSG_HOME_FILES, MSG_HOME_TERMINAL, MSG_HOME_SETTINGS, MSG_HOME_ABOUT
    };
    const enum ui_icon icons[ICON_COUNT] = {
        UI_ICON_FILES, UI_ICON_TERM, UI_ICON_SETTINGS, UI_ICON_ABOUT
    };
    u32 i;

    if (!s_show_icons) {
        return;
    }
    for (i = 0; i < ICON_COUNT; ++i) {
        u32 x;
        u32 y;
        u32 w;
        u32 h;
        u32 ix;
        u32 iy;
        bool hot = (s_hover == HIT_ICON + i);
        struct rgb label;
        icon_geom(i, &x, &y, &w, &h);
        label = hot ? THEME_ACCENT : DESK_INK;
        if (hot) {
            draw_glass(x, y, w, h > 24u ? h - 24u : h, 18, THEME_GLASS, 64u);
        }
        ix = x + (w > 48u ? (w - 48u) / 2u : 0);
        iy = y + 10u;
        draw_icon(ix, iy, 48, icons[i], hot ? THEME_ACCENT : DESK_INK);
        draw_text_centered(x + w / 2u, y + 68u, i18n(labels[i]), label, 1);
    }
}

static bool dock_app_running(enum win_kind kind)
{
    u32 i;

    for (i = 0; i < MAX_WIN; ++i) {
        if (s_win[i].used && s_win[i].kind == kind) {
            return true;
        }
    }
    return false;
}

static void draw_status(void)
{
    u32 sx;
    u32 sy;
    u32 sw;
    u32 sh;
    u32 bat = 22u;
    bool hot = (s_hover == HIT_STATUS);

    status_geom(&sx, &sy, &sw, &sh);
    draw_round_fill_hard(sx, sy, sw, sh, sh / 2u, THEME_GLASS, hot ? 240u : 230u);
    draw_icon(sx + (sw > bat ? (sw - bat) / 2u : 0),
              sy + (sh > bat ? (sh - bat) / 2u : 0),
              bat, battery_icon(), DESK_WHITE);
}

void draw_dock(void)
{
    const enum ui_icon apps[DOCK_APPS] = {
        UI_ICON_LAUNCHER, UI_ICON_FILES, UI_ICON_TERM,
        UI_ICON_SETTINGS, UI_ICON_ABOUT
    };
    const enum win_kind kinds[DOCK_APPS] = {
        WIN_FILES, WIN_FILES, WIN_TERM, WIN_SETTINGS, WIN_ABOUT
    };
    u32 dx;
    u32 dy;
    u32 dw;
    u32 dh;
    u32 i;
    bool launcher_hot = (s_hover == HIT_LAUNCHER) || s_menu;

    dock_geom(&dx, &dy, &dw, &dh);
    draw_round_fill_hard(dx, dy, dw, dh, dh / 2u, THEME_GLASS, 240u);

    for (i = 0; i < DOCK_APPS; ++i) {
        u32 sx = dock_slot_x(dx, i);
        u32 ix = sx + (DOCK_SLOT > DOCK_ICON ? (DOCK_SLOT - DOCK_ICON) / 2u : 0);
        u32 iy = dy + (dh > DOCK_ICON ? (dh - DOCK_ICON) / 2u : 0);
        bool hot;
        bool run;
        struct rgb col;

        if (i == 0u) {
            hot = launcher_hot;
            run = false;
        } else {
            hot = (s_hover == HIT_DOCK + (i - 1u));
            run = dock_app_running(kinds[i]);
        }
        if (hot) {
            draw_round_fill(sx + 6u, dy + 8u, DOCK_SLOT - 12u, dh - 16u, 14u,
                            THEME_HOVER, 170u);
        }
        col = (i == 0u) ? THEME_ACCENT : (hot ? THEME_ACCENT : THEME_FG);
        draw_icon(ix, iy, DOCK_ICON, apps[i], col);
        if (run) {
            u32 dot = sx + (DOCK_SLOT > 6u ? (DOCK_SLOT - 6u) / 2u : 0);
            draw_round_fill(dot, dy + dh - 11u, 6u, 6u, 3u, THEME_ACCENT, 255u);
        }
    }
}

void draw_taskbar(void)
{
    draw_dock();
    draw_status();
}

void draw_menu(void)
{
    const enum msg_id labels[MENU_COUNT] = {
        MSG_HOME_FILES, MSG_HOME_TERMINAL, MSG_HOME_SETTINGS,
        MSG_HOME_ABOUT, MSG_HOME_LOGOUT, MSG_HOME_POWER
    };
    const enum ui_icon icons[MENU_COUNT] = {
        UI_ICON_FILES, UI_ICON_TERM, UI_ICON_SETTINGS,
        UI_ICON_ABOUT, UI_ICON_LOGOUT, UI_ICON_POWER
    };
    u32 mx;
    u32 my;
    u32 mw;
    u32 mh;
    u32 i;

    menu_geom(&mx, &my, &mw, &mh);
    draw_round_fill_hard(mx, my, mw, mh, THEME_RAD_CARD, THEME_GLASS, 240u);
    for (i = 0; i < MENU_COUNT; ++i) {
        u32 ry = my + MENU_INSET + i * MENU_ROW;
        u32 text_y = ry + (MENU_ROW > FONT_HEIGHT ? (MENU_ROW - FONT_HEIGHT) / 2u : 0);
        u32 icon_y = ry + (MENU_ROW > 24u ? (MENU_ROW - 24u) / 2u : 0);
        bool hot = (s_hover == HIT_MENU + i);
        if (hot) {
            draw_round_fill(mx + MENU_INSET, ry + 2u, mw - MENU_INSET * 2u,
                            MENU_ROW - 4u, (MENU_ROW - 4u) / 2u, THEME_HOVER, 210u);
        }
        draw_icon(mx + 16, icon_y, 24, icons[i], THEME_FG);
        draw_text(mx + 50, text_y, i18n(labels[i]),
                  hot ? THEME_ACCENT : THEME_FG, 1);
    }
}

static void draw_ctx(void)
{
    const enum msg_id labels[CTX_COUNT] = {
        MSG_WP_DEFAULT, MSG_WP_ABSTRACT, MSG_HOME_SETTINGS,
        MSG_HOME_ABOUT, MSG_CTX_SHOW_ICONS
    };
    const enum ui_icon icons[CTX_COUNT] = {
        UI_ICON_ABOUT, UI_ICON_ABOUT, UI_ICON_SETTINGS,
        UI_ICON_ABOUT, UI_ICON_FILES
    };
    u32 mx;
    u32 my;
    u32 mw;
    u32 mh;
    u32 i;
    u32 rows_y;
    u32 desk_sel = wallpaper_desk_id();

    ctx_geom(&mx, &my, &mw, &mh);
    draw_round_fill_hard(mx, my, mw, mh, THEME_RAD_CARD, THEME_GLASS, 240u);
    draw_text(mx + 18u, my + MENU_INSET + 6u, i18n(MSG_CTX_TITLE),
              THEME_FG_DIM, 1);
    rows_y = my + MENU_INSET + CTX_HEADER;
    for (i = 0; i < CTX_COUNT; ++i) {
        u32 extra = (i >= 2u) ? CTX_SEP : 0;
        u32 ry = rows_y + extra + i * MENU_ROW;
        u32 text_y = ry + (MENU_ROW > FONT_HEIGHT ? (MENU_ROW - FONT_HEIGHT) / 2u : 0);
        u32 icon_y = ry + (MENU_ROW > 24u ? (MENU_ROW - 24u) / 2u : 0);
        bool hot = (s_hover == HIT_CTX + i);
        bool on = (i < 2u && desk_sel == i);
        const char *label;

        if (i == 2u) {
            fb_fill_rect(mx + 16u, ry - 4u, mw > 32u ? mw - 32u : mw, 1u,
                         THEME_BORDER.r, THEME_BORDER.g, THEME_BORDER.b);
        }
        if (hot) {
            draw_round_fill(mx + MENU_INSET, ry + 2u, mw - MENU_INSET * 2u,
                            MENU_ROW - 4u, (MENU_ROW - 4u) / 2u, THEME_HOVER, 210u);
        }
        if (i == 4u) {
            label = i18n(s_show_icons ? MSG_CTX_HIDE_ICONS : MSG_CTX_SHOW_ICONS);
        } else {
            label = i18n(labels[i]);
        }
        if (i < 2u) {
            draw_round_fill(mx + 18u, icon_y + 4u, 16u, 16u, 5u,
                            on ? THEME_ACCENT : THEME_BORDER, on ? 255u : 180u);
        } else {
            draw_icon(mx + 16u, icon_y, 24u, icons[i], THEME_FG);
        }
        draw_text(mx + 50u, text_y, label, (hot || on) ? THEME_ACCENT : THEME_FG, 1);
    }
}

void paint_desktop_base(void)
{
    u32 w = fb_width();

    draw_bg_atmosphere();
    fb_overlay(THEME_BG0.r, THEME_BG0.g, THEME_BG0.b, 18u);
    if (w > 720u) {
        draw_text(24u, 22u, name_os, DESK_MUTED, 1);
    }
    draw_text_centered(w / 2u, 18u, s_clock, DESK_INK, 2);
    draw_text_centered(w / 2u, 18u + FONT_HEIGHT * 2u + 4u, s_date, DESK_MUTED, 1);
    draw_desktop_icons();
}

void desktop_redraw(void)
{
    fb_compose_begin();
    paint_desktop_base();
    paint_windows();
    draw_taskbar();
    if (s_menu) {
        draw_menu();
    }
    if (s_ctx) {
        draw_ctx();
    }
    if (s_spot) {
        fb_overlay(0, 0, 0, 110u);
        draw_spot();
    }
    if (s_spot_need_xfade && s_spot_fade_from != NULL) {
        s_spot_need_xfade = false;
        /* Blend the last on-screen frame into this scene so the dim does
         * not land as a single memcpy wipe on the live LFB. */
        ui_crossfade_from_n(s_spot_fade_from, 10u);
    }
    ui_comp_mark_full();
    ui_comp_present();
}
