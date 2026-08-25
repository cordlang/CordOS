#include "session.h"
#include "animation.h"
#include "brand.h"
#include "config.h"
#include "draw.h"
#include "fb.h"
#include "gfx_session.h"
#include "i18n.h"
#include "keyboard.h"
#include "keycodes.h"
#include "power.h"
#include "shell.h"
#include "theme.h"
#include "time.h"
#include "vga.h"
#include "userdb.h"
#include "vfs.h"

#define LOGIN_USER_MAX 32
#define LOGIN_PASS_MAX 32
#define DEV_USER "admin"
#define DEV_PASS "admin"

enum home_item {
    HOME_FILES = 0,
    HOME_TERMINAL,
    HOME_SETTINGS,
    HOME_ABOUT,
    HOME_LOGOUT,
    HOME_POWER,
    HOME_COUNT
};

static u32 splash_done_mask;
static u8 splash_bar;

static void ui_wait_ms(u32 ms)
{
    u32 start = time_uptime_ms();

    while ((time_uptime_ms() - start) < ms) {
        __asm__ volatile("hlt");
    }
}

static void ui_drain_keys(void)
{
    while (keyboard_has_char()) {
        (void)keyboard_get_codepoint();
    }
}

static u32 ui_get_key(void)
{
    for (;;) {
        if (!keyboard_has_char()) {
            __asm__ volatile("hlt");
            continue;
        }
        return keyboard_get_codepoint();
    }
}

static void draw_progress(u32 done_count)
{
    char bar[17];
    u32 i;

    for (i = 0; i < 16; ++i) {
        bar[i] = (i < done_count * 4u) ? '#' : '-';
    }
    bar[16] = '\0';
    vga_print("  [");
    vga_print(bar);
    vga_print("]\n");
}

static u32 splash_smooth(u32 t)
{
    if (t > 255u) {
        t = 255u;
    }
    return (t * t * (3u * 255u - 2u * t)) / (255u * 255u);
}

static void splash_animate_bar(u8 from, u8 to)
{
    u32 i;
    const u32 steps = 12u;

    for (i = 1; i <= steps; ++i) {
        u32 t = splash_smooth((i * 255u) / steps);
        u8 v;
        if (to >= from) {
            v = (u8)((u32)from + (((u32)(to - from) * t) / 255u));
        } else {
            v = (u8)((u32)from - (((u32)(from - to) * t) / 255u));
        }
        draw_boot_splash_ex(v, 255u, 0);
        ui_wait_ms(16u);
    }
    splash_bar = to;
}

void session_splash_begin(void)
{
    splash_done_mask = 0;
    splash_bar = 0;

    if (fb_available()) {
        u8 *old = NULL;
        u32 i;

        cursor_hide();
        if (fb_compose_ready()) {
            old = fb_layer_alloc();
            if (old != NULL) {
                fb_copy_front(old);
            }
        }
        draw_boot_splash_to_back(0, 0, 18);
        if (old != NULL) {
            ui_crossfade_from(old);
        } else {
            fb_compose_present();
        }
        for (i = 1; i <= 14u; ++i) {
            u32 t = splash_smooth((i * 255u) / 14u);
            i32 shift = (i32)((18u * (255u - t)) / 255u);
            draw_boot_splash_ex(0, (u8)t, shift);
            ui_wait_ms(16u);
        }
        ui_wait_ms(60u);
        return;
    }

    vga_clear();
    vga_print("\n\n      ");
    vga_print(name_os);
    vga_print("\n\n      ");
    vga_print(i18n(MSG_SPLASH_TITLE));
    vga_print("...\n\n");
}

void session_splash_stage(u32 stage)
{
    enum msg_id labels[4] = {
        MSG_SPLASH_STAGE0,
        MSG_SPLASH_STAGE1,
        MSG_SPLASH_STAGE2,
        MSG_SPLASH_STAGE3
    };
    u32 i;

    if (stage > 3u) {
        return;
    }

    splash_done_mask |= (1u << stage);

    if (fb_available()) {
        u8 bar = (u8)(((stage + 1u) * 255u) / 4u);
        (void)labels;
        splash_animate_bar(splash_bar, bar);
        ui_wait_ms(40u);
        return;
    }

    vga_clear();
    vga_print("\n\n      ");
    vga_print(name_os);
    vga_print("\n\n      ");
    vga_print(i18n(MSG_SPLASH_TITLE));
    vga_print("...\n\n");

    for (i = 0; i < 4u; ++i) {
        vga_print("      ");
        if (splash_done_mask & (1u << i)) {
            vga_print("[x] ");
        } else {
            vga_print("[ ] ");
        }
        vga_print(i18n(labels[i]));
        vga_print("\n");
    }

    vga_print("\n");
    draw_progress(stage + 1u);
    ui_wait_ms(180);
}

