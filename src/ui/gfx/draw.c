#include "draw.h"
#include "cursor.h"
#include "fb.h"
#include "font.h"
#include "icons.h"
#include "persist.h"
#include "pmm.h"
#include "utf8.h"
#include "wallpaper.h"

static u8 *s_frost;
static u8 *s_frost_desk;
static u8 *s_frost_login;
static u8 *s_frost_login_alt;
static bool s_frost_ok;
static u32 s_login_wp;
static u32 s_desk_wp;
static u32 s_icon_style = ICON_STYLE_BOLD;

static u32 clamp_rad(u32 w, u32 h, u32 rad)
{
    u32 m = w < h ? w : h;
    if (rad * 2u > m) {
        rad = m / 2u;
    }
    return rad;
}

static u32 isqrt_u32(u32 n)
{
    u32 op = n;
    u32 res = 0;
    u32 one = 1u << 30;

    if (n == 0) {
        return 0;
    }
    while (one > op) {
        one >>= 2;
    }
    while (one != 0) {
        if (op >= res + one) {
            op -= res + one;
            res = (res >> 1) + one;
        } else {
            res >>= 1;
        }
        one >>= 2;
    }
    return res;
}

/* Signed distance to a rounded box, 8.8 fixed. 0 = edge. */
static i32 sdf_round_box_88(i32 px, i32 py, i32 x, i32 y, i32 w, i32 h, i32 rad)
{
    i32 cx = x * 256 + w * 128;
    i32 cy = y * 256 + h * 128;
    i32 dx = px * 256 + 128 - cx;
    i32 dy = py * 256 + 128 - cy;
    i32 bx;
    i32 by;
    i32 qx;
    i32 qy;
    i32 ox;
    i32 oy;
    i32 inside;
    u32 len;

    if (dx < 0) {
        dx = -dx;
    }
    if (dy < 0) {
        dy = -dy;
    }
    bx = w * 128 - rad * 256;
    by = h * 128 - rad * 256;
    if (bx < 0) {
        bx = 0;
    }
    if (by < 0) {
        by = 0;
    }
    qx = dx - bx;
    qy = dy - by;
    ox = (qx > 0) ? qx : 0;
    oy = (qy > 0) ? qy : 0;
    len = isqrt_u32((u32)ox * (u32)ox + (u32)oy * (u32)oy);
    inside = (qx > qy) ? qx : qy;
    if (inside > 0) {
        inside = 0;
    }
    return (i32)len + inside - rad * 256;
}

static u8 cover_from_d88(i32 d88)
{
    /* 1px AA: full inside at d<=-0.5, none at d>=+0.5 */
    if (d88 <= -128) {
        return 255u;
    }
    if (d88 >= 128) {
        return 0;
    }
    return (u8)(((128 - d88) * 255) / 256);
}

static void blend_band(u32 x, u32 y, u32 w, u32 h, u32 rad, struct rgb color,
                       u8 alpha, i32 x0, i32 y0, i32 x1, i32 y1)
{
    i32 py;
    i32 px;
    i32 ix = (i32)x;
    i32 iy = (i32)y;
    i32 iw = (i32)w;
    i32 ih = (i32)h;
    i32 ir = (i32)rad;
    i32 fw = (i32)fb_width();
    i32 fh = (i32)fb_height();

    if (x0 < 0) {
        x0 = 0;
    }
    if (y0 < 0) {
        y0 = 0;
    }
    if (x1 > fw) {
        x1 = fw;
    }
    if (y1 > fh) {
        y1 = fh;
    }
    /* Keep AA inside the box — a 1px ring outside shows up as hairlines
     * on the left/right of rounded chrome (dock, pills). */
    if (x0 < ix) {
        x0 = ix;
    }
    if (y0 < iy) {
        y0 = iy;
    }
    if (x1 > ix + iw) {
        x1 = ix + iw;
    }
    if (y1 > iy + ih) {
        y1 = iy + ih;
    }
    for (py = y0; py < y1; ++py) {
        for (px = x0; px < x1; ++px) {
            u8 c = cover_from_d88(sdf_round_box_88(px, py, ix, iy, iw, ih, ir));
            u8 a;
            if (c == 0) {
                continue;
            }
            a = (u8)(((u32)c * (u32)alpha) / 255u);
            if (a != 0) {
                fb_blend_pixel((u32)px, (u32)py, color.r, color.g, color.b, a);
            }
        }
    }
}

void draw_round_fill(u32 x, u32 y, u32 w, u32 h, u32 rad, struct rgb color,
                     u8 alpha)
{
    i32 inner_x;
    i32 inner_y;
    i32 inner_w;
    i32 inner_h;
    u32 fw = fb_width();
    u32 fh = fb_height();

    if (w == 0 || h == 0 || alpha == 0) {
        return;
    }
    rad = clamp_rad(w, h, rad);

    if (rad == 0) {
        if (alpha == 255u) {
            fb_fill_rect(x, y, w, h, color.r, color.g, color.b);
        } else {
            u32 yy;
            u32 xx;
            for (yy = 0; yy < h; ++yy) {
                for (xx = 0; xx < w; ++xx) {
                    fb_blend_pixel(x + xx, y + yy, color.r, color.g, color.b, alpha);
                }
            }
        }
        return;
    }

    inner_x = (i32)x + (i32)rad + 1;
    inner_y = (i32)y + (i32)rad + 1;
    inner_w = (i32)w - (i32)rad * 2 - 2;
    inner_h = (i32)h - (i32)rad * 2 - 2;
    if (inner_w > 0 && inner_h > 0) {
        if (alpha == 255u) {
            fb_fill_rect((u32)inner_x, (u32)inner_y, (u32)inner_w, (u32)inner_h,
                         color.r, color.g, color.b);
        } else {
            u32 yy;
            u32 xx;
            for (yy = 0; yy < (u32)inner_h; ++yy) {
                for (xx = 0; xx < (u32)inner_w; ++xx) {
                    fb_blend_pixel((u32)inner_x + xx, (u32)inner_y + yy,
                                   color.r, color.g, color.b, alpha);
                }
            }
        }
        blend_band(x, y, w, h, rad, color, alpha,
                   (i32)x, (i32)y, (i32)(x + w), inner_y);
        blend_band(x, y, w, h, rad, color, alpha,
                   (i32)x, inner_y + inner_h, (i32)(x + w), (i32)(y + h));
        blend_band(x, y, w, h, rad, color, alpha,
                   (i32)x, inner_y, inner_x, inner_y + inner_h);
        blend_band(x, y, w, h, rad, color, alpha,
                   inner_x + inner_w, inner_y, (i32)(x + w), inner_y + inner_h);
    } else {
        blend_band(x, y, w, h, rad, color, alpha,
                   (i32)x, (i32)y, (i32)(x + w), (i32)(y + h));
    }
    (void)fw;
    (void)fh;
}

