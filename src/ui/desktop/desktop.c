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

#define DOCK_M     ui_px(18u)
#define DOCK_H     ui_px(66u)
#define DOCK_SLOT  ui_px(56u)
#define DOCK_PADX  ui_px(18u)
#define DOCK_APPS  5u
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
#define ICON_COUNT 4
#define MENU_COUNT 6
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

static const struct rgb DESK_WHITE = { 0xF7, 0xF8, 0xFA };
static const struct rgb DESK_INK   = { 0x1A, 0x1A, 0x1C };
static const struct rgb DESK_MUTED = { 0x4A, 0x4A, 0x52 };

enum win_kind {
    WIN_FILES = 0,
    WIN_TERM,
    WIN_SETTINGS,
    WIN_ABOUT,
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

static struct window s_win[MAX_WIN];
static u32 s_z[MAX_WIN];
static u32 s_zcount;
static bool s_menu;
static bool s_spot;
static bool s_ctx;
static i32 s_ctx_x;
static i32 s_ctx_y;
static bool s_show_icons;
static i32 s_drag;
static i32 s_dx;
static i32 s_dy;
static u32 s_hover;
static bool s_logout;
static char s_clock[8];
static char s_date[12];
static u32 s_last_min = 0xFFFFFFFFu;
static u8 s_last_day = 0xFFu;
static u8 s_last_month = 0xFFu;
static u16 s_last_year = 0xFFFFu;
static u32 s_widget_last_hover = HIT_NONE;
static bool s_widget_repaint_base;
static u32 s_last_click_hit = HIT_NONE;
static u32 s_last_click_ms;

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

static char s_spot_q[SPOT_QMAX];
static u32 s_spot_len;
static struct spot_item s_spot_hits[SPOT_MAX];
static u32 s_spot_n;
static u32 s_spot_sel;
static u8 *s_spot_fade_from;
static bool s_spot_need_xfade;

static void settings_layout(const struct window *w, u32 *lang_y, u32 *wp_y,
                            u32 *ic_y);
static void settings_lang_btn(u32 bx, u32 lang_y, u32 i, u32 *x, u32 *y);
static u32 hit_test(i32 px, i32 py);
static enum cursor_kind desktop_cursor_kind(u32 hit);
static void desktop_redraw(void);
static void action_open(u32 item);
static void spot_rebuild(void);
static void spot_open(void);
static void spot_close(void);
static void spot_activate(u32 index);
static void spot_geom(u32 *x, u32 *y, u32 *w, u32 *h);

static bool is_double_click(u32 hit)
{
    const u32 dbl_ms = 350u;
    u32 now = time_uptime_ms();
    bool dbl = (hit == s_last_click_hit) && (now - s_last_click_ms) <= dbl_ms;

    s_last_click_hit = hit;
    s_last_click_ms = now;
    return dbl;
}

static int streq(const char *a, const char *b)
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

static int str_starts(const char *s, const char *p)
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

static void copy_n(char *dst, u32 max, const char *src)
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

static bool in_rect(i32 px, i32 py, i32 x, i32 y, i32 w, i32 h)
{
    return px >= x && py >= y && px < (x + w) && py < (y + h);
}

static void date_refresh(u8 day, u8 mon)
{
    const char *mn;

    if (day < 1u || day > 31u) {
        day = 1;
    }
    if (mon < 1u || mon > 12u) {
        mon = 1;
    }
    mn = i18n_month_abbr(mon);
    if (i18n_date_day_first()) {
        s_date[0] = (char)('0' + (day / 10u));
        s_date[1] = (char)('0' + (day % 10u));
        s_date[2] = ' ';
        s_date[3] = mn[0];
        s_date[4] = mn[1];
        s_date[5] = mn[2];
        s_date[6] = '\0';
    } else {
        s_date[0] = mn[0];
        s_date[1] = mn[1];
        s_date[2] = mn[2];
        s_date[3] = ' ';
        s_date[4] = (char)('0' + (day / 10u));
        s_date[5] = (char)('0' + (day % 10u));
        s_date[6] = '\0';
    }
}

static void clock_refresh(void)
{
    struct rtc_time now;

    if (!rtc_read(&now)) {
        s_clock[0] = '0';
        s_clock[1] = '0';
        s_clock[2] = ':';
        s_clock[3] = '0';
        s_clock[4] = '0';
        s_clock[5] = '\0';
        date_refresh(1u, 1u);
        return;
    }
    s_clock[0] = (char)('0' + (now.hour / 10u));
    s_clock[1] = (char)('0' + (now.hour % 10u));
    s_clock[2] = ':';
    s_clock[3] = (char)('0' + (now.minute / 10u));
    s_clock[4] = (char)('0' + (now.minute % 10u));
    s_clock[5] = '\0';
    date_refresh(now.day, now.month);
}

static enum ui_icon battery_icon(void)
{
    u32 pct = power_battery_percent();

