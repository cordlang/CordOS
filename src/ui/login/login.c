#include "gfx_session.h"
#include "animation.h"
#include "brand.h"
#include "compositor.h"
#include "config.h"
#include "desktop.h"
#include "draw.h"
#include "fb.h"
#include "metrics.h"
#include "widget.h"
#include "font.h"
#include "i18n.h"
#include "io.h"
#include "keyboard.h"
#include "keycodes.h"
#include "mouse.h"
#include "power.h"
#include "rtc.h"
#include "shell.h"
#include "theme.h"
#include "time.h"
#include "userdb.h"
#include "wallpaper.h"

#define LOGIN_PASS_MAX 32
#define DEV_PASS "admin"
#define LOGIN_LOGO   BRAND_LOGIN_W
#define LOGIN_PASS_W ui_px(240u)
#define LOGIN_PASS_H ui_px(34u)
#define LOGIN_GO     ui_px(22u)
#define LOGIN_POWER  ui_px(36u)
#define HIT_NONE     0xFFu
#define HIT_PASS     1u
#define HIT_GO       2u
#define HIT_POWER    3u
#define HIT_USER     4u

static const struct rgb LOGIN_WHITE = { 0xF7, 0xF8, 0xFA };
static const struct rgb LOGIN_MUTED = { 0xC4, 0xC8, 0xD0 };
static const struct rgb LOGIN_INK   = { 0x1A, 0x1A, 0x1C };
static const struct rgb LOGIN_GLASS = { 0x1C, 0x24, 0x30 };
#define LOGIN_PLACE_H ui_px(16u)

struct login_draw {
    const char *pass;
    bool focused;
    bool caret;
    bool go_hot;
    bool pass_hot;
    bool power_hot;
    i32 shake_x;
};

struct login_geom {
    u32 cx;
    u32 date_y;
    u32 clock_y;
    u32 logo_x;
    u32 logo_y;
    u32 name_y;
    u32 pass_x;
    u32 pass_y;
    u32 pass_w;
    u32 pass_h;
    u32 go_x;
    u32 go_y;
    u32 power_x;
    u32 power_y;
};

static void login_cursor_at(void);

static int streq(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b) {
        ++a;
        ++b;
    }
    return *a == *b;
}

static void field_append(char *buf, u32 *len, u32 max, u32 code)
{
    if (code < 32u || code == 127u || key_is_special(code) || code > 0x7Fu) {
        return;
    }
    if (*len + 1u >= max) {
        return;
    }
    buf[*len] = (char)code;
    ++(*len);
    buf[*len] = '\0';
}

static void field_backspace(char *buf, u32 *len)
{
    if (*len == 0) {
        return;
    }
    --(*len);
    buf[*len] = '\0';
}

static bool in_rect(i32 px, i32 py, u32 x, u32 y, u32 w, u32 h)
{
    return px >= (i32)x && py >= (i32)y &&
           px < (i32)(x + w) && py < (i32)(y + h);
}

static void login_wait_ms(u32 ms)
{
    u32 start = time_uptime_ms();

    while ((time_uptime_ms() - start) < ms) {
        __asm__ volatile("hlt");
    }
}

static void login_clock(char *out, const struct rtc_time *now)
{
    out[0] = (char)('0' + (now->hour / 10u));
    out[1] = (char)('0' + (now->hour % 10u));
    out[2] = ':';
    out[3] = (char)('0' + (now->minute / 10u));
    out[4] = (char)('0' + (now->minute % 10u));
    out[5] = '\0';
}

static u8 login_weekday(u16 year, u8 month, u8 day)
{
    static const u8 offsets[12] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
    u32 y = year;

    if (month < 3u) {
        --y;
    }
    return (u8)((y + y / 4u - y / 100u + y / 400u +
                 offsets[month - 1u] + day) % 7u + 1u);
}

