#include "fb.h"
#include "gamma.h"
#include "io.h"
#include "multiboot2.h"
#include "pmm.h"
#include "serial.h"
#include "string.h"
#include "vmm.h"

/*
 * High canonical window for MMIO framebuffer (outside identity RAM).
 *
 * This window is not a single page: it spans the whole visible surface plus
 * slack, so it reaches FB_VIRT_BASE + ~10 MiB at 1920x1080x32 and up to
 * ~128 MiB at 8K. Any other driver that maps MMIO must stay clear of it —
 * see the window map in src/drivers/usb/ehci.c. A driver mapping inside this
 * range replaces a framebuffer page-table entry, and the affected pixels then
 * never reach VRAM at all.
 */
#define FB_VIRT_BASE 0xFFFF800000100000ull

/* Bochs/QEMU/VirtualBox VBE DISPI — bump past Multiboot 640x480. */
#define VBE_DISPI_IOPORT_INDEX 0x01CEu
#define VBE_DISPI_IOPORT_DATA  0x01CFu
#define VBE_DISPI_INDEX_ID           0x0u
#define VBE_DISPI_INDEX_XRES         0x1u
#define VBE_DISPI_INDEX_YRES         0x2u
#define VBE_DISPI_INDEX_BPP          0x3u
#define VBE_DISPI_INDEX_ENABLE       0x4u
#define VBE_DISPI_INDEX_VIRT_WIDTH   0x6u
#define VBE_DISPI_INDEX_VIRT_HEIGHT  0x7u
#define VBE_DISPI_INDEX_X_OFFSET     0x8u
#define VBE_DISPI_INDEX_Y_OFFSET     0x9u
#define VBE_DISPI_DISABLED           0x00u
#define VBE_DISPI_ENABLED            0x01u
#define VBE_DISPI_GETCAPS            0x02u
#define VBE_DISPI_8BIT_DAC           0x20u
#define VBE_DISPI_LFB_ENABLED        0x40u
#define VBE_DISPI_ID0                0xB0C0u
#define VBE_DISPI_ID5                0xB0C5u

static u8 *fb_front;
static u8 *fb_back;
static u8 *fb_base;
static u64 fb_phys;
static u32 fb_pitch_bytes;
static u32 fb_page_bytes;
static u32 fb_vis_page;
static bool fb_flip_ok;
static bool fb_scene_dirty;
static u32 fb_w;
static u32 fb_h;
static u8 fb_bits;
static bool fb_ready;
static u32 fb_want_w;
static u32 fb_want_h;
/* 1 when a clamp column exists on each side of the visible row (pitch > width). */
static u32 fb_x0;
/* Last full-frame overlay. Glass samples frost without walking dest, so
 * it must reuse this or the dock silhouette is a 1px strip of raw wallpaper. */
static u8 ov_r;
static u8 ov_g;
static u8 ov_b;
static u8 ov_a;

static u32 fb_pxb(void)
{
    return fb_bits / 8u;
}

/* XRGB with opaque X. Alpha 0 made VBox/DWM treat edge samples as
 * transparent, so the neighboring scanline (the other side of the
 * wallpaper) showed through as a 1px hairline. */
static u32 fb_pack32(u8 r, u8 g, u8 b)
{
    return 0xFF000000u | ((u32)r << 16) | ((u32)g << 8) | (u32)b;
}

static u32 fb_vis_off(u32 x, u32 y)
{
    return y * fb_pitch_bytes + (x + fb_x0) * fb_pxb();
}

static u32 *fb_vis32(u8 *base, u32 y)
{
    return (u32 *)(base + y * fb_pitch_bytes) + fb_x0;
}

static void fb_halo_row(u8 *base, u32 y)
{
    u32 *raw;
    u32 pitch_px;

    /* Only pad unused bytes on the same scanline. Never write vis[w]
     * when pitch==width: that is the next row and shears the frame. */
    if (fb_bits != 32 || base == NULL || fb_w == 0 || fb_x0 != 0) {
        return;
    }
    raw = (u32 *)(base + y * fb_pitch_bytes);
    pitch_px = fb_pitch_bytes / 4u;
    if (pitch_px > fb_w) {
        u32 x;
        u32 edge = raw[fb_w - 1u];

        for (x = fb_w; x < pitch_px; ++x) {
            raw[x] = edge;
        }
    }
}

static void fb_halo_all(u8 *base)
{
    u32 y;

    if (base == NULL) {
        return;
    }
    for (y = 0; y < fb_h; ++y) {
        fb_halo_row(base, y);
    }
}

static void fb_seal_edges(void);

static void fb_put_pixel_raw(u32 x, u32 y, u8 r, u8 g, u8 b)
{
    u8 *pixel;
    u32 offset;

    if (!fb_ready || x >= fb_w || y >= fb_h) {
        return;
    }

    offset = fb_vis_off(x, y);
    pixel = fb_base + offset;

    if (fb_bits == 32) {
        pixel[0] = b;
        pixel[1] = g;
        pixel[2] = r;
        pixel[3] = 0xFF;
    } else if (fb_bits == 24) {
        pixel[0] = b;
        pixel[1] = g;
        pixel[2] = r;
    } else if (fb_bits == 16) {
        u16 c = (u16)(((u16)(r >> 3) << 11) | ((u16)(g >> 2) << 5) | (u16)(b >> 3));
        pixel[0] = (u8)(c & 0xffu);
        pixel[1] = (u8)(c >> 8);
    }
}

static bool fb_map_range(u64 phys, u64 size)
{
    u64 off;
    u64 pages;

    if (size == 0) {
        return false;
    }

    pages = (size + PAGE_SIZE - 1u) / PAGE_SIZE;
    for (off = 0; off < pages * PAGE_SIZE; off += PAGE_SIZE) {
        vmm_map_page(FB_VIRT_BASE + off, (phys + off) & ~0xFFFull,
                     PAGE_PRESENT | PAGE_WRITE);
    }

    fb_front = (u8 *)(FB_VIRT_BASE + (phys & 0xFFFull));
    fb_base = fb_front;

    /* Log the reserved span so a driver mapping MMIO on top of the
     * framebuffer is obvious in the boot log instead of showing up as a
     * stale band on screen. */
    serial_write("fb: virt window ");
    serial_print_hex((u32)(FB_VIRT_BASE >> 32));
    serial_print_hex((u32)FB_VIRT_BASE);
    serial_write(" .. ");
    serial_print_hex((u32)((FB_VIRT_BASE + pages * PAGE_SIZE) >> 32));
    serial_print_hex((u32)(FB_VIRT_BASE + pages * PAGE_SIZE));
    serial_write("\n");
    return true;
}

