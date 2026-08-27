#ifndef CORDOS_DESKTOP_PRIV_H
#define CORDOS_DESKTOP_PRIV_H

#include "desktop.h"
#include "animation.h"
#include "compositor.h"
#include "config.h"
#include "draw.h"
#include "fb.h"
#include "metrics.h"
#include "widget.h"
#include "font.h"
#include "i18n.h"
#include "io.h"
#include "keyboard.h"
#include "keycodes.h"
#include "mouse.h"
#include "persist.h"
#include "pmm.h"
#include "power.h"
#include "rtc.h"
#include "shell.h"
#include "theme.h"
#include "time.h"
#include "utf8.h"
#include "vfs.h"
#include "icons.h"
#include "wallpaper.h"
#include "net.h"
#include "sysmon.h"

#define DOCK_M     ui_px(18u)
#define DOCK_H     ui_px(66u)
#define DOCK_SLOT  ui_px(56u)
#define DOCK_PADX  ui_px(18u)
#define DOCK_APPS  6u
#define DOCK_ICON  ui_px(40u)
#define DESK_TOP   ui_px(118u)
#define STAT_H     ui_px(36u)
#define TITLE_H    ui_px(48u)
#define MENU_ROW    ui_px(44u)
#define MENU_INSET  ui_px(10u)
#define CTX_HEADER  ui_px(34u)
#define CTX_SEP     ui_px(8u)
#define FILE_ROW   FONT_LINE
#define MAX_WIN    4
#define MAX_FILES  8
#define TERM_ROWS  11
#define TERM_COLS  52
#define ICON_COUNT 5
#define MENU_COUNT 7
#define CTX_COUNT  5

#define HIT_NONE     0u
#define HIT_DESKTOP  1u
#define HIT_LAUNCHER 2u
#define HIT_BAR      3u
#define HIT_ICON     0x100u
#define HIT_MENU     0x200u
#define HIT_TASK     0x300u
#define HIT_CLOSE    0x400u
#define HIT_TITLE    0x500u
#define HIT_BODY     0x600u
#define HIT_FILE     0x700u
#define HIT_LANG     0x800u
#define HIT_LANG_MAX 16u
#define HIT_POWER_OK 0x810u
#define HIT_POWER_NO 0x811u
#define HIT_WP_0     0x812u
#define HIT_WP_1     0x813u
#define HIT_IC_0     0x820u
#define HIT_CTX      0xA00u
#define HIT_DOCK     0xB00u
#define HIT_STATUS   0xC00u
#define HIT_SPOT     0xF00u
#define HIT_SPOT_ROW 0xF10u
#define SPOT_MAX     8u
#define SPOT_QMAX    48u
#define SPOT_BAR_H   ui_px(64u)
#define SPOT_ROW_H   ui_px(52u)
#define WP_THUMB_W   ui_px(200u)
#define WP_THUMB_H   ui_px(112u)
#define WP_THUMB_GAP ui_px(18u)
#define IC_THUMB     ui_px(64u)
#define IC_THUMB_GAP ui_px(14u)

enum win_kind {
    WIN_FILES = 0,
    WIN_TERM,
    WIN_SETTINGS,
    WIN_ABOUT,
    WIN_ACTIVITY,
    WIN_POWER
};

struct window {
    bool used;
    enum win_kind kind;
    i32 x;
    i32 y;
    u32 w;
    u32 h;
    u32 file_sel;
    char preview[360];
    char term_lines[TERM_ROWS][TERM_COLS + 1];
    u32 term_count;
    char term_input[TERM_COLS];
    u32 term_len;
};

enum spot_kind {
    SPOT_APP = 0,
    SPOT_FILE
};

struct spot_item {
    enum spot_kind kind;
    u32 arg;
    enum ui_icon icon;
    enum msg_id cat;
    char title[36];
};

struct file_acc {
    char names[MAX_FILES][33];
    u32 sizes[MAX_FILES];
    u32 count;
};