static void login_date(char *out, u32 out_len, const struct rtc_time *now)
{
    static const char *const days_en[] = {
        "Sunday", "Monday", "Tuesday", "Wednesday",
        "Thursday", "Friday", "Saturday"
    };
    static const char *const days_es[] = {
        "Domingo", "Lunes", "Martes", "Miercoles",
        "Jueves", "Viernes", "Sabado"
    };
    static const char *const months_en[] = {
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    };
    static const char *const months_es[] = {
        "enero", "febrero", "marzo", "abril", "mayo", "junio",
        "julio", "agosto", "septiembre", "octubre", "noviembre", "diciembre"
    };
    u8 weekday = login_weekday(now->year, now->month, now->day);
    u8 day = now->day;
    u8 month = now->month;
    const char *wd;
    const char *mo;
    u32 i = 0;
    char num[4];
    u32 n;
    u32 d;

    if (weekday < 1u || weekday > 7u) {
        weekday = 1;
    }
    if (month < 1u || month > 12u) {
        month = 1;
    }
    if (day < 1u || day > 31u) {
        day = 1;
    }

    if (i18n_lang() == LANG_EN) {
        wd = days_en[weekday - 1u];
        mo = months_en[month - 1u];
    } else {
        wd = days_es[weekday - 1u];
        mo = months_es[month - 1u];
    }

    while (*wd != '\0' && i + 1u < out_len) {
        out[i++] = *wd++;
    }
    if (i + 2u < out_len) {
        out[i++] = ',';
        out[i++] = ' ';
    }

    if (i18n_lang() == LANG_EN) {
        while (*mo != '\0' && i + 1u < out_len) {
            out[i++] = *mo++;
        }
        if (i + 1u < out_len) {
            out[i++] = ' ';
        }
        n = 0;
        d = day;
        if (d == 0) {
            num[n++] = '0';
        } else {
            while (d > 0 && n < sizeof(num)) {
                num[n++] = (char)('0' + (d % 10u));
                d /= 10u;
            }
        }
        while (n > 0 && i + 1u < out_len) {
            out[i++] = num[--n];
        }
    } else {
        n = 0;
        d = day;
        if (d == 0) {
            num[n++] = '0';
        } else {
            while (d > 0 && n < sizeof(num)) {
                num[n++] = (char)('0' + (d % 10u));
                d /= 10u;
            }
        }
        while (n > 0 && i + 1u < out_len) {
            out[i++] = num[--n];
        }
        if (i + 4u < out_len) {
            out[i++] = ' ';
            out[i++] = 'd';
            out[i++] = 'e';
            out[i++] = ' ';
        }
        while (*mo != '\0' && i + 1u < out_len) {
            out[i++] = *mo++;
        }
    }
    out[i] = '\0';
}

static void login_layout(struct login_geom *g)
{
    u32 w = fb_width();
    u32 h = fb_height();
    u32 cluster_h;
    u32 clock_bottom;
    u32 power_top;
    u32 avail;

    g->cx = w / 2u;
    g->pass_w = LOGIN_PASS_W;
    if (g->pass_w + ui_margin() * 2u > w) {
        g->pass_w = (w > ui_margin() * 2u) ? (w - ui_margin() * 2u) : w;
    }
    g->pass_h = LOGIN_PASS_H;

    g->date_y = (h >= 900u) ? (h / 9u) : ((h >= 720u) ? ui_px(40u) : ui_px(12u));
    g->clock_y = g->date_y + FONT_HEIGHT + 6u;
    clock_bottom = g->clock_y + FONT_TITLE_H + 20u;

    /* Logo + name + field, centered in leftover space. */
    cluster_h = LOGIN_LOGO + 10u + FONT_HEIGHT + 16u + LOGIN_PASS_H;
    g->power_x = (g->cx > LOGIN_POWER / 2u) ? (g->cx - LOGIN_POWER / 2u) : 0;
    g->power_y = (h > LOGIN_POWER + 28u) ? (h - LOGIN_POWER - 22u)
                                        : (clock_bottom + cluster_h + 8u);
    power_top = g->power_y;
    if (power_top > clock_bottom + cluster_h) {
        avail = power_top - clock_bottom;
        g->logo_y = clock_bottom + (avail - cluster_h) / 2u;
    } else if (clock_bottom + cluster_h < h) {
        g->logo_y = clock_bottom;
    } else {
        g->logo_y = 8u;
    }
    g->logo_x = (g->cx > LOGIN_LOGO / 2u) ? (g->cx - LOGIN_LOGO / 2u) : 0;
    g->name_y = g->logo_y + LOGIN_LOGO + 10u;
    g->pass_y = g->name_y + FONT_HEIGHT + 16u;
    g->pass_x = (g->cx > g->pass_w / 2u) ? (g->cx - g->pass_w / 2u) : 0;
    g->go_x = g->pass_x + g->pass_w - LOGIN_GO - 6u;
    g->go_y = g->pass_y + (g->pass_h > LOGIN_GO ? (g->pass_h - LOGIN_GO) / 2u : 0);
}