void session_splash_finish(void)
{
    if (!fb_available()) {
        return;
    }
    if (splash_bar < 255u) {
        splash_animate_bar(splash_bar, 255u);
    }
    ui_wait_ms(220u);
}

static int streq(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b) {
        ++a;
        ++b;
    }
    return *a == *b;
}

static void field_append(char *buf, u32 *len, u32 max, u32 code)
{
    if (code < 32u || code == 127u || key_is_special(code)) {
        return;
    }
    if (code > 0x7Fu) {
        return; /* Ola 0 ASCII fields */
    }
    if (*len + 1u >= max) {
        return;
    }
    buf[*len] = (char)code;
    ++(*len);
    buf[*len] = '\0';
}

static void field_backspace(char *buf, u32 *len)
{
    if (*len == 0) {
        return;
    }
    --(*len);
    buf[*len] = '\0';
}

static void draw_login(u32 focus, const char *user, const char *pass,
                       bool show_error)
{
    u32 i;
    u32 plen;

    vga_clear();
    vga_print("\n\n              ");
    vga_print(name_os);
    vga_print("\n\n");

    vga_print("         ");
    vga_print(focus == 0 ? "> " : "  ");
    vga_print(i18n(MSG_LOGIN_USER));
    vga_print("\n         [ ");
    vga_print(user);
    vga_print(" ]\n\n");

    vga_print("         ");
    vga_print(focus == 1 ? "> " : "  ");
    vga_print(i18n(MSG_LOGIN_PASSWORD));
    vga_print("\n         [ ");
    plen = 0;
    while (pass[plen] != '\0') {
        ++plen;
    }
    for (i = 0; i < plen; ++i) {
        vga_print("*");
    }
    vga_print(" ]\n\n");

    vga_print("         ");
    vga_print(focus == 2 ? "> [ " : "  [ ");
    vga_print(i18n(MSG_LOGIN_ENTER));
    vga_print(" ]\n\n");

    if (show_error) {
        vga_print("         ");
        vga_print(i18n(MSG_LOGIN_BAD));
        vga_print("\n\n");
    }

    vga_print("         ");
    vga_print(i18n(MSG_LOGIN_HINT));
    vga_print("\n");
}

static bool try_auth(const char *user, const char *pass)
{
    if (userdb_count() > 0u) {
        return userdb_auth(user, pass) != 0;
    }
    return streq(user, DEV_USER) && streq(pass, DEV_PASS);
}

static bool login_run(void)
{
    char user[LOGIN_USER_MAX];
    char pass[LOGIN_PASS_MAX];
    u32 ulen = 0;
    u32 plen = 0;
    u32 focus = 0;
    bool bad = false;

    user[0] = '\0';
    pass[0] = '\0';
    ui_drain_keys();
    draw_login(focus, user, pass, bad);

    for (;;) {
        u32 key = ui_get_key();

        if (key == KEY_F1) {
            shell_run();
            draw_login(focus, user, pass, bad);
            continue;
        }

        if (key == '\t' || key == KEY_DOWN) {
            focus = (focus + 1u) % 3u;
            draw_login(focus, user, pass, bad);
            continue;
        }

        if (key == KEY_UP) {
            focus = (focus == 0) ? 2u : (focus - 1u);
            draw_login(focus, user, pass, bad);
            continue;
        }

        if (key == '\n' || key == '\r') {
            if (focus < 2u) {
                focus = focus + 1u;
                draw_login(focus, user, pass, bad);
                continue;
            }
            if (try_auth(user, pass)) {
                return true;
            }
            bad = true;
            plen = 0;
            pass[0] = '\0';
            focus = 1;
            draw_login(focus, user, pass, bad);
            continue;
        }

        if (key == '\b' || key == 127u) {
            if (focus == 0) {
                field_backspace(user, &ulen);
            } else if (focus == 1) {
                field_backspace(pass, &plen);
            }
            draw_login(focus, user, pass, bad);
            continue;
        }

        if (focus == 0) {
            field_append(user, &ulen, LOGIN_USER_MAX, key);
            draw_login(focus, user, pass, bad);
        } else if (focus == 1) {
            field_append(pass, &plen, LOGIN_PASS_MAX, key);
            draw_login(focus, user, pass, bad);
        }
    }
}