bool fb_available(void)
{
    return fb_ready;
}

u32 fb_width(void)
{
    return fb_w;
}

u32 fb_height(void)
{
    return fb_h;
}

u32 fb_pitch(void)
{
    return fb_pitch_bytes;
}

u8 fb_bpp(void)
{
    return fb_bits;
}

u8 *fb_buffer(void)
{
    return fb_ready ? fb_front : NULL;
}

bool fb_set_pixel(u32 x, u32 y, u8 r, u8 g, u8 b)
{
    if (!fb_ready || x >= fb_w || y >= fb_h) {
        return false;
    }
    fb_put_pixel_raw(x, y, r, g, b);
    return true;
}

bool fb_get_pixel(u32 x, u32 y, u8 *r, u8 *g, u8 *b)
{
    u8 *pixel;
    u32 offset;

    if (!fb_ready || fb_base == NULL || x >= fb_w || y >= fb_h ||
        r == NULL || g == NULL || b == NULL) {
        return false;
    }

    offset = fb_vis_off(x, y);
    pixel = fb_base + offset;

    if (fb_bits == 32 || fb_bits == 24) {
        *b = pixel[0];
        *g = pixel[1];
        *r = pixel[2];
        return true;
    }
    if (fb_bits == 16) {
        u16 c = (u16)pixel[0] | ((u16)pixel[1] << 8);
        *r = (u8)((c >> 11) << 3);
        *g = (u8)(((c >> 5) & 0x3Fu) << 2);
        *b = (u8)((c & 0x1Fu) << 3);
        return true;
    }
    return false;
}

static u8 gamma_mix(u8 src, u8 dst, u8 a)
{
    u32 inv = 255u - (u32)a;
    u32 lin = ((u32)gamma_srgb_to_lin[src] * (u32)a +
               (u32)gamma_srgb_to_lin[dst] * inv) / 255u;
    if (lin > 4095u) {
        lin = 4095u;
    }
    return gamma_lin_to_srgb[lin];
}

void fb_blend_pixel(u32 x, u32 y, u8 r, u8 g, u8 b, u8 a)
{
    if (a == 0) {
        return;
    }
    if (a == 255u) {
        fb_set_pixel(x, y, r, g, b);
        return;
    }
    if (!fb_ready || x >= fb_w || y >= fb_h || fb_base == NULL) {
        return;
    }

    if (fb_bits == 32) {
        u32 *p = (u32 *)(fb_base + fb_vis_off(x, y));
        u32 dst = *p;
        u8 nr = gamma_mix(r, (u8)((dst >> 16) & 0xFFu), a);
        u8 ng = gamma_mix(g, (u8)((dst >> 8) & 0xFFu), a);
        u8 nb = gamma_mix(b, (u8)(dst & 0xFFu), a);
        *p = fb_pack32(nr, ng, nb);
        return;
    }

    {
        u8 dr;
        u8 dg;
        u8 db;
        if (!fb_get_pixel(x, y, &dr, &dg, &db)) {
            return;
        }
        fb_set_pixel(x, y, gamma_mix(r, dr, a), gamma_mix(g, dg, a),
                     gamma_mix(b, db, a));
    }
}

void fb_fill_rect(u32 x, u32 y, u32 w, u32 h, u8 r, u8 g, u8 b)
{
    u32 yy;
    u32 xx;
    u32 x2;
    u32 y2;

    if (!fb_ready || w == 0 || h == 0) {
        return;
    }

    x2 = x + w;
    y2 = y + h;
    if (x >= fb_w || y >= fb_h) {
        return;
    }
    if (x2 > fb_w) {
        x2 = fb_w;
    }
    if (y2 > fb_h) {
        y2 = fb_h;
    }

    if (fb_bits == 32) {
        u32 color = fb_pack32(r, g, b);
        for (yy = y; yy < y2; ++yy) {
            u32 *row = fb_vis32(fb_base, yy);
            for (xx = x; xx < x2; ++xx) {
                row[xx] = color;
            }
            if (x == 0 || x2 >= fb_w) {
                fb_halo_row(fb_base, yy);
            }
        }
        return;
    }

    for (yy = y; yy < y2; ++yy) {
        for (xx = x; xx < x2; ++xx) {
            fb_put_pixel_raw(xx, yy, r, g, b);
        }
    }
}

void fb_clear(u8 r, u8 g, u8 b)
{
    fb_fill_rect(0, 0, fb_w, fb_h, r, g, b);
}

void fb_overlay(u8 r, u8 g, u8 b, u8 alpha)
{
    u32 y;
    u32 x;

    if (!fb_ready || fb_base == NULL || alpha == 0) {
        return;
    }
    ov_r = r;
    ov_g = g;
    ov_b = b;
    ov_a = alpha;
    if (alpha == 255u) {
        fb_fill_rect(0, 0, fb_w, fb_h, r, g, b);
        return;
    }

    if (fb_bits == 32) {
        for (y = 0; y < fb_h; ++y) {
            u32 *row = fb_vis32(fb_base, y);
            for (x = 0; x < fb_w; ++x) {
                u32 dst = row[x];
                u8 nr = gamma_mix(r, (u8)((dst >> 16) & 0xFFu), alpha);
                u8 ng = gamma_mix(g, (u8)((dst >> 8) & 0xFFu), alpha);
                u8 nb = gamma_mix(b, (u8)(dst & 0xFFu), alpha);
                row[x] = fb_pack32(nr, ng, nb);
            }
        }
        fb_seal_edges();
        return;
    }

    for (y = 0; y < fb_h; ++y) {
        for (x = 0; x < fb_w; ++x) {
            fb_blend_pixel(x, y, r, g, b, alpha);
        }
    }
}