static void frost_sample(u32 x, u32 y, u8 *r, u8 *g, u8 *b)
{
    u32 sx;
    u32 sy;
    const u8 *p;

    *r = 0x12;
    *g = 0x16;
    *b = 0x1C;
    if (!s_frost_ok || s_frost == NULL) {
        return;
    }
    fb_cover_src_xy(WALLPAPER_W, WALLPAPER_H, x, y, &sx, &sy);
    if (sx >= WALLPAPER_W || sy >= WALLPAPER_H) {
        return;
    }
    p = s_frost + (sy * WALLPAPER_W + sx) * 3u;
    *r = p[0];
    *g = p[1];
    *b = p[2];
}

static void box_blur_u8(u8 *img, u8 *tmp, u32 w, u32 h, u32 radius)
{
    u32 y;
    u32 x;
    i32 k;

    for (y = 0; y < h; ++y) {
        for (x = 0; x < w; ++x) {
            u32 sum[3] = { 0, 0, 0 };
            u32 n = 0;
            for (k = -(i32)radius; k <= (i32)radius; ++k) {
                i32 xx = (i32)x + k;
                const u8 *p;
                if (xx < 0) {
                    xx = 0;
                }
                if (xx >= (i32)w) {
                    xx = (i32)w - 1;
                }
                p = img + (y * w + (u32)xx) * 3u;
                sum[0] += p[0];
                sum[1] += p[1];
                sum[2] += p[2];
                ++n;
            }
            tmp[(y * w + x) * 3u] = (u8)(sum[0] / n);
            tmp[(y * w + x) * 3u + 1u] = (u8)(sum[1] / n);
            tmp[(y * w + x) * 3u + 2u] = (u8)(sum[2] / n);
        }
    }
    for (y = 0; y < h; ++y) {
        for (x = 0; x < w; ++x) {
            u32 sum[3] = { 0, 0, 0 };
            u32 n = 0;
            for (k = -(i32)radius; k <= (i32)radius; ++k) {
                i32 yy = (i32)y + k;
                const u8 *p;
                if (yy < 0) {
                    yy = 0;
                }
                if (yy >= (i32)h) {
                    yy = (i32)h - 1;
                }
                p = tmp + ((u32)yy * w + x) * 3u;
                sum[0] += p[0];
                sum[1] += p[1];
                sum[2] += p[2];
                ++n;
            }
            img[(y * w + x) * 3u] = (u8)(sum[0] / n);
            img[(y * w + x) * 3u + 1u] = (u8)(sum[1] / n);
            img[(y * w + x) * 3u + 2u] = (u8)(sum[2] / n);
        }
    }
}

static void frost_fill(u8 *dst, const u8 *src, u8 *small, u8 *tmp)
{
    const u32 dw = WALLPAPER_W / 4u;
    const u32 dh = WALLPAPER_H / 4u;
    u32 y;
    u32 x;
    u32 pass;

    for (y = 0; y < dh; ++y) {
        for (x = 0; x < dw; ++x) {
            u32 sy = y * 4u;
            u32 sx = x * 4u;
            u32 s0 = 0;
            u32 s1 = 0;
            u32 s2 = 0;
            u32 yy;
            u32 xx;
            for (yy = 0; yy < 4u; ++yy) {
                for (xx = 0; xx < 4u; ++xx) {
                    const u8 *p =
                        src + ((sy + yy) * WALLPAPER_W + (sx + xx)) * 3u;
                    s0 += p[0];
                    s1 += p[1];
                    s2 += p[2];
                }
            }
            small[(y * dw + x) * 3u] = (u8)(s0 / 16u);
            small[(y * dw + x) * 3u + 1u] = (u8)(s1 / 16u);
            small[(y * dw + x) * 3u + 2u] = (u8)(s2 / 16u);
        }
    }
    for (pass = 0; pass < 3u; ++pass) {
        box_blur_u8(small, tmp, dw, dh, 2u);
    }

    for (y = 0; y < WALLPAPER_H; ++y) {
        u32 fy = (y * (dh - 1u) * 256u) / (WALLPAPER_H - 1u);
        u32 y0 = fy >> 8;
        u32 y1 = (y0 + 1u < dh) ? (y0 + 1u) : y0;
        u32 ty = fy & 255u;
        for (x = 0; x < WALLPAPER_W; ++x) {
            u32 fx = (x * (dw - 1u) * 256u) / (WALLPAPER_W - 1u);
            u32 x0 = fx >> 8;
            u32 x1 = (x0 + 1u < dw) ? (x0 + 1u) : x0;
            u32 tx = fx & 255u;
            const u8 *p00 = small + (y0 * dw + x0) * 3u;
            const u8 *p10 = small + (y0 * dw + x1) * 3u;
            const u8 *p01 = small + (y1 * dw + x0) * 3u;
            const u8 *p11 = small + (y1 * dw + x1) * 3u;
            u8 *d = dst + (y * WALLPAPER_W + x) * 3u;
            u32 ch;
            for (ch = 0; ch < 3u; ++ch) {
                u32 a = ((u32)p00[ch] * (256u - tx) + (u32)p10[ch] * tx) >> 8;
                u32 b = ((u32)p01[ch] * (256u - tx) + (u32)p11[ch] * tx) >> 8;
                d[ch] = (u8)((a * (256u - ty) + b * ty) >> 8);
            }
        }
    }
}

