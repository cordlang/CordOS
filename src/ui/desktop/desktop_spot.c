#include "desktop_priv.h"

void spot_geom(u32 *x, u32 *y, u32 *w, u32 *h)
{
    u32 sw = fb_width();
    u32 sh = fb_height();
    u32 rows = 0;
    u32 pad = 0;

    *w = 680u;
    if (*w + 48u > sw) {
        *w = sw > 48u ? sw - 48u : sw;
    }
    if (s_spot_n > 0u) {
        rows = s_spot_n * SPOT_ROW_H;
        pad = 10u;
    } else if (s_spot_q[0] != '\0') {
        rows = SPOT_ROW_H;
        pad = 10u;
    }
    *h = SPOT_BAR_H + rows + pad;
    *x = sw > *w ? (sw - *w) / 2u : 0;
    *y = sh > *h ? (sh - *h) / 2u : 16u;
}

static int spot_fold_eq_at(const char *hay, const char *needle, u32 off)
{
    u32 i = 0;
    char hc;
    char nc;

    while (needle[i] != '\0') {
        hc = hay[off + i];
        nc = needle[i];
        if (hc == '\0') {
            return 0;
        }
        if (hc >= 'A' && hc <= 'Z') {
            hc = (char)(hc + 32);
        }
        if (nc >= 'A' && nc <= 'Z') {
            nc = (char)(nc + 32);
        }
        if (hc != nc) {
            return 0;
        }
        ++i;
    }
    return 1;
}

static int spot_match(const char *hay, const char *needle)
{
    u32 i;

    if (needle == NULL || needle[0] == '\0') {
        return 1;
    }
    if (hay == NULL) {
        return 0;
    }
    for (i = 0; hay[i] != '\0'; ++i) {
        if (spot_fold_eq_at(hay, needle, i)) {
            return 1;
        }
    }
    return 0;
}

static void spot_add(enum spot_kind kind, u32 arg, enum ui_icon icon,
                    enum msg_id cat, const char *title)
{
    if (s_spot_n >= SPOT_MAX || title == NULL) {
        return;
    }
    s_spot_hits[s_spot_n].kind = kind;
    s_spot_hits[s_spot_n].arg = arg;
    s_spot_hits[s_spot_n].icon = icon;
    s_spot_hits[s_spot_n].cat = cat;
    copy_n(s_spot_hits[s_spot_n].title, 36, title);
    s_spot_n++;
}

static void spot_rebuild(void)
{
    static const enum msg_id labels[MENU_COUNT] = {
        MSG_HOME_FILES, MSG_HOME_TERMINAL, MSG_HOME_SETTINGS,
        MSG_HOME_ABOUT, MSG_HOME_LOGOUT, MSG_HOME_POWER
    };
    static const enum ui_icon icons[MENU_COUNT] = {
        UI_ICON_FILES, UI_ICON_TERM, UI_ICON_SETTINGS,
        UI_ICON_ABOUT, UI_ICON_LOGOUT, UI_ICON_POWER
    };
    static const char *alias[MENU_COUNT] = {
        "files archivos explorer",
        "terminal shell consola cmd",
        "settings ajustes config",
        "about acerca info",
        "logout salir sesion sign out",
        "power apagar shutdown halt"
    };
    const char *q = s_spot_q;
    u32 i;

    s_spot_n = 0;
    for (i = 0; i < MENU_COUNT; ++i) {
        const char *title = i18n(labels[i]);

        if (spot_match(title, q) || spot_match(alias[i], q)) {
            spot_add(SPOT_APP, i, icons[i], MSG_SPOTLIGHT_APPS, title);
        }
    }
    files_reload();
    if (q[0] != '\0') {
        for (i = 0; i < s_files.count && s_spot_n < SPOT_MAX; ++i) {
            if (spot_match(s_files.names[i], q)) {
                spot_add(SPOT_FILE, i, UI_ICON_FILES, MSG_SPOTLIGHT_FILES,
                         s_files.names[i]);
            }
        }
    }
    if (s_spot_sel >= s_spot_n) {
        s_spot_sel = s_spot_n > 0u ? s_spot_n - 1u : 0;
    }
}

static void spot_arm_xfade(void)
{
    if (!fb_compose_ready()) {
        return;
    }
    if (s_spot_fade_from == NULL) {
        s_spot_fade_from = fb_layer_alloc();
    }
    if (s_spot_fade_from != NULL) {
        fb_copy_front(s_spot_fade_from);
        s_spot_need_xfade = true;
    }
}

void spot_open(void)
{
    s_menu = false;
    s_ctx = false;
    spot_arm_xfade();
    s_spot = true;
    s_spot_q[0] = '\0';
    s_spot_len = 0;
    s_spot_sel = 0;
    spot_rebuild();
}

void spot_close(void)
{
    if (s_spot) {
        spot_arm_xfade();
    }
    s_spot = false;
    s_spot_q[0] = '\0';
    s_spot_len = 0;
    s_spot_n = 0;
    s_spot_sel = 0;
}

