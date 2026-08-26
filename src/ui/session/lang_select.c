#include "lang_select.h"
#include "config.h"
#include "draw.h"
#include "fb.h"
#include "metrics.h"
#include "i18n.h"
#include "keyboard.h"
#include "keycodes.h"
#include "mouse.h"
#include "multiboot2.h"
#include "theme.h"
#include "vga.h"

static void draw_picker_text(u32 focus)
{
    u32 i;
    u32 n = i18n_lang_count();

    vga_clear();
    vga_print(i18n(MSG_LANG_TITLE));
    vga_print("\n\n");
    vga_print(i18n(MSG_LANG_SUBTITLE));
    vga_print("\n\n");

    for (i = 0; i < n; ++i) {
        vga_print(i == focus ? "> " : "  ");
        vga_print(i18n_lang_name((enum lang_id)i));
        vga_print("\n");
    }

    vga_print("\n");
    vga_print(i18n(MSG_LANG_HINT));
    vga_print("\n");
}

static void u32_to_dec(u32 value, char *out, u32 out_len)
{
    char dig[10];
    u32 d = 0;
    u32 n = 0;
    u32 v = value;

    if (out_len == 0) {
        return;
    }
    if (v == 0) {
        dig[d++] = '0';
    } else {
        while (v > 0 && d < sizeof(dig)) {
            dig[d++] = (char)('0' + (v % 10u));
            v /= 10u;
        }
    }
    while (d > 0 && n + 1u < out_len) {
        out[n++] = dig[--d];
    }
    out[n] = '\0';
}

struct lang_geom {
    u32 px;
    u32 py;
    u32 panel_w;
    u32 panel_h;
    u32 button_y;
    u32 button_h;
};

static void lang_layout(struct lang_geom *g)
{
    u32 w = fb_width();
    u32 h = fb_height();

    g->panel_w = ui_content_w() + ui_px(80u);
    g->panel_h = ui_px(360u);
    g->button_y = ui_px(88u);
    g->button_h = ui_px(56u);
    {
        u32 n = i18n_lang_count();
        u32 list = n * (g->button_h + 16u);

        g->panel_h = ui_px(200u) + list;
    }

    if (g->panel_w + 40u > w) {
        g->panel_w = w > 40u ? w - 40u : w;
    }
    if (g->panel_h + 40u > h) {
        g->panel_h = h > 40u ? h - 40u : h;
    }
    g->px = (w > g->panel_w) ? (w - g->panel_w) / 2u : 20u;
    g->py = (h > g->panel_h) ? (h - g->panel_h) / 2u : 40u;
}

static void draw_picker_gfx(u32 focus)
{
    struct lang_geom g;
    u32 w = fb_width();
    u32 h = fb_height();
    u32 brand_w;
    u32 title_scale = 2u;
    char res[48];
    char a[12];
    char b[12];
    u32 i;
    u32 btn_w;

    lang_layout(&g);
    btn_w = g.panel_w > 80u ? g.panel_w - 80u : g.panel_w;

    fb_compose_begin();
    draw_bg_frosted();
    brand_w = draw_text_width(name_os, title_scale);
    draw_text((w > brand_w) ? (w - brand_w) / 2u : 20,
              g.py > 72u ? g.py - 72u : 16,
              name_os, THEME_INK, title_scale);

    draw_panel(g.px, g.py, g.panel_w, g.panel_h, false);
    draw_text(g.px + 36, g.py + 32, i18n(MSG_LANG_SUBTITLE), THEME_FG_DIM, 1);

    {
        u32 li;
        u32 n = i18n_lang_count();

        for (li = 0; li < n; ++li) {
            draw_button(g.px + 40,
                        g.py + g.button_y + li * (g.button_h + 16u),
                        btn_w, g.button_h,
                        i18n_lang_name((enum lang_id)li),
                        focus == li);
        }
    }

    draw_text(g.px + 36, g.py + g.panel_h - 72,
              i18n(MSG_LANG_HINT), THEME_FG_DIM, 1);

    u32_to_dec(w, a, sizeof(a));
    u32_to_dec(h, b, sizeof(b));
    i = 0;
    res[i++] = '(';
    {
        const char *p = a;
        while (*p && i + 1u < sizeof(res)) {
            res[i++] = *p++;
        }
    }
    if (i + 1u < sizeof(res)) {
        res[i++] = 'x';
    }
    {
        const char *p = b;
        while (*p && i + 1u < sizeof(res)) {
            res[i++] = *p++;
        }
    }
    if (i + 1u < sizeof(res)) {
        res[i++] = ')';
    }
    res[i] = '\0';
    draw_text(g.px + 36, g.py + g.panel_h - 40, res, THEME_ACCENT, 1);

    cursor_flip((u32)mouse_x(), (u32)mouse_y());
}