static void draw_login_label(u32 x, u32 y, const char *text, u32 scale,
                             bool on_light)
{
    draw_text(x, y, text, on_light ? LOGIN_INK : LOGIN_WHITE, scale);
}

static u8 mix_u8(u8 a, u8 b, u8 t)
{
    return (u8)(((u32)a * (255u - (u32)t) + (u32)b * (u32)t) / 255u);
}

static void draw_go_arrow(u32 cx, u32 cy, struct rgb color)
{
    i32 y;
    i32 x;

    for (y = -5; y <= 5; ++y) {
        i32 ay = (y < 0) ? -y : y;
        i32 tip = 5 - ay;
        for (x = -2; x <= tip; ++x) {
            u8 a = 255u;
            if (x == tip || ay == 5) {
                a = 170u;
            }
            fb_blend_pixel((u32)((i32)cx + x), (u32)((i32)cy + y),
                           color.r, color.g, color.b, a);
        }
    }
}

static void draw_login_logo(u32 x, u32 y)
{
    /* 1:1 pixels from a vector raster — no kernel downscale. */
    draw_round_fill(x - 3u, y - 3u, LOGIN_LOGO + 6u, LOGIN_LOGO + 6u,
                    (LOGIN_LOGO + 6u) / 2u, LOGIN_INK, 70u);
    fb_blit_rgba(brand_login_rgba, BRAND_LOGIN_W, BRAND_LOGIN_H, (i32)x, (i32)y);
}

static void draw_pass_dots(u32 x, u32 y, u32 h, const char *pass, bool caret)
{
    u32 n = 0;
    u32 i;
    u32 dot = 5u;
    u32 gap = 6u;
    u32 max_dots;
    u32 start;
    u32 py;

    while (pass[n] != '\0') {
        ++n;
    }
    max_dots = 18u;
    start = (n > max_dots) ? (n - max_dots) : 0;
    py = y + (h > dot ? (h - dot) / 2u : 0);
    for (i = start; i < n; ++i) {
        u32 dx = x + (i - start) * (dot + gap);
        draw_round_fill(dx, py, dot, dot, dot / 2u, LOGIN_WHITE, 230u);
    }
    if (caret) {
        u32 cx = x + (n - start) * (dot + gap);
        fb_fill_rect(cx, y + 8u, 2u, (h > 16u) ? (h - 16u) : h,
                     LOGIN_WHITE.r, LOGIN_WHITE.g, LOGIN_WHITE.b);
    }
}

static void draw_caret(u32 x, u32 y, u32 h)
{
    fb_fill_rect(x, y + 8u, 2u, (h > 16u) ? (h - 16u) : h,
                 LOGIN_WHITE.r, LOGIN_WHITE.g, LOGIN_WHITE.b);
}