void fb_hline(u32 x, u32 y, u32 w, u8 r, u8 g, u8 b)
{
    fb_fill_rect(x, y, w, 1, r, g, b);
}

void fb_vline(u32 x, u32 y, u32 h, u8 r, u8 g, u8 b)
{
    fb_fill_rect(x, y, 1, h, r, g, b);
}

void fb_rect(u32 x, u32 y, u32 w, u32 h, u8 r, u8 g, u8 b)
{
    if (w == 0 || h == 0) {
        return;
    }
    fb_hline(x, y, w, r, g, b);
    fb_hline(x, y + h - 1u, w, r, g, b);
    fb_vline(x, y, h, r, g, b);
    fb_vline(x + w - 1u, y, h, r, g, b);
}

static void bga_write(u16 index, u16 value)
{
    outw(VBE_DISPI_IOPORT_INDEX, index);
    outw(VBE_DISPI_IOPORT_DATA, value);
}

static u16 bga_read(u16 index)
{
    outw(VBE_DISPI_IOPORT_INDEX, index);
    return inw(VBE_DISPI_IOPORT_DATA);
}

static bool bga_available(void)
{
    u16 id;

    /* Handshake: some hosts only report a valid ID after writing B0C4/B0C5. */
    bga_write(VBE_DISPI_INDEX_ID, VBE_DISPI_ID5);
    id = bga_read(VBE_DISPI_INDEX_ID);
    if (id >= VBE_DISPI_ID0 && id <= VBE_DISPI_ID5) {
        return true;
    }
    bga_write(VBE_DISPI_INDEX_ID, 0xB0C4u);
    id = bga_read(VBE_DISPI_INDEX_ID);
    return id >= VBE_DISPI_ID0 && id <= VBE_DISPI_ID5;
}

/* Prefer modes that VirtualBox/QEMU actually honor via Bochs VBE. */
static bool bga_try_mode(u16 width, u16 height, u16 bpp, bool clamp_cols)
{
    u16 got_w;
    u16 got_h;
    u16 got_bpp;
    u16 virt_width;
    u16 x_offset;

    if (!bga_available()) {
        return false;
    }

    bga_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
    bga_write(VBE_DISPI_INDEX_XRES, width);
    bga_write(VBE_DISPI_INDEX_YRES, height);
    bga_write(VBE_DISPI_INDEX_BPP, bpp);
    if (clamp_cols) {
        virt_width = (u16)(width + 2u);
        x_offset = 1;
    } else {
        virt_width = width;
        x_offset = 0;
    }
    /* Single virtual page, scanned from Y_OFFSET=0 only. A double-height
     * surface + Y_OFFSET page-flip looked tear-free in a settled snapshot,
     * but VBoxVGA renders the panned (offset) page with a 1px full-width
     * seam on the host. Because a full re-present flips pages, that seam
     * blinked in lockstep with the text caret (every caret toggle = one
     * flip). Staying on page 0 removes the seam entirely. */
    bga_write(VBE_DISPI_INDEX_VIRT_WIDTH, virt_width);
    bga_write(VBE_DISPI_INDEX_VIRT_HEIGHT, height);
    bga_write(VBE_DISPI_INDEX_X_OFFSET, x_offset);
    bga_write(VBE_DISPI_INDEX_Y_OFFSET, 0);
    bga_write(VBE_DISPI_INDEX_ENABLE,
              (u16)(VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED | VBE_DISPI_8BIT_DAC));

    got_w = bga_read(VBE_DISPI_INDEX_XRES);
    got_h = bga_read(VBE_DISPI_INDEX_YRES);
    got_bpp = bga_read(VBE_DISPI_INDEX_BPP);
    if (got_w != width || got_h != height || got_bpp != bpp) {
        return false;
    }

    fb_w = width;
    fb_h = height;
    fb_bits = (u8)bpp;
    if (clamp_cols) {
        fb_x0 = 1;
        fb_pitch_bytes = (u32)(width + 2u) * (bpp / 8u);
    } else {
        fb_x0 = 0;
        fb_pitch_bytes = (u32)width * (bpp / 8u);
    }
    fb_page_bytes = fb_pitch_bytes * (u32)height;
    fb_vis_page = 0;

    /* No hardware page-flip: it caused a host-side seam on the panned page
     * (see comment above). Present is a single clean copy onto page 0. */
    fb_flip_ok = false;
    (void)x_offset;
    (void)virt_width;
    return true;
}

static u32 parse_u32(const char *s, const char **end)
{
    u32 v = 0;
    while (*s >= '0' && *s <= '9') {
        v = v * 10u + (u32)(*s - '0');
        ++s;
    }
    if (end != NULL) {
        *end = s;
    }
    return v;
}

void fb_parse_cmdline(const void *mb2_addr)
{
    const struct multiboot2_tag *tag;
    const struct multiboot2_tag_string *str_tag;
    const char *p;

    fb_want_w = 0;
    fb_want_h = 0;
    if (mb2_addr == NULL) {
        return;
    }
    tag = multiboot2_find_tag(mb2_addr, MULTIBOOT2_TAG_TYPE_CMDLINE);
    if (tag == NULL) {
        return;
    }
    str_tag = (const struct multiboot2_tag_string *)tag;
    p = str_tag->string;
    while (*p != '\0') {
        if ((p[0] == 'g' || p[0] == 'G') &&
            (p[1] == 'f' || p[1] == 'F') &&
            (p[2] == 'x' || p[2] == 'X') &&
            p[3] == '=') {
            const char *end;
            u32 w;
            u32 h;
            w = parse_u32(p + 4, &end);
            if (*end == 'x' || *end == 'X') {
                h = parse_u32(end + 1, NULL);
                if (w >= 640u && h >= 400u && w <= 7680u && h <= 4320u) {
                    fb_want_w = w;
                    fb_want_h = h;
                    serial_write("fb: cmdline gfx=");
                    serial_print_u32(w);
                    serial_write("x");
                    serial_print_u32(h);
                    serial_write("\n");
                }
            }
            return;
        }
        ++p;
    }
}

