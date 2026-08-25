#include "onboarding.h"
#include "animation.h"
#include "brand.h"
#include "draw.h"
#include "fb.h"
#include "font.h"
#include "i18n.h"
#include "keyboard.h"
#include "keycodes.h"
#include "mouse.h"
#include "net.h"
#include "pci.h"
#include "persist.h"
#include "theme.h"
#include "time.h"
#include "userdb.h"
#include "virtio_net.h"
#include "wallpaper.h"
#include "wlan.h"

enum ob_step {
    OB_LANG = 0,
    OB_WELCOME,
    OB_NAME,
    OB_PASS,
    OB_WIFI,
    OB_CHECK,
    OB_ANOTHER
};

#define HIT_NONE   0u
#define HIT_BACK   1u
#define HIT_NEXT   2u
#define HIT_SKIP   3u
#define HIT_FIELD  4u
#define HIT_LANG0  5u
#define HIT_LANG1  6u
#define HIT_NET0   10u

#define NAME_MAX 24u
#define PASS_MAX 32u
#define FIELD_H  34u
#define PLACE_H  16u
#define ROW_H    44u
#define ROW_GAP  10u
#define BTN_H    38u
#define LOGO     BRAND_LOGIN_W

static const struct rgb OB_WHITE = { 0xF7, 0xF8, 0xFA };
static const struct rgb OB_MUTED = { 0xC4, 0xC8, 0xD0 };
static const struct rgb OB_INK   = { 0x1A, 0x1A, 0x1C };
static const struct rgb OB_GLASS = { 0x1C, 0x24, 0x30 };

static char s_name[NAME_MAX];
static u32 s_nlen;
static char s_pass[PASS_MAX];
static u32 s_plen;
static u32 s_net;
static bool s_net_ok;
static bool s_checking;
static u8 s_check_t;
static bool s_ask_lang;
static char s_wifi_pass[PASS_MAX];
static u32 s_wplen;
static bool s_scanned;
static u32 s_wifi_top;
static u8 s_enter;
static bool s_caret;
static bool s_light;
static u8 *s_fade_from;

static void wait_ms(u32 ms)
{
    u32 start = time_uptime_ms();

    while ((time_uptime_ms() - start) < ms) {
        __asm__ volatile("hlt");
    }
}

