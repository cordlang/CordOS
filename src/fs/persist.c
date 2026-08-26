#include "persist.h"
#include "i18n.h"
#include "icons.h"
#include "nosfs.h"
#include "serial.h"
#include "string.h"
#include "wallpaper.h"

#define PERSIST_FILE "config.txt"
#define PERSIST_BUF  256u

#define KEY_LANG       "lang"
#define KEY_LOGIN_WP   "login_wp"
#define KEY_DESK_WP    "desk_wp"
#define KEY_ICON_STYLE "icon_style"

static bool s_avail;
static int s_loading;
static u32 s_lang;
static u32 s_login_wp;
static u32 s_desk_wp;
static u32 s_icon_style;
static int s_have_lang;
static int s_have_wp;
static int s_have_desk;
static int s_have_ic;

static int key_eq(const char *a, const char *b)
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

static u32 parse_u32(const char *s)
{
    u32 v = 0;

    while (*s >= '0' && *s <= '9') {
        v = v * 10u + (u32)(*s - '0');
        ++s;
    }
    return v;
}

static void apply_line(const char *key, const char *val)
{
    u32 v = parse_u32(val);

    if (key_eq(key, KEY_LANG)) {
        s_lang = v;
        s_have_lang = 1;
    } else if (key_eq(key, KEY_LOGIN_WP)) {
        s_login_wp = v;
        s_have_wp = 1;
    } else if (key_eq(key, KEY_DESK_WP)) {
        s_desk_wp = v;
        s_have_desk = 1;
    } else if (key_eq(key, KEY_ICON_STYLE)) {
        s_icon_style = v;
        s_have_ic = 1;
    }
}

static void parse_buf(const char *buf, u32 n)
{
    char key[24];
    char val[16];
    u32 i = 0;
    u32 k;
    u32 v;

    while (i < n) {
        while (i < n && (buf[i] == '\n' || buf[i] == '\r' || buf[i] == ' ')) {
            ++i;
        }
        if (i >= n) {
            break;
        }
        k = 0;
        while (i < n && buf[i] != '=' && buf[i] != '\n' && buf[i] != '\r') {
            if (k + 1u < sizeof(key)) {
                key[k++] = buf[i];
            }
            ++i;
        }
        key[k] = '\0';
        if (i < n && buf[i] == '=') {
            ++i;
        }
        v = 0;
        while (i < n && buf[i] != '\n' && buf[i] != '\r') {
            if (v + 1u < sizeof(val)) {
                val[v++] = buf[i];
            }
            ++i;
        }
        val[v] = '\0';
        if (key[0] != '\0') {
            apply_line(key, val);
        }
    }
}

static void put_u32(char *dst, u32 *pos, u32 max, u32 value)
{
    char tmp[11];
    int n = 0;
    u32 v = value;

    if (v == 0) {
        tmp[n++] = '0';
    } else {
        while (v > 0 && n < (int)sizeof(tmp)) {
            tmp[n++] = (char)('0' + (v % 10u));
            v /= 10u;
        }
    }
    while (n > 0 && *pos + 1u < max) {
        dst[(*pos)++] = tmp[--n];
    }
}

static void put_str(char *dst, u32 *pos, u32 max, const char *s)
{
    while (*s != '\0' && *pos + 1u < max) {
        dst[(*pos)++] = *s++;
    }
}

static int flush_file(void)
{
    char buf[PERSIST_BUF];
    u32 pos = 0;

    put_str(buf, &pos, sizeof(buf), KEY_LANG);
    put_str(buf, &pos, sizeof(buf), "=");
    put_u32(buf, &pos, sizeof(buf), s_lang);
    put_str(buf, &pos, sizeof(buf), "\n");
    put_str(buf, &pos, sizeof(buf), KEY_LOGIN_WP);
    put_str(buf, &pos, sizeof(buf), "=");
    put_u32(buf, &pos, sizeof(buf), s_login_wp);
    put_str(buf, &pos, sizeof(buf), "\n");
    put_str(buf, &pos, sizeof(buf), KEY_DESK_WP);
    put_str(buf, &pos, sizeof(buf), "=");
    put_u32(buf, &pos, sizeof(buf), s_desk_wp);
    put_str(buf, &pos, sizeof(buf), "\n");
    put_str(buf, &pos, sizeof(buf), KEY_ICON_STYLE);
    put_str(buf, &pos, sizeof(buf), "=");
    put_u32(buf, &pos, sizeof(buf), s_icon_style);
    put_str(buf, &pos, sizeof(buf), "\n");
    buf[pos] = '\0';

    if (nosfs_disk_put(PERSIST_FILE, buf, pos) < 0) {
        return -1;
    }
    return 0;
}