static void paint_login_screen(const struct login_draw *d)
{
    struct login_geom g;
    u32 w = fb_width();
    char clock[8];
    char date[48];
    u32 clock_w;
    u32 date_w;
    u32 name_w;
    const char *name = "Admin";
    const struct user_rec *ur = userdb_current();

    if (ur != NULL && ur->name[0] != '\0') {
        name = ur->name;
    }
    u32 pass_x;
    u32 go_x;
    u8 glass_a;
    u8 rim_a;
    u8 focus;
    struct rgb tint;
    bool has_pass;
    struct rtc_time now = { 0, 0, 0, 1, 1, 2000 };

    login_layout(&g);
    (void)rtc_read(&now);
    login_clock(clock, &now);
    login_date(date, sizeof(date), &now);
    pass_x = (u32)((i32)g.pass_x + d->shake_x);
    go_x = (u32)((i32)g.go_x + d->shake_x);
    if (d->focused) {
        focus = 255u;
    } else if (d->pass_hot) {
        focus = 110u;
    } else {
        focus = 0;
    }

    fb_compose_begin();
    draw_bg_login();
    fb_overlay(4, 10, 22, 22u);

    date_w = draw_text_width(date, 1);
    clock_w = draw_text_width(clock, 2);
    {
        u32 dx = (w > date_w) ? (w - date_w) / 2u : 0;
        u32 cx = (w > clock_w) ? (w - clock_w) / 2u : 0;
        u32 band_x = (dx < cx) ? dx : cx;
        u32 band_w = date_w > clock_w ? date_w : clock_w;
        u32 band_h = (g.clock_y - g.date_y) + FONT_TITLE_H;
        bool time_light = draw_region_is_light(band_x, g.date_y, band_w, band_h);
        struct rgb chrome = time_light ? LOGIN_INK : LOGIN_WHITE;

        cursor_set_on_light(time_light);
        draw_login_label(dx, g.date_y, date, 1, time_light);
        draw_login_label(cx, g.clock_y, clock, 2, time_light);

        draw_login_logo(g.logo_x, g.logo_y);

        name_w = draw_text_width(name, 1);
        {
            u32 nx = (w > name_w) ? (w - name_w) / 2u : 0;
            draw_login_label(nx, g.name_y, name, 1, time_light);
        }

        tint = LOGIN_GLASS;
        glass_a = mix_u8(38u, 86u, focus);
        rim_a = mix_u8(16u, 64u, focus);

        draw_round_fill(pass_x > 0u ? pass_x - 1u : 0u,
                        g.pass_y > 0u ? g.pass_y - 1u : 0u,
                        g.pass_w + 2u, g.pass_h + 2u,
                        (g.pass_h + 2u) / 2u,
                        draw_region_is_light(pass_x, g.pass_y, g.pass_w, g.pass_h)
                            ? LOGIN_INK : LOGIN_WHITE,
                        rim_a);
        draw_glass(pass_x, g.pass_y, g.pass_w, g.pass_h, g.pass_h / 2u, tint, glass_a);
        if (g.pass_w > 24u) {
            draw_round_fill(pass_x + 12u, g.pass_y + 1u, g.pass_w - 24u, 1u, 0,
                            LOGIN_WHITE, (u8)(12u + (u32)focus / 12u));
        }

        has_pass = d->pass != NULL && d->pass[0] != '\0';
        if (has_pass) {
            draw_pass_dots(pass_x + 14u, g.pass_y, g.pass_h, d->pass,
                           d->focused && d->caret);
        } else {
            u32 ty = g.pass_y + (g.pass_h > LOGIN_PLACE_H
                                     ? (g.pass_h - LOGIN_PLACE_H) / 2u
                                     : 0);
            draw_text_h(pass_x + 14u, ty, i18n(MSG_LOGIN_PASSWORD), LOGIN_MUTED,
                        LOGIN_PLACE_H);
            if (d->focused && d->caret) {
                draw_caret(pass_x + 14u, g.pass_y, g.pass_h);
            }
        }

        {
            u8 go_a = d->go_hot ? 100u : mix_u8(42u, 78u, focus);
            draw_round_fill(go_x > 0 ? go_x - 1u : 0,
                            g.go_y > 0 ? g.go_y - 1u : 0,
                            LOGIN_GO + 2u, LOGIN_GO + 2u, (LOGIN_GO + 2u) / 2u,
                            draw_region_is_light(go_x, g.go_y, LOGIN_GO, LOGIN_GO)
                                ? LOGIN_INK : LOGIN_WHITE,
                            rim_a);
            draw_glass(go_x, g.go_y, LOGIN_GO, LOGIN_GO, LOGIN_GO / 2u, tint, go_a);
            draw_go_arrow(go_x + LOGIN_GO / 2u, g.go_y + LOGIN_GO / 2u, chrome);
        }

        draw_round_fill(g.power_x > 0 ? g.power_x - 1u : 0,
                        g.power_y > 0 ? g.power_y - 1u : 0,
                        LOGIN_POWER + 2u, LOGIN_POWER + 2u,
                        (LOGIN_POWER + 2u) / 2u,
                        draw_region_is_light(g.power_x, g.power_y,
                                             LOGIN_POWER, LOGIN_POWER)
                            ? LOGIN_INK : LOGIN_WHITE,
                        d->power_hot ? 70u : 22u);
        draw_glass(g.power_x, g.power_y, LOGIN_POWER, LOGIN_POWER,
                   LOGIN_POWER / 2u, tint, d->power_hot ? 92u : 56u);
        draw_icon(g.power_x + 8u, g.power_y + 8u, 20u, UI_ICON_POWER, chrome);
    }
}

static void draw_login_screen(const struct login_draw *d, bool show_cursor)
{
    paint_login_screen(d);
    if (show_cursor) {
        ui_comp_mark_full();
        ui_comp_present();
    } else {
        cursor_invalidate();
        fb_compose_present();
    }
}

