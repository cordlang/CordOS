#include "widget.h"
#include "compositor.h"
#include "draw.h"
#include "fb.h"
#include "font.h"
#include "mouse.h"
#include "theme.h"

#define UI_MAX 48u
#define UI_HOVER_STEP 48u

static const struct rgb W_GLASS = { 0x1C, 0x24, 0x30 };
static const struct rgb W_WHITE = { 0xF7, 0xF8, 0xFA };
static const struct rgb W_MUTED = { 0xC4, 0xC8, 0xD0 };

struct ui_item {
    u32 id;
    u8 hover;
    u8 target;
    bool used;
    bool shown;
};

static struct ui_item s_items[UI_MAX];
static u32 s_n;
static i32 s_mx;
static i32 s_my;
static u8 s_buttons;
static u8 s_prev_buttons;
static bool s_click;
static bool s_pointer;
static bool s_busy;
static bool s_want_full;
static bool s_begun;

static u8 mix_u8(u8 a, u8 b, u8 t)
{
    return (u8)(((u32)a * (255u - (u32)t) + (u32)b * (u32)t) / 255u);
}

static bool in_rect(i32 px, i32 py, u32 x, u32 y, u32 w, u32 h)
{
    return px >= (i32)x && py >= (i32)y && px < (i32)(x + w) &&
           py < (i32)(y + h);
}

static struct ui_item *item_get(u32 id)
{
    u32 i;

    for (i = 0; i < s_n; ++i) {
        if (s_items[i].id == id) {
            s_items[i].used = true;
            return &s_items[i];
        }
    }
    if (s_n >= UI_MAX) {
        return &s_items[0];
    }
    s_items[s_n].id = id;
    s_items[s_n].hover = 0;
    s_items[s_n].target = 0;
    s_items[s_n].used = true;
    s_items[s_n].shown = false;
    ++s_n;
    return &s_items[s_n - 1u];
}

static u8 approach(u8 cur, u8 dest)
{
    if (cur < dest) {
        u8 d = (u8)(dest - cur);
        return (u8)(cur + (d > UI_HOVER_STEP ? UI_HOVER_STEP : d));
    }
    if (cur > dest) {
        u8 d = (u8)(cur - dest);
        return (u8)(cur - (d > UI_HOVER_STEP ? UI_HOVER_STEP : d));
    }
    return cur;
}

static void paint_glass(u32 x, u32 y, u32 w, u32 h, u8 focus)
{
    u8 glass_a = mix_u8(38u, 86u, focus);
    u8 rim_a = mix_u8(16u, 64u, focus);
    u32 rad = h / 2u;

    fb_compose_begin();
    if (x > 0 && y > 0) {
        draw_round_fill(x - 1u, y - 1u, w + 2u, h + 2u, (h + 2u) / 2u, W_WHITE,
                        rim_a);
    }
    draw_glass(x, y, w, h, rad, W_GLASS, glass_a);
    if (w > 24u) {
        draw_round_fill(x + 12u, y + 1u, w - 24u, 1u, 0, W_WHITE,
                        (u8)(12u + (u32)focus / 12u));
    }
}

static void paint_label(u32 x, u32 y, u32 w, u32 h, const char *label)
{
    u32 tw;
    u32 tx;
    u32 ty;

    if (label == NULL) {
        return;
    }
    tw = draw_text_width(label, 1);
    tx = x + (w > tw ? (w - tw) / 2u : 0);
    ty = y + (h > FONT_HEIGHT ? (h - FONT_HEIGHT) / 2u : 0);
    draw_text(tx, ty, label, W_WHITE, 1);
}

static bool widget_hot(u32 x, u32 y, u32 w, u32 h, bool enabled)
{
    return enabled && in_rect(s_mx, s_my, x, y, w, h);
}

static bool widget_click(bool hot, bool enabled)
{
    bool down = (s_buttons & MOUSE_LEFT) != 0;
    bool was = (s_prev_buttons & MOUSE_LEFT) != 0;

    if (!enabled || !hot || s_click) {
        return false;
    }
    if (down && !was) {
        s_click = true;
        return true;
    }
    return false;
}

static bool run_visual(struct ui_item *it, u32 x, u32 y, u32 w, u32 h,
                       bool hot, bool selected)
{
    u8 prev;
    u8 next;

    it->target = selected ? 255u : (hot ? 255u : 0);
    prev = it->hover;
    next = approach(prev, it->target);
    it->hover = next;
    if (next != it->target) {
        s_busy = true;
    }
    if (hot) {
        s_pointer = true;
    }
    if (it->shown && prev == next && next == it->target && !s_want_full) {
        return false;
    }
    it->shown = true;
    paint_glass(x, y, w, h, next);
    ui_comp_damage(x, y, w, h);
    return true;
}

void ui_begin(i32 mx, i32 my, u8 buttons, u32 now_ms)
{
    u32 i;

    (void)now_ms;
    s_mx = mx;
    s_my = my;
    s_prev_buttons = s_begun ? s_buttons : buttons;
    s_buttons = buttons;
    s_click = false;
    s_pointer = false;
    s_busy = false;
    s_want_full = false;
    s_begun = true;
    for (i = 0; i < s_n; ++i) {
        s_items[i].used = false;
    }
}