static bool fb_upgrade_mode(void)
{
    if (!bga_available()) {
        serial_write("fb: Bochs VBE not present; keeping Multiboot mode\n");
        return false;
    }

    /* Match the host/GRUB framebuffer when Multiboot handed a different size. */
    if (fb_want_w != 0 && fb_want_h != 0 &&
        (fb_want_w != fb_w || fb_want_h != fb_h)) {
        serial_write("fb: setting native mode via Bochs VBE\n");
        if (bga_try_mode((u16)fb_want_w, (u16)fb_want_h, 32, false)) {
            serial_write("fb: Bochs VBE set ");
            serial_print_u32(fb_w);
            serial_write("x");
            serial_print_u32(fb_h);
            serial_write("\n");
            return true;
        }
        serial_write("fb: native Bochs mode failed; keeping Multiboot\n");
    }

    /*
     * Multiboot often already hands us 1920x1080 with a tight pitch.
     * That path used to return here, so the 1px clamp columns never
     * installed — host filtering then shows wallpaper hairlines at the
     * bottom left/right. Re-set the same mode to get the padded pitch.
     */
    if (fb_w >= 1920u && fb_h >= 1080u) {
        if (fb_x0 > 0) {
            return true;
        }
        if (bga_try_mode((u16)fb_w, (u16)fb_h, 32, true)) {
            serial_write("fb: Bochs VBE clamp columns on ");
            serial_print_u32(fb_w);
            serial_write("x");
            serial_print_u32(fb_h);
            serial_write("\n");
            return true;
        }
        return false;
    }

    return false;
}

void fb_init(const void *mb2_addr)
{
    const struct multiboot2_tag *tag;
    const struct multiboot2_tag_framebuffer *fb;
    u64 bytes;

    fb_ready = false;
    fb_front = NULL;
    fb_back = NULL;
    fb_base = NULL;
    fb_phys = 0;
    fb_w = 0;
    fb_h = 0;
    fb_pitch_bytes = 0;
    fb_page_bytes = 0;
    fb_vis_page = 0;
    fb_flip_ok = false;
    fb_bits = 0;
    fb_x0 = 0;

    tag = multiboot2_find_tag(mb2_addr, MULTIBOOT2_TAG_TYPE_FRAMEBUFFER);
    if (tag == NULL) {
        serial_write("fb: no Multiboot2 framebuffer tag\n");
        return;
    }

    fb = (const struct multiboot2_tag_framebuffer *)tag;

    if (fb->fb_type == MULTIBOOT2_FRAMEBUFFER_TYPE_EGA_TEXT) {
        serial_write("fb: EGA text — staying on VGA text UI\n");
        return;
    }

    if (fb->fb_type != MULTIBOOT2_FRAMEBUFFER_TYPE_RGB) {
        serial_write("fb: unsupported type\n");
        return;
    }

    if (fb->bpp != 16 && fb->bpp != 24 && fb->bpp != 32) {
        serial_write("fb: unsupported bpp\n");
        return;
    }

    if (fb->addr == 0 || fb->width == 0 || fb->height == 0 || fb->pitch == 0) {
        serial_write("fb: invalid geometry\n");
        return;
    }

    fb_phys = fb->addr;
    fb_w = fb->width;
    fb_h = fb->height;
    fb_pitch_bytes = fb->pitch;
    fb_bits = fb->bpp;
    {
        u32 pitch_px = fb_pitch_bytes / fb_pxb();

        if (pitch_px > fb_w) {
            fb_x0 = (pitch_px - fb_w) / 2u;
            if (fb_x0 == 0) {
                fb_x0 = 1;
            }
        }
    }

    serial_write("fb: Multiboot ");
    serial_print_u32(fb_w);
    serial_write("x");
    serial_print_u32(fb_h);
    serial_write("x");
    serial_print_u32(fb_bits);
    serial_write("\n");

    (void)fb_upgrade_mode();

    /* Map both stacked pages when hardware double buffering is on so the
     * hidden page is real memory, plus slack for alignment. */
    bytes = (u64)fb_pitch_bytes * (u64)fb_h;
    if (fb_flip_ok) {
        bytes *= 2ull;
    }
    bytes += 0x200000ull;
    if (!fb_map_range(fb_phys, bytes)) {
        serial_write("fb: map failed\n");
        return;
    }

    fb_ready = true;
    fb_compose_init();
    serial_write("fb: pitch ");
    serial_print_u32(fb_pitch_bytes);
    serial_write(" (row ");
    serial_print_u32(fb_pitch_bytes / (fb_bits >= 8u ? (fb_bits / 8u) : 1u));
    serial_write("px) ready ");
    serial_print_u32(fb_w);
    serial_write("x");
    serial_print_u32(fb_h);
    serial_write("x");
    serial_print_u32(fb_bits);
    serial_write("\n");
    serial_write(fb_flip_ok ? "fb: hw page-flip ENABLED\n"
                            : "fb: hw page-flip OFF (single buffer)\n");
}

static u32 fb_pixel_bytes(void)
{
    return fb_bits / 8u;
}

static u64 fb_layer_bytes(void)
{
    return (u64)fb_pitch_bytes * (u64)fb_h;
}

void fb_compose_init(void)
{
    u64 bytes;
    u32 pages;
    u64 phys;

    fb_back = NULL;
    if (!fb_ready || fb_front == NULL || fb_pitch_bytes == 0 || fb_h == 0) {
        return;
    }

    bytes = fb_layer_bytes();
    pages = (u32)((bytes + PAGE_SIZE - 1u) / PAGE_SIZE);
    phys = pmm_alloc_contiguous(pages);
    if (phys == 0) {
        serial_write("fb: compose alloc failed\n");
        return;
    }
    fb_back = (u8 *)phys;
}

bool fb_compose_ready(void)
{
    return fb_ready && fb_back != NULL && fb_front != NULL;
}

void fb_shade_as_overlay(u8 *r, u8 *g, u8 *b)
{
    if (r == NULL || g == NULL || b == NULL || ov_a == 0) {
        return;
    }
    if (ov_a == 255u) {
        *r = ov_r;
        *g = ov_g;
        *b = ov_b;
        return;
    }
    *r = gamma_mix(ov_r, *r, ov_a);
    *g = gamma_mix(ov_g, *g, ov_a);
    *b = gamma_mix(ov_b, *b, ov_a);
}