static bool in_circle(i32 px, i32 py, u32 x, u32 y, u32 size, i32 pad)
{
    i32 cx = (i32)x + (i32)(size / 2u);
    i32 cy = (i32)y + (i32)(size / 2u);
    i32 r = (i32)(size / 2u) + pad;
    i32 dx = px - cx;
    i32 dy = py - cy;

    return dx * dx + dy * dy <= r * r;
}

static u32 login_hit_raw(i32 px, i32 py, i32 pad)
{
    struct login_geom g;

    login_layout(&g);
    if (in_circle(px, py, g.go_x, g.go_y, LOGIN_GO, pad)) {
        return HIT_GO;
    }
    if (in_rect(px, py, g.pass_x, g.pass_y, g.pass_w, g.pass_h)) {
        return HIT_PASS;
    }
    if (in_circle(px, py, g.power_x, g.power_y, LOGIN_POWER, pad)) {
        return HIT_POWER;
    }
    {
        const struct user_rec *ur = userdb_current();
        const char *nm = (ur != NULL && ur->name[0] != '\0') ? ur->name : "Admin";
        u32 nw = draw_text_width(nm, 1);
        u32 nx = (fb_width() > nw) ? (fb_width() - nw) / 2u : 0;
        if (in_rect(px, py, nx, g.name_y, nw, FONT_HEIGHT)) {
            return HIT_USER;
        }
    }
    return HIT_NONE;
}

static u32 login_hit_sticky(i32 px, i32 py, u32 prev)
{
    u32 hit = login_hit_raw(px, py, 2);

    if (hit != HIT_NONE) {
        return hit;
    }
    /* Stay hovered until the pointer leaves a wider margin — kills 1px flicker. */
    if (prev == HIT_GO || prev == HIT_POWER) {
        if (login_hit_raw(px, py, 10) == prev) {
            return prev;
        }
    }
    if (prev == HIT_PASS && login_hit_raw(px, py, 0) == HIT_PASS) {
        return HIT_PASS;
    }
    return HIT_NONE;
}

static enum cursor_kind login_cursor_kind(u32 hit)
{
    if (hit == HIT_PASS || hit == HIT_GO || hit == HIT_POWER ||
        hit == HIT_USER) {
        return CURSOR_KIND_POINTER;
    }
    return CURSOR_KIND_ARROW;
}

static u32 s_login_hover;

static void login_cursor_at(void)
{
    cursor_set_kind(login_cursor_kind(s_login_hover));
    cursor_draw((u32)mouse_x(), (u32)mouse_y());
}

static bool try_auth(const char *pass)
{
    if (userdb_count() > 0u) {
        return userdb_auth_current(pass) != 0;
    }
    return streq(pass, DEV_PASS);
}

static void login_view_fill(struct login_draw *d, const char *pass,
                            bool focused, bool go_hot, bool pass_hot,
                            bool power_hot, i32 shake_x)
{
    u32 now = time_uptime_ms();

    d->pass = pass;
    d->focused = focused;
    d->caret = focused && (((now / 520u) & 1u) == 0u);
    d->go_hot = go_hot;
    d->pass_hot = pass_hot;
    d->power_hot = power_hot;
    d->shake_x = shake_x;
}

static void login_fail_shake(const char *pass, bool go_hot, bool pass_hot,
                             bool power_hot)
{
    struct login_draw d;
    u32 i;

    for (i = 0; i < 6u; ++i) {
        i32 off = ((i & 1u) == 0u) ? 10 : -10;
        login_view_fill(&d, pass, true, go_hot, pass_hot, power_hot, off);
        d.caret = false;
        draw_login_screen(&d, false);
        login_wait_ms(28u);
    }
    login_view_fill(&d, "", true, go_hot, pass_hot, power_hot, 0);
    d.caret = false;
    draw_login_screen(&d, true);
}

