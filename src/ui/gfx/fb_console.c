#include "fb_console.h"
#include "draw.h"
#include "fb.h"
#include "font.h"
#include "theme.h"

static bool s_enabled;
static u32 s_origin_x;
static u32 s_origin_y;

void fb_console_enable(void)
{
    if (!fb_available()) {
        s_enabled = false;
        return;
    }

    s_origin_x = 28;
    s_origin_y = 28;
    fb_compose_begin();
    draw_bg_atmosphere();
    draw_panel(16, 16,
               fb_width() > 32 ? fb_width() - 32 : fb_width(),
               fb_height() > 32 ? fb_height() - 32 : fb_height(),
               true);
    draw_text(36, 32, "Terminal", THEME_ACCENT, 1);
    fb_compose_present();
    s_enabled = true;
}

void fb_console_disable(void)
{
    s_enabled = false;
}

bool fb_console_is_enabled(void)
{
    return s_enabled;
}

void fb_console_put_cell(u8 row, u8 column, char character)
{
    u32 x;
    u32 y;
    char tmp[2];

    if (!s_enabled || !fb_available()) {
        return;
    }

    /* Skip status chrome; leave title row. */
    if (row < 2) {
        return;
    }

    /* Console grid: wide enough for digits, not the atlas pad. */
    x = s_origin_x + (u32)column * 14u;
    y = s_origin_y + 20u + (u32)(row - 2) * FONT_LINE;

    fb_fill_rect(x, y, 14u, FONT_LINE,
                 THEME_BG1.r, THEME_BG1.g, THEME_BG1.b);

    if (character == ' ' || character == '\0') {
        return;
    }

    tmp[0] = character;
    tmp[1] = '\0';
    draw_text(x, y, tmp, THEME_FG, 1);
}