static u8 *fb_visible_page(void)
{
    if (fb_flip_ok && fb_page_bytes != 0) {
        return fb_front + (u64)fb_vis_page * (u64)fb_page_bytes;
    }
    return fb_front;
}

void fb_compose_begin(void)
{
    /* Full background blits reset the overlay; partial redraws must retain it. */
    if (fb_back != NULL) {
        fb_base = fb_back;
        fb_scene_dirty = true;
    }
}

void fb_compose_end(void)
{
    if (fb_front != NULL) {
        fb_base = fb_visible_page();
    }
}

static u8 *fb_hidden_page(void)
{
    if (fb_flip_ok && fb_page_bytes != 0) {
        return fb_front + (u64)(1u - fb_vis_page) * (u64)fb_page_bytes;
    }
    return fb_front;
}

static void fb_flip_hidden(void)
{
    u32 next;
    u16 want;

    if (!fb_flip_ok) {
        return;
    }
    next = 1u - fb_vis_page;
    want = (u16)(next * fb_h);
    bga_write(VBE_DISPI_INDEX_Y_OFFSET, want);
    if (bga_read(VBE_DISPI_INDEX_Y_OFFSET) != want) {
        /* Do not memcpy onto the live page — that is the 1px tear. */
        fb_flip_ok = false;
        return;
    }
    fb_vis_page = next;
}

static void fb_store_layer(u8 *dst, const u8 *src)
{
    size_t bytes = (size_t)fb_layer_bytes();
    u32 y;
    u32 x;

    /* Bottom-up so a mid-copy host snapshot is not stuck at y=h-128. */
    if (fb_bits == 32) {
        for (y = fb_h; y > 0; ) {
            const u32 *s;
            volatile u32 *d;

            --y;
            s = fb_vis32((u8 *)src, y);
            d = (volatile u32 *)fb_vis32(dst, y);
            for (x = 0; x < fb_w; ++x) {
                d[x] = s[x];
            }
        }
        return;
    }
    memcpy(dst, src, bytes);
}

static void fb_fill_page(u8 *page)
{
    fb_store_layer(page, fb_back);
}

/* Seal edges/clamp columns of an arbitrary page (not necessarily fb_base). */
static void fb_seal_page(u8 *page)
{
    u32 y;

    if (fb_bits != 32 || page == NULL) {
        return;
    }
    if (fb_x0 > 0) {
        for (y = 0; y < fb_h; ++y) {
            u32 *raw = (u32 *)(page + y * fb_pitch_bytes);
            raw[0] = raw[fb_x0];
            raw[fb_x0 + fb_w] = raw[fb_x0 + fb_w - 1u];
        }
    } else {
        fb_halo_all(page);
    }
}

static void fb_memcpy_visible(void)
{
    /* One clean top-of-frame copy of the finished scene onto the live page,
     * then seal the edges. No second "repair" pass: a separate band re-copy
     * is exactly what let a host snapshot catch the buffer mid-write and
     * show a 1px seam. Consecutive frames differ only by the caret/cursor,
     * so a single-buffer copy has no visible tear. */
    fb_fill_page(fb_front);
    fb_seal_page(fb_front);
}

void fb_compose_present(void)
{
    if (!fb_compose_ready()) {
        fb_compose_end();
        return;
    }
    if (fb_scene_dirty) {
        fb_memcpy_visible();
        fb_scene_dirty = false;
    }
    fb_base = fb_visible_page();
}

void fb_present_dimmed(u8 black_alpha)
{
    u32 y;
    u32 x;

    if (!fb_compose_ready()) {
        return;
    }
    if (black_alpha == 0) {
        fb_compose_present();
        return;
    }
    if (fb_bits == 32) {
        u8 *page = fb_hidden_page();

        for (y = 0; y < fb_h; ++y) {
            const u32 *src = fb_vis32(fb_back, y);
            u32 *dst = fb_vis32(page, y);
            for (x = 0; x < fb_w; ++x) {
                u32 p = src[x];
                u8 r = gamma_mix(0, (u8)((p >> 16) & 0xFFu), black_alpha);
                u8 g = gamma_mix(0, (u8)((p >> 8) & 0xFFu), black_alpha);
                u8 b = gamma_mix(0, (u8)(p & 0xFFu), black_alpha);
                dst[x] = fb_pack32(r, g, b);
            }
            fb_halo_row(page, y);
        }
        fb_flip_hidden();
        fb_scene_dirty = false;
        fb_base = fb_visible_page();
        return;
    }
    memcpy(fb_hidden_page(), fb_back, (size_t)fb_layer_bytes());
    fb_flip_hidden();
    fb_scene_dirty = false;
    fb_base = fb_visible_page();
    fb_overlay(0, 0, 0, black_alpha);
}

void fb_compose_present_rect(u32 x, u32 y, u32 w, u32 h)
{
    u32 yy;
    u32 x2;
    u32 y2;
    u32 copy;
    u32 bpp;

    if (!fb_compose_ready() || w == 0 || h == 0) {
        fb_compose_end();
        return;
    }

    bpp = fb_pixel_bytes();
    if (bpp == 0) {
        fb_compose_end();
        return;
    }

    x2 = x + w;
    y2 = y + h;
    if (x >= fb_w || y >= fb_h) {
        fb_compose_end();
        return;
    }
    if (x2 > fb_w) {
        x2 = fb_w;
    }
    if (y2 > fb_h) {
        y2 = fb_h;
    }
    {
        u32 hx = x + fb_x0;
        u32 hx2 = x2 + fb_x0;

        if (fb_x0 > 0 && x == 0) {
            hx = 0;
        }
        if (x2 >= fb_w) {
            hx2 = fb_pitch_bytes / bpp;
        }
        copy = (hx2 - hx) * bpp;
        {
            u8 *vis = fb_visible_page();

            for (yy = y; yy < y2; ++yy) {
                u32 off = yy * fb_pitch_bytes + hx * bpp;
                memcpy(vis + off, fb_back + off, copy);
            }
        }
    }
    fb_base = fb_visible_page();
}

void fb_compose_copy_rect(u32 x, u32 y, u32 w, u32 h)
{
    fb_compose_present_rect(x, y, w, h);
}