static u8 *frost_alloc(void)
{
    u32 pages = (u32)((WALLPAPER_W * WALLPAPER_H * 3u + PAGE_SIZE - 1u) / PAGE_SIZE);
    u64 phys = pmm_alloc_contiguous(pages);

    if (phys == 0) {
        return NULL;
    }
    return (u8 *)phys;
}

void draw_quality_init(void)
{
    const u32 dw = WALLPAPER_W / 4u;
    const u32 dh = WALLPAPER_H / 4u;
    u8 *small;
    u8 *tmp;
    u32 pages;

    s_frost = NULL;
    s_frost_desk = NULL;
    s_frost_login = NULL;
    s_frost_login_alt = NULL;
    s_frost_ok = false;
    s_login_wp = LOGIN_WP_DEFAULT;
    s_desk_wp = DESK_WP_DEFAULT;
    pages = (u32)((dw * dh * 3u + PAGE_SIZE - 1u) / PAGE_SIZE);
    {
        u64 phys = pmm_alloc_contiguous(pages);
        if (phys == 0) {
            return;
        }
        small = (u8 *)phys;
    }
    {
        u64 phys = pmm_alloc_contiguous(pages);
        if (phys == 0) {
            return;
        }
        tmp = (u8 *)phys;
    }

    s_frost_desk = frost_alloc();
    if (s_frost_desk != NULL) {
        frost_fill(s_frost_desk, wallpaper_rgb, small, tmp);
    }
    s_frost_login = frost_alloc();
    if (s_frost_login != NULL) {
        frost_fill(s_frost_login, wallpaper_login_rgb, small, tmp);
    }
    s_frost_login_alt = frost_alloc();
    if (s_frost_login_alt != NULL) {
        frost_fill(s_frost_login_alt, wallpaper_login_alt_rgb, small, tmp);
    }
    s_frost = s_frost_login != NULL ? s_frost_login : s_frost_desk;
    s_frost_ok = s_frost != NULL;
}

static void frost_use(u8 *map)
{
    s_frost = map;
    s_frost_ok = map != NULL;
}

void draw_bg_atmosphere(void)
{
    if (s_desk_wp == DESK_WP_ABSTRACT) {
        frost_use(s_frost_login_alt);
        fb_blit_rgb_cover(wallpaper_login_alt_rgb, WALLPAPER_W, WALLPAPER_H);
    } else {
        frost_use(s_frost_desk);
        fb_blit_rgb_cover(wallpaper_rgb, WALLPAPER_W, WALLPAPER_H);
    }
}

void wallpaper_set_login(u32 id)
{
    if (id >= LOGIN_WP_COUNT) {
        id = LOGIN_WP_DEFAULT;
    }
    s_login_wp = id;
    (void)persist_set_u32("login_wp", id);
}

u32 wallpaper_login_id(void)
{
    return s_login_wp;
}

void wallpaper_set_desk(u32 id)
{
    if (id >= DESK_WP_COUNT) {
        id = DESK_WP_DEFAULT;
    }
    s_desk_wp = id;
    (void)persist_set_u32("desk_wp", id);
}

u32 wallpaper_desk_id(void)
{
    return s_desk_wp;
}

const u8 *wallpaper_desk_pixels_id(u32 id)
{
    if (id == DESK_WP_ABSTRACT) {
        return wallpaper_login_alt_rgb;
    }
    return wallpaper_rgb;
}

const u8 *wallpaper_desk_pixels(void)
{
    return wallpaper_desk_pixels_id(s_desk_wp);
}

const u8 *wallpaper_login_pixels_id(u32 id)
{
    if (id == LOGIN_WP_ABSTRACT) {
        return wallpaper_login_alt_rgb;
    }
    return wallpaper_login_rgb;
}

const u8 *wallpaper_login_pixels(void)
{
    return wallpaper_login_pixels_id(s_login_wp);
}

void icon_set_style(u32 id)
{
    if (id >= ICON_STYLE_COUNT) {
        id = ICON_STYLE_BOLD;
    }
    s_icon_style = id;
    (void)persist_set_u32("icon_style", id);
}

u32 icon_style(void)
{
    return s_icon_style;
}

void draw_bg_login(void)
{
    if (s_login_wp == LOGIN_WP_ABSTRACT) {
        frost_use(s_frost_login_alt);
    } else {
        frost_use(s_frost_login);
    }
    fb_blit_rgb_cover(wallpaper_login_pixels(), WALLPAPER_W, WALLPAPER_H);
}

void draw_bg_login_frosted(void)
{
    if (s_login_wp == LOGIN_WP_ABSTRACT) {
        frost_use(s_frost_login_alt);
    } else {
        frost_use(s_frost_login);
    }
    if (s_frost_ok && s_frost != NULL) {
        fb_blit_rgb_cover(s_frost, WALLPAPER_W, WALLPAPER_H);
        fb_overlay(8, 10, 14, 70u);
    } else {
        draw_bg_login();
        fb_overlay(6, 8, 12, 90u);
    }
}

void draw_wallpaper_thumb(u32 x, u32 y, u32 w, u32 h, u32 wp_id, bool selected)
{
    const u8 *rgb = wallpaper_login_pixels_id(wp_id);
    u32 row;
    u32 col;
    struct rgb ring = selected ? THEME_ACCENT : THEME_BORDER;

    if (w == 0 || h == 0 || rgb == NULL) {
        return;
    }
    draw_round_fill(x > 3u ? x - 3u : 0, y > 3u ? y - 3u : 0,
                    w + 6u, h + 6u, 12u, ring, selected ? 255u : 180u);
    for (row = 0; row < h; ++row) {
        u32 sy = (row * WALLPAPER_H) / h;
        for (col = 0; col < w; ++col) {
            u32 sx = (col * WALLPAPER_W) / w;
            const u8 *p = rgb + (sy * WALLPAPER_W + sx) * 3u;
            fb_set_pixel(x + col, y + row, p[0], p[1], p[2]);
        }
    }
}