    if (power_on_ac() && pct < 95u) {
        return UI_ICON_BAT_CHARGE;
    }
    if (pct <= 20u) {
        return UI_ICON_BAT_LOW;
    }
    if (pct <= 65u) {
        return UI_ICON_BAT_HALF;
    }
    return UI_ICON_BAT_FULL;
}

static bool clock_changed(void)
{
    struct rtc_time now;

    if (!rtc_read(&now)) {
        return false;
    }
    if (now.minute == (u8)s_last_min && now.day == s_last_day &&
        now.month == s_last_month && now.year == s_last_year) {
        return false;
    }
    s_last_min = now.minute;
    s_last_day = now.day;
    s_last_month = now.month;
    s_last_year = now.year;
    s_clock[0] = (char)('0' + (now.hour / 10u));
    s_clock[1] = (char)('0' + (now.hour % 10u));
    s_clock[2] = ':';
    s_clock[3] = (char)('0' + (now.minute / 10u));
    s_clock[4] = (char)('0' + (now.minute % 10u));
    s_clock[5] = '\0';
    date_refresh(now.day, now.month);
    return true;
}

static bool s_cursor_valid;
static i32 s_cursor_x;
static i32 s_cursor_y;

static void wallpaper_apply(u32 id)
{
    wallpaper_set_login(id);
    wallpaper_set_desk(id);
}

static void desktop_cursor_update(bool scene_is_back)
{
    i32 mx = mouse_x();
    i32 my = mouse_y();
    bool moved = !s_cursor_valid || mx != s_cursor_x || my != s_cursor_y;
    u32 hit;
    enum cursor_kind kind;

    if (!scene_is_back && !moved) {
        return;
    }
    if (scene_is_back) {
        cursor_invalidate();
    } else {
        cursor_hide();
    }
    if (mx < 0) {
        mx = 0;
    }
    if (my < 0) {
        my = 0;
    }
    hit = hit_test(mx, my);
    kind = desktop_cursor_kind(hit);
    cursor_set_kind(kind);
    s_cursor_x = mx;
    s_cursor_y = my;
    s_cursor_valid = true;
}

static const char *win_title(enum win_kind kind)
{
    switch (kind) {
    case WIN_FILES:    return i18n(MSG_HOME_FILES);
    case WIN_TERM:     return i18n(MSG_HOME_TERMINAL);
    case WIN_SETTINGS: return i18n(MSG_HOME_SETTINGS);
    case WIN_ABOUT:    return i18n(MSG_HOME_ABOUT);
    case WIN_POWER:    return i18n(MSG_HOME_POWER);
    default:           return "?";
    }
}

static i32 z_top(void)
{
    if (s_zcount == 0) {
        return -1;
    }
    return (i32)s_z[s_zcount - 1u];
}

static void z_raise(u32 id)
{
    u32 i;
    u32 o = 0;

    for (i = 0; i < s_zcount; ++i) {
        if (s_z[i] != id) {
            s_z[o++] = s_z[i];
        }
    }
    s_z[o] = id;
    s_zcount = o + 1u;
}

static void win_close(u32 id)
{
    u32 i;
    u32 o = 0;

    if (id >= MAX_WIN) {
        return;
    }
    s_win[id].used = false;
    for (i = 0; i < s_zcount; ++i) {
        if (s_z[i] != id) {
            s_z[o++] = s_z[i];
        }
    }
    s_zcount = o;
    if (s_drag == (i32)id) {
        s_drag = -1;
    }
}

static void term_clear(struct window *w)
{
    u32 i;

    w->term_count = 0;
    w->term_len = 0;
    w->term_input[0] = '\0';
    for (i = 0; i < TERM_ROWS; ++i) {
        w->term_lines[i][0] = '\0';
    }
}

static void term_push(struct window *w, const char *line)
{
    u32 i;

    if (w->term_count == TERM_ROWS) {
        for (i = 1; i < TERM_ROWS; ++i) {
            copy_n(w->term_lines[i - 1u], TERM_COLS + 1u, w->term_lines[i]);
        }
        w->term_count = TERM_ROWS - 1u;
    }
    copy_n(w->term_lines[w->term_count], TERM_COLS + 1u, line);
    w->term_count++;
}

struct file_acc {
    char names[MAX_FILES][33];
    u32 sizes[MAX_FILES];
    u32 count;
};

static struct file_acc s_files;

static void file_cb(const char *name, u32 size, void *arg)
{
    struct file_acc *acc = (struct file_acc *)arg;

    if (acc->count >= MAX_FILES) {
        return;
    }
    copy_n(acc->names[acc->count], 33, name);
    acc->sizes[acc->count] = size;
    acc->count++;
}

static void files_reload(void)
{
    s_files.count = 0;
    (void)vfs_list("/", file_cb, &s_files);
}

static void files_preview(struct window *w, u32 index)
{
    int fd;
    ssize_t n;
    u32 i;

    w->file_sel = index;
    w->preview[0] = '\0';
    if (index >= s_files.count) {
        return;
    }
    fd = vfs_open(s_files.names[index]);
    if (fd < 0) {
        copy_n(w->preview, sizeof(w->preview), i18n(MSG_CAT_NOT_FOUND));
        return;
    }
    n = vfs_read(fd, w->preview, sizeof(w->preview) - 1u);
    vfs_close(fd);
    if (n < 0) {
        n = 0;
    }
    w->preview[n] = '\0';
    for (i = 0; w->preview[i] != '\0'; ++i) {
        if (w->preview[i] == '\n' || w->preview[i] == '\r') {
            w->preview[i] = ' ';
        }
    }
}

static void u32_to_dec(u32 v, char *out, u32 max)
{
    char tmp[12];
    u32 n = 0;
    u32 i = 0;

    if (max == 0) {
        return;
    }
    if (v == 0) {
        out[0] = '0';
        out[1] = '\0';
        return;
    }
    while (v > 0 && n < sizeof(tmp)) {
        tmp[n++] = (char)('0' + (v % 10u));
        v /= 10u;
    }
    while (n > 0 && i + 1u < max) {
        out[i++] = tmp[--n];
    }
    out[i] = '\0';
}

static void term_net_line(const char *line, void *ctx)
{
    struct window *tw = (struct window *)ctx;

    if (tw == NULL || line == NULL) {
        return;
    }
    term_push(tw, line);
    desktop_redraw();
}

static void term_exec(struct window *w)
{
    char line[TERM_COLS + 4];
    const char *cmd = w->term_input;

    line[0] = '>';
    line[1] = ' ';
    copy_n(line + 2, TERM_COLS - 1u, cmd);
    term_push(w, line);

    while (*cmd == ' ') {
        ++cmd;
    }

    if (cmd[0] == '\0') {
        /* empty */
    } else if (streq(cmd, "help")) {
        term_push(w, i18n(MSG_TERM_BANNER));
    } else if (streq(cmd, "clear")) {
        term_clear(w);
        term_push(w, i18n(MSG_TERM_BANNER));
    } else if (streq(cmd, "exit")) {
        /* handled by caller via return code using term_len sentinel */
        w->term_len = 0xFFFFu;
        return;
    } else if (streq(cmd, "ls")) {
        u32 i;
        files_reload();
        for (i = 0; i < s_files.count; ++i) {
            term_push(w, s_files.names[i]);
        }
    } else if (str_starts(cmd, "echo ")) {
        term_push(w, cmd + 5);
    } else if (str_starts(cmd, "cat ")) {
        int fd;
        char buf[TERM_COLS];
        ssize_t n;
        fd = vfs_open(cmd + 4);
        if (fd < 0) {
            term_push(w, i18n(MSG_CAT_NOT_FOUND));
        } else {
            n = vfs_read(fd, buf, sizeof(buf) - 1u);
            vfs_close(fd);
            if (n < 0) {
                n = 0;
            }
            buf[n] = '\0';
            term_push(w, buf);
        }
    } else if (str_starts(cmd, "lang ")) {
        if (i18n_set_lang_code(cmd + 5)) {
            (void)persist_set_u32("lang", (u32)i18n_lang());
            term_push(w, i18n_lang_name(i18n_lang()));
        }
    } else if (streq(cmd, "ticks")) {
        char buf[24];
        copy_n(buf, sizeof(buf), "ticks=");
        u32_to_dec(time_ticks(), buf + 6, sizeof(buf) - 6u);
        term_push(w, buf);
    } else if (streq(cmd, "mem")) {
        char buf[28];
        copy_n(buf, sizeof(buf), "free=");
        u32_to_dec(free_frames_os, buf + 5, sizeof(buf) - 5u);
        term_push(w, buf);
    } else if (streq(cmd, "ping") || str_starts(cmd, "ping ")) {
        const char *arg = cmd + 4;
        u32 ip = 0;

        while (*arg == ' ') {
            ++arg;
        }
        if (*arg != '\0') {
            if (!net_parse_ip(arg, &ip)) {
                term_push(w, i18n(MSG_PING_USAGE));
                w->term_len = 0;
                w->term_input[0] = '\0';
                return;
            }
        }
        net_ping_run(ip, 4u, term_net_line, w);
    } else if (streq(cmd, "net") || streq(cmd, "ifconfig")) {
        net_status_run(term_net_line, w);
    } else {
        term_push(w, i18n(MSG_UNKNOWN_CMD));
    }

    w->term_len = 0;
    w->term_input[0] = '\0';
}

static i32 win_open(enum win_kind kind)
{
    u32 i;
    i32 id = -1;
    struct window *w;
    u32 screen_w = fb_width();
    u32 screen_h = fb_height();

    for (i = 0; i < MAX_WIN; ++i) {
        if (s_win[i].used && s_win[i].kind == kind && kind != WIN_TERM) {
            z_raise(i);
            return (i32)i;
        }
        if (!s_win[i].used && id < 0) {
            id = (i32)i;
        }
    }
    if (id < 0) {
        id = (i32)s_z[0];
        win_close((u32)id);
    }

    w = &s_win[id];
    w->used = true;
    w->kind = kind;
    w->w = kind == WIN_POWER ? 440u : 760u;
    if (kind == WIN_POWER) {
        w->h = 220u;
    } else if (kind == WIN_SETTINGS) {
        w->h = 620u;
    } else {
        w->h = 520u;
    }
    if (w->w + 80u > screen_w) {
        w->w = screen_w > 40u ? screen_w - 40u : screen_w;
    }
    if (w->h + DOCK_H + 56u > screen_h) {
        w->h = screen_h > DOCK_H + 56u ? screen_h - DOCK_H - 56u : 120u;
    }
    w->x = (i32)(120u + (u32)id * 36u);
    w->y = (i32)(64u + (u32)id * 32u);
    if (kind == WIN_POWER) {
        w->x = (i32)((screen_w - w->w) / 2u);
        w->y = (i32)((screen_h - DOCK_H - DOCK_M - w->h) / 2u);
    }
    w->file_sel = 0xFFFFFFFFu;
    w->preview[0] = '\0';
    term_clear(w);
    if (kind == WIN_TERM) {
        term_push(w, name_os);
        term_push(w, i18n(MSG_TERM_BANNER));
    }
    if (kind == WIN_FILES) {
        files_reload();
        if (s_files.count > 0) {
            files_preview(w, 0);
        }
    }
    z_raise((u32)id);
    return id;
}

static void dock_geom(u32 *x, u32 *y, u32 *w, u32 *h)
{
    u32 screen_w = fb_width();
    u32 dock_w = DOCK_PADX * 2u + DOCK_APPS * DOCK_SLOT;

    if (dock_w + 16u > screen_w) {
        dock_w = screen_w > 16u ? screen_w - 16u : screen_w;
    }
    *w = dock_w;
    *h = DOCK_H;
    *x = screen_w > dock_w ? (screen_w - dock_w) / 2u : 0;
    *y = fb_height() > (DOCK_M + DOCK_H) ? fb_height() - DOCK_M - DOCK_H : 0;
}

static u32 dock_slot_x(u32 dx, u32 i)
{
    return dx + DOCK_PADX + i * DOCK_SLOT;
}

static void status_geom(u32 *x, u32 *y, u32 *w, u32 *h)
{
    u32 bat = 22u;

    *h = STAT_H;
    *w = 12u + bat + 12u;
    *x = fb_width() > *w + 22u ? fb_width() - *w - 22u : 8u;
    *y = 18u;
}

static void icon_geom(u32 i, u32 *x, u32 *y, u32 *w, u32 *h)
{
    u32 sw = fb_width();
    u32 sh = fb_height();
    u32 cell_w = ui_px(128u);
    u32 cell_h = ui_px(108u);
    u32 gap = ui_px(28u);
    u32 cols = (sw >= ui_px(780u)) ? ICON_COUNT : 2u;
    u32 rows = (ICON_COUNT + cols - 1u) / cols;
    u32 grid_w = cols * cell_w + (cols - 1u) * gap;
    u32 grid_h = rows * cell_h + (rows - 1u) * gap;
    u32 usable = (sh > DOCK_H + DOCK_M + DESK_TOP)
                     ? (sh - DOCK_H - DOCK_M - DESK_TOP) : sh;
    u32 x0 = (sw > grid_w) ? (sw - grid_w) / 2u : 16u;
    u32 y0 = DESK_TOP + ((usable > grid_h) ? (usable - grid_h) / 2u : 8u);
    u32 col = i % cols;
    u32 row = i / cols;

    *x = x0 + col * (cell_w + gap);
    *y = y0 + row * (cell_h + gap);
    *w = cell_w;
    *h = cell_h;
}

static void menu_geom(u32 *x, u32 *y, u32 *w, u32 *h)
{
    u32 dx;
    u32 dy;
    u32 dw;
    u32 dh;
    dock_geom(&dx, &dy, &dw, &dh);
    *w = 268u;
    *h = MENU_INSET * 2u + MENU_COUNT * MENU_ROW;
    *x = dock_slot_x(dx, 0);
    if (*x + *w + 8u > fb_width()) {
        *x = fb_width() > *w + 8u ? fb_width() - *w - 8u : 0;
    }
    *y = dy > *h + 12u ? dy - *h - 12u : 8u;
}

static void ctx_geom(u32 *x, u32 *y, u32 *w, u32 *h)
{
    u32 sw = fb_width();
    u32 sh = fb_height();

    *w = 272u;
    *h = MENU_INSET * 2u + CTX_HEADER + CTX_COUNT * MENU_ROW + CTX_SEP;
    *x = (s_ctx_x > 0) ? (u32)s_ctx_x : 0;
    *y = (s_ctx_y > 0) ? (u32)s_ctx_y : 0;
    if (*x + *w + 8u > sw) {
        *x = sw > *w + 8u ? sw - *w - 8u : 0;
    }
    if (*y + *h + 8u > sh) {
        if ((u32)s_ctx_y > *h + 8u) {
            *y = (u32)s_ctx_y - *h;
        } else {
            *y = 8u;
        }
    }
}

static void spot_geom(u32 *x, u32 *y, u32 *w, u32 *h)
{
    u32 sw = fb_width();
    u32 sh = fb_height();
    u32 rows = 0;
    u32 pad = 0;

    *w = 680u;
    if (*w + 48u > sw) {
        *w = sw > 48u ? sw - 48u : sw;
    }
    if (s_spot_n > 0u) {
        rows = s_spot_n * SPOT_ROW_H;
        pad = 10u;
    } else if (s_spot_q[0] != '\0') {
        rows = SPOT_ROW_H;
        pad = 10u;
    }
    *h = SPOT_BAR_H + rows + pad;
    *x = sw > *w ? (sw - *w) / 2u : 0;
    *y = sh > *h ? (sh - *h) / 2u : 16u;
}

static u32 hit_test(i32 px, i32 py)
{
    u32 i;
    u32 x;
    u32 y;
    u32 iw;
    u32 ih;

    if (s_spot) {
        u32 sx;
        u32 sy;
        u32 sw;
        u32 sh;

        spot_geom(&sx, &sy, &sw, &sh);
        if (in_rect(px, py, (i32)sx, (i32)sy, (i32)sw, (i32)sh)) {
            u32 row;
            u32 ry;

            if (py < (i32)(sy + SPOT_BAR_H)) {
                return HIT_SPOT;
            }
            for (row = 0; row < s_spot_n && row < SPOT_MAX; ++row) {
                ry = sy + SPOT_BAR_H + row * SPOT_ROW_H;
                if (in_rect(px, py, (i32)sx, (i32)ry, (i32)sw,
                            (i32)SPOT_ROW_H)) {
                    return HIT_SPOT_ROW + row;
                }
            }
            return HIT_SPOT;
        }
    }

    if (s_ctx) {
        u32 cx;
        u32 cy;
        u32 cw;
        u32 ch;
        ctx_geom(&cx, &cy, &cw, &ch);
        if (in_rect(px, py, (i32)cx, (i32)cy, (i32)cw, (i32)ch)) {
            u32 i;
            u32 rows_y = cy + MENU_INSET + CTX_HEADER;
            if (py < (i32)rows_y) {
                return HIT_BAR;
            }
            for (i = 0; i < CTX_COUNT; ++i) {
                u32 extra = (i >= 2u) ? CTX_SEP : 0;
                u32 ry = rows_y + extra + i * MENU_ROW;
                if (in_rect(px, py, (i32)cx, (i32)ry, (i32)cw, (i32)MENU_ROW)) {
                    return HIT_CTX + i;
                }
            }
            return HIT_BAR;
        }
    }

    if (s_menu) {
        u32 mx;
        u32 my;
        u32 mw;
        u32 mh;
        menu_geom(&mx, &my, &mw, &mh);
        if (in_rect(px, py, (i32)mx, (i32)my, (i32)mw, (i32)mh)) {
            i32 rel = py - (i32)my - (i32)MENU_INSET;
            u32 row;
            if (rel < 0) {
                return HIT_BAR;
            }
            row = (u32)rel / MENU_ROW;
            if (row < MENU_COUNT) {
                return HIT_MENU + row;
            }
            return HIT_BAR;
        }
    }

    for (i = s_zcount; i > 0; --i) {
        u32 id = s_z[i - 1u];
        struct window *w = &s_win[id];
        i32 wx = w->x;
        i32 wy = w->y;
        i32 ww = (i32)w->w;
        i32 wh = (i32)w->h;

        if (!w->used) {
            continue;
        }
        if (!in_rect(px, py, wx, wy, ww, wh)) {
            continue;
        }
        if (in_rect(px, py, wx + 10, wy + 10, 28, 28)) {
            return HIT_CLOSE + id;
        }
        if (in_rect(px, py, wx, wy, ww, (i32)TITLE_H)) {
            return HIT_TITLE + id;
        }
        if (w->kind == WIN_FILES) {
            u32 f;
            for (f = 0; f < s_files.count; ++f) {
                if (in_rect(px, py, wx + 12,
                            wy + (i32)TITLE_H + 12 + (i32)FILE_ROW +
                                (i32)f * (i32)FILE_ROW,
                            188, (i32)FILE_ROW)) {
                    return HIT_FILE + f;
                }
            }
        }
        if (w->kind == WIN_SETTINGS) {
            u32 lang_y;
            u32 wp_y;
            u32 ic_y;
            u32 s;

            settings_layout(w, &lang_y, &wp_y, &ic_y);
            {
                u32 li;
                u32 n = i18n_lang_count();

                if (n > HIT_LANG_MAX) {
                    n = HIT_LANG_MAX;
                }
                for (li = 0; li < n; ++li) {
                    u32 lx;
                    u32 ly;

                    settings_lang_btn((u32)wx, lang_y, li, &lx, &ly);
                    if (in_rect(px, py, (i32)lx, (i32)ly, 140, 40)) {
                        return HIT_LANG + li;
                    }
                }
            }
            if (in_rect(px, py, wx + 24, (i32)wp_y, (i32)WP_THUMB_W,
                        (i32)WP_THUMB_H)) {
                return HIT_WP_0;
            }
            if (in_rect(px, py, wx + 24 + (i32)WP_THUMB_W + (i32)WP_THUMB_GAP,
                        (i32)wp_y, (i32)WP_THUMB_W, (i32)WP_THUMB_H)) {
                return HIT_WP_1;
            }
            for (s = 0; s < ICON_STYLE_COUNT; ++s) {
                i32 ix = wx + 24 + (i32)s * (i32)(IC_THUMB + IC_THUMB_GAP);
                if (in_rect(px, py, ix, (i32)ic_y, (i32)IC_THUMB,
                            (i32)IC_THUMB)) {
                    return HIT_IC_0 + s;
                }
            }
        }
        if (w->kind == WIN_POWER) {
            i32 by = wy + (i32)TITLE_H + 16 + (i32)FONT_LINE + 12;
            if (in_rect(px, py, wx + 24, by, 140, 40)) {
                return HIT_POWER_OK;
            }
            if (in_rect(px, py, wx + 180, by, 140, 40)) {
                return HIT_POWER_NO;
            }
        }
        return HIT_BODY + id;
    }

    {
        u32 dx;
        u32 dy;
        u32 dw;
        u32 dh;
        dock_geom(&dx, &dy, &dw, &dh);
        if (in_rect(px, py, (i32)dx, (i32)dy, (i32)dw, (i32)dh)) {
            u32 s;
            for (s = 0; s < DOCK_APPS; ++s) {
                u32 sx = dock_slot_x(dx, s);
                if (in_rect(px, py, (i32)sx, (i32)dy, (i32)DOCK_SLOT, (i32)dh)) {
                    if (s == 0u) {
                        return HIT_LAUNCHER;
                    }
                    return HIT_DOCK + (s - 1u);
                }
            }
            return HIT_BAR;
        }
    }

    {
        u32 sx;
        u32 sy;
        u32 sw;
        u32 sh;
        status_geom(&sx, &sy, &sw, &sh);
        if (in_rect(px, py, (i32)sx, (i32)sy, (i32)sw, (i32)sh)) {
            return HIT_STATUS;
        }
    }

    if (s_show_icons) {
        for (i = 0; i < ICON_COUNT; ++i) {
            icon_geom(i, &x, &y, &iw, &ih);
            if (in_rect(px, py, (i32)x, (i32)y, (i32)iw, (i32)ih)) {
                return HIT_ICON + i;
            }
        }
    }
    return HIT_DESKTOP;
}

static enum cursor_kind desktop_cursor_kind(u32 hit)
{
    if (hit >= HIT_ICON && hit < HIT_ICON + ICON_COUNT) {
        return CURSOR_KIND_POINTER;
    }
    if (hit == HIT_LAUNCHER) {
        return CURSOR_KIND_POINTER;
    }
    if (hit >= HIT_MENU && hit < HIT_MENU + MENU_COUNT) {
        return CURSOR_KIND_POINTER;
    }
    if (hit >= HIT_TASK && hit < HIT_TASK + MAX_WIN) {
        return CURSOR_KIND_POINTER;
    }
    if (hit >= HIT_DOCK && hit < HIT_DOCK + ICON_COUNT) {
        return CURSOR_KIND_POINTER;
    }
    if (hit >= HIT_CTX && hit < HIT_CTX + CTX_COUNT) {
        return CURSOR_KIND_POINTER;
    }
    if (hit == HIT_SPOT ||
        (hit >= HIT_SPOT_ROW && hit < HIT_SPOT_ROW + SPOT_MAX)) {
        return CURSOR_KIND_POINTER;
    }
    if (hit >= HIT_CLOSE && hit < HIT_CLOSE + MAX_WIN) {
        return CURSOR_KIND_POINTER;
    }
    if (hit >= HIT_FILE && hit < HIT_FILE + MAX_FILES) {
        return CURSOR_KIND_POINTER;
    }
    if ((hit >= HIT_LANG && hit < HIT_LANG + HIT_LANG_MAX) ||
        hit == HIT_POWER_OK || hit == HIT_POWER_NO ||
        hit == HIT_WP_0 || hit == HIT_WP_1 ||
        (hit >= HIT_IC_0 && hit < HIT_IC_0 + ICON_STYLE_COUNT)) {
        return CURSOR_KIND_POINTER;
    }
    return CURSOR_KIND_ARROW;
}

static void action_open(u32 item)
{
    s_menu = false;
    s_ctx = false;
    s_spot = false;
    if (item == 0) {
        (void)win_open(WIN_FILES);
    } else if (item == 1) {
        (void)win_open(WIN_TERM);
    } else if (item == 2) {
        (void)win_open(WIN_SETTINGS);
    } else if (item == 3) {
        (void)win_open(WIN_ABOUT);
    } else if (item == 4) {
        s_logout = true;
    } else if (item == 5) {
        (void)win_open(WIN_POWER);
    }
}

static int spot_fold_eq_at(const char *hay, const char *needle, u32 off)
{
    u32 i = 0;
    char hc;
    char nc;

    while (needle[i] != '\0') {
        hc = hay[off + i];
        nc = needle[i];
        if (hc == '\0') {
            return 0;
        }
        if (hc >= 'A' && hc <= 'Z') {
            hc = (char)(hc + 32);
        }
        if (nc >= 'A' && nc <= 'Z') {
            nc = (char)(nc + 32);
        }
        if (hc != nc) {
            return 0;
        }
        ++i;
    }
    return 1;
}

static int spot_match(const char *hay, const char *needle)
{
    u32 i;

    if (needle == NULL || needle[0] == '\0') {
        return 1;
    }
    if (hay == NULL) {
        return 0;
    }
    for (i = 0; hay[i] != '\0'; ++i) {
        if (spot_fold_eq_at(hay, needle, i)) {
            return 1;
        }
    }
    return 0;
}

static void spot_add(enum spot_kind kind, u32 arg, enum ui_icon icon,
                    enum msg_id cat, const char *title)
{
    if (s_spot_n >= SPOT_MAX || title == NULL) {
        return;
    }
    s_spot_hits[s_spot_n].kind = kind;
    s_spot_hits[s_spot_n].arg = arg;
    s_spot_hits[s_spot_n].icon = icon;
    s_spot_hits[s_spot_n].cat = cat;
    copy_n(s_spot_hits[s_spot_n].title, 36, title);
    s_spot_n++;
}

static void spot_rebuild(void)
{
    static const enum msg_id labels[MENU_COUNT] = {
        MSG_HOME_FILES, MSG_HOME_TERMINAL, MSG_HOME_SETTINGS,
        MSG_HOME_ABOUT, MSG_HOME_LOGOUT, MSG_HOME_POWER
    };
    static const enum ui_icon icons[MENU_COUNT] = {
        UI_ICON_FILES, UI_ICON_TERM, UI_ICON_SETTINGS,
        UI_ICON_ABOUT, UI_ICON_LOGOUT, UI_ICON_POWER
    };
    static const char *alias[MENU_COUNT] = {
        "files archivos explorer",
        "terminal shell consola cmd",
        "settings ajustes config",
        "about acerca info",
        "logout salir sesion sign out",
        "power apagar shutdown halt"
    };
    const char *q = s_spot_q;
    u32 i;

    s_spot_n = 0;
    for (i = 0; i < MENU_COUNT; ++i) {
        const char *title = i18n(labels[i]);

        if (spot_match(title, q) || spot_match(alias[i], q)) {
            spot_add(SPOT_APP, i, icons[i], MSG_SPOTLIGHT_APPS, title);
        }
    }
    files_reload();
    if (q[0] != '\0') {
        for (i = 0; i < s_files.count && s_spot_n < SPOT_MAX; ++i) {
            if (spot_match(s_files.names[i], q)) {
                spot_add(SPOT_FILE, i, UI_ICON_FILES, MSG_SPOTLIGHT_FILES,
                         s_files.names[i]);
            }
        }
    }
    if (s_spot_sel >= s_spot_n) {
        s_spot_sel = s_spot_n > 0u ? s_spot_n - 1u : 0;
    }
}

static void spot_arm_xfade(void)
{
    if (!fb_compose_ready()) {
        return;
    }
    if (s_spot_fade_from == NULL) {
        s_spot_fade_from = fb_layer_alloc();
    }
    if (s_spot_fade_from != NULL) {
        fb_copy_front(s_spot_fade_from);
        s_spot_need_xfade = true;
    }
}

static void spot_open(void)
{
    s_menu = false;
    s_ctx = false;
    spot_arm_xfade();
    s_spot = true;
    s_spot_q[0] = '\0';
    s_spot_len = 0;
    s_spot_sel = 0;
    spot_rebuild();
}

static void spot_close(void)
{
    if (s_spot) {
        spot_arm_xfade();
    }
    s_spot = false;
    s_spot_q[0] = '\0';
    s_spot_len = 0;
    s_spot_n = 0;
    s_spot_sel = 0;
}

static void spot_activate(u32 index)
{
    struct spot_item hit;

    if (index >= s_spot_n) {
        return;
    }
    hit = s_spot_hits[index];
    spot_close();
    if (hit.kind == SPOT_APP) {
        action_open(hit.arg);
        return;
    }
    {
        i32 id = win_open(WIN_FILES);

        if (id >= 0) {
            files_preview(&s_win[id], hit.arg);
        }
    }
}

static void spot_append(u32 code)
{
    char tmp[4];
    u32 n;
    u32 i;

    if (code < 32u || code == 127u || key_is_special(code)) {
        return;
    }
    n = utf8_encode(code, tmp);
    if (n == 0 || s_spot_len + n >= SPOT_QMAX) {
        return;
    }
    for (i = 0; i < n; ++i) {
        s_spot_q[s_spot_len++] = tmp[i];
    }
    s_spot_q[s_spot_len] = '\0';
    s_spot_sel = 0;
    spot_rebuild();
}

static void spot_backspace(void)
{
    if (s_spot_len == 0) {
        return;
    }
    s_spot_len--;
    while (s_spot_len > 0 &&
           (((u8)s_spot_q[s_spot_len]) & 0xC0u) == 0x80u) {
        s_spot_len--;
    }
    s_spot_q[s_spot_len] = '\0';
    s_spot_sel = 0;
    spot_rebuild();
}

static void draw_spot_lens(u32 x, u32 y, u32 size, struct rgb color)
{
    u32 hole = size > 8u ? size - 8u : size / 2u;
    u32 hx;
    u32 hy;

    if (size < 10u) {
        size = 10u;
        hole = 4u;
    }
    draw_round_fill(x, y, size, size, size / 2u, color, 210u);
    hx = x + (size > hole ? (size - hole) / 2u : 0);
    hy = y + (size > hole ? (size - hole) / 2u : 0);
    draw_round_fill(hx, hy, hole, hole, hole / 2u, THEME_GLASS, 255u);
    fb_fill_rect(x + size - 4u, y + size - 3u, size / 2u, 3u, color.r, color.g,
                 color.b);
}

static void draw_spot(void)
{
    u32 x;
    u32 y;
    u32 w;
    u32 h;
    u32 i;
    u32 tx;
    u32 ty;
    u32 text_h = 22u;
    struct rgb panel = { 0x1A, 0x1A, 0x1E };
    struct rgb sel = { 0x32, 0x5A, 0xC8 };
    const char *show;

    spot_geom(&x, &y, &w, &h);
    draw_round_fill_hard(x, y, w, h, 22u, panel, 240u);

    draw_spot_lens(x + 22u, y + (SPOT_BAR_H > 22u ? (SPOT_BAR_H - 22u) / 2u : 0),
                   22u, THEME_FG_DIM);

    tx = x + 56u;
    ty = y + (SPOT_BAR_H > text_h ? (SPOT_BAR_H - text_h) / 2u : 0);
    if (s_spot_q[0] != '\0') {
        show = s_spot_q;
        draw_text_h(tx, ty, show, THEME_FG, text_h);
        fb_fill_rect(tx + draw_text_width_h(show, text_h) + 3u, ty + 2u, 2u,
                     text_h > 6u ? text_h - 6u : text_h, THEME_FG.r, THEME_FG.g,
                     THEME_FG.b);
    } else {
        draw_text_h(tx, ty, i18n(MSG_SPOTLIGHT_PLACE), THEME_FG_DIM, text_h);
        fb_fill_rect(tx, ty + 2u, 2u, text_h > 6u ? text_h - 6u : text_h,
                     THEME_FG.r, THEME_FG.g, THEME_FG.b);
    }

    if (s_spot_n == 0u) {
        if (s_spot_q[0] != '\0') {
            draw_text(x + 24u, y + SPOT_BAR_H + 8u, i18n(MSG_SPOTLIGHT_NONE),
                      THEME_FG_DIM, 1);
        }
        return;
    }

    fb_fill_rect(x + 18u, y + SPOT_BAR_H - 1u, w > 36u ? w - 36u : w, 1u,
                 THEME_BORDER.r, THEME_BORDER.g, THEME_BORDER.b);

    for (i = 0; i < s_spot_n; ++i) {
        u32 ry = y + SPOT_BAR_H + i * SPOT_ROW_H;
        u32 iy = ry + (SPOT_ROW_H > 28u ? (SPOT_ROW_H - 28u) / 2u : 0);
        u32 title_y = ry + 8u;
        bool hot = (i == s_spot_sel);
        u32 cat_w;

        if (hot) {
            draw_round_fill(x + 10u, ry + 4u, w > 20u ? w - 20u : w,
                            SPOT_ROW_H > 8u ? SPOT_ROW_H - 8u : SPOT_ROW_H, 12u,
                            sel, 220u);
        }
        draw_icon(x + 22u, iy, 28u, s_spot_hits[i].icon,
                  hot ? THEME_FG : THEME_FG_DIM);
        draw_text_clip(x + 62u, title_y, x + w - 160u, s_spot_hits[i].title,
                       THEME_FG, 1);
        cat_w = draw_text_width(i18n(s_spot_hits[i].cat), 1);
        draw_text(x + w - 24u - cat_w, title_y + 2u, i18n(s_spot_hits[i].cat),
                  hot ? THEME_FG : THEME_FG_DIM, 1);
    }
}

static void draw_desktop_icons(void)
{
    const enum msg_id labels[ICON_COUNT] = {
        MSG_HOME_FILES, MSG_HOME_TERMINAL, MSG_HOME_SETTINGS, MSG_HOME_ABOUT
    };
    const enum ui_icon icons[ICON_COUNT] = {
        UI_ICON_FILES, UI_ICON_TERM, UI_ICON_SETTINGS, UI_ICON_ABOUT
    };
    u32 i;

    if (!s_show_icons) {
        return;
    }
    for (i = 0; i < ICON_COUNT; ++i) {
        u32 x;
        u32 y;
        u32 w;
        u32 h;
        u32 ix;
        u32 iy;
        bool hot = (s_hover == HIT_ICON + i);
        struct rgb label;
        icon_geom(i, &x, &y, &w, &h);
        label = hot ? THEME_ACCENT : DESK_INK;
        if (hot) {
            draw_glass(x, y, w, h > 24u ? h - 24u : h, 18, THEME_GLASS, 64u);
        }
        ix = x + (w > 48u ? (w - 48u) / 2u : 0);
        iy = y + 10u;
        draw_icon(ix, iy, 48, icons[i], hot ? THEME_ACCENT : DESK_INK);
        draw_text_centered(x + w / 2u, y + 68u, i18n(labels[i]), label, 1);
    }
}

static void settings_layout(const struct window *w, u32 *lang_y, u32 *wp_y,
                            u32 *ic_y)
{
    u32 py = (u32)w->y + TITLE_H + 16u + FONT_LINE * 2u;
    u32 n = i18n_lang_count();
    u32 rows = (n + 1u) / 2u;

    if (rows == 0u) {
        rows = 1u;
    }
    *lang_y = py;
    py += rows * 48u + FONT_HEIGHT + 12u + FONT_LINE;
    *wp_y = py;
    py += WP_THUMB_H + 8u + FONT_LINE + 12u + FONT_LINE;
    *ic_y = py;
}

static void settings_lang_btn(u32 bx, u32 lang_y, u32 i, u32 *x, u32 *y)
{
    *x = bx + 24u + (i % 2u) * 156u;
    *y = lang_y + (i / 2u) * 48u;
}

static void draw_win_body(struct window *w, u32 id)
{
    u32 bx = (u32)w->x;
    u32 by = (u32)w->y;
    u32 max_x = bx + w->w - 8u;
    u32 i;

    (void)id;
    if (w->kind == WIN_FILES) {
        u32 py = by + TITLE_H + 12u;
        draw_text(bx + 16, py, i18n(MSG_FILES_TITLE), THEME_FG_DIM, 1);
        py += FILE_ROW;
        for (i = 0; i < s_files.count; ++i) {
            bool on = (w->file_sel == i) || (s_hover == HIT_FILE + i);
            if (on) {
                draw_round_fill(bx + 12, py, 188, FILE_ROW - 2u, 10,
                                THEME_HOVER, 220u);
            }
            draw_text_clip(bx + 20,
                           py + (FILE_ROW > FONT_HEIGHT ? (FILE_ROW - FONT_HEIGHT) / 2u
                                                        : 0),
                           bx + 188, s_files.names[i],
                           on ? THEME_ACCENT : THEME_FG, 1);
            py += FILE_ROW;
        }
        fb_fill_rect(bx + 210, by + TITLE_H + 12u, 1, w->h - TITLE_H - 24u,
                     THEME_BORDER.r, THEME_BORDER.g, THEME_BORDER.b);
        draw_text_clip(bx + 222, by + TITLE_H + 18u, max_x,
                       w->preview[0] != '\0' ? w->preview : i18n(MSG_FILE_PREVIEW),
                       THEME_FG, 1);
    } else if (w->kind == WIN_TERM) {
        u32 py = by + TITLE_H + 12u;
        draw_round_fill(bx + 10, by + TITLE_H + 6u, w->w - 20u,
                        w->h - TITLE_H - 16u, 12, THEME_FIELD, 230u);
        for (i = 0; i < w->term_count; ++i) {
            draw_text_clip(bx + 12, py, max_x, w->term_lines[i], THEME_FG, 1);
            py += FONT_LINE;
        }
        {
            char prompt[TERM_COLS + 4];
            prompt[0] = '>';
            prompt[1] = ' ';
            copy_n(prompt + 2, TERM_COLS - 1u, w->term_input);
            draw_text_clip(bx + 12, py, max_x, prompt, THEME_ACCENT, 1);
            fb_fill_rect(bx + 12u + draw_text_width(prompt, 1), py, 2,
                         FONT_HEIGHT > 8u ? FONT_HEIGHT - 8u : FONT_HEIGHT,
                         THEME_ACCENT.r, THEME_ACCENT.g, THEME_ACCENT.b);
        }
    } else if (w->kind == WIN_SETTINGS) {
        u32 lang_y;
        u32 wp_y;
        u32 ic_y;
        u32 t1x;
        u32 s;
        u32 sel = wallpaper_desk_id();
        u32 ic_sel = icon_style();
        const enum msg_id ic_names[ICON_STYLE_COUNT] = {
            MSG_IC_LINEAR, MSG_IC_BOLD, MSG_IC_BROKEN, MSG_IC_BULK
        };

        settings_layout(w, &lang_y, &wp_y, &ic_y);
        draw_text(bx + 24, by + TITLE_H + 16u, i18n(MSG_SETTINGS_BODY), THEME_FG, 1);
        draw_text(bx + 24, by + TITLE_H + 16u + FONT_LINE, i18n(MSG_LANG_CLICK),
                  THEME_FG_DIM, 1);
        {
            u32 li;
            u32 n = i18n_lang_count();
            u32 rows;

            if (n > HIT_LANG_MAX) {
                n = HIT_LANG_MAX;
            }
            rows = (n + 1u) / 2u;
            if (rows == 0u) {
                rows = 1u;
            }
            for (li = 0; li < n; ++li) {
                u32 lx;
                u32 ly;
                enum lang_id id = (enum lang_id)li;

                settings_lang_btn(bx, lang_y, li, &lx, &ly);
                draw_button(lx, ly, 140, 40, i18n_lang_name(id),
                            i18n_lang() == id || s_hover == HIT_LANG + li);
            }
            draw_text(bx + 24, lang_y + rows * 48u, i18n_lang_name(i18n_lang()),
                      THEME_ACCENT, 1);
        }
        draw_text(bx + 24, wp_y - FONT_LINE, i18n(MSG_SETTINGS_WP), THEME_FG, 1);
        t1x = bx + 24u + WP_THUMB_W + WP_THUMB_GAP;
        draw_wallpaper_thumb(bx + 24u, wp_y, WP_THUMB_W, WP_THUMB_H,
                             DESK_WP_DEFAULT,
                             sel == DESK_WP_DEFAULT || s_hover == HIT_WP_0);
        draw_wallpaper_thumb(t1x, wp_y, WP_THUMB_W, WP_THUMB_H,
                             DESK_WP_ABSTRACT,
                             sel == DESK_WP_ABSTRACT || s_hover == HIT_WP_1);
        draw_text(bx + 24u, wp_y + WP_THUMB_H + 8u, i18n(MSG_WP_DEFAULT),
                  sel == LOGIN_WP_DEFAULT ? THEME_ACCENT : THEME_FG_DIM, 1);
        draw_text(t1x, wp_y + WP_THUMB_H + 8u, i18n(MSG_WP_ABSTRACT),
                  sel == LOGIN_WP_ABSTRACT ? THEME_ACCENT : THEME_FG_DIM, 1);
        draw_text(bx + 24, ic_y - FONT_LINE, i18n(MSG_SETTINGS_ICONS), THEME_FG, 1);
        for (s = 0; s < ICON_STYLE_COUNT; ++s) {
            u32 ix = bx + 24u + s * (IC_THUMB + IC_THUMB_GAP);
            bool on = (ic_sel == s) || (s_hover == HIT_IC_0 + s);
            draw_icon_style_thumb(ix, ic_y, IC_THUMB, IC_THUMB, s, on);
            draw_text(ix, ic_y + IC_THUMB + 8u, i18n(ic_names[s]),
                      ic_sel == s ? THEME_ACCENT : THEME_FG_DIM, 1);
        }
    } else if (w->kind == WIN_ABOUT) {
        u32 py = by + TITLE_H + 12u;
        draw_text(bx + 24, py, name_os, THEME_FG, 2);
        py += FONT_TITLE_H + 8u;
        draw_text(bx + 24, py, version_os, THEME_FG_DIM, 1);
        py += FONT_LINE;
        draw_text(bx + 24, py, arch_os, THEME_FG_DIM, 1);
        py += FONT_LINE;
        draw_text_clip(bx + 24, py, max_x, i18n(MSG_ABOUT_BODY), THEME_FG, 1);
        py += FONT_LINE;
        draw_text_clip(bx + 24, py, max_x, i18n(MSG_HOME_HINT), THEME_FG_DIM, 1);
    } else if (w->kind == WIN_POWER) {
        u32 py = by + TITLE_H + 16u;
        draw_text(bx + 24, py, i18n(MSG_POWER_CONFIRM), THEME_FG, 1);
        py += FONT_LINE + 12u;
        draw_button(bx + 24, py, 140, 40, i18n(MSG_HOME_POWER),
                    s_hover == HIT_POWER_OK);
        draw_button(bx + 180, py, 140, 40, i18n(MSG_POWER_CANCEL),
                    s_hover == HIT_POWER_NO);
    }
}

static bool dock_app_running(enum win_kind kind)
{
    u32 i;

    for (i = 0; i < MAX_WIN; ++i) {
        if (s_win[i].used && s_win[i].kind == kind) {
            return true;
        }
    }
    return false;
}

static void draw_status(void)
{
    u32 sx;
    u32 sy;
    u32 sw;
    u32 sh;
    u32 bat = 22u;
    bool hot = (s_hover == HIT_STATUS);

    status_geom(&sx, &sy, &sw, &sh);
    draw_round_fill_hard(sx, sy, sw, sh, sh / 2u, THEME_GLASS, hot ? 240u : 230u);
    draw_icon(sx + (sw > bat ? (sw - bat) / 2u : 0),
              sy + (sh > bat ? (sh - bat) / 2u : 0),
              bat, battery_icon(), DESK_WHITE);
}

static void draw_dock(void)
{
    const enum ui_icon apps[DOCK_APPS] = {
        UI_ICON_LAUNCHER, UI_ICON_FILES, UI_ICON_TERM,
        UI_ICON_SETTINGS, UI_ICON_ABOUT
    };
    const enum win_kind kinds[DOCK_APPS] = {
        WIN_FILES, WIN_FILES, WIN_TERM, WIN_SETTINGS, WIN_ABOUT
    };
    u32 dx;
    u32 dy;
    u32 dw;
    u32 dh;
    u32 i;
    bool launcher_hot = (s_hover == HIT_LAUNCHER) || s_menu;

    dock_geom(&dx, &dy, &dw, &dh);
    draw_round_fill_hard(dx, dy, dw, dh, dh / 2u, THEME_GLASS, 240u);

    for (i = 0; i < DOCK_APPS; ++i) {
        u32 sx = dock_slot_x(dx, i);
        u32 ix = sx + (DOCK_SLOT > DOCK_ICON ? (DOCK_SLOT - DOCK_ICON) / 2u : 0);
        u32 iy = dy + (dh > DOCK_ICON ? (dh - DOCK_ICON) / 2u : 0);
        bool hot;
        bool run;
        struct rgb col;

        if (i == 0u) {
            hot = launcher_hot;
            run = false;
        } else {
            hot = (s_hover == HIT_DOCK + (i - 1u));
            run = dock_app_running(kinds[i]);
        }
        if (hot) {
            draw_round_fill(sx + 6u, dy + 8u, DOCK_SLOT - 12u, dh - 16u, 14u,
                            THEME_HOVER, 170u);
        }
        col = (i == 0u) ? THEME_ACCENT : (hot ? THEME_ACCENT : THEME_FG);
        draw_icon(ix, iy, DOCK_ICON, apps[i], col);
        if (run) {
            u32 dot = sx + (DOCK_SLOT > 6u ? (DOCK_SLOT - 6u) / 2u : 0);
            draw_round_fill(dot, dy + dh - 11u, 6u, 6u, 3u, THEME_ACCENT, 255u);
        }
    }
}

static void draw_taskbar(void)
{
    draw_dock();
    draw_status();
}

static void draw_menu(void)
{
    const enum msg_id labels[MENU_COUNT] = {
        MSG_HOME_FILES, MSG_HOME_TERMINAL, MSG_HOME_SETTINGS,
        MSG_HOME_ABOUT, MSG_HOME_LOGOUT, MSG_HOME_POWER
    };
    const enum ui_icon icons[MENU_COUNT] = {
        UI_ICON_FILES, UI_ICON_TERM, UI_ICON_SETTINGS,
        UI_ICON_ABOUT, UI_ICON_LOGOUT, UI_ICON_POWER
    };
    u32 mx;
    u32 my;
    u32 mw;
    u32 mh;
    u32 i;

    menu_geom(&mx, &my, &mw, &mh);
    draw_round_fill_hard(mx, my, mw, mh, THEME_RAD_CARD, THEME_GLASS, 240u);
    for (i = 0; i < MENU_COUNT; ++i) {
        u32 ry = my + MENU_INSET + i * MENU_ROW;
        u32 text_y = ry + (MENU_ROW > FONT_HEIGHT ? (MENU_ROW - FONT_HEIGHT) / 2u : 0);
        u32 icon_y = ry + (MENU_ROW > 24u ? (MENU_ROW - 24u) / 2u : 0);
        bool hot = (s_hover == HIT_MENU + i);
        if (hot) {
            draw_round_fill(mx + MENU_INSET, ry + 2u, mw - MENU_INSET * 2u,
                            MENU_ROW - 4u, (MENU_ROW - 4u) / 2u, THEME_HOVER, 210u);
        }
        draw_icon(mx + 16, icon_y, 24, icons[i], THEME_FG);
        draw_text(mx + 50, text_y, i18n(labels[i]),
                  hot ? THEME_ACCENT : THEME_FG, 1);
    }
}

static void draw_ctx(void)
{
    const enum msg_id labels[CTX_COUNT] = {
        MSG_WP_DEFAULT, MSG_WP_ABSTRACT, MSG_HOME_SETTINGS,
        MSG_HOME_ABOUT, MSG_CTX_SHOW_ICONS
    };
    const enum ui_icon icons[CTX_COUNT] = {
        UI_ICON_ABOUT, UI_ICON_ABOUT, UI_ICON_SETTINGS,
        UI_ICON_ABOUT, UI_ICON_FILES
    };
    u32 mx;
    u32 my;
    u32 mw;
    u32 mh;
    u32 i;
    u32 rows_y;
    u32 desk_sel = wallpaper_desk_id();

    ctx_geom(&mx, &my, &mw, &mh);
    draw_round_fill_hard(mx, my, mw, mh, THEME_RAD_CARD, THEME_GLASS, 240u);
    draw_text(mx + 18u, my + MENU_INSET + 6u, i18n(MSG_CTX_TITLE),
              THEME_FG_DIM, 1);
    rows_y = my + MENU_INSET + CTX_HEADER;
    for (i = 0; i < CTX_COUNT; ++i) {
        u32 extra = (i >= 2u) ? CTX_SEP : 0;
        u32 ry = rows_y + extra + i * MENU_ROW;
        u32 text_y = ry + (MENU_ROW > FONT_HEIGHT ? (MENU_ROW - FONT_HEIGHT) / 2u : 0);
        u32 icon_y = ry + (MENU_ROW > 24u ? (MENU_ROW - 24u) / 2u : 0);
        bool hot = (s_hover == HIT_CTX + i);
        bool on = (i < 2u && desk_sel == i);
        const char *label;

        if (i == 2u) {
            fb_fill_rect(mx + 16u, ry - 4u, mw > 32u ? mw - 32u : mw, 1u,
                         THEME_BORDER.r, THEME_BORDER.g, THEME_BORDER.b);
        }
        if (hot) {
            draw_round_fill(mx + MENU_INSET, ry + 2u, mw - MENU_INSET * 2u,
                            MENU_ROW - 4u, (MENU_ROW - 4u) / 2u, THEME_HOVER, 210u);
        }
        if (i == 4u) {
            label = i18n(s_show_icons ? MSG_CTX_HIDE_ICONS : MSG_CTX_SHOW_ICONS);
        } else {
            label = i18n(labels[i]);
        }
        if (i < 2u) {
            draw_round_fill(mx + 18u, icon_y + 4u, 16u, 16u, 5u,
                            on ? THEME_ACCENT : THEME_BORDER, on ? 255u : 180u);
        } else {
            draw_icon(mx + 16u, icon_y, 24u, icons[i], THEME_FG);
        }
        draw_text(mx + 50u, text_y, label, (hot || on) ? THEME_ACCENT : THEME_FG, 1);
    }
}

static void clamp_win(struct window *w)
{
    u32 screen_w = fb_width();
    u32 screen_h = fb_height();

    if (w->x < 0) {
        w->x = 0;
    }
    if (w->y < 0) {
        w->y = 0;
    }
    if ((u32)w->x + 48u > screen_w) {
        w->x = (i32)(screen_w > 48u ? screen_w - 48u : 0);
    }
    if ((u32)w->y + TITLE_H + DOCK_H + DOCK_M + 8u > screen_h) {
        w->y = (i32)(screen_h - DOCK_H - DOCK_M - TITLE_H - 8u);
    }
}

static void paint_windows(void)
{
    u32 i;

    for (i = 0; i < s_zcount; ++i) {
        u32 id = s_z[i];
        struct window *w = &s_win[id];
        bool focused;
        bool close_hot;

        if (!w->used) {
            continue;
        }
        clamp_win(w);
        focused = (z_top() == (i32)id);
        close_hot = (s_hover == HIT_CLOSE + id);
        draw_window_frame((u32)w->x, (u32)w->y, w->w, w->h,
                          win_title(w->kind), focused, close_hot);
        draw_win_body(w, id);
    }
}

static void paint_desktop_base(void)
{
    u32 w = fb_width();

    draw_bg_atmosphere();
    fb_overlay(THEME_BG0.r, THEME_BG0.g, THEME_BG0.b, 18u);
    if (w > 720u) {
        draw_text(24u, 22u, name_os, DESK_MUTED, 1);
    }
    draw_text_centered(w / 2u, 18u, s_clock, DESK_INK, 2);
    draw_text_centered(w / 2u, 18u + FONT_HEIGHT * 2u + 4u, s_date, DESK_MUTED, 1);
    draw_desktop_icons();
}

static void desktop_redraw(void)
{
    fb_compose_begin();
    paint_desktop_base();
    paint_windows();
    draw_taskbar();
    if (s_menu) {
        draw_menu();
    }
    if (s_ctx) {
        draw_ctx();
    }
    if (s_spot) {
        fb_overlay(0, 0, 0, 110u);
        draw_spot();
    }
    if (s_spot_need_xfade && s_spot_fade_from != NULL) {
        s_spot_need_xfade = false;
        /* Blend the last on-screen frame into this scene so the dim does
         * not land as a single memcpy wipe on the live LFB. */
        ui_crossfade_from_n(s_spot_fade_from, 10u);
    }
    ui_comp_mark_full();
    ui_comp_present();
}

static void desktop_drag(u32 id, i32 old_x, i32 old_y)
{
    (void)id;
    (void)old_x;
    (void)old_y;

    /* The partial path repainted complete overlapping windows outside the
     * restored damage rect. Keep drag rendering coherent until it has true
     * per-window clipping. */
    desktop_redraw();
}

static void power_halt(void)
{
    cursor_hide();
    fb_compose_begin();
    draw_bg_atmosphere();
    draw_text(48, 120, name_os, THEME_INK, 2);
    draw_text(48, 180, i18n(MSG_POWER_MSG), THEME_INK, 1);
    cursor_invalidate();
    fb_compose_present();
    machine_power_off();
}

static void handle_click(u32 hit, bool dbl)
{
    if (s_spot) {
        if (hit >= HIT_SPOT_ROW && hit < HIT_SPOT_ROW + SPOT_MAX) {
            spot_activate(hit - HIT_SPOT_ROW);
            return;
        }
        if (hit == HIT_SPOT) {
            return;
        }
        spot_close();
        if (hit == HIT_DESKTOP || hit == HIT_BAR || hit == HIT_STATUS) {
            return;
        }
    }
    if (hit >= HIT_CTX && hit < HIT_CTX + CTX_COUNT) {
        u32 item = hit - HIT_CTX;
        s_ctx = false;
        if (item == 0u) {
            wallpaper_apply(DESK_WP_DEFAULT);
        } else if (item == 1u) {
            wallpaper_apply(DESK_WP_ABSTRACT);
        } else if (item == 2u) {
            (void)win_open(WIN_SETTINGS);
        } else if (item == 3u) {
            (void)win_open(WIN_ABOUT);
        } else if (item == 4u) {
            s_show_icons = !s_show_icons;
        }
        return;
    }
    s_ctx = false;
    if (hit >= HIT_MENU && hit < HIT_MENU + MENU_COUNT) {
        action_open(hit - HIT_MENU);
        return;
    }
    if (hit >= HIT_DOCK && hit < HIT_DOCK + ICON_COUNT) {
        if (dbl) {
            action_open(hit - HIT_DOCK);
        }
        return;
    }
    if (hit >= HIT_ICON && hit < HIT_ICON + ICON_COUNT) {
        if (dbl) {
            action_open(hit - HIT_ICON);
        }
        return;
    }
    if (hit >= HIT_CLOSE && hit < HIT_CLOSE + MAX_WIN) {
        win_close(hit - HIT_CLOSE);
        return;
    }
    if (hit >= HIT_TITLE && hit < HIT_TITLE + MAX_WIN) {
        u32 id = hit - HIT_TITLE;
        z_raise(id);
        s_drag = (i32)id;
        s_dx = mouse_x() - s_win[id].x;
        s_dy = mouse_y() - s_win[id].y;
        return;
    }
    if (hit >= HIT_TASK && hit < HIT_TASK + MAX_WIN) {
        z_raise(hit - HIT_TASK);
        s_menu = false;
        return;
    }
    if (hit >= HIT_BODY && hit < HIT_BODY + MAX_WIN) {
        z_raise(hit - HIT_BODY);
        return;
    }
    if (hit >= HIT_FILE && hit < HIT_FILE + MAX_FILES) {
        i32 top = z_top();
        if (top >= 0 && s_win[top].kind == WIN_FILES) {
            files_preview(&s_win[top], hit - HIT_FILE);
        }
        return;
    }
    if (hit >= HIT_LANG && hit < HIT_LANG + HIT_LANG_MAX) {
        u32 li = hit - HIT_LANG;

        if (li < i18n_lang_count()) {
            i18n_set_lang((enum lang_id)li);
            (void)persist_set_u32("lang", li);
            clock_refresh();
        }
        return;
    }
    if (hit == HIT_WP_0) {
        wallpaper_apply(DESK_WP_DEFAULT);
        return;
    }
    if (hit == HIT_WP_1) {
        wallpaper_apply(DESK_WP_ABSTRACT);
        return;
    }
    if (hit >= HIT_IC_0 && hit < HIT_IC_0 + ICON_STYLE_COUNT) {
        icon_set_style(hit - HIT_IC_0);
        return;
    }
    if (hit == HIT_POWER_OK) {
        power_halt();
    }
    if (hit == HIT_POWER_NO) {
        i32 top = z_top();
        if (top >= 0 && s_win[top].kind == WIN_POWER) {
            win_close((u32)top);
        }
        return;
    }
    if (hit == HIT_LAUNCHER) {
        s_ctx = false;
        spot_close();
        s_menu = !s_menu;
        return;
    }
    if (hit == HIT_DESKTOP || hit == HIT_BAR || hit == HIT_STATUS) {
        s_menu = false;
        s_ctx = false;
        spot_close();
        return;
    }
    s_ctx = false;
}

static void handle_right_click(u32 hit)
{
    if (s_spot) {
        spot_close();
        return;
    }
    if (hit == HIT_DESKTOP) {
        s_menu = false;
        s_ctx = true;
        s_ctx_x = mouse_x();
        s_ctx_y = mouse_y();
        return;
    }
    s_ctx = false;
}

static void handle_key(u32 key)
{
    i32 top = z_top();
    struct window *w;

    if (key == KEY_SPOTLIGHT) {
        if (s_spot) {
            spot_close();
        } else {
            spot_open();
        }
        return;
    }
    if (s_spot) {
        if (key == 27u) {
            spot_close();
            return;
        }
        if (key == KEY_DOWN) {
            if (s_spot_n > 0u) {
                s_spot_sel = (s_spot_sel + 1u) % s_spot_n;
            }
            return;
        }
        if (key == KEY_UP) {
            if (s_spot_n > 0u) {
                s_spot_sel = (s_spot_sel == 0u) ? (s_spot_n - 1u) : (s_spot_sel - 1u);
            }
            return;
        }
        if (key == '\n' || key == '\r') {
            spot_activate(s_spot_sel);
            return;
        }
        if (key == '\b' || key == 127u) {
            spot_backspace();
            return;
        }
        spot_append(key);
        return;
    }
    if (key == KEY_F1) {
        shell_run();
        return;
    }
    if (key == 27u) {
        if (s_ctx) {
            s_ctx = false;
            return;
        }
        if (s_menu) {
            s_menu = false;
            return;
        }
        if (top >= 0) {
            win_close((u32)top);
        }
        return;
    }
    if (top < 0) {
        return;
    }
    w = &s_win[top];
    if (w->kind != WIN_TERM) {
        return;
    }
    if (key == '\n' || key == '\r') {
        term_exec(w);
        if (w->term_len == 0xFFFFu) {
            win_close((u32)top);
        }
        return;
    }
    if (key == '\b' || key == 127u) {
        if (w->term_len > 0 && w->term_len < 0xFFFFu) {
            w->term_len--;
            w->term_input[w->term_len] = '\0';
        }
        return;
    }
    if (key < 32u || key_is_special(key) || key > 0x7Fu) {
        return;
    }
    if (w->term_len + 1u < TERM_COLS) {
        w->term_input[w->term_len] = (char)key;
        w->term_len++;
        w->term_input[w->term_len] = '\0';
    }
}

static bool desk_chrome_hit(u32 hit)
{
    if (hit == HIT_LAUNCHER) {
        return true;
    }
    if (hit >= HIT_DOCK && hit < HIT_DOCK + ICON_COUNT) {
        return true;
    }
    if (hit >= HIT_MENU && hit < HIT_MENU + MENU_COUNT) {
        return true;
    }
    if (hit == HIT_SPOT) {
        return true;
    }
    if (hit >= HIT_SPOT_ROW && hit < HIT_SPOT_ROW + SPOT_MAX) {
        return true;
    }
    return false;
}

static bool desktop_widgets(void)
{
    u32 dx;
    u32 dy;
    u32 dw;
    u32 dh;
    u32 i;
    bool need_full = false;
    bool full = ui_comp_is_full();
    bool chrome_changed = s_widget_last_hover != s_hover &&
                          (desk_chrome_hit(s_widget_last_hover) ||
                           desk_chrome_hit(s_hover));
    bool repaint_base = full || s_widget_repaint_base || chrome_changed;

    /* Partial dock paint is undimmed and punches a hole through Spotlight. */
    if (s_spot) {
        repaint_base = false;
    }

    dock_geom(&dx, &dy, &dw, &dh);
    if (repaint_base) {
        fb_compose_begin();
        draw_dock();
        ui_comp_damage(dx, dy, dw, dh);
        if (s_menu) {
            u32 mx;
            u32 my;
            u32 mw;
            u32 mh;

            menu_geom(&mx, &my, &mw, &mh);
            draw_menu();
            ui_comp_damage(mx, my, mw, mh);
        }
    } else {
        desktop_cursor_update(false);
    }
    ui_begin(mouse_x(), mouse_y(), mouse_buttons(), time_uptime_ms());
    ui_set_cursor_kind(desktop_cursor_kind(hit_test(mouse_x(), mouse_y())));
    for (i = 0; i < DOCK_APPS; ++i) {
        u32 sx = dock_slot_x(dx, i);

        if (ui_clicked(0xD00u + i, sx, dy, DOCK_SLOT, dh)) {
            if (i == 0u) {
                s_menu = !s_menu;
                s_ctx = false;
                if (s_menu) {
                    spot_close();
                }
                need_full = true;
            } else {
                s_menu = false;
                s_ctx = false;
                if (is_double_click(HIT_DOCK + (i - 1u))) {
                    action_open(i - 1u);
                    need_full = true;
                }
            }
        }
    }
    if (s_menu) {
        u32 mx;
        u32 my;
        u32 mw;
        u32 mh;

        menu_geom(&mx, &my, &mw, &mh);
        for (i = 0; i < MENU_COUNT; ++i) {
            u32 ry = my + MENU_INSET + i * MENU_ROW;

            if (ui_clicked(0xE00u + i, mx + MENU_INSET, ry + 2u,
                           mw > MENU_INSET * 2u ? mw - MENU_INSET * 2u : mw,
                           MENU_ROW > 4u ? MENU_ROW - 4u : MENU_ROW)) {
                action_open(i);
                need_full = true;
            }
        }
    }
    if (s_spot) {
        u32 sx;
        u32 sy;
        u32 sw;
        u32 sh;

        spot_geom(&sx, &sy, &sw, &sh);
        for (i = 0; i < s_spot_n; ++i) {
            u32 ry = sy + SPOT_BAR_H + i * SPOT_ROW_H;

            if (ui_clicked(0xF20u + i, sx + 10u, ry + 4u,
                           sw > 20u ? sw - 20u : sw,
                           SPOT_ROW_H > 8u ? SPOT_ROW_H - 8u : SPOT_ROW_H)) {
                spot_activate(i);
                need_full = true;
            }
        }
    }
    if (repaint_base) {
        desktop_cursor_update(true);
    }
    ui_end();
    s_widget_last_hover = s_hover;
    s_widget_repaint_base = ui_busy();
    return need_full;
}

void desktop_run(void)
{
    u32 i;
    bool dirty = true;
    i32 last_x = -1;
    i32 last_y = -1;
    u8 *from = NULL;

    for (i = 0; i < MAX_WIN; ++i) {
        s_win[i].used = false;
    }
    s_zcount = 0;
    s_menu = false;
    s_spot = false;
    s_spot_q[0] = '\0';
    s_spot_len = 0;
    s_spot_n = 0;
    s_spot_sel = 0;
    s_spot_need_xfade = false;
    s_ctx = false;
    s_ctx_x = 0;
    s_ctx_y = 0;
    s_show_icons = true;
    s_drag = -1;
    s_hover = HIT_NONE;
    s_logout = false;
    s_last_min = 0xFFFFFFFFu;
    s_last_day = 0xFFu;
    s_last_month = 0xFFu;
    s_last_year = 0xFFFFu;
    s_widget_last_hover = HIT_NONE;
    s_widget_repaint_base = false;
    s_cursor_valid = false;
    cursor_hide();
    if (fb_compose_ready()) {
        from = fb_layer_alloc();
        if (from != NULL) {
            fb_copy_front(from);
        }
    }
    files_reload();
    clock_refresh();
    mouse_set_bounds(fb_width(), fb_height());

    while (keyboard_has_char()) {
        (void)keyboard_get_codepoint();
    }
    while (mouse_has_event()) {
        (void)mouse_get_event();
    }

    fb_compose_begin();
    paint_desktop_base();
    paint_windows();
    draw_taskbar();
    if (from != NULL) {
        ui_crossfade_from(from);
        s_spot_fade_from = from;
    }
    desktop_cursor_update(true);
    ui_comp_mark_full();
    ui_comp_present();
    dirty = false;
    last_x = mouse_x();
    last_y = mouse_y();

    for (;;) {
        if (s_logout) {
            return;
        }

        if (keyboard_has_char()) {
            handle_key(keyboard_get_codepoint());
            dirty = true;
        } else if (mouse_has_event()) {
            struct mouse_event ev = mouse_get_event();
            u32 hit = hit_test(ev.x, ev.y);

            if (ev.kind == MOUSE_EV_MOVE) {
                if (s_drag >= 0 && s_win[s_drag].used) {
                    i32 ox = s_win[s_drag].x;
                    i32 oy = s_win[s_drag].y;
                    s_win[s_drag].x = ev.x - s_dx;
                    s_win[s_drag].y = ev.y - s_dy;
                    clamp_win(&s_win[s_drag]);
                    if (ox != s_win[s_drag].x || oy != s_win[s_drag].y) {
                        desktop_drag((u32)s_drag, ox, oy);
                    }
                    last_x = mouse_x();
                    last_y = mouse_y();
                } else if (hit != s_hover) {
                    bool was = desk_chrome_hit(s_hover);
                    bool now = desk_chrome_hit(hit);

                    s_hover = hit;
                    if (s_spot && hit >= HIT_SPOT_ROW &&
                        hit < HIT_SPOT_ROW + SPOT_MAX) {
                        s_spot_sel = hit - HIT_SPOT_ROW;
                        dirty = true;
                    } else if (!was || !now || s_spot) {
                        dirty = true;
                    }
                }
            } else if (ev.kind == MOUSE_EV_DOWN && ev.button == MOUSE_LEFT) {
                s_hover = hit;
                if (!desk_chrome_hit(hit)) {
                    handle_click(hit, is_double_click(hit));
                    dirty = true;
                }
            } else if (ev.kind == MOUSE_EV_DOWN && ev.button == MOUSE_RIGHT) {
                handle_right_click(hit);
                dirty = true;
            } else if (ev.kind == MOUSE_EV_UP && ev.button == MOUSE_LEFT) {
                s_drag = -1;
            }
        } else if (clock_changed()) {
            dirty = true;
        }

        if (!dirty && (mouse_x() != last_x || mouse_y() != last_y)) {
            u32 live = hit_test(mouse_x(), mouse_y());

            if (live != s_hover) {
                bool was = desk_chrome_hit(s_hover);
                bool now = desk_chrome_hit(live);

                s_hover = live;
                if (s_spot && live >= HIT_SPOT_ROW &&
                    live < HIT_SPOT_ROW + SPOT_MAX) {
                    s_spot_sel = live - HIT_SPOT_ROW;
                    dirty = true;
                } else if (!was || !now || s_spot) {
                    dirty = true;
                }
            }
        }

        if (dirty) {
            desktop_redraw();
            ui_invalidate();
            dirty = false;
        }
        if (desktop_widgets()) {
            dirty = true;
        }
        last_x = mouse_x();
        last_y = mouse_y();
        if (!dirty && !ui_busy() && !mouse_has_event()) {
            __asm__ volatile("hlt");
        }
    }
}
