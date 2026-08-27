#include "compositor.h"
#include "draw.h"
#include "fb.h"
#include "mouse.h"

struct ui_rect {
    u32 x;
    u32 y;
    u32 w;
    u32 h;
    bool on;
};

static struct ui_rect s_damage;
static bool s_full;

static u32 add_sat(u32 a, u32 b)
{
    if (b > ~0u - a) {
        return ~0u;
    }
    return a + b;
}

static void rect_union(struct ui_rect *a, u32 x, u32 y, u32 w, u32 h)
{
    u32 x2;
    u32 y2;
    u32 ax2;
    u32 ay2;

    if (w == 0 || h == 0) {
        return;
    }
    if (!a->on) {
        a->x = x;
        a->y = y;
        a->w = w;
        a->h = h;
        a->on = true;
        return;
    }
    x2 = add_sat(x, w);
    y2 = add_sat(y, h);
    ax2 = add_sat(a->x, a->w);
    ay2 = add_sat(a->y, a->h);
    if (x < a->x) {
        a->x = x;
    }
    if (y < a->y) {
        a->y = y;
    }
    if (x2 > ax2) {
        ax2 = x2;
    }
    if (y2 > ay2) {
        ay2 = y2;
    }
    a->w = ax2 - a->x;
    a->h = ay2 - a->y;
}

static u32 clip_span(u32 origin, u32 extent, u32 limit)
{
    if (origin >= limit) {
        return 0;
    }
    if (extent > limit - origin) {
        return limit - origin;
    }
    return extent;
}

void ui_comp_init(void)
{
    s_damage.on = false;
    s_full = false;
}

void ui_comp_scene_begin(void)
{
    fb_compose_begin();
    s_full = true;
    s_damage.on = false;
}

void ui_comp_damage(u32 x, u32 y, u32 w, u32 h)
{
    u32 fw = fb_width();
    u32 fh = fb_height();
    u32 pad = 2u;

    if (w == 0 || h == 0 || fw == 0 || fh == 0) {
        return;
    }
    if (x >= pad) {
        x -= pad;
        w = add_sat(w, pad);
    } else {
        w = add_sat(w, x);
        x = 0;
    }
    if (y >= pad) {
        y -= pad;
        h = add_sat(h, pad);
    } else {
        h = add_sat(h, y);
        y = 0;
    }
    w = add_sat(w, pad);
    h = add_sat(h, pad);
    w = clip_span(x, w, fw);
    h = clip_span(y, h, fh);
    if (w == 0 || h == 0) {
        return;
    }
    rect_union(&s_damage, x, y, w, h);
}

void ui_comp_mark_full(void)
{
    s_full = true;
}

bool ui_comp_is_full(void)
{
    return s_full;
}

bool ui_comp_has_damage(void)
{
    return s_full || s_damage.on;
}

static void stamp_cursor(void)
{
    i32 mx = mouse_x();
    i32 my = mouse_y();

    if (mx < 0) {
        mx = 0;
    }
    if (my < 0) {
        my = 0;
    }
    cursor_draw((u32)mx, (u32)my);
}

void ui_comp_present(void)
{
    if (s_full) {
        ui_comp_present_heavy();
        return;
    }
    cursor_hide();
    if (s_damage.on) {
        fb_compose_present_rect(s_damage.x, s_damage.y, s_damage.w, s_damage.h);
    }
    fb_compose_end();
    stamp_cursor();
    s_damage.on = false;
    s_full = false;
}

void ui_comp_present_heavy(void)
{
    cursor_invalidate();
    fb_compose_present();
    fb_compose_end();
    stamp_cursor();
    s_damage.on = false;
    s_full = false;
}