void draw_bg_frosted(void)
{
    if (s_frost_ok) {
        fb_blit_rgb_cover(s_frost, WALLPAPER_W, WALLPAPER_H);
        fb_overlay(8, 10, 14, 70u);
    } else {
        draw_bg_atmosphere();
        fb_overlay(6, 8, 12, 110u);
    }
}

static void glass_pixel(u32 px, u32 py, struct rgb tint, u8 alpha, u8 cover)
{
    u8 fr;
    u8 fg;
    u8 fb;
    u8 r;
    u8 g;
    u8 b;

    if (cover == 0) {
        return;
    }
    frost_sample(px, py, &fr, &fg, &fb);
    /* Dest is wallpaper + overlay; frost is the raw photo. Without this
     * the SDF rim of the dock is a 1px strip of the undimmed fondo. */
    fb_shade_as_overlay(&fr, &fg, &fb);
    r = (u8)(((u32)fr * (255u - alpha) + (u32)tint.r * (u32)alpha) / 255u);
    g = (u8)(((u32)fg * (255u - alpha) + (u32)tint.g * (u32)alpha) / 255u);
    b = (u8)(((u32)fb * (255u - alpha) + (u32)tint.b * (u32)alpha) / 255u);
    if (cover == 255u) {
        fb_set_pixel(px, py, r, g, b);
        return;
    }
    /* AA against the already-drawn dest (sharp wallpaper + overlay).
     * Mixing with frost painted a 1px frosted scanline through the photo. */
    {
        u8 dr;
        u8 dg;
        u8 db;

        if (!fb_get_pixel(px, py, &dr, &dg, &db)) {
            return;
        }
        r = (u8)(((u32)r * (u32)cover + (u32)dr * (255u - (u32)cover)) / 255u);
        g = (u8)(((u32)g * (u32)cover + (u32)dg * (255u - (u32)cover)) / 255u);
        b = (u8)(((u32)b * (u32)cover + (u32)db * (255u - (u32)cover)) / 255u);
    }
    fb_set_pixel(px, py, r, g, b);
}

void draw_glass(u32 x, u32 y, u32 w, u32 h, u32 rad, struct rgb tint, u8 alpha)
{
    i32 px;
    i32 py;
    i32 ix;
    i32 iy;
    i32 iw;
    i32 ih;
    i32 ir;
    i32 inner_x;
    i32 inner_y;
    i32 inner_w;
    i32 inner_h;

    if (w == 0 || h == 0 || alpha == 0) {
        return;
    }
    if (!s_frost_ok) {
        draw_round_fill(x, y, w, h, rad, tint, alpha);
        return;
    }
    rad = clamp_rad(w, h, rad);
    ix = (i32)x;
    iy = (i32)y;
    iw = (i32)w;
    ih = (i32)h;
    ir = (i32)rad;
    inner_x = ix + ir + 1;
    inner_y = iy + ir + 1;
    inner_w = iw - ir * 2 - 2;
    inner_h = ih - ir * 2 - 2;

    if (inner_w > 0 && inner_h > 0) {
        for (py = inner_y; py < inner_y + inner_h; ++py) {
            for (px = inner_x; px < inner_x + inner_w; ++px) {
                glass_pixel((u32)px, (u32)py, tint, alpha, 255u);
            }
        }
    }
    {
        i32 fw = (i32)fb_width();
        i32 fh = (i32)fb_height();

        for (py = iy; py < iy + ih; ++py) {
            if (py < 0 || py >= fh) {
                continue;
            }
            for (px = ix; px < ix + iw; ++px) {
                u8 c;
                if (px < 0 || px >= fw) {
                    continue;
                }
                if (inner_w > 0 && inner_h > 0 &&
                    px >= inner_x && px < inner_x + inner_w &&
                    py >= inner_y && py < inner_y + inner_h) {
                    continue;
                }
                c = cover_from_d88(sdf_round_box_88(px, py, ix, iy, iw, ih, ir));
                glass_pixel((u32)px, (u32)py, tint, alpha, c);
            }
        }
    }
}

static u32 font_index(u32 codepoint)
{
    u32 i;

    for (i = 0; i < FONT_GLYPHS; ++i) {
        if (font_codepoints[i] == codepoint) {
            return i;
        }
    }
    return (u32)'?' - 32u;
}

static u32 glyph_advance(u32 index, u32 scale)
{
    u32 advance;
    u32 cap;

    if (index >= FONT_GLYPHS) {
        index = 0;
    }
    if (scale >= 2u) {
        advance = (u32)font_title_advance[index];
        cap = FONT_TITLE_W;
    } else {
        advance = (u32)font_advance[index];
        cap = FONT_WIDTH;
    }
    if (advance < 1u) {
        advance = 1u;
    }
    if (advance > cap) {
        advance = cap;
    }
    return advance;
}

static u8 draw_rec601_luma(u8 r, u8 g, u8 b)
{
    return (u8)(((u32)r * 77u + (u32)g * 150u + (u32)b * 29u) >> 8);
}

bool draw_region_is_light(u32 x, u32 y, u32 w, u32 h)
{
    const u32 nx = 6u;
    const u32 ny = 3u;
    u32 gx;
    u32 gy;
    u32 sum = 0;
    u32 n = 0;
    u32 fw = fb_width();
    u32 fh = fb_height();

    if (w == 0 || h == 0 || fw == 0 || fh == 0) {
        return false;
    }
    for (gy = 0; gy < ny; ++gy) {
        u32 py = y + (h * (2u * gy + 1u)) / (2u * ny);
        if (py >= fh) {
            py = fh - 1u;
        }
        for (gx = 0; gx < nx; ++gx) {
            u32 px = x + (w * (2u * gx + 1u)) / (2u * nx);
            u8 r;
            u8 g;
            u8 b;

            if (px >= fw) {
                px = fw - 1u;
            }
            if (!fb_get_pixel(px, py, &r, &g, &b)) {
                continue;
            }
            sum += draw_rec601_luma(r, g, b);
            ++n;
        }
    }
    return n != 0u && (sum / n) >= 128u;
}