void ui_want_full(void)
{
    s_want_full = true;
    ui_comp_mark_full();
}

void ui_end(void)
{
    u32 i;
    u32 w = 0;

    for (i = 0; i < s_n; ++i) {
        if (s_items[i].used) {
            s_items[w++] = s_items[i];
        }
    }
    s_n = w;
    cursor_set_kind(s_pointer ? CURSOR_KIND_POINTER : CURSOR_KIND_ARROW);
    if (s_want_full || ui_comp_has_damage()) {
        ui_comp_present();
    } else {
        fb_compose_end();
        cursor_draw((u32)(s_mx < 0 ? 0 : s_mx), (u32)(s_my < 0 ? 0 : s_my));
    }
}

bool ui_busy(void)
{
    return s_busy;
}

bool ui_took_click(void)
{
    return s_click;
}

bool ui_button(u32 id, u32 x, u32 y, u32 w, u32 h, const char *label,
               bool selected, bool enabled)
{
    struct ui_item *it = item_get(id);
    bool hot = widget_hot(x, y, w, h, enabled);
    bool painted;

    painted = run_visual(it, x, y, w, h, hot, selected);
    if (!enabled) {
        it->target = 24u;
    }
    if (painted) {
        paint_label(x, y, w, h, label);
    }
    return widget_click(hot, enabled);
}

bool ui_pill(u32 id, u32 x, u32 y, u32 w, u32 h, const char *label,
             bool selected)
{
    struct ui_item *it = item_get(id);
    bool hot = widget_hot(x, y, w, h, true);
    bool painted;

    painted = run_visual(it, x, y, w, h, hot, selected);
    if (painted && label != NULL) {
        draw_text_clip(x + 18u, y + 12u, x + (w > 52u ? w - 52u : w), label,
                       W_WHITE, 1);
    }
    return widget_click(hot, true);
}

bool ui_field(u32 id, u32 x, u32 y, u32 w, u32 h, const char *text,
              bool password, bool focused, const char *placeholder)
{
    struct ui_item *it = item_get(id);
    bool hot = widget_hot(x, y, w, h, true);
    bool painted;
    bool has = text != NULL && text[0] != '\0';
    u32 ty = y + (h > 16u ? (h - 16u) / 2u : 0);

    painted = run_visual(it, x, y, w, h, hot, focused);
    if (painted) {
        if (has && password) {
            u32 n = 0;
            u32 i;
            u32 start;
            const u32 max_dots = 18u;
            const u32 dot = 5u;
            const u32 gap = 6u;

            while (text[n] != '\0') {
                ++n;
            }
            start = (n > max_dots) ? (n - max_dots) : 0;
            for (i = start; i < n; ++i) {
                u32 dx = x + 14u + (i - start) * (dot + gap);
                draw_round_fill(dx, y + (h > dot ? (h - dot) / 2u : 0), dot, dot,
                                dot / 2u, W_WHITE, 230u);
            }
        } else if (has) {
            draw_text_h(x + 14u, ty, text, W_WHITE, 16u);
        } else if (placeholder != NULL) {
            draw_text_h(x + 14u, ty, placeholder, W_MUTED, 16u);
        }
        if (focused) {
            u32 cx = x + 14u;
            if (has && !password) {
                cx += draw_text_width_h(text, 16u) + 2u;
            }
            fb_fill_rect(cx, y + 8u, 2u, (h > 16u) ? (h - 16u) : h, W_WHITE.r,
                         W_WHITE.g, W_WHITE.b);
        }
    }
    return widget_click(hot, true);
}

bool ui_icon_btn(u32 id, u32 x, u32 y, u32 w, u32 h, enum ui_icon icon,
                 bool accent, bool running)
{
    struct ui_item *it = item_get(id);
    bool hot = widget_hot(x, y, w, h, true);
    u8 prev;
    u8 next;
    u32 ix;
    u32 iy;
    u32 isz = (h > 16u) ? (h - 16u) : h;
    struct rgb col;

    if (isz > 40u) {
        isz = 40u;
    }
    it->target = (hot || accent) ? 255u : 0;
    prev = it->hover;
    next = approach(prev, it->target);
    it->hover = next;
    if (next != it->target) {
        s_busy = true;
    }
    if (hot) {
        s_pointer = true;
    }
    if (!it->shown || prev != next || s_want_full) {
        it->shown = true;
        u8 a = (u8)(20u + ((u32)next * 150u) / 255u);

        fb_compose_begin();
        if (next > 8u) {
            draw_round_fill(x + 6u, y + 8u, w > 12u ? w - 12u : w,
                            h > 16u ? h - 16u : h, 14u, THEME_HOVER, a);
        }
        ix = x + (w > isz ? (w - isz) / 2u : 0);
        iy = y + (h > isz ? (h - isz) / 2u : 0);
        col = (accent || next > 128u) ? THEME_ACCENT : THEME_FG;
        draw_icon(ix, iy, isz, icon, col);
        if (running) {
            u32 dot = x + (w > 6u ? (w - 6u) / 2u : 0);
            draw_round_fill(dot, y + h - 11u, 6u, 6u, 3u, THEME_ACCENT, 255u);
        }
        ui_comp_damage(x, y, w, h);
    }
    return widget_click(hot, true);
}