static void draw_picker(u32 focus)
{
    if (fb_available()) {
        draw_picker_gfx(focus);
    } else {
        draw_picker_text(focus);
    }
}

bool lang_try_cmdline(void *mb2_addr)
{
    const struct multiboot2_tag *tag;
    const struct multiboot2_tag_string *str_tag;
    const char *p;

    if (mb2_addr == NULL) {
        return false;
    }

    tag = multiboot2_find_tag(mb2_addr, MULTIBOOT2_TAG_TYPE_CMDLINE);
    if (tag == NULL || tag->size < sizeof(struct multiboot2_tag) + 1) {
        return false;
    }

    str_tag = (const struct multiboot2_tag_string *)tag;
    p = str_tag->string;

    while (*p != '\0') {
        if ((p[0] == 'l' || p[0] == 'L') &&
            (p[1] == 'a' || p[1] == 'A') &&
            (p[2] == 'n' || p[2] == 'N') &&
            (p[3] == 'g' || p[3] == 'G') &&
            p[4] == '=') {
            return i18n_set_lang_code(p + 5);
        }
        ++p;
    }

    return false;
}

static u32 lang_hit(i32 px, i32 py)
{
    struct lang_geom g;
    u32 i;
    u32 n = i18n_lang_count();

    lang_layout(&g);

    for (i = 0; i < n; ++i) {
        u32 y = g.py + g.button_y + i * (g.button_h + 16u);

        if (px >= (i32)(g.px + 40) && px < (i32)(g.px + g.panel_w - 40u) &&
            py >= (i32)y && py < (i32)(y + g.button_h)) {
            return i;
        }
    }
    return 0xFFu;
}

void lang_select_run(void)
{
    u32 n = i18n_lang_count();
    u32 focus = (u32)i18n_lang();
    bool dirty = true;
    i32 last_x = -1;
    i32 last_y = -1;

    if (n == 0u) {
        n = 1u;
    }
    if (focus >= n) {
        focus = 0;
    }

    if (fb_available()) {
        mouse_set_bounds(fb_width(), fb_height());
    }

    for (;;) {
        if (keyboard_has_char()) {
            u32 code = keyboard_get_codepoint();

            if (code == KEY_UP || code == KEY_DOWN || code == '\t') {
                if (code == KEY_UP) {
                    focus = (focus + n - 1u) % n;
                } else {
                    focus = (focus + 1u) % n;
                }
                dirty = true;
            } else if (code == '\n' || code == '\r') {
                i18n_set_lang((enum lang_id)focus);
                return;
            } else if (code >= '1' && code <= '9' &&
                       (code - '1') < n) {
                focus = code - '1';
                dirty = true;
            }
        } else if (fb_available() && mouse_has_event()) {
            struct mouse_event ev = mouse_get_event();
            u32 hit = lang_hit(ev.x, ev.y);

            if (hit < n && hit != focus) {
                focus = hit;
                dirty = true;
            }
            if (ev.kind == MOUSE_EV_DOWN && ev.button == MOUSE_LEFT && hit < n) {
                i18n_set_lang((enum lang_id)hit);
                return;
            }
        }

        if (!dirty && fb_available() &&
            (mouse_x() != last_x || mouse_y() != last_y)) {
            u32 live = lang_hit(mouse_x(), mouse_y());

            if (live < n && live != focus) {
                focus = live;
                dirty = true;
            }
        }

        if (dirty) {
            draw_picker(focus);
            dirty = false;
            last_x = mouse_x();
            last_y = mouse_y();
        } else if (fb_available() && (mouse_x() != last_x || mouse_y() != last_y)) {
            cursor_draw((u32)mouse_x(), (u32)mouse_y());
            last_x = mouse_x();
            last_y = mouse_y();
        } else {
            __asm__ volatile("hlt");
        }
    }
}