static void screen_wait_back(void)
{
    for (;;) {
        u32 key = ui_get_key();
        if (key == '\n' || key == '\r' || key == 27u || key == KEY_F1) {
            if (key == KEY_F1) {
                shell_run();
            }
            return;
        }
    }
}

static void app_files(void)
{
    vga_clear();
    vga_print(name_os);
    vga_print("\n\n");
    vga_print(i18n(MSG_FILES_TITLE));
    vga_print("\n\n");
    if (vfs_ls("/") < 0) {
        vga_print(i18n(MSG_LS_ERROR));
        vga_print("\n");
    }
    vga_print("\n");
    vga_print(i18n(MSG_FILES_HINT));
    vga_print("\n");
    screen_wait_back();
}

static void app_about(void)
{
    vga_clear();
    vga_print("\n\n      ");
    vga_print(name_os);
    vga_print(" ");
    vga_print(version_os);
    vga_print("\n\n      ");
    vga_print(i18n(MSG_ABOUT_BODY));
    vga_print("\n\n      ");
    vga_print(i18n(MSG_FILES_HINT));
    vga_print("\n");
    screen_wait_back();
}

static void app_settings(void)
{
    vga_clear();
    vga_print("\n\n      ");
    vga_print(i18n(MSG_HOME_SETTINGS));
    vga_print("\n\n      ");
    vga_print(i18n(MSG_SETTINGS_BODY));
    vga_print("\n      ");
    vga_print(i18n_lang_name(i18n_lang()));
    vga_print("\n\n      ");
    vga_print(i18n(MSG_FILES_HINT));
    vga_print("\n");
    screen_wait_back();
}

static void power_halt(void)
{
    vga_clear();
    vga_print("\n\n      ");
    vga_print(name_os);
    vga_print("\n\n      ");
    vga_print(i18n(MSG_POWER_MSG));
    vga_print("\n");
    machine_power_off();
}

static enum home_item home_run(void)
{
    enum home_item focus = HOME_FILES;
    enum msg_id labels[HOME_COUNT] = {
        MSG_HOME_FILES,
        MSG_HOME_TERMINAL,
        MSG_HOME_SETTINGS,
        MSG_HOME_ABOUT,
        MSG_HOME_LOGOUT,
        MSG_HOME_POWER
    };

    ui_drain_keys();

    for (;;) {
        u32 i;
        u32 key;

        vga_clear();
        vga_print("\n\n      ");
        vga_print(name_os);
        vga_print("\n      ");
        vga_print(i18n(MSG_HOME_READY));
        vga_print("\n\n");

        for (i = 0; i < (u32)HOME_COUNT; ++i) {
            vga_print("      ");
            if (i == (u32)focus) {
                vga_print("> ");
            } else {
                vga_print("  ");
            }
            vga_print(i18n(labels[i]));
            vga_print("\n");
        }

        vga_print("\n      ");
        vga_print(i18n(MSG_HOME_HINT));
        vga_print("\n");

        key = ui_get_key();

        if (key == KEY_F1) {
            shell_run();
            continue;
        }

        if (key == KEY_DOWN || key == '\t' || key == 's' || key == 'S' ||
            key == 'j' || key == 'J') {
            focus = (enum home_item)(((u32)focus + 1u) % (u32)HOME_COUNT);
            continue;
        }

        if (key == KEY_UP || key == 'w' || key == 'W' ||
            key == 'k' || key == 'K') {
            focus = (enum home_item)(
                ((u32)focus + (u32)HOME_COUNT - 1u) % (u32)HOME_COUNT);
            continue;
        }

        if (key == '\n' || key == '\r') {
            return focus;
        }
    }
}

void session_run(void)
{
    if (fb_available()) {
        gfx_session_run();
        return;
    }

    for (;;) {
        while (!login_run()) {
        }

        for (;;) {
            enum home_item action = home_run();

            if (action == HOME_FILES) {
                app_files();
            } else if (action == HOME_TERMINAL) {
                shell_run();
            } else if (action == HOME_SETTINGS) {
                app_settings();
            } else if (action == HOME_ABOUT) {
                app_about();
            } else if (action == HOME_LOGOUT) {
                break;
            } else if (action == HOME_POWER) {
                power_halt();
            }
        }
    }
}