/* Staging area for fb_present_sprite_rect. Sized for the union of two cursor
 * boxes with headroom; callers must fall back if their rect is larger. */
#define FB_SPRITE_STAGE_W 96u
#define FB_SPRITE_STAGE_H 96u
static u32 fb_sprite_stage[FB_SPRITE_STAGE_W * FB_SPRITE_STAGE_H];

bool fb_present_sprite_rect(u32 rx, u32 ry, u32 rw, u32 rh,
                            const u8 *rgba, u32 sw, u32 sh, i32 sx, i32 sy)
{
    u32 row;
    u32 col;

    if (!fb_compose_ready() || fb_bits != 32 ||
        rw == 0 || rh == 0 ||
        rw > FB_SPRITE_STAGE_W || rh > FB_SPRITE_STAGE_H ||
        rx >= fb_w || ry >= fb_h || rw > fb_w - rx || rh > fb_h - ry) {
        return false;
    }

    /* Build the finished rect in RAM: the composed scene first, then the
     * sprite blended on top. Nothing reaches the framebuffer until every
     * pixel is final, so the host cannot sample a partially drawn sprite
     * or a gap where the sprite used to be. */
    for (row = 0; row < rh; ++row) {
        const u32 *scene = fb_vis32(fb_back, ry + row) + rx;
        u32 *out = fb_sprite_stage + (size_t)row * rw;
        i32 srow;

        for (col = 0; col < rw; ++col) {
            out[col] = scene[col];
        }

        if (rgba == NULL) {
            continue;
        }
        srow = (i32)(ry + row) - sy;
        if (srow < 0 || (u32)srow >= sh) {
            continue;
        }
        for (col = 0; col < rw; ++col) {
            i32 scol = (i32)(rx + col) - sx;
            u32 i;
            u8 a;

            if (scol < 0 || (u32)scol >= sw) {
                continue;
            }
            i = ((u32)srow * sw + (u32)scol) * 4u;
            a = rgba[i + 3u];
            if (a == 0) {
                continue;
            }
            if (a == 255u) {
                out[col] = fb_pack32(rgba[i], rgba[i + 1u], rgba[i + 2u]);
            } else {
                u32 dst = out[col];

                out[col] = fb_pack32(
                    gamma_mix(rgba[i], (u8)((dst >> 16) & 0xFFu), a),
                    gamma_mix(rgba[i + 1u], (u8)((dst >> 8) & 0xFFu), a),
                    gamma_mix(rgba[i + 2u], (u8)(dst & 0xFFu), a));
            }
        }
    }

    {
        u8 *vis = fb_visible_page();

        for (row = 0; row < rh; ++row) {
            const u32 *s = fb_sprite_stage + (size_t)row * rw;
            volatile u32 *d = (volatile u32 *)(fb_vis32(vis, ry + row) + rx);

            for (col = 0; col < rw; ++col) {
                d[col] = s[col];
            }
        }
    }

    fb_base = fb_visible_page();
    return true;
}

void fb_copy_front(u8 *dst)
{
    u8 *src;

    if (dst == NULL || !fb_ready || fb_front == NULL) {
        return;
    }
    src = fb_front;
    if (fb_flip_ok && fb_page_bytes != 0) {
        src = fb_front + (u64)fb_vis_page * (u64)fb_page_bytes;
    }
    memcpy(dst, src, (size_t)fb_layer_bytes());
}

void fb_blend_to_front(const u8 *from, u8 amount)
{
    u32 y;
    u32 x;
    u32 inv;

    if (!fb_compose_ready() || from == NULL) {
        return;
    }
    if (amount == 0) {
        memcpy(fb_hidden_page(), from, (size_t)fb_layer_bytes());
        fb_flip_hidden();
        fb_scene_dirty = false;
        fb_base = fb_visible_page();
        return;
    }
    if (amount == 255u) {
        memcpy(fb_hidden_page(), fb_back, (size_t)fb_layer_bytes());
        fb_flip_hidden();
        fb_scene_dirty = false;
        fb_base = fb_visible_page();
        return;
    }
    inv = 255u - (u32)amount;
    if (fb_bits == 32) {
        u8 *page = fb_hidden_page();

        for (y = 0; y < fb_h; ++y) {
            const u32 *a = fb_vis32((u8 *)from, y);
            const u32 *b = fb_vis32(fb_back, y);
            u32 *out = fb_vis32(page, y);
            for (x = 0; x < fb_w; ++x) {
                u32 pa = a[x];
                u32 pb = b[x];
                u8 r = (u8)((((pa >> 16) & 0xFFu) * inv +
                             ((pb >> 16) & 0xFFu) * (u32)amount) / 255u);
                u8 g = (u8)((((pa >> 8) & 0xFFu) * inv +
                             ((pb >> 8) & 0xFFu) * (u32)amount) / 255u);
                u8 bl = (u8)(((pa & 0xFFu) * inv +
                              (pb & 0xFFu) * (u32)amount) / 255u);
                out[x] = fb_pack32(r, g, bl);
            }
            fb_halo_row(page, y);
        }
        fb_flip_hidden();
        fb_scene_dirty = false;
        fb_base = fb_visible_page();
        return;
    }
    memcpy(fb_hidden_page(), fb_back, (size_t)fb_layer_bytes());
    fb_flip_hidden();
    fb_scene_dirty = false;
    fb_base = fb_visible_page();
}

u8 *fb_layer_alloc(void)
{
    u64 bytes;
    u32 pages;
    u64 phys;

    if (!fb_ready) {
        return NULL;
    }
    bytes = fb_layer_bytes();
    pages = (u32)((bytes + PAGE_SIZE - 1u) / PAGE_SIZE);
    phys = pmm_alloc_contiguous(pages);
    if (phys == 0) {
        return NULL;
    }
    return (u8 *)phys;
}

void fb_layer_capture(u8 *layer)
{
    if (layer == NULL || !fb_ready || fb_base == NULL) {
        return;
    }
    memcpy(layer, fb_base, (size_t)fb_layer_bytes());
}