static u32 draw_glyph(u32 x, u32 y, u32 codepoint, struct rgb color, u32 scale)
{
    const u8 *cover;
    u32 gw;
    u32 gh;
    u32 row;
    u32 col;
    u32 index;
    u32 advance;
    u32 paint_w;

    index = font_index(codepoint);

    /* scale>=2 uses a native title atlas — never pixel-double (that looks jagged). */
    if (scale >= 2u) {
        cover = font_title_alpha[index];
        gw = FONT_TITLE_W;
        gh = FONT_TITLE_H;
    } else {
        cover = font_alpha[index];
        gw = FONT_WIDTH;
        gh = FONT_HEIGHT;
    }

    advance = glyph_advance(index, scale);
    /* Paint only the advance (plus 1px of AA overhang). Blitting the full
     * cell used to leave a FONT_WIDTH-sized hole after every letter. */
    paint_w = advance + 1u;
    if (paint_w > gw) {
        paint_w = gw;
    }

    for (row = 0; row < gh; ++row) {
        for (col = 0; col < paint_w; ++col) {
            u8 a = cover[row * gw + col];
            if (a != 0) {
                fb_blend_pixel(x + col, y + row, color.r, color.g, color.b, a);
            }
        }
    }

    return advance;
}

u32 draw_text_width(const char *text, u32 scale)
{
    u32 n = 0;

    while (text != NULL && *text != '\0') {
        u32 codepoint = utf8_decode(&text);
        n += glyph_advance(font_index(codepoint), scale);
    }
    return n;
}

void draw_text_centered(u32 cx, u32 y, const char *text, struct rgb color,
                        u32 scale)
{
    u32 tw = draw_text_width(text, scale);
    u32 x = (cx > tw / 2u) ? (cx - tw / 2u) : 0;
    draw_text(x, y, text, color, scale);
}

void draw_text(u32 x, u32 y, const char *text, struct rgb color, u32 scale)
{
    u32 cx = x;

    if (text == NULL) {
        return;
    }

    while (*text != '\0') {
        cx += draw_glyph(cx, y, utf8_decode(&text), color, scale);
    }
}

static u32 draw_glyph_h(u32 x, u32 y, u32 codepoint, struct rgb color, u32 height)
{
    const u8 *cover;
    u32 gw = FONT_WIDTH;
    u32 gh = FONT_HEIGHT;
    u32 index = font_index(codepoint);
    u32 advance = glyph_advance(index, 1);
    u32 dst_adv;
    u32 dst_w;
    u32 row;
    u32 col;

    if (height < 1u) {
        height = 1u;
    }
    if (height >= FONT_HEIGHT) {
        return draw_glyph(x, y, codepoint, color, 1);
    }
    cover = font_alpha[index];
    dst_adv = (advance * height + FONT_HEIGHT / 2u) / FONT_HEIGHT;
    if (dst_adv < 1u) {
        dst_adv = 1u;
    }
    dst_w = dst_adv + 1u;
    {
        u32 src_w = advance + 1u;
        if (src_w > gw) {
            src_w = gw;
        }
        if (src_w < 1u) {
            src_w = 1u;
        }
        for (row = 0; row < height; ++row) {
            u32 fy = (height == 1u) ? 0
                                    : (row * (gh - 1u) * 256u) / (height - 1u);
            u32 y0 = fy >> 8;
            u32 y1 = (y0 + 1u < gh) ? (y0 + 1u) : y0;
            u32 ty = fy & 255u;
            for (col = 0; col < dst_w; ++col) {
                u32 fx = (dst_w == 1u) ? 0
                                       : (col * (src_w - 1u) * 256u) / (dst_w - 1u);
                u32 x0 = fx >> 8;
                u32 x1 = (x0 + 1u < src_w) ? (x0 + 1u) : x0;
                u32 tx = fx & 255u;
                u32 a0 = cover[y0 * gw + x0];
                u32 a1 = cover[y0 * gw + x1];
                u32 b0 = cover[y1 * gw + x0];
                u32 b1 = cover[y1 * gw + x1];
                u32 a = (a0 * (256u - tx) + a1 * tx) >> 8;
                u32 b = (b0 * (256u - tx) + b1 * tx) >> 8;
                u8 cov = (u8)((a * (256u - ty) + b * ty) >> 8);
                if (cov != 0) {
                    fb_blend_pixel(x + col, y + row, color.r, color.g, color.b, cov);
                }
            }
        }
    }
    return dst_adv;
}

u32 draw_text_width_h(const char *text, u32 height)
{
    u32 n = 0;

    if (height >= FONT_HEIGHT) {
        return draw_text_width(text, 1);
    }
    if (height < 1u) {
        height = 1u;
    }
    while (text != NULL && *text != '\0') {
        u32 adv = glyph_advance(font_index(utf8_decode(&text)), 1);
        u32 a = (adv * height + FONT_HEIGHT / 2u) / FONT_HEIGHT;
        n += (a < 1u) ? 1u : a;
    }
    return n;
}

void draw_text_h(u32 x, u32 y, const char *text, struct rgb color, u32 height)
{
    u32 cx = x;

    if (text == NULL) {
        return;
    }
    if (height >= FONT_HEIGHT) {
        draw_text(x, y, text, color, 1);
        return;
    }
    while (*text != '\0') {
        cx += draw_glyph_h(cx, y, utf8_decode(&text), color, height);
    }
}

