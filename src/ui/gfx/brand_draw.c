#include "brand.h"
#include "draw.h"
#include "fb.h"

static u32 umin(u32 a, u32 b)
{
    return a < b ? a : b;
}

static void splash_paint(u8 progress, u8 mark_alpha, i32 y_shift)
{
    u32 w;
    u32 h;
    u32 logo;
    u32 name_w;
    u32 name_h;
    i32 lx;
    i32 ly;
    i32 nx;
    i32 ny;
    u32 bar_w;
    u32 bar_x;
    u32 bar_y;
    u32 fill;
    u32 stack;
    struct rgb track = { 0x22, 0x26, 0x2C };
    struct rgb fillc = { 0xF4, 0xF4, 0xF2 };

    w = fb_width();
    h = fb_height();
    draw_bg_login_frosted();

    logo = BRAND_LOGO_W;
    if (logo > w / 5u) {
        logo = w / 5u;
    }
    if (logo > h / 4u) {
        logo = h / 4u;
    }
    if (logo < 64u) {
        logo = umin(64u, umin(w, h));
    }
    name_w = (BRAND_NAME_W * logo) / BRAND_LOGO_W;
    name_h = (BRAND_NAME_H * logo) / BRAND_LOGO_W;
    if (name_w > w - 16u && w > 16u) {
        name_w = w - 16u;
        name_h = (BRAND_NAME_H * name_w) / BRAND_NAME_W;
        logo = (BRAND_LOGO_W * name_w) / BRAND_NAME_W;
    }

    stack = logo + 28u + name_h;
    ly = (i32)((h > stack) ? (h - stack) / 2u : 24u) + y_shift;
    lx = (i32)((w > logo) ? (w - logo) / 2u : 0);
    ny = ly + (i32)logo + 28;
    nx = (i32)((w > name_w) ? (w - name_w) / 2u : 0);

    if (mark_alpha != 0) {
        fb_blit_rgba_scaled_a(brand_logo_rgba, BRAND_LOGO_W, BRAND_LOGO_H,
                              lx, ly, logo, logo, mark_alpha);
        fb_blit_rgba_scaled_a(brand_name_rgba, BRAND_NAME_W, BRAND_NAME_H,
                              nx, ny, name_w, name_h, mark_alpha);
    }

    bar_w = umin(196u, logo + 16u);
    bar_x = (w > bar_w) ? (w - bar_w) / 2u : 0;
    bar_y = (u32)ny + name_h + 36u;
    if (bar_y + 6u < h && mark_alpha != 0) {
        u8 track_a = (u8)(((u32)mark_alpha * 180u) / 255u);
        draw_round_fill(bar_x, bar_y, bar_w, 4u, 2u, track, track_a);
        fill = ((u32)progress * bar_w) / 255u;
        if (fill > 0) {
            draw_round_fill(bar_x, bar_y, fill, 4u, 2u, fillc, mark_alpha);
        }
    }
}

void draw_boot_splash_to_back(u8 progress, u8 mark_alpha, i32 y_shift)
{
    if (!fb_available()) {
        return;
    }
    fb_compose_begin();
    splash_paint(progress, mark_alpha, y_shift);
}

void draw_boot_splash_ex(u8 progress, u8 mark_alpha, i32 y_shift)
{
    if (!fb_available()) {
        return;
    }
    draw_boot_splash_to_back(progress, mark_alpha, y_shift);
    fb_compose_present();
}

void draw_boot_splash(u8 progress)
{
    draw_boot_splash_ex(progress, 255u, 0);
}