void spot_activate(u32 index)
{
    struct spot_item hit;

    if (index >= s_spot_n) {
        return;
    }
    hit = s_spot_hits[index];
    spot_close();
    if (hit.kind == SPOT_APP) {
        action_open(hit.arg);
        return;
    }
    {
        i32 id = win_open(WIN_FILES);

        if (id >= 0) {
            files_preview(&s_win[id], hit.arg);
        }
    }
}

void spot_append(u32 code)
{
    char tmp[4];
    u32 n;
    u32 i;

    if (code < 32u || code == 127u || key_is_special(code)) {
        return;
    }
    n = utf8_encode(code, tmp);
    if (n == 0 || s_spot_len + n >= SPOT_QMAX) {
        return;
    }
    for (i = 0; i < n; ++i) {
        s_spot_q[s_spot_len++] = tmp[i];
    }
    s_spot_q[s_spot_len] = '\0';
    s_spot_sel = 0;
    spot_rebuild();
}

void spot_backspace(void)
{
    if (s_spot_len == 0) {
        return;
    }
    s_spot_len--;
    while (s_spot_len > 0 &&
           (((u8)s_spot_q[s_spot_len]) & 0xC0u) == 0x80u) {
        s_spot_len--;
    }
    s_spot_q[s_spot_len] = '\0';
    s_spot_sel = 0;
    spot_rebuild();
}

static void draw_spot_lens(u32 x, u32 y, u32 size, struct rgb color)
{
    u32 hole = size > 8u ? size - 8u : size / 2u;
    u32 hx;
    u32 hy;

    if (size < 10u) {
        size = 10u;
        hole = 4u;
    }
    draw_round_fill(x, y, size, size, size / 2u, color, 210u);
    hx = x + (size > hole ? (size - hole) / 2u : 0);
    hy = y + (size > hole ? (size - hole) / 2u : 0);
    draw_round_fill(hx, hy, hole, hole, hole / 2u, THEME_GLASS, 255u);
    fb_fill_rect(x + size - 4u, y + size - 3u, size / 2u, 3u, color.r, color.g,
                 color.b);
}

void draw_spot(void)
{
    u32 x;
    u32 y;
    u32 w;
    u32 h;
    u32 i;
    u32 tx;
    u32 ty;
    u32 text_h = 22u;
    struct rgb panel = { 0x1A, 0x1A, 0x1E };
    struct rgb sel = { 0x32, 0x5A, 0xC8 };
    const char *show;

    spot_geom(&x, &y, &w, &h);
    draw_round_fill_hard(x, y, w, h, 22u, panel, 240u);

    draw_spot_lens(x + 22u, y + (SPOT_BAR_H > 22u ? (SPOT_BAR_H - 22u) / 2u : 0),
                   22u, THEME_FG_DIM);

    tx = x + 56u;
    ty = y + (SPOT_BAR_H > text_h ? (SPOT_BAR_H - text_h) / 2u : 0);
    if (s_spot_q[0] != '\0') {
        show = s_spot_q;
        draw_text_h(tx, ty, show, THEME_FG, text_h);
        fb_fill_rect(tx + draw_text_width_h(show, text_h) + 3u, ty + 2u, 2u,
                     text_h > 6u ? text_h - 6u : text_h, THEME_FG.r, THEME_FG.g,
                     THEME_FG.b);
    } else {
        draw_text_h(tx, ty, i18n(MSG_SPOTLIGHT_PLACE), THEME_FG_DIM, text_h);
        fb_fill_rect(tx, ty + 2u, 2u, text_h > 6u ? text_h - 6u : text_h,
                     THEME_FG.r, THEME_FG.g, THEME_FG.b);
    }

    if (s_spot_n == 0u) {
        if (s_spot_q[0] != '\0') {
            draw_text(x + 24u, y + SPOT_BAR_H + 8u, i18n(MSG_SPOTLIGHT_NONE),
                      THEME_FG_DIM, 1);
        }
        return;
    }

    fb_fill_rect(x + 18u, y + SPOT_BAR_H - 1u, w > 36u ? w - 36u : w, 1u,
                 THEME_BORDER.r, THEME_BORDER.g, THEME_BORDER.b);

    for (i = 0; i < s_spot_n; ++i) {
        u32 ry = y + SPOT_BAR_H + i * SPOT_ROW_H;
        u32 iy = ry + (SPOT_ROW_H > 28u ? (SPOT_ROW_H - 28u) / 2u : 0);
        u32 title_y = ry + 8u;
        bool hot = (i == s_spot_sel);
        u32 cat_w;

        if (hot) {
            draw_round_fill(x + 10u, ry + 4u, w > 20u ? w - 20u : w,
                            SPOT_ROW_H > 8u ? SPOT_ROW_H - 8u : SPOT_ROW_H, 12u,
                            sel, 220u);
        }
        draw_icon(x + 22u, iy, 28u, s_spot_hits[i].icon,
                  hot ? THEME_FG : THEME_FG_DIM);
        draw_text_clip(x + 62u, title_y, x + w - 160u, s_spot_hits[i].title,
                       THEME_FG, 1);
        cat_w = draw_text_width(i18n(s_spot_hits[i].cat), 1);
        draw_text(x + w - 24u - cat_w, title_y + 2u, i18n(s_spot_hits[i].cat),
                  hot ? THEME_FG : THEME_FG_DIM, 1);
    }
}