extern struct window s_win[MAX_WIN];
extern u32 s_z[MAX_WIN];
extern u32 s_zcount;
extern bool s_menu;
extern bool s_spot;
extern bool s_ctx;
extern i32 s_ctx_x;
extern i32 s_ctx_y;
extern bool s_show_icons;
extern i32 s_drag;
extern i32 s_dx;
extern i32 s_dy;
extern u32 s_hover;
extern bool s_logout;
extern char s_clock[8];
extern char s_date[12];
extern u32 s_last_min;
extern u8 s_last_day;
extern u8 s_last_month;
extern u16 s_last_year;
extern u32 s_widget_last_hover;
extern bool s_widget_repaint_base;
extern u32 s_last_click_hit;
extern u32 s_last_click_ms;
extern char s_spot_q[SPOT_QMAX];
extern u32 s_spot_len;
extern struct spot_item s_spot_hits[SPOT_MAX];
extern u32 s_spot_n;
extern u32 s_spot_sel;
extern u8 *s_spot_fade_from;
extern bool s_spot_need_xfade;
extern struct file_acc s_files;
extern bool s_cursor_valid;
extern i32 s_cursor_x;
extern i32 s_cursor_y;

static inline int streq(const char *a, const char *b)
{
    if (a == NULL || b == NULL) {
        return 0;
    }
    while (*a != '\0' && *a == *b) {
        ++a;
        ++b;
    }
    return *a == *b;
}

static inline int str_starts(const char *s, const char *p)
{
    while (*p != '\0') {
        if (*s != *p) {
            return 0;
        }
        ++s;
        ++p;
    }
    return 1;
}

static inline void copy_n(char *dst, u32 max, const char *src)
{
    u32 i = 0;

    if (dst == NULL || max == 0) {
        return;
    }
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    while (src[i] != '\0' && i + 1u < max) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
}

static inline bool in_rect(i32 px, i32 py, i32 x, i32 y, i32 w, i32 h)
{
    if (w <= 0 || h <= 0 || px < x || py < y) {
        return false;
    }
    return (u32)(px - x) < (u32)w && (u32)(py - y) < (u32)h;
}

static inline const char *win_title(enum win_kind kind)
{
    switch (kind) {
    case WIN_FILES:    return i18n(MSG_HOME_FILES);
    case WIN_TERM:     return i18n(MSG_HOME_TERMINAL);
    case WIN_SETTINGS: return i18n(MSG_HOME_SETTINGS);
    case WIN_ABOUT:    return i18n(MSG_HOME_ABOUT);
    case WIN_ACTIVITY: return i18n(MSG_HOME_ACTIVITY);
    case WIN_POWER:    return i18n(MSG_HOME_POWER);
    default:           return "?";
    }
}

static inline u32 dock_slot_x(u32 dx, u32 i)
{
    return dx + DOCK_PADX + i * DOCK_SLOT;
}

i32 z_top(void);
void z_raise(u32 id);
void win_close(u32 id);
void files_reload(void);
void files_preview(struct window *w, u32 index);
void term_exec(struct window *w);
i32 win_open(enum win_kind kind);
void action_open(u32 item);
void settings_layout(const struct window *w, u32 *lang_y, u32 *wp_y, u32 *ic_y);
void settings_lang_btn(u32 bx, u32 lang_y, u32 i, u32 *x, u32 *y);
void clamp_win(struct window *w);
void paint_windows(void);

void clock_refresh(void);
bool clock_changed(void);
void dock_geom(u32 *x, u32 *y, u32 *w, u32 *h);
void status_geom(u32 *x, u32 *y, u32 *w, u32 *h);
void icon_geom(u32 i, u32 *x, u32 *y, u32 *w, u32 *h);
void menu_geom(u32 *x, u32 *y, u32 *w, u32 *h);
void ctx_geom(u32 *x, u32 *y, u32 *w, u32 *h);
void draw_dock(void);
void draw_taskbar(void);
void draw_menu(void);
void paint_desktop_base(void);
void desktop_redraw(void);

void spot_geom(u32 *x, u32 *y, u32 *w, u32 *h);
void spot_open(void);
void spot_close(void);
void spot_activate(u32 index);
void spot_append(u32 code);
void spot_backspace(void);
void draw_spot(void);

void desktop_cursor_update(bool scene_is_back);
u32 hit_test(i32 px, i32 py);
enum cursor_kind desktop_cursor_kind(u32 hit);
void desktop_drag(u32 id, i32 old_x, i32 old_y);
void handle_click(u32 hit, bool dbl);
void handle_right_click(u32 hit);
void handle_key(u32 key);
bool desk_chrome_hit(u32 hit);
bool desktop_widgets(void);
bool is_double_click(u32 hit);

#endif
