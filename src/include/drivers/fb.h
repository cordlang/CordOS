#ifndef CORDOS_FB_H
#define CORDOS_FB_H

#include "types.h"

/* Multiboot2 framebuffer tag (type 8) — common header. */
struct multiboot2_tag_framebuffer {
    u32 type;
    u32 size;
    u64 addr;
    u32 pitch;
    u32 width;
    u32 height;
    u8 bpp;
    u8 fb_type; /* 0 indexed, 1 RGB, 2 EGA text */
    u16 reserved;
};

#define MULTIBOOT2_FRAMEBUFFER_TYPE_INDEXED  0
#define MULTIBOOT2_FRAMEBUFFER_TYPE_RGB      1
#define MULTIBOOT2_FRAMEBUFFER_TYPE_EGA_TEXT 2

bool fb_available(void);
u32 fb_width(void);
u32 fb_height(void);
u32 fb_pitch(void);
u8 fb_bpp(void);
u8 *fb_buffer(void);

bool fb_set_pixel(u32 x, u32 y, u8 r, u8 g, u8 b);
bool fb_get_pixel(u32 x, u32 y, u8 *r, u8 *g, u8 *b);
void fb_blend_pixel(u32 x, u32 y, u8 r, u8 g, u8 b, u8 a);
void fb_fill_rect(u32 x, u32 y, u32 w, u32 h, u8 r, u8 g, u8 b);
void fb_clear(u8 r, u8 g, u8 b);
void fb_overlay(u8 r, u8 g, u8 b, u8 alpha);
/* Apply the last fb_overlay to a sampled color (frost, etc.) so glass
 * AA matches the already-dimmed destination instead of the raw photo. */
void fb_shade_as_overlay(u8 *r, u8 *g, u8 *b);
void fb_hline(u32 x, u32 y, u32 w, u8 r, u8 g, u8 b);
void fb_vline(u32 x, u32 y, u32 h, u8 r, u8 g, u8 b);
void fb_rect(u32 x, u32 y, u32 w, u32 h, u8 r, u8 g, u8 b);

void fb_parse_cmdline(const void *mb2_addr);
void fb_init(const void *mb2_addr);

void fb_compose_init(void);
bool fb_compose_ready(void);
void fb_compose_begin(void);
void fb_compose_end(void);
void fb_compose_present(void);
/* Copy back→front while mixing black. Avoids a bright flash before overlay. */
void fb_present_dimmed(u8 black_alpha);
void fb_compose_present_rect(u32 x, u32 y, u32 w, u32 h);
void fb_compose_copy_rect(u32 x, u32 y, u32 w, u32 h);
/* Composite an RGBA sprite over the composed scene and land the whole rect on
 * the visible page in one pass; pass rgba == NULL to just restore the scene.
 * False means the fast path is unavailable (rect too large, or not 32bpp) and
 * the caller should use its own path. */
bool fb_present_sprite_rect(u32 rx, u32 ry, u32 rw, u32 rh,
                            const u8 *rgba, u32 sw, u32 sh, i32 sx, i32 sy);
void fb_copy_front(u8 *dst);
void fb_blend_to_front(const u8 *from, u8 amount);
u8 *fb_layer_alloc(void);
void fb_layer_capture(u8 *layer);
void fb_layer_restore_rect(const u8 *layer, u32 x, u32 y, u32 w, u32 h);

/* Map a framebuffer pixel to source coords under the same cover policy as blit. */
void fb_cover_src_xy(u32 src_w, u32 src_h, u32 dx, u32 dy, u32 *sx, u32 *sy);
void fb_blit_rgb_cover(const u8 *rgb, u32 src_w, u32 src_h);
void fb_blit_rgba(const u8 *rgba, u32 src_w, u32 src_h, i32 dx, i32 dy);
void fb_blit_rgba_scaled(const u8 *rgba, u32 src_w, u32 src_h, i32 dx, i32 dy,
                         u32 dst_w, u32 dst_h);
void fb_blit_rgba_scaled_a(const u8 *rgba, u32 src_w, u32 src_h, i32 dx, i32 dy,
                           u32 dst_w, u32 dst_h, u8 alpha);

#endif
