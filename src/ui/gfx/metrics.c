#include "metrics.h"
#include "fb.h"

u32 ui_px(u32 at_1080)
{
    u32 h = fb_height();
    u32 v;

    if (at_1080 == 0) {
        return 0;
    }
    if (h == 0 || h == 1080u) {
        return at_1080;
    }
    v = (at_1080 * h + 540u) / 1080u;
    return v < 1u ? 1u : v;
}

u32 ui_margin(void)
{
    u32 w = fb_width();
    u32 m = ui_px(48u);

    if (w < 800u) {
        m = 16u;
    } else if (w < 1100u) {
        m = 32u;
    }
    if (m * 2u + 200u > w) {
        m = (w > 240u) ? 16u : 8u;
    }
    return m;
}

u32 ui_content_w(void)
{
    u32 w = fb_width();
    u32 m = ui_margin() * 2u;
    u32 want = ui_px(420u);
    u32 maxc;

    if (w <= m + 200u) {
        return (w > 32u) ? (w - 32u) : w;
    }
    maxc = w - m;
    if (want > 720u) {
        want = 720u;
    }
    if (want > maxc) {
        want = maxc;
    }
    if (want < 240u && maxc >= 240u) {
        want = 240u;
    }
    return want;
}

u32 ui_text_scale(void)
{
    u32 h = fb_height();

    if (h >= 1600u) {
        return 2u;
    }
    return 1u;
}

u32 ui_gap(void)
{
    u32 g = ui_px(16u);

    return g < 8u ? 8u : g;
}