void fb_layer_restore_rect(const u8 *layer, u32 x, u32 y, u32 w, u32 h)
{
    u32 yy;
    u32 x2;
    u32 y2;
    u32 copy;
    u32 bpp;

    if (layer == NULL || !fb_ready || fb_base == NULL || w == 0 || h == 0) {
        return;
    }

    bpp = fb_pixel_bytes();
    if (bpp == 0) {
        return;
    }

    x2 = x + w;
    y2 = y + h;
    if (x >= fb_w || y >= fb_h) {
        return;
    }
    if (x2 > fb_w) {
        x2 = fb_w;
    }
    if (y2 > fb_h) {
        y2 = fb_h;
    }
    {
        u32 hx = x + fb_x0;
        u32 hx2 = x2 + fb_x0;

        if (fb_x0 > 0 && x == 0) {
            hx = 0;
        }
        if (x2 >= fb_w) {
            hx2 = fb_pitch_bytes / bpp;
        }
        copy = (hx2 - hx) * bpp;
        for (yy = y; yy < y2; ++yy) {
            u32 off = yy * fb_pitch_bytes + hx * bpp;
            memcpy(fb_base + off, layer + off, copy);
        }
    }
}

static void fb_cover_params(u32 src_w, u32 src_h, u32 *num, u32 *den,
                            u32 *src_x0, u32 *src_y0)
{
    u32 scale_num;
    u32 scale_den;
    u32 cropped_w;
    u32 cropped_h;

    /* Cover = larger scale so the photo always fills the FB (no 1px bars). */
    if ((u64)fb_w * (u64)src_h >= (u64)fb_h * (u64)src_w) {
        scale_num = fb_w;
        scale_den = src_w;
    } else {
        scale_num = fb_h;
        scale_den = src_h;
    }
    cropped_w = (u32)(((u64)src_w * scale_num + (scale_den - 1u)) / scale_den);
    cropped_h = (u32)(((u64)src_h * scale_num + (scale_den - 1u)) / scale_den);
    *src_x0 = (cropped_w > fb_w)
                  ? (u32)(((u64)(cropped_w - fb_w) * scale_den) /
                          (2u * (u64)scale_num))
                  : 0;
    *src_y0 = (cropped_h > fb_h)
                  ? (u32)(((u64)(cropped_h - fb_h) * scale_den) /
                          (2u * (u64)scale_num))
                  : 0;
    *num = scale_num;
    *den = scale_den;
}

void fb_cover_src_xy(u32 src_w, u32 src_h, u32 dx, u32 dy, u32 *sx, u32 *sy)
{
    u32 scale_num;
    u32 scale_den;
    u32 src_x0;
    u32 src_y0;
    u32 x;
    u32 y;

    if (sx == NULL || sy == NULL || src_w == 0 || src_h == 0 ||
        fb_w == 0 || fb_h == 0) {
        if (sx != NULL) {
            *sx = 0;
        }
        if (sy != NULL) {
            *sy = 0;
        }
        return;
    }

    fb_cover_params(src_w, src_h, &scale_num, &scale_den, &src_x0, &src_y0);
    x = src_x0 + (dx * scale_den) / scale_num;
    y = src_y0 + (dy * scale_den) / scale_num;
    if (x >= src_w) {
        x = src_w - 1u;
    }
    if (y >= src_h) {
        y = src_h - 1u;
    }
    *sx = x;
    *sy = y;
}

static void fb_seal_edges(void)
{
    u32 y;

    if (fb_bits == 32 && fb_base != NULL && fb_x0 > 0) {
        for (y = 0; y < fb_h; ++y) {
            u32 *raw = (u32 *)(fb_base + y * fb_pitch_bytes);
            raw[0] = raw[fb_x0];
            raw[fb_x0 + fb_w] = raw[fb_x0 + fb_w - 1u];
        }
    } else if (fb_bits == 32 && fb_base != NULL && fb_x0 == 0) {
        fb_halo_all(fb_base);
    }
}

void fb_blit_rgb_cover(const u8 *rgb, u32 src_w, u32 src_h)
{
    u32 y;
    u32 x;
    u32 scale_num;
    u32 scale_den;
    u32 src_x0;
    u32 src_y0;

    if (!fb_ready || rgb == NULL || src_w == 0 || src_h == 0) {
        return;
    }
    ov_a = 0;

    /* Crisp 1:1 when wallpaper matches the active framebuffer. */
    if (fb_bits == 32 && src_w == fb_w && src_h == fb_h) {
        for (y = 0; y < fb_h; ++y) {
            u32 *row = fb_vis32(fb_base, y);
            const u8 *src_row = rgb + y * src_w * 3u;
            for (x = 0; x < fb_w; ++x) {
                const u8 *src = src_row + x * 3u;
                row[x] = fb_pack32(src[0], src[1], src[2]);
            }
        }
        fb_seal_edges();
        return;
    }

    fb_cover_params(src_w, src_h, &scale_num, &scale_den, &src_x0, &src_y0);

    if (fb_bits == 32) {
        for (y = 0; y < fb_h; ++y) {
            u32 *row = fb_vis32(fb_base, y);
            u32 sy = src_y0 + (y * scale_den) / scale_num;
            if (sy >= src_h) {
                sy = src_h - 1u;
            }
            for (x = 0; x < fb_w; ++x) {
                u32 sx = src_x0 + (x * scale_den) / scale_num;
                const u8 *p;
                if (sx >= src_w) {
                    sx = src_w - 1u;
                }
                p = rgb + (sy * src_w + sx) * 3u;
                row[x] = fb_pack32(p[0], p[1], p[2]);
            }
        }
        fb_seal_edges();
        return;
    }

    for (y = 0; y < fb_h; ++y) {
        for (x = 0; x < fb_w; ++x) {
            u32 sx;
            u32 sy;
            const u8 *p;
            fb_cover_src_xy(src_w, src_h, x, y, &sx, &sy);
            p = rgb + (sy * src_w + sx) * 3u;
            fb_put_pixel_raw(x, y, p[0], p[1], p[2]);
        }
    }
    fb_seal_edges();
}