void draw_text_clip(u32 x, u32 y, u32 max_x, const char *text, struct rgb color,
                    u32 scale)
{
    u32 cx = x;

    if (text == NULL) {
        return;
    }
    while (*text != '\0') {
        u32 codepoint = utf8_decode(&text);
        u32 index = font_index(codepoint);
        u32 advance = glyph_advance(index, scale);
        if (cx + advance > max_x) {
            break;
        }
        draw_glyph(cx, y, codepoint, color, scale);
        cx += advance;
    }
}

void draw_icon_styled(u32 x, u32 y, u32 size, enum ui_icon icon, struct rgb color,
                      u32 style)
{
    const u8 *src;
    u32 dim;
    u32 row;
    u32 col;

    if ((u32)icon >= ICON_PACK) {
        icon = UI_ICON_FILES;
    }
    if (style >= ICON_STYLE_COUNT) {
        style = ICON_STYLE_BOLD;
    }

    if (size <= 28u) {
        src = icon24_rgba[style][icon];
        dim = ICON_PX_SM;
    } else {
        src = icon48_rgba[style][icon];
        dim = ICON_PX;
    }

    if (size < 8u) {
        size = 8u;
    }

    if (size == dim) {
        for (row = 0; row < size; ++row) {
            for (col = 0; col < size; ++col) {
                u32 idx = (row * dim + col) * 4u;
                u8 a = src[idx + 3u];
                if (a == 0) {
                    continue;
                }
                if (dim == ICON_PX_SM) {
                    fb_blend_pixel(x + col, y + row, color.r, color.g, color.b, a);
                } else {
                    fb_blend_pixel(x + col, y + row, src[idx], src[idx + 1u],
                                   src[idx + 2u], a);
                }
            }
        }
        return;
    }

    for (row = 0; row < size; ++row) {
        u32 fy = (row * (dim - 1u) * 256u) / (size - 1u);
        u32 y0 = fy >> 8;
        u32 y1 = (y0 + 1u < dim) ? (y0 + 1u) : y0;
        u32 ty = fy & 255u;
        for (col = 0; col < size; ++col) {
            u32 fx = (col * (dim - 1u) * 256u) / (size - 1u);
            u32 x0 = fx >> 8;
            u32 x1 = (x0 + 1u < dim) ? (x0 + 1u) : x0;
            u32 tx = fx & 255u;
            const u8 *p00 = src + (y0 * dim + x0) * 4u;
            const u8 *p10 = src + (y0 * dim + x1) * 4u;
            const u8 *p01 = src + (y1 * dim + x0) * 4u;
            const u8 *p11 = src + (y1 * dim + x1) * 4u;
            u8 ch[4];
            u32 k;
            for (k = 0; k < 4u; ++k) {
                u32 a = ((u32)p00[k] * (256u - tx) + (u32)p10[k] * tx) >> 8;
                u32 b = ((u32)p01[k] * (256u - tx) + (u32)p11[k] * tx) >> 8;
                ch[k] = (u8)((a * (256u - ty) + b * ty) >> 8);
            }
            if (ch[3] == 0) {
                continue;
            }
            if (dim == ICON_PX_SM) {
                fb_blend_pixel(x + col, y + row, color.r, color.g, color.b, ch[3]);
            } else {
                fb_blend_pixel(x + col, y + row, ch[0], ch[1], ch[2], ch[3]);
            }
        }
    }
}

void draw_icon(u32 x, u32 y, u32 size, enum ui_icon icon, struct rgb color)
{
    draw_icon_styled(x, y, size, icon, color, s_icon_style);
}

void draw_icon_style_thumb(u32 x, u32 y, u32 w, u32 h, u32 style, bool selected)
{
    struct rgb ring = selected ? THEME_ACCENT : THEME_BORDER;
    u32 ix;
    u32 iy;
    u32 isz;

    if (w < 16u || h < 16u) {
        return;
    }
    draw_round_fill(x > 3u ? x - 3u : 0, y > 3u ? y - 3u : 0,
                    w + 6u, h + 6u, 14u, ring, selected ? 255u : 180u);
    draw_glass(x, y, w, h, 12u, THEME_GLASS, 200u);
    isz = (w < h ? w : h) > 20u ? (w < h ? w : h) - 16u : 16u;
    if (isz > 48u) {
        isz = 48u;
    }
    ix = x + (w > isz ? (w - isz) / 2u : 0);
    iy = y + (h > isz ? (h - isz) / 2u : 0);
    draw_icon_styled(ix, iy, isz, UI_ICON_SETTINGS, THEME_FG, style);
}

#define TITLEBAR_H 48u

static void draw_mark_x(u32 cx, u32 cy, struct rgb col)
{
    i32 i;
    i32 ox = (i32)cx;
    i32 oy = (i32)cy;

    for (i = -4; i <= 4; ++i) {
        fb_blend_pixel((u32)(ox + i), (u32)(oy + i), col.r, col.g, col.b, 255u);
        fb_blend_pixel((u32)(ox + i), (u32)(oy - i), col.r, col.g, col.b, 255u);
        if (i < 4) {
            fb_blend_pixel((u32)(ox + i + 1), (u32)(oy + i), col.r, col.g, col.b, 180u);
            fb_blend_pixel((u32)(ox + i), (u32)(oy + i + 1), col.r, col.g, col.b, 180u);
            fb_blend_pixel((u32)(ox + i + 1), (u32)(oy - i), col.r, col.g, col.b, 180u);
            fb_blend_pixel((u32)(ox + i), (u32)(oy - i - 1), col.r, col.g, col.b, 180u);
        }
    }
}

static void draw_mark_minus(u32 cx, u32 cy, struct rgb col)
{
    u32 i;

    for (i = 0; i < 9u; ++i) {
        fb_blend_pixel(cx - 4u + i, cy, col.r, col.g, col.b, 255u);
        fb_blend_pixel(cx - 4u + i, cy + 1u, col.r, col.g, col.b, 220u);
    }
}