void persist_init(void)
{
    char buf[PERSIST_BUF];
    int n;

    s_avail = false;
    s_have_lang = 0;
    s_have_wp = 0;
    s_have_desk = 0;
    s_have_ic = 0;
    s_lang = (u32)i18n_lang();
    s_login_wp = wallpaper_login_id();
    s_desk_wp = wallpaper_desk_id();
    s_icon_style = icon_style();

    if (!nosfs_disk_ready()) {
        serial_write("persist: no writable disk (live session; settings/users stay in RAM)\n");
        return;
    }

    s_avail = true;
    n = nosfs_disk_read(PERSIST_FILE, 0, buf, sizeof(buf) - 1u);
    if (n > 0) {
        buf[n] = '\0';
        parse_buf(buf, (u32)n);
    }

    s_loading = 1;
    if (s_have_lang) {
        i18n_set_lang((enum lang_id)s_lang);
    }
    if (s_have_wp) {
        wallpaper_set_login(s_login_wp);
    }
    if (s_have_desk) {
        wallpaper_set_desk(s_desk_wp);
    }
    if (s_have_ic) {
        icon_set_style(s_icon_style);
    }
    s_loading = 0;

    if (!s_have_lang || !s_have_wp || !s_have_desk || !s_have_ic) {
        s_lang = (u32)i18n_lang();
        s_login_wp = wallpaper_login_id();
        s_desk_wp = wallpaper_desk_id();
        s_icon_style = icon_style();
        (void)flush_file();
    }

    serial_write("persist: ready (");
    serial_write(PERSIST_FILE);
    serial_write(")\n");
}

bool persist_available(void)
{
    return s_avail;
}

int persist_set_u32(const char *key, u32 value)
{
    int dirty = 0;

    if (!s_avail || key == NULL) {
        return -1;
    }

    if (key_eq(key, KEY_LANG)) {
        if (!s_have_lang || s_lang != value) {
            s_lang = value;
            s_have_lang = 1;
            dirty = 1;
        }
    } else if (key_eq(key, KEY_LOGIN_WP)) {
        if (!s_have_wp || s_login_wp != value) {
            s_login_wp = value;
            s_have_wp = 1;
            dirty = 1;
        }
    } else if (key_eq(key, KEY_DESK_WP)) {
        if (!s_have_desk || s_desk_wp != value) {
            s_desk_wp = value;
            s_have_desk = 1;
            dirty = 1;
        }
    } else if (key_eq(key, KEY_ICON_STYLE)) {
        if (!s_have_ic || s_icon_style != value) {
            s_icon_style = value;
            s_have_ic = 1;
            dirty = 1;
        }
    } else {
        return -1;
    }

    if (s_loading || !dirty) {
        return 0;
    }
    return flush_file();
}

int persist_get_u32(const char *key, u32 *value)
{
    if (!s_avail || key == NULL || value == NULL) {
        return -1;
    }
    if (key_eq(key, KEY_LANG)) {
        if (!s_have_lang) {
            return -1;
        }
        *value = s_lang;
        return 0;
    }
    if (key_eq(key, KEY_LOGIN_WP)) {
        if (!s_have_wp) {
            return -1;
        }
        *value = s_login_wp;
        return 0;
    }
    if (key_eq(key, KEY_DESK_WP)) {
        if (!s_have_desk) {
            return -1;
        }
        *value = s_desk_wp;
        return 0;
    }
    if (key_eq(key, KEY_ICON_STYLE)) {
        if (!s_have_ic) {
            return -1;
        }
        *value = s_icon_style;
        return 0;
    }
    return -1;
}
