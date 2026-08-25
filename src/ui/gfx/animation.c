#include "animation.h"
#include "draw.h"
#include "fb.h"
#include "mouse.h"
#include "time.h"

#define UI_FADE_STEPS 8u
#define UI_FADE_DELAY_MS 16u
#define UI_XFADE_STEPS 14u

static void ui_wait_ms(u32 milliseconds)
{
    u32 start = time_uptime_ms();

    while (time_uptime_ms() - start < milliseconds) {
        __asm__ volatile("hlt");
    }
}

static void ui_present_overlay(u8 alpha)
{
    /* Mix black while copying back→front so the undimmed scene never
     * flashes on screen for a frame. */
    fb_present_dimmed(alpha);
}

static void ui_transition_cursor_end(void)
{
    fb_compose_begin();
    cursor_invalidate();
    cursor_flip((u32)mouse_x(), (u32)mouse_y());
}

void ui_fade_in(void)
{
    u32 step;

    if (!fb_compose_ready()) {
        return;
    }

    cursor_invalidate();
    for (step = 0; step <= UI_FADE_STEPS; ++step) {
        u8 alpha = (u8)(255u - (step * 255u) / UI_FADE_STEPS);
        ui_present_overlay(alpha);
        ui_wait_ms(UI_FADE_DELAY_MS);
    }
    ui_transition_cursor_end();
}

void ui_fade_out(void)
{
    u32 step;

    if (!fb_compose_ready()) {
        return;
    }

    cursor_invalidate();
    for (step = 0; step <= UI_FADE_STEPS; ++step) {
        u8 alpha = (u8)((step * 255u) / UI_FADE_STEPS);
        ui_present_overlay(alpha);
        ui_wait_ms(UI_FADE_DELAY_MS);
    }
    cursor_invalidate();
}

void ui_crossfade_from(const u8 *old_front)
{
    u32 step;

    if (old_front == NULL || !fb_compose_ready()) {
        if (fb_compose_ready()) {
            fb_compose_present();
        }
        return;
    }

    cursor_invalidate();
    for (step = 0; step <= UI_XFADE_STEPS; ++step) {
        u32 t = (step * 255u) / UI_XFADE_STEPS;
        /* Smoothstep so it eases in and out. */
        t = (t * t * (3u * 255u - 2u * t)) / (255u * 255u);
        if (t > 255u) {
            t = 255u;
        }
        fb_blend_to_front(old_front, (u8)t);
        ui_wait_ms(UI_FADE_DELAY_MS);
    }
    /* New scene is on both buffers. Point at back so the caller can
     * cursor_flip without wiping the fade result. */
    fb_compose_begin();
    cursor_invalidate();
}