static bool in_rect(i32 px, i32 py, u32 x, u32 y, u32 w, u32 h)
{
    return px >= (i32)x && py >= (i32)y && px < (i32)(x + w) &&
           py < (i32)(y + h);
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

static u8 mix_u8(u8 a, u8 b, u8 t)
{
    return (u8)(((u32)a * (255u - (u32)t) + (u32)b * (u32)t) / 255u);
}

static u8 ease_out(u8 t)
{
    u32 u = 255u - (u32)t;

    return (u8)(255u - (u * u) / 255u);
}

static u8 rec601_luma(u8 r, u8 g, u8 b)
{
    return (u8)(((u32)r * 77u + (u32)g * 150u + (u32)b * 29u) >> 8);
}

static bool wp_at(u32 x, u32 y, u8 *r, u8 *g, u8 *b)
{
    const u8 *rgb = wallpaper_login_pixels();
    u32 fb_w = fb_width();
    u32 fb_h = fb_height();
    u32 copy_w;
    u32 copy_h;
    u32 src_x0;
    u32 src_y0;
    u32 dst_x0;
    u32 dst_y0;
    u32 sx;
    u32 sy;
    const u8 *p;

    *r = 0x12;
    *g = 0x16;
    *b = 0x1C;
    if (rgb == NULL || fb_w == 0 || fb_h == 0) {
        return false;
    }
    copy_w = (WALLPAPER_W < fb_w) ? WALLPAPER_W : fb_w;
    copy_h = (WALLPAPER_H < fb_h) ? WALLPAPER_H : fb_h;
    src_x0 = (WALLPAPER_W > fb_w) ? (WALLPAPER_W - fb_w) / 2u : 0;
    src_y0 = (WALLPAPER_H > fb_h) ? (WALLPAPER_H - fb_h) / 2u : 0;
    dst_x0 = (fb_w > WALLPAPER_W) ? (fb_w - WALLPAPER_W) / 2u : 0;
    dst_y0 = (fb_h > WALLPAPER_H) ? (fb_h - WALLPAPER_H) / 2u : 0;
    if (x < dst_x0 || y < dst_y0) {
        return false;
    }
    sx = src_x0 + (x - dst_x0);
    sy = src_y0 + (y - dst_y0);
    if (sx >= src_x0 + copy_w || sy >= src_y0 + copy_h ||
        sx >= WALLPAPER_W || sy >= WALLPAPER_H) {
        return false;
    }
    p = rgb + (sy * WALLPAPER_W + sx) * 3u;
    *r = p[0];
    *g = p[1];
    *b = p[2];
    return true;
}

static bool region_is_light(u32 x, u32 y, u32 w, u32 h)
{
    u32 gx;
    u32 gy;
    u32 sum = 0;
    u32 n = 0;

    if (w == 0 || h == 0) {
        return false;
    }
    for (gy = 0; gy < 3u; ++gy) {
        u32 py = y + (h * (2u * gy + 1u)) / 6u;
        for (gx = 0; gx < 6u; ++gx) {
            u32 px = x + (w * (2u * gx + 1u)) / 12u;
            u8 r;
            u8 g;
            u8 b;
            u8 luma;

            wp_at(px, py, &r, &g, &b);
            luma = rec601_luma(r, g, b);
            luma = (u8)(((u32)luma * 233u + 9u * 22u) / 255u);
            sum += luma;
            ++n;
        }
    }
    return n != 0u && (sum / n) >= 128u;
}

struct ob_geom {
    u32 cx;
    u32 x0;
    u32 cw;
    u32 logo_x;
    u32 logo_y;
    u32 dot_y;
    u32 title_y;
    u32 body_y;
    u32 content_y;
    u32 btn_y;
    u32 btn_w;
    u32 btn_h;
    u32 wifi_rows;
    u32 content_h;
};

static u32 ob_content_h(enum ob_step step);

static void ob_layout(struct ob_geom *g, enum ob_step step)
{
    u32 w = fb_width();
    u32 h = fb_height();
    u32 stack;

    g->cx = w / 2u;
    g->cw = (w >= 1100u) ? 420u : ((w >= 800u) ? 360u : (w > 64u ? w - 64u : w));
    g->x0 = (w > g->cw) ? (w - g->cw) / 2u : 8u;
    g->btn_h = BTN_H;
    g->btn_w = 160u;
    g->wifi_rows = (h >= 900u) ? 4u : 3u;
    g->content_h = ob_content_h(step);

    /* Logo → dots → title → body → fields → buttons, as one block. */
    stack = LOGO + 16u + 8u + 20u + FONT_HEIGHT + 8u + FONT_LINE + 4u +
            g->content_h + 28u + g->btn_h;
    if (stack + 24u < h) {
        g->logo_y = (h - stack) / 2u;
    } else {
        g->logo_y = 12u;
    }
    g->logo_x = (g->cx > LOGO / 2u) ? (g->cx - LOGO / 2u) : 0;
    g->dot_y = g->logo_y + LOGO + 16u;
    g->title_y = g->dot_y + 20u;
    g->body_y = g->title_y + FONT_HEIGHT + 8u;
    g->content_y = g->body_y + FONT_LINE + 4u;
    g->btn_y = g->content_y + g->content_h + 28u;
    if (g->btn_y + g->btn_h + 12u > h) {
        g->btn_y = (h > g->btn_h + 12u) ? (h - g->btn_h - 12u) : 0;
    }
}

#define BTN_GAP 16u

static u32 net_count(void);
static bool net_is_eth(u32 index);
static bool net_needs_pass(u32 index);

struct ob_btns {
    bool back;
    bool skip;
    bool next;
    const char *next_lab;
    const char *skip_lab;
    u32 back_x;
    u32 skip_x;
    u32 next_x;
    u32 bw;
};

static void ob_btns_prep(enum ob_step step, struct ob_btns *b,
                         const struct ob_geom *g)
{
    u32 n = 0;
    u32 total;
    u32 x;

    b->bw = g->btn_w;
    b->back = (step != OB_LANG && step != OB_WELCOME && step != OB_ANOTHER);
    b->skip = (step == OB_WIFI || step == OB_CHECK || step == OB_ANOTHER);
    b->next = true;
    b->next_lab = i18n(MSG_OB_NEXT);
    b->skip_lab = i18n(MSG_OB_SKIP);

    if (step == OB_WIFI) {
        b->next_lab = i18n(MSG_OB_CONNECT);
        b->next = s_scanned && net_count() > 0u &&
                  (net_is_eth(s_net) || !net_needs_pass(s_net) ||
                   s_wplen > 0u);
    } else if (step == OB_ANOTHER) {
        b->next_lab = i18n(MSG_OB_ADD);
        b->skip_lab = i18n(MSG_OB_NEXT);
    } else if (step == OB_NAME) {
        b->next = s_nlen > 0;
    } else if (step == OB_PASS) {
        b->next = s_plen > 0;
    } else if (step == OB_CHECK) {
        b->next = !s_checking;
    }

    if (b->back) {
        ++n;
    }
    if (b->skip) {
        ++n;
    }
    if (b->next) {
        ++n;
    }

    total = n * b->bw;
    if (n > 1u) {
        total += (n - 1u) * BTN_GAP;
    }
    x = (g->cx > total / 2u) ? (g->cx - total / 2u) : 8u;
    b->back_x = x;
    b->skip_x = x;
    b->next_x = x;
    if (b->back) {
        b->back_x = x;
        x += b->bw + BTN_GAP;
    }
    if (b->skip) {
        b->skip_x = x;
        x += b->bw + BTN_GAP;
    }
    if (b->next) {
        b->next_x = x;
    }
}

static bool net_wired(void)
{
    return virtio_net_present_os || pci_has_class(PCI_CLASS_NET);
}

static u32 wifi_n(void)
{
    return wlan_count();
}

static u32 net_count(void)
{
    u32 n = wifi_n();

    if (net_wired()) {
        ++n;
    }
    return n;
}

static bool net_is_eth(u32 index)
{
    return index >= wifi_n() && net_wired();
}

static const struct wlan_bss *net_wifi(u32 index)
{
    if (net_is_eth(index)) {
        return NULL;
    }
    return wlan_get(index);
}

static bool net_needs_pass(u32 index)
{
    const struct wlan_bss *b = net_wifi(index);

    return b != NULL && b->sec != WLAN_SEC_OPEN;
}

static u32 wifi_visible(u32 index)
{
    u32 rows = (fb_height() >= 900u) ? 4u : 3u;

    return net_needs_pass(index) ? (rows > 1u ? rows - 1u : 1u) : rows;
}

static u32 ob_content_h(enum ob_step step)
{
    if (step == OB_LANG) {
        return ROW_H * 2u + ROW_GAP;
    }
    if (step == OB_NAME || step == OB_PASS) {
        return FIELD_H;
    }
    if (step == OB_WIFI) {
        u32 vis;
        u32 hh;

        if (!s_scanned || net_count() == 0u) {
            return FONT_LINE;
        }
        vis = wifi_visible(s_net);
        hh = vis * (ROW_H + ROW_GAP);
        if (hh >= ROW_GAP) {
            hh -= ROW_GAP;
        }
        if (net_needs_pass(s_net)) {
            hh += 4u + FIELD_H;
        }
        return hh;
    }
    if (step == OB_CHECK) {
        return 40u;
    }
    return 0;
}

static bool ob_probe_link(void)
{
    bool assoc;
    u32 target;

    if (net_is_eth(s_net)) {
        assoc = net_wired();
    } else {
        assoc = wlan_connect(s_net, s_wifi_pass);
    }
    if (!assoc) {
        return false;
    }
    if (!net_ensure_up()) {
        return false;
    }
    target = net_gateway();
    if (target == 0u) {
        target = 0x0A000202u;
    }
    return net_ping(target, 1200u) >= 0;
}

static void ob_scroll_wifi(void)
{
    u32 vis = wifi_visible(s_net);
    u32 n = wifi_n();

    if (net_is_eth(s_net)) {
        s_wifi_top = (n > vis) ? (n - vis) : 0;
        return;
    }
    if (s_net < s_wifi_top) {
        s_wifi_top = s_net;
    }
    if (s_net >= s_wifi_top + vis) {
        s_wifi_top = s_net + 1u - vis;
    }
}

static i32 enter_dy(void)
{
    u8 e = ease_out(s_enter);

    return (i32)(((255u - (u32)e) * 32u) / 255u);
}

static u8 enter_a(u8 a)
{
    return (u8)(((u32)a * (u32)ease_out(s_enter)) / 255u);
}

static struct rgb chrome_col(void)
{
    return s_light ? OB_INK : OB_WHITE;
}

static struct rgb muted_col(void)
{
    if (s_light) {
        struct rgb c = { 0x4A, 0x4A, 0x52 };

        return c;
    }
    return OB_MUTED;
}

static void ob_label(u32 x, u32 y, const char *text, u32 scale)
{
    draw_text(x, y, text, chrome_col(), scale);
}

static void ob_draw_logo(u32 x, u32 y)
{
    draw_round_fill(x > 3u ? x - 3u : 0, y > 3u ? y - 3u : 0,
                    LOGO + 6u, LOGO + 6u, (LOGO + 6u) / 2u, OB_INK, 70u);
    fb_blit_rgba(brand_login_rgba, BRAND_LOGIN_W, BRAND_LOGIN_H, (i32)x, (i32)y);
}

static void ob_draw_dots(const struct ob_geom *g, enum ob_step step)
{
    u32 i;
    u32 n = 7u;
    u32 gap = 14u;
    u32 total = (n - 1u) * gap;
    u32 x0 = (g->cx > total / 2u) ? (g->cx - total / 2u) : 8u;
    u32 cur = (u32)step;
    struct rgb c = chrome_col();

    for (i = 0; i < n; ++i) {
        u8 a = (i <= cur) ? enter_a(210u) : enter_a(48u);
        u32 d = (i == cur) ? 7u : 5u;

        draw_round_fill(x0 + i * gap - d / 2u, g->dot_y, d, d, d / 2u, c, a);
    }
}

static void ob_draw_caret(u32 x, u32 y, u32 h)
{
    if (!s_caret) {
        return;
    }
    fb_fill_rect(x, y + 8u, 2u, (h > 16u) ? (h - 16u) : h,
                 OB_WHITE.r, OB_WHITE.g, OB_WHITE.b);
}

static void ob_draw_pass_dots(u32 x, u32 y, u32 h, const char *pass)
{
    u32 n = 0;
    u32 i;
    u32 dot = 5u;
    u32 gap = 6u;
    u32 start;
    u32 py;
    u32 max_dots = 18u;

    while (pass[n] != '\0') {
        ++n;
    }
    start = (n > max_dots) ? (n - max_dots) : 0;
    py = y + (h > dot ? (h - dot) / 2u : 0);
    for (i = start; i < n; ++i) {
        u32 dx = x + (i - start) * (dot + gap);
        draw_round_fill(dx, py, dot, dot, dot / 2u, OB_WHITE, 230u);
    }
    ob_draw_caret(x + (n - start) * (dot + gap), y, h);
}

static void ob_draw_glass_pill(u32 x, u32 y, u32 w, u32 h, u8 focus)
{
    u8 glass_a = mix_u8(38u, 86u, focus);
    u8 rim_a = mix_u8(16u, 64u, focus);

    draw_round_fill(x > 0 ? x - 1u : 0, y > 0 ? y - 1u : 0, w + 2u, h + 2u,
                    (h + 2u) / 2u, OB_WHITE, rim_a);
    draw_glass(x, y, w, h, h / 2u, OB_GLASS, glass_a);
    if (w > 24u) {
        draw_round_fill(x + 12u, y + 1u, w - 24u, 1u, 0, OB_WHITE,
                        (u8)(12u + (u32)focus / 12u));
    }
}

static void ob_draw_glass_field(u32 x, u32 y, u32 w, u32 h, const char *text,
                               bool password, bool focused, const char *place)
{
    u8 focus = focused ? 255u : 0;
    bool has = text != NULL && text[0] != '\0';

    ob_draw_glass_pill(x, y, w, h, focus);
    if (has && password) {
        ob_draw_pass_dots(x + 14u, y, h, text);
    } else if (has) {
        u32 ty = y + (h > PLACE_H ? (h - PLACE_H) / 2u : 0);
        draw_text_h(x + 14u, ty, text, OB_WHITE, PLACE_H);
        if (focused) {
            ob_draw_caret(x + 14u + draw_text_width_h(text, PLACE_H) + 2u, y, h);
        }
    } else {
        u32 ty = y + (h > PLACE_H ? (h - PLACE_H) / 2u : 0);
        draw_text_h(x + 14u, ty, place, OB_MUTED, PLACE_H);
        if (focused) {
            ob_draw_caret(x + 14u, y, h);
        }
    }
}

static void ob_draw_glass_btn(u32 x, u32 y, u32 w, u32 h, const char *label,
                             bool hot)
{
    u8 focus = hot ? 255u : 40u;
    u32 tw;
    u32 tx;
    u32 ty;

    ob_draw_glass_pill(x, y, w, h, focus);
    tw = draw_text_width(label, 1);
    tx = x + (w > tw ? (w - tw) / 2u : 0);
    ty = y + (h > FONT_HEIGHT ? (h - FONT_HEIGHT) / 2u : 0);
    draw_text(tx, ty, label, OB_WHITE, 1);
}

static void ob_draw_lock(u32 x, u32 y, struct rgb c, u8 a)
{
    draw_round_fill(x, y + 5u, 10u, 8u, 2u, c, a);
    draw_round_fill(x + 2u, y, 6u, 7u, 3u, c, a);
    draw_round_fill(x + 3u, y + 2u, 4u, 4u, 2u, OB_GLASS, 220u);
}

static void ob_draw_bars(u32 x, u32 y, u8 quality)
{
    u32 i;
    u32 n = (quality >= 75u) ? 4u : (quality >= 50u) ? 3u :
            (quality >= 25u) ? 2u : 1u;

    for (i = 0; i < 4u; ++i) {
        u32 h = 5u + i * 3u;
        u8 a = (i < n) ? 230u : 55u;

        draw_round_fill(x + i * 5u, y + (16u - h), 3u, h, 1u, OB_WHITE, a);
    }
}

static void draw_step(enum ob_step step, u32 hit, u32 lang_focus, bool field_on,
                      bool show_cursor)
{
    struct ob_geom g;
    const char *title = "";
    const char *body = "";
    u32 w = fb_width();
    i32 dy = enter_dy();
    u32 title_y;
    u32 body_y;
    u32 content_y;

    ob_layout(&g, step);
    title_y = (u32)((i32)g.title_y + dy);
    body_y = (u32)((i32)g.body_y + dy);
    content_y = (u32)((i32)g.content_y + dy);

    fb_compose_begin();
    draw_bg_login();
    fb_overlay(4, 10, 22, 22u);

    s_light = region_is_light(g.x0, g.logo_y, g.cw, 180u);
    cursor_set_on_light(s_light);

    ob_draw_logo(g.logo_x, g.logo_y);
    ob_draw_dots(&g, step);

    if (step == OB_LANG) {
        title = i18n(MSG_OB_LANG_TITLE);
        body = i18n(MSG_OB_LANG_BODY);
    } else if (step == OB_WELCOME) {
        title = i18n(MSG_OB_WELCOME_TITLE);
        body = i18n(MSG_OB_WELCOME_BODY);
    } else if (step == OB_NAME) {
        title = i18n(MSG_OB_NAME_TITLE);
        body = i18n(MSG_OB_NAME_BODY);
    } else if (step == OB_PASS) {
        title = i18n(MSG_OB_PASS_TITLE);
        body = i18n(MSG_OB_PASS_BODY);
    } else if (step == OB_WIFI) {
        title = i18n(MSG_OB_WIFI_TITLE);
        body = i18n(MSG_OB_WIFI_BODY);
    } else if (step == OB_CHECK) {
        title = i18n(MSG_OB_WIFI_CHECK);
        body = s_checking ? i18n(MSG_OB_WIFI_CHECK) :
               (s_net_ok ? i18n(MSG_OB_WIFI_OK) : i18n(MSG_OB_WIFI_FAIL));
    } else {
        title = i18n(MSG_OB_ANOTHER_TITLE);
        body = i18n(MSG_OB_ANOTHER_BODY);
    }

    {
        u32 tw = draw_text_width(title, 1);
        u32 bw = draw_text_width(body, 1);
        u32 tx = (w > tw) ? (w - tw) / 2u : g.x0;
        u32 bx = (bw <= g.cw) ? ((w > bw) ? (w - bw) / 2u : g.x0) : g.x0;

        ob_label(tx, title_y, title, 1);
        if (bw <= g.cw) {
            draw_text(bx, body_y, body, muted_col(), 1);
        } else {
            draw_text_clip(g.x0, body_y, g.x0 + g.cw, body, muted_col(), 1);
        }
    }

    if (step == OB_LANG) {
        bool a = lang_focus == 0 || hit == HIT_LANG0;
        bool b = lang_focus == 1 || hit == HIT_LANG1;

        ob_draw_glass_btn(g.x0, content_y, g.cw, ROW_H, "Español", a);
        ob_draw_glass_btn(g.x0, content_y + ROW_H + ROW_GAP, g.cw, ROW_H,
                          "English", b);
    } else if (step == OB_NAME) {
        ob_draw_glass_field(g.x0, content_y, g.cw, FIELD_H, s_name, false,
                            field_on, i18n(MSG_OB_NAME_PLACE));
    } else if (step == OB_PASS) {
        ob_draw_glass_field(g.x0, content_y, g.cw, FIELD_H, s_pass, true,
                            field_on, i18n(MSG_OB_PASS_PLACE));
    } else if (step == OB_WIFI) {
        u32 n = net_count();

        if (!s_scanned) {
            draw_text_clip(g.x0, content_y, g.x0 + g.cw, i18n(MSG_OB_WIFI_SCAN),
                           muted_col(), 1);
        } else if (n == 0) {
            draw_text_clip(g.x0, content_y, g.x0 + g.cw, i18n(MSG_OB_WIFI_NONE),
                           muted_col(), 1);
        } else {
            u32 i;
            u32 row_y = content_y;
            u32 vis = wifi_visible(s_net);
            u32 shown = 0;
            u32 wifi_total = wifi_n();

            ob_scroll_wifi();
            for (i = s_wifi_top; i < wifi_total && shown < vis; ++i) {
                const struct wlan_bss *nb = wlan_get(i);
                bool on = (s_net == i) || (hit == HIT_NET0 + i);
                const char *nm = (nb != NULL) ? nb->ssid : "";

                ob_draw_glass_pill(g.x0, row_y, g.cw, ROW_H, on ? 255u : 24u);
                draw_text_clip(g.x0 + 18u, row_y + 12u, g.x0 + g.cw - 52u, nm,
                               OB_WHITE, 1);
                if (nb != NULL && nb->sec != WLAN_SEC_OPEN) {
                    ob_draw_lock(g.x0 + g.cw - 50u, row_y + 14u, OB_WHITE, 220u);
                }
                ob_draw_bars(g.x0 + g.cw - 30u, row_y + 14u,
                             nb != NULL ? nb->quality : 0);
                row_y += ROW_H + ROW_GAP;
                ++shown;
            }
            if (net_wired() && shown < vis) {
                u32 ei = wifi_total;
                bool on = (s_net == ei) || (hit == HIT_NET0 + ei);

                ob_draw_glass_pill(g.x0, row_y, g.cw, ROW_H, on ? 255u : 24u);
                draw_text(g.x0 + 18u, row_y + 12u, i18n(MSG_OB_NET_WIRED),
                          OB_WHITE, 1);
                row_y += ROW_H + ROW_GAP;
            }
            if (net_needs_pass(s_net)) {
                ob_draw_glass_field(g.x0, row_y + 4u, g.cw, FIELD_H, s_wifi_pass,
                                    true, field_on, i18n(MSG_OB_WIFI_PASS));
            }
        }
    } else if (step == OB_CHECK) {
        u32 bar_w = g.cw;
        u32 fill = s_checking ? ((u32)s_check_t * bar_w) / 12u : bar_w;
        u32 by = content_y + 28u;

        ob_draw_glass_pill(g.x0, by, bar_w, 12u, 40u);
        if (fill > 2u) {
            draw_round_fill(g.x0 + 2u, by + 2u, fill > 4u ? fill - 4u : 1u, 8u,
                            4u, OB_WHITE, 230u);
        }
    }

    {
        struct ob_btns b;

        ob_btns_prep(step, &b, &g);
        if (b.back) {
            ob_draw_glass_btn(b.back_x, g.btn_y, b.bw, g.btn_h, i18n(MSG_OB_BACK),
                              hit == HIT_BACK);
        }
        if (b.skip) {
            ob_draw_glass_btn(b.skip_x, g.btn_y, b.bw, g.btn_h, b.skip_lab,
                              hit == HIT_SKIP);
        }
        if (b.next) {
            ob_draw_glass_btn(b.next_x, g.btn_y, b.bw, g.btn_h, b.next_lab,
                              hit == HIT_NEXT);
        }
    }

    if (show_cursor) {
        cursor_flip((u32)mouse_x(), (u32)mouse_y());
    }
    /* else: leave the scene on the back buffer for a crossfade. */
}

static u32 ob_hit(enum ob_step step, i32 mx, i32 my, u32 lang_focus)
{
    struct ob_geom g;
    struct ob_btns b;
    i32 dy = enter_dy();
    u32 content_y;

    (void)lang_focus;
    ob_layout(&g, step);
    ob_btns_prep(step, &b, &g);
    content_y = (u32)((i32)g.content_y + dy);

    if (b.next && in_rect(mx, my, b.next_x, g.btn_y, b.bw, g.btn_h)) {
        return HIT_NEXT;
    }
    if (b.skip && in_rect(mx, my, b.skip_x, g.btn_y, b.bw, g.btn_h)) {
        return HIT_SKIP;
    }
    if (b.back && in_rect(mx, my, b.back_x, g.btn_y, b.bw, g.btn_h)) {
        return HIT_BACK;
    }
    if (step == OB_LANG) {
        if (in_rect(mx, my, g.x0, content_y, g.cw, ROW_H)) {
            return HIT_LANG0;
        }
        if (in_rect(mx, my, g.x0, content_y + ROW_H + ROW_GAP, g.cw, ROW_H)) {
            return HIT_LANG1;
        }
    }
    if ((step == OB_NAME || step == OB_PASS) &&
        in_rect(mx, my, g.x0, content_y, g.cw, FIELD_H)) {
        return HIT_FIELD;
    }
    if (step == OB_WIFI && s_scanned) {
        u32 i;
        u32 row_y = content_y;
        u32 vis = wifi_visible(s_net);
        u32 shown = 0;
        u32 wifi_total = wifi_n();

        for (i = s_wifi_top; i < wifi_total && shown < vis; ++i) {
            if (in_rect(mx, my, g.x0, row_y, g.cw, ROW_H)) {
                return HIT_NET0 + i;
            }
            row_y += ROW_H + ROW_GAP;
            ++shown;
        }
        if (net_wired() && shown < vis) {
            if (in_rect(mx, my, g.x0, row_y, g.cw, ROW_H)) {
                return HIT_NET0 + wifi_total;
            }
            row_y += ROW_H + ROW_GAP;
        }
        if (net_needs_pass(s_net) &&
            in_rect(mx, my, g.x0, row_y + 4u, g.cw, FIELD_H)) {
            return HIT_FIELD;
        }
    }
    return HIT_NONE;
}

static enum ob_step step_back(enum ob_step step)
{
    if (step == OB_WELCOME) {
        return s_ask_lang ? OB_LANG : OB_WELCOME;
    }
    if (step == OB_NAME) {
        return OB_WELCOME;
    }
    if (step == OB_PASS) {
        return OB_NAME;
    }
    if (step == OB_WIFI) {
        return OB_PASS;
    }
    if (step == OB_CHECK) {
        return OB_WIFI;
    }
    return step;
}

static void ob_enter(enum ob_step step, u32 hover, u32 lang_focus, bool field_on)
{
    s_enter = 255;
    cursor_hide();
    if (s_fade_from != NULL) {
        fb_copy_front(s_fade_from);
    }
    draw_step(step, hover, lang_focus, field_on, false);
    if (s_fade_from != NULL) {
        ui_crossfade_from(s_fade_from);
    }
    cursor_flip((u32)mouse_x(), (u32)mouse_y());
}

static void commit_user(void)
{
    struct ob_geom g;
    u32 tw;
    u32 tx;

    ob_layout(&g, OB_ANOTHER);
    fb_compose_begin();
    draw_bg_login();
    fb_overlay(4, 10, 22, 22u);
    s_light = region_is_light(g.x0, g.logo_y, g.cw, 180u);
    ob_draw_logo(g.logo_x, g.logo_y);
    tw = draw_text_width(i18n(MSG_OB_CREATING), 1);
    tx = (fb_width() > tw) ? (fb_width() - tw) / 2u : g.x0;
    ob_label(tx, g.content_y, i18n(MSG_OB_CREATING), 1);
    cursor_flip((u32)mouse_x(), (u32)mouse_y());
    (void)userdb_add(s_name, s_pass);
    wait_ms(380u);
}

void onboarding_run(bool ask_lang)
{
    enum ob_step step;
    enum ob_step prev;
    u32 lang_focus = (i18n_lang() == LANG_EN) ? 1u : 0u;
    bool field_on = true;
    bool dirty = true;
    i32 lx = -1;
    i32 ly = -1;
    u32 hover = HIT_NONE;
    bool last_caret = false;
    u32 last_hover = HIT_NONE;
    bool first = true;

    s_ask_lang = ask_lang;
    s_name[0] = '\0';
    s_nlen = 0;
    s_pass[0] = '\0';
    s_plen = 0;
    s_net = 0;
    s_net_ok = false;
    s_checking = false;
    s_check_t = 0;
    s_wifi_pass[0] = '\0';
    s_wplen = 0;
    s_scanned = false;
    s_wifi_top = 0;
    s_enter = 255;
    s_caret = true;
    s_fade_from = fb_compose_ready() ? fb_layer_alloc() : NULL;
    step = ask_lang ? OB_LANG : OB_WELCOME;
    prev = step;

    if (!fb_available()) {
        return;
    }
    mouse_set_bounds(fb_width(), fb_height());
    while (keyboard_has_char()) {
        (void)keyboard_get_codepoint();
    }
    while (mouse_has_event()) {
        (void)mouse_get_event();
    }

    for (;;) {
        hover = ob_hit(step, mouse_x(), mouse_y(), lang_focus);
        if (hover == HIT_NEXT || hover == HIT_SKIP || hover == HIT_BACK ||
            hover == HIT_FIELD || hover == HIT_LANG0 || hover == HIT_LANG1 ||
            hover >= HIT_NET0) {
            cursor_set_kind(CURSOR_KIND_POINTER);
        } else {
            cursor_set_kind(CURSOR_KIND_ARROW);
        }

        s_caret = field_on &&
                  (step == OB_NAME || step == OB_PASS ||
                   (step == OB_WIFI && net_needs_pass(s_net))) &&
                  (((time_uptime_ms() / 520u) & 1u) == 0u);
        if (s_caret != last_caret) {
            last_caret = s_caret;
            if (step == OB_NAME || step == OB_PASS ||
                (step == OB_WIFI && net_needs_pass(s_net))) {
                dirty = true;
            }
        }

        if (hover != last_hover) {
            last_hover = hover;
            dirty = true;
        }

        if (!first && step == prev && step == OB_WIFI && !s_scanned) {
            draw_step(step, hover, lang_focus, field_on, true);
            (void)wlan_scan();
            s_scanned = true;
            s_net = 0;
            s_wifi_top = 0;
            field_on = net_needs_pass(s_net);
            dirty = true;
        }

        if (!first && step == prev && step == OB_CHECK && s_checking) {
            if (s_check_t == 0u) {
                draw_step(step, hover, lang_focus, field_on, true);
                s_net_ok = ob_probe_link();
            }
            wait_ms(70u);
            s_check_t++;
            if (s_check_t >= 12u) {
                s_checking = false;
            }
            dirty = true;
        }

        if (keyboard_has_char()) {
            u32 key = keyboard_get_codepoint();

            if (key == KEY_F1) {
                continue;
            }
            if (key == 27u && step != OB_LANG && step != OB_WELCOME) {
                step = step_back(step);
                dirty = true;
            } else if (key == '\t') {
                if (step == OB_LANG) {
                    lang_focus ^= 1u;
                    dirty = true;
                } else if (step == OB_WIFI && net_count() > 0u) {
                    s_net = (s_net + 1u) % net_count();
                    s_wifi_pass[0] = '\0';
                    s_wplen = 0;
                    field_on = net_needs_pass(s_net);
                    dirty = true;
                }
            } else if (key == KEY_DOWN && step == OB_WIFI && net_count() > 0u) {
                s_net = (s_net + 1u) % net_count();
                s_wifi_pass[0] = '\0';
                s_wplen = 0;
                field_on = net_needs_pass(s_net);
                dirty = true;
            } else if (key == KEY_UP && step == OB_WIFI && net_count() > 0u) {
                s_net = (s_net + net_count() - 1u) % net_count();
                s_wifi_pass[0] = '\0';
                s_wplen = 0;
                field_on = net_needs_pass(s_net);
                dirty = true;
            } else if (key == '\n' || key == '\r') {
                if (step == OB_LANG) {
                    i18n_set_lang(lang_focus == 0 ? LANG_ES : LANG_EN);
                    (void)persist_set_u32("lang", (u32)i18n_lang());
                    step = OB_WELCOME;
                    dirty = true;
                } else if (step == OB_NAME) {
                    if (s_nlen > 0) {
                        step = OB_PASS;
                        field_on = true;
                        dirty = true;
                    }
                } else if (step == OB_PASS) {
                    if (s_plen > 0) {
                        step = OB_WIFI;
                        dirty = true;
                    }
                } else if (step == OB_WIFI) {
                    if (net_count() == 0u) {
                        commit_user();
                        step = OB_ANOTHER;
                    } else if (net_is_eth(s_net) || !net_needs_pass(s_net) ||
                               s_wplen > 0u) {
                        step = OB_CHECK;
                        s_checking = true;
                        s_check_t = 0;
                        s_net_ok = false;
                    }
                    dirty = true;
                } else if (step == OB_CHECK && !s_checking) {
                    commit_user();
                    step = OB_ANOTHER;
                    dirty = true;
                } else if (step == OB_WELCOME) {
                    step = OB_NAME;
                    field_on = true;
                    dirty = true;
                } else if (step == OB_ANOTHER) {
                    return;
                }
            } else if (key == '\b' || key == 127u) {
                if (step == OB_NAME) {
                    field_backspace(s_name, &s_nlen);
                    dirty = true;
                } else if (step == OB_PASS) {
                    field_backspace(s_pass, &s_plen);
                    dirty = true;
                } else if (step == OB_WIFI && net_needs_pass(s_net)) {
                    field_backspace(s_wifi_pass, &s_wplen);
                    dirty = true;
                }
            } else if (step == OB_NAME && field_on) {
                field_append(s_name, &s_nlen, NAME_MAX, key);
                dirty = true;
            } else if (step == OB_PASS && field_on) {
                field_append(s_pass, &s_plen, PASS_MAX, key);
                dirty = true;
            } else if (step == OB_WIFI && field_on && net_needs_pass(s_net)) {
                field_append(s_wifi_pass, &s_wplen, PASS_MAX, key);
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
            hit = ob_hit(step, mouse_x(), mouse_y(), lang_focus);
            if (ev.kind == MOUSE_EV_DOWN && ev.button == MOUSE_LEFT) {
                if (hit == HIT_LANG0) {
                    lang_focus = 0;
                    i18n_set_lang(LANG_ES);
                    (void)persist_set_u32("lang", 0);
                    dirty = true;
                } else if (hit == HIT_LANG1) {
                    lang_focus = 1;
                    i18n_set_lang(LANG_EN);
                    (void)persist_set_u32("lang", 1);
                    dirty = true;
                } else if (hit == HIT_FIELD) {
                    field_on = true;
                    dirty = true;
                } else if (hit >= HIT_NET0 && hit < HIT_NET0 + net_count()) {
                    if (s_net != hit - HIT_NET0) {
                        s_wifi_pass[0] = '\0';
                        s_wplen = 0;
                    }
                    s_net = hit - HIT_NET0;
                    field_on = net_needs_pass(s_net);
                    dirty = true;
                } else if (hit == HIT_BACK) {
                    step = step_back(step);
                    dirty = true;
                } else if (hit == HIT_SKIP) {
                    if (step == OB_WIFI || step == OB_CHECK) {
                        commit_user();
                        step = OB_ANOTHER;
                        dirty = true;
                    } else if (step == OB_ANOTHER) {
                        return;
                    }
                } else if (hit == HIT_NEXT) {
                    if (step == OB_LANG) {
                        i18n_set_lang(lang_focus == 0 ? LANG_ES : LANG_EN);
                        (void)persist_set_u32("lang", (u32)i18n_lang());
                        step = OB_WELCOME;
                    } else if (step == OB_WELCOME) {
                        step = OB_NAME;
                        field_on = true;
                    } else if (step == OB_NAME && s_nlen > 0) {
                        step = OB_PASS;
                        field_on = true;
                    } else if (step == OB_PASS && s_plen > 0) {
                        step = OB_WIFI;
                    } else if (step == OB_WIFI) {
                        if (net_count() == 0u) {
                            commit_user();
                            step = OB_ANOTHER;
                        } else if (net_is_eth(s_net) || !net_needs_pass(s_net) ||
                                   s_wplen > 0u) {
                            step = OB_CHECK;
                            s_checking = true;
                            s_check_t = 0;
                            s_net_ok = false;
                        }
                    } else if (step == OB_CHECK && !s_checking) {
                        commit_user();
                        step = OB_ANOTHER;
                    } else if (step == OB_ANOTHER) {
                        s_name[0] = '\0';
                        s_nlen = 0;
                        s_pass[0] = '\0';
                        s_plen = 0;
                        s_net = 0;
                        s_wifi_pass[0] = '\0';
                        s_wplen = 0;
                        step = OB_NAME;
                        field_on = true;
                    }
                    dirty = true;
                }
            }
        }

        if (mouse_x() != lx || mouse_y() != ly) {
            cursor_set_kind((hover == HIT_NEXT || hover == HIT_SKIP ||
                             hover == HIT_BACK || hover == HIT_FIELD ||
                             hover == HIT_LANG0 || hover == HIT_LANG1 ||
                             hover >= HIT_NET0)
                                ? CURSOR_KIND_POINTER
                                : CURSOR_KIND_ARROW);
            cursor_draw((u32)mouse_x(), (u32)mouse_y());
            lx = mouse_x();
            ly = mouse_y();
        }

        if (first || step != prev) {
            ob_enter(step, hover, lang_focus, field_on);
            first = false;
            prev = step;
            dirty = false;
            lx = mouse_x();
            ly = mouse_y();
        } else if (dirty) {
            draw_step(step, hover, lang_focus, field_on, true);
            dirty = false;
            lx = mouse_x();
            ly = mouse_y();
        } else if (mouse_x() == lx && mouse_y() == ly &&
                   !(step == OB_CHECK && s_checking)) {
            __asm__ volatile("hlt");
        }
    }
}