void draw_window_frame(u32 x, u32 y, u32 w, u32 h, const char *title,
                       bool focused, bool close_hot)
{
    u32 rad = THEME_RAD_WIN;
    u32 by;
    struct rgb ink = { 0x3A, 0x22, 0x22 };
    struct rgb ink2 = { 0x3A, 0x32, 0x14 };

    if (w < 56u || h < 40u) {
        return;
    }

    draw_glass(x, y, w, h, rad, THEME_GLASS, 90u);
    draw_round_fill(x, y, w, TITLEBAR_H, rad, focused ? THEME_TITLE : THEME_BG2, 220u);
    fb_fill_rect(x, y + TITLEBAR_H / 2u, w, TITLEBAR_H - TITLEBAR_H / 2u,
                 (focused ? THEME_TITLE : THEME_BG2).r,
                 (focused ? THEME_TITLE : THEME_BG2).g,
                 (focused ? THEME_TITLE : THEME_BG2).b);

    by = y + (TITLEBAR_H - 16u) / 2u;
    draw_round_fill(x + 16, by, 16, 16, 8,
                    close_hot ? THEME_FG : THEME_DANGER, 255u);
    draw_mark_x(x + 24, by + 8, close_hot ? THEME_BG0 : ink);
    draw_round_fill(x + 38, by, 16, 16, 8, THEME_ACCENT, 200u);
    draw_mark_minus(x + 46, by + 8, ink2);
    draw_round_fill(x + 60, by, 16, 16, 8, THEME_ACCENT, 110u);
    {
        u32 ty = y + (TITLEBAR_H > FONT_HEIGHT ? (TITLEBAR_H - FONT_HEIGHT) / 2u : 0);
        draw_text_clip(x + 88, ty, x + w - 18, title != NULL ? title : "",
                       focused ? THEME_FG : THEME_FG_DIM, 1);
    }
}

static bool cursor_on;
static i32 cursor_sx;
static i32 cursor_sy;
static enum cursor_kind s_cursor_kind;
static bool s_cursor_on_light;
static u8 cursor_under[CURSOR_H * CURSOR_W * 3u];
static u32 cursor_under_w;
static u32 cursor_under_h;
static i32 cursor_under_x;
static i32 cursor_under_y;
static bool cursor_under_ok;

static void cursor_restore_under(void);
static void cursor_save_under(i32 x, i32 y);

static void cursor_hotspot(u32 *hot_x, u32 *hot_y, const u8 **spr)
{
    bool light = s_cursor_on_light;

    if (s_cursor_kind == CURSOR_KIND_POINTER) {
        *spr = light ? cursor_pointer_dark_rgba : cursor_pointer_rgba;
        *hot_x = CURSOR_PTR_HOT_X;
        *hot_y = CURSOR_PTR_HOT_Y;
    } else {
        *spr = light ? cursor_arrow_dark_rgba : cursor_rgba;
        *hot_x = CURSOR_HOT_X;
        *hot_y = CURSOR_HOT_Y;
    }
}

static void cursor_erase(void)
{
    if (!cursor_on) {
        return;
    }
    fb_compose_end();
    cursor_restore_under();
    cursor_on = false;
}

static void cursor_blit_base(u32 mx, u32 my)
{
    const u8 *spr;
    u32 hot_x;
    u32 hot_y;
    u32 row;
    u32 col;
    i32 x;
    i32 y;
    u32 fw = fb_width();
    u32 fh = fb_height();

    cursor_hotspot(&hot_x, &hot_y, &spr);
    x = (i32)mx - (i32)hot_x;
    y = (i32)my - (i32)hot_y;
    for (row = 0; row < CURSOR_H; ++row) {
        for (col = 0; col < CURSOR_W; ++col) {
            u32 i = (row * CURSOR_W + col) * 4u;
            u8 a = spr[i + 3u];
            i32 px;
            i32 py;
            if (a == 0) {
                continue;
            }
            px = x + (i32)col;
            py = y + (i32)row;
            if (px < 0 || py < 0 || (u32)px >= fw || (u32)py >= fh) {
                continue;
            }
            fb_blend_pixel((u32)px, (u32)py, spr[i], spr[i + 1u], spr[i + 2u], a);
        }
    }
    cursor_sx = x;
    cursor_sy = y;
    cursor_on = true;
}

static void cursor_save_under(i32 x, i32 y)
{
    u32 row;
    u32 col;
    u32 fw = fb_width();
    u32 fh = fb_height();
    i32 x0 = x;
    i32 y0 = y;
    u32 w = CURSOR_W;
    u32 h = CURSOR_H;

    cursor_under_ok = false;
    if (x0 < 0) {
        w -= (u32)(-x0);
        x0 = 0;
    }
    if (y0 < 0) {
        h -= (u32)(-y0);
        y0 = 0;
    }
    if ((u32)x0 >= fw || (u32)y0 >= fh || w == 0 || h == 0) {
        return;
    }
    if ((u32)x0 + w > fw) {
        w = fw - (u32)x0;
    }
    if ((u32)y0 + h > fh) {
        h = fh - (u32)y0;
    }
    for (row = 0; row < h; ++row) {
        for (col = 0; col < w; ++col) {
            u8 r;
            u8 g;
            u8 b;
            u32 i = (row * CURSOR_W + col) * 3u;

            if (!fb_get_pixel((u32)x0 + col, (u32)y0 + row, &r, &g, &b)) {
                r = 0;
                g = 0;
                b = 0;
            }
            cursor_under[i] = r;
            cursor_under[i + 1u] = g;
            cursor_under[i + 2u] = b;
        }
    }
    cursor_under_x = x0;
    cursor_under_y = y0;
    cursor_under_w = w;
    cursor_under_h = h;
    cursor_under_ok = true;
}

static void cursor_restore_under(void)
{
    u32 row;
    u32 col;

    if (!cursor_under_ok) {
        return;
    }
    for (row = 0; row < cursor_under_h; ++row) {
        for (col = 0; col < cursor_under_w; ++col) {
            u32 i = (row * CURSOR_W + col) * 3u;
            (void)fb_set_pixel((u32)cursor_under_x + col,
                               (u32)cursor_under_y + row,
                               cursor_under[i], cursor_under[i + 1u],
                               cursor_under[i + 2u]);
        }
    }
    cursor_under_ok = false;
}