void fb_blit_rgba(const u8 *rgba, u32 src_w, u32 src_h, i32 dx, i32 dy)
{
    u32 row;
    u32 col;

    if (!fb_ready || rgba == NULL || src_w == 0 || src_h == 0) {
        return;
    }
    for (row = 0; row < src_h; ++row) {
        i32 py = dy + (i32)row;
        if (py < 0) {
            continue;
        }
        if ((u32)py >= fb_h) {
            break;
        }
        for (col = 0; col < src_w; ++col) {
            i32 px = dx + (i32)col;
            const u8 *p;
            u8 a;
            if (px < 0 || (u32)px >= fb_w) {
                continue;
            }
            p = rgba + (row * src_w + col) * 4u;
            a = p[3];
            if (a == 0) {
                continue;
            }
            if (a == 255u) {
                fb_set_pixel((u32)px, (u32)py, p[0], p[1], p[2]);
            } else {
                fb_blend_pixel((u32)px, (u32)py, p[0], p[1], p[2], a);
            }
        }
    }
}

void fb_blit_rgba_scaled(const u8 *rgba, u32 src_w, u32 src_h, i32 dx, i32 dy,
                         u32 dst_w, u32 dst_h)
{
    u32 row;
    u32 col;

    if (dst_w == src_w && dst_h == src_h) {
        fb_blit_rgba(rgba, src_w, src_h, dx, dy);
        return;
    }
    if (!fb_ready || rgba == NULL || src_w == 0 || src_h == 0 ||
        dst_w == 0 || dst_h == 0) {
        return;
    }

    for (row = 0; row < dst_h; ++row) {
        i32 py = dy + (i32)row;
        u32 fy;
        u32 y0;
        u32 y1;
        u32 ty;
        if (py < 0) {
            continue;
        }
        if ((u32)py >= fb_h) {
            break;
        }
        fy = (dst_h == 1u) ? 0 : (row * (src_h - 1u) * 256u) / (dst_h - 1u);
        y0 = fy >> 8;
        y1 = (y0 + 1u < src_h) ? (y0 + 1u) : y0;
        ty = fy & 255u;
        for (col = 0; col < dst_w; ++col) {
            i32 px = dx + (i32)col;
            u32 fx;
            u32 x0;
            u32 x1;
            u32 tx;
            const u8 *p00;
            const u8 *p10;
            const u8 *p01;
            const u8 *p11;
            u8 ch[4];
            u32 k;
            if (px < 0 || (u32)px >= fb_w) {
                continue;
            }
            fx = (dst_w == 1u) ? 0 : (col * (src_w - 1u) * 256u) / (dst_w - 1u);
            x0 = fx >> 8;
            x1 = (x0 + 1u < src_w) ? (x0 + 1u) : x0;
            tx = fx & 255u;
            p00 = rgba + (y0 * src_w + x0) * 4u;
            p10 = rgba + (y0 * src_w + x1) * 4u;
            p01 = rgba + (y1 * src_w + x0) * 4u;
            p11 = rgba + (y1 * src_w + x1) * 4u;
            for (k = 0; k < 4u; ++k) {
                u32 a = ((u32)p00[k] * (256u - tx) + (u32)p10[k] * tx) >> 8;
                u32 b = ((u32)p01[k] * (256u - tx) + (u32)p11[k] * tx) >> 8;
                ch[k] = (u8)((a * (256u - ty) + b * ty) >> 8);
            }
            if (ch[3] == 0) {
                continue;
            }
            if (ch[3] == 255u) {
                fb_set_pixel((u32)px, (u32)py, ch[0], ch[1], ch[2]);
            } else {
                fb_blend_pixel((u32)px, (u32)py, ch[0], ch[1], ch[2], ch[3]);
            }
        }
    }
}

void fb_blit_rgba_scaled_a(const u8 *rgba, u32 src_w, u32 src_h, i32 dx, i32 dy,
                           u32 dst_w, u32 dst_h, u8 alpha)
{
    u32 row;
    u32 col;

    if (alpha == 0) {
        return;
    }
    if (alpha == 255u) {
        fb_blit_rgba_scaled(rgba, src_w, src_h, dx, dy, dst_w, dst_h);
        return;
    }
    if (!fb_ready || rgba == NULL || src_w == 0 || src_h == 0 ||
        dst_w == 0 || dst_h == 0) {
        return;
    }

    for (row = 0; row < dst_h; ++row) {
        i32 py = dy + (i32)row;
        u32 fy;
        u32 y0;
        u32 y1;
        u32 ty;
        if (py < 0) {
            continue;
        }
        if ((u32)py >= fb_h) {
            break;
        }
        fy = (dst_h == 1u) ? 0 : (row * (src_h - 1u) * 256u) / (dst_h - 1u);
        y0 = fy >> 8;
        y1 = (y0 + 1u < src_h) ? (y0 + 1u) : y0;
        ty = fy & 255u;
        for (col = 0; col < dst_w; ++col) {
            i32 px = dx + (i32)col;
            u32 fx;
            u32 x0;
            u32 x1;
            u32 tx;
            const u8 *p00;
            const u8 *p10;
            const u8 *p01;
            const u8 *p11;
            u8 ch[4];
            u32 k;
            u8 a;
            if (px < 0 || (u32)px >= fb_w) {
                continue;
            }
            fx = (dst_w == 1u) ? 0 : (col * (src_w - 1u) * 256u) / (dst_w - 1u);
            x0 = fx >> 8;
            x1 = (x0 + 1u < src_w) ? (x0 + 1u) : x0;
            tx = fx & 255u;
            p00 = rgba + (y0 * src_w + x0) * 4u;
            p10 = rgba + (y0 * src_w + x1) * 4u;
            p01 = rgba + (y1 * src_w + x0) * 4u;
            p11 = rgba + (y1 * src_w + x1) * 4u;
            for (k = 0; k < 4u; ++k) {
                u32 lo = ((u32)p00[k] * (256u - tx) + (u32)p10[k] * tx) >> 8;
                u32 hi = ((u32)p01[k] * (256u - tx) + (u32)p11[k] * tx) >> 8;
                ch[k] = (u8)((lo * (256u - ty) + hi * ty) >> 8);
            }
            a = (u8)(((u32)ch[3] * (u32)alpha) / 255u);
            if (a != 0) {
                fb_blend_pixel((u32)px, (u32)py, ch[0], ch[1], ch[2], a);
            }
        }
    }
}