static bool gfx_login(void)
{
    char pass[LOGIN_PASS_MAX];
    u32 plen = 0;
    bool dirty = true;
    bool go_hot = false;
    bool pass_hot = false;
    bool power_hot = false;
    bool focused = false;
    i32 last_x = -1;
    i32 last_y = -1;
    u8 last_min = 0xFFu;
    bool last_caret = false;
    u8 *from = NULL;
    bool intro = true;

    pass[0] = '\0';
    s_login_hover = HIT_NONE;
    cursor_hide();
    if (fb_compose_ready()) {
        from = fb_layer_alloc();
        if (from != NULL) {
            fb_copy_front(from);
        }
    }
    while (keyboard_has_char()) {
        (void)keyboard_get_codepoint();
    }
    while (mouse_has_event()) {
        (void)mouse_get_event();
    }
    mouse_set_bounds(fb_width(), fb_height());

    for (;;) {
        if (keyboard_has_char()) {
            u32 key = keyboard_get_codepoint();

            if (key == KEY_F1) {
                shell_run();
                dirty = true;
            } else if (key == KEY_LEFT || key == KEY_RIGHT) {
                if (userdb_count() > 1u) {
                    userdb_select_next();
                    dirty = true;
                }
            } else if (key == 27u) {
                focused = false;
                dirty = true;
            } else if (key == '\n' || key == '\r') {
                focused = true;
                if (try_auth(pass)) {
                    return true;
                }
                login_fail_shake(pass, go_hot, pass_hot, power_hot);
                plen = 0;
                pass[0] = '\0';
                dirty = true;
            } else if (key == '\b' || key == 127u) {
                focused = true;
                field_backspace(pass, &plen);
                dirty = true;
            } else {
                focused = true;
                field_append(pass, &plen, LOGIN_PASS_MAX, key);
                dirty = true;
            }
        } else if (mouse_has_event()) {
            struct mouse_event ev = mouse_get_event();
            u32 hit;

            while (mouse_has_event()) {
                struct mouse_event more = mouse_get_event();
                if (more.kind == MOUSE_EV_DOWN || more.kind == MOUSE_EV_UP) {
                    ev = more;
                }
            }
            hit = login_hit_sticky(mouse_x(), mouse_y(), s_login_hover);
            s_login_hover = hit;
            if (ev.kind == MOUSE_EV_DOWN && ev.button == MOUSE_LEFT) {
                if (hit == HIT_GO) {
                    focused = true;
                    if (try_auth(pass)) {
                        return true;
                    }
                    login_fail_shake(pass, go_hot, pass_hot, power_hot);
                    plen = 0;
                    pass[0] = '\0';
                    dirty = true;
                } else if (hit == HIT_PASS) {
                    focused = true;
                    dirty = true;
                } else if (hit == HIT_USER) {
                    if (userdb_count() > 1u) {
                        userdb_select_next();
                        dirty = true;
                    }
                } else if (hit == HIT_POWER) {
                    cursor_hide();
                    fb_compose_begin();
                    draw_bg_login();
                    fb_overlay(6, 8, 12, 140u);
                    draw_text(48, 120, i18n(MSG_POWER_MSG), LOGIN_WHITE, 1);
                    fb_compose_present();
                    login_wait_ms(250u);
                    machine_power_off();
                } else {
                    focused = false;
                    dirty = true;
                }
            }
        }

        {
            struct rtc_time rtc_now;
            u32 now = time_uptime_ms();
            bool caret = focused && (((now / 520u) & 1u) == 0u);
            u32 live = login_hit_sticky(mouse_x(), mouse_y(), s_login_hover);
            bool next_go = (live == HIT_GO);
            bool next_pass = (live == HIT_PASS);
            bool next_power = (live == HIT_POWER);

            if (rtc_read(&rtc_now) && rtc_now.minute != last_min) {
                last_min = rtc_now.minute;
                dirty = true;
            }
            s_login_hover = live;
            if (next_go != go_hot || next_pass != pass_hot ||
                next_power != power_hot) {
                go_hot = next_go;
                pass_hot = next_pass;
                power_hot = next_power;
                dirty = true;
            }
            if (caret != last_caret) {
                last_caret = caret;
                dirty = true;
            }
            (void)now;
        }

        if (mouse_x() != last_x || mouse_y() != last_y) {
            if (!dirty) {
                login_cursor_at();
            }
            last_x = mouse_x();
            last_y = mouse_y();
        }

        if (dirty) {
            struct login_draw d;

            login_view_fill(&d, pass, focused, go_hot, pass_hot, power_hot, 0);
            if (intro && from != NULL) {
                paint_login_screen(&d);
                ui_crossfade_from(from);
                cursor_set_kind(login_cursor_kind(s_login_hover));
                cursor_flip((u32)mouse_x(), (u32)mouse_y());
            } else {
                draw_login_screen(&d, true);
            }
            intro = false;
            dirty = false;
            last_x = mouse_x();
            last_y = mouse_y();
        } else if (mouse_x() == last_x && mouse_y() == last_y) {
            __asm__ volatile("hlt");
        }
    }
}

void gfx_session_run(void)
{
    for (;;) {
        while (!gfx_login()) {
        }
        desktop_run();
    }
}