void cursor_invalidate(void)
{
    cursor_on = false;
    cursor_under_ok = false;
}

void cursor_hide(void)
{
    cursor_erase();
}

void cursor_set_kind(enum cursor_kind kind)
{
    if (kind != CURSOR_KIND_ARROW && kind != CURSOR_KIND_POINTER) {
        kind = CURSOR_KIND_ARROW;
    }
    s_cursor_kind = kind;
}

void cursor_set_on_light(bool on_light)
{
    s_cursor_on_light = on_light;
}

static void cursor_stamp(u32 x, u32 y)
{
    const u8 *spr;
    u32 hot_x;
    u32 hot_y;
    u32 fw = fb_width();
    u32 fh = fb_height();

    if (fw == 0 || fh == 0) {
        return;
    }
    if (x >= fw) {
        x = fw - 1u;
    }
    if (y >= fh) {
        y = fh - 1u;
    }
    cursor_hotspot(&hot_x, &hot_y, &spr);
    (void)spr;
    fb_compose_end();
    cursor_restore_under();
    cursor_save_under((i32)x - (i32)hot_x, (i32)y - (i32)hot_y);
    cursor_blit_base(x, y);
}

void cursor_draw(u32 x, u32 y)
{
    /* Mouse-move path: never present the whole frame. A full LFB copy
     * while the pointer is moving is what made the 1px splash row
     * appear and vanish with the cursor. */
    cursor_stamp(x, y);
}

void cursor_flip(u32 x, u32 y)
{
    /* Present wipes the LFB. Drop the saved under from the previous
     * frame or restore_under paints the old unhovered button back
     * under the pointer (hover looks dead). */
    if (!fb_compose_ready()) {
        fb_compose_present();
        cursor_invalidate();
        cursor_stamp(x, y);
        return;
    }
    cursor_invalidate();
    fb_compose_present();
    cursor_stamp(x, y);
}

void draw_panel(u32 x, u32 y, u32 w, u32 h, bool focused)
{
    draw_glass(x, y, w, h, THEME_RAD_CARD, THEME_GLASS, 100u);
    if (focused) {
        draw_round_fill(x + 2, y + 2, w > 4u ? w - 4u : w, 3, 2, THEME_ACCENT, 255u);
    }
}

void draw_field(u32 x, u32 y, u32 w, u32 h, const char *text, bool password,
                bool focused)
{
    char masked[64];
    const char *show = text;
    u32 i;
    u32 max_chars;
    u32 tx;
    u32 ty;
    u32 rad = h / 2u > THEME_RAD_FIELD ? THEME_RAD_FIELD : h / 2u;
    bool light = draw_region_is_light(x, y, w, h);
    struct rgb border = light ? (struct rgb){ 0x1A, 0x1A, 0x1C }
                              : (struct rgb){ 0xF7, 0xF8, 0xFA };

    draw_round_fill(x > 0u ? x - 1u : 0u, y > 0u ? y - 1u : 0u,
                    w + 2u, h + 2u, rad + 1u, border, focused ? 230u : 150u);
    draw_round_fill(x, y, w, h, rad, THEME_FIELD, 230u);
    if (focused) {
        draw_round_fill(x, y, w, 3, 2, THEME_ACCENT, 255u);
        draw_round_fill(x, y + h - 3u, w, 3, 2, THEME_ACCENT, 255u);
    }

    if (password && text != NULL) {
        for (i = 0; i < sizeof(masked) - 1u && text[i] != '\0'; ++i) {
            masked[i] = '*';
        }
        masked[i] = '\0';
        show = masked;
    }

    max_chars = (w > 28u) ? (w - 28u) : 0;
    if (show != NULL && max_chars > 0) {
        while (show[0] != '\0' && draw_text_width(show, 1) > max_chars) {
            ++show;
        }
    }

    tx = x + 14;
    ty = y + (h > FONT_HEIGHT ? (h - FONT_HEIGHT) / 2u : 0);
    draw_text(tx, ty, show != NULL ? show : "", THEME_FG, 1);

    if (focused) {
        u32 caret = tx + draw_text_width(show != NULL ? show : "", 1) + 1u;
        fb_fill_rect(caret, ty + 2, 2, FONT_HEIGHT > 4u ? FONT_HEIGHT - 4u : FONT_HEIGHT,
                     THEME_ACCENT.r, THEME_ACCENT.g, THEME_ACCENT.b);
    }
}

void draw_button(u32 x, u32 y, u32 w, u32 h, const char *label, bool focused)
{
    u32 tw;
    u32 tx;
    u32 ty;
    u32 rad = h < THEME_RAD_BTN * 2u ? h / 2u : THEME_RAD_BTN;
    bool light = draw_region_is_light(x, y, w, h);
    struct rgb border = light ? (struct rgb){ 0x1A, 0x1A, 0x1C }
                              : (struct rgb){ 0xF7, 0xF8, 0xFA };

    draw_round_fill(x > 0u ? x - 1u : 0u, y > 0u ? y - 1u : 0u,
                    w + 2u, h + 2u, rad + 1u, border, focused ? 230u : 150u);

    if (focused) {
        draw_round_fill(x, y, w, h, rad, THEME_ACCENT, 255u);
        tw = draw_text_width(label, 1);
        tx = x + (w > tw ? (w - tw) / 2u : 0);
        ty = y + (h > FONT_HEIGHT ? (h - FONT_HEIGHT) / 2u : 0);
        draw_text(tx, ty, label, THEME_BG0, 1);
    } else {
        draw_round_fill(x, y, w, h, rad, THEME_HOVER, 230u);
        tw = draw_text_width(label, 1);
        tx = x + (w > tw ? (w - tw) / 2u : 0);
        ty = y + (h > FONT_HEIGHT ? (h - FONT_HEIGHT) / 2u : 0);
        draw_text(tx, ty, label, THEME_FG, 1);
    }
}
