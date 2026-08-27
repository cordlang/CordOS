#include "userdb.h"
#include "nosfs.h"
#include "serial.h"
#include "string.h"

#define USERS_FILE "users.db"

static struct user_rec s_users[USERDB_MAX];
static u32 s_count;
static u32 s_cur;

#define PASS_TAG     "h1"
#define PASS_TAG_LEN 2u
#define PASS_HEX_LEN 16u
#define PASS_STORED  19u

static u64 fnv1a64_pass(const char *slug, const char *pass)
{
    static const char pepper[] = "CordOS.userdb.v1";
    u64 h = 14695981039346656037ull;
    const char *p;

    for (p = slug; p != NULL && *p != '\0'; ++p) {
        h ^= (u8)*p;
        h *= 1099511628211ull;
    }
    h ^= 0xFFu;
    h *= 1099511628211ull;
    for (p = pass; p != NULL && *p != '\0'; ++p) {
        h ^= (u8)*p;
        h *= 1099511628211ull;
    }
    for (p = pepper; *p != '\0'; ++p) {
        h ^= (u8)*p;
        h *= 1099511628211ull;
    }
    return h;
}

static void pass_store(char *dst, u32 max, const char *pass, const char *slug)
{
    static const char hex[] = "0123456789abcdef";
    u64 h = fnv1a64_pass(slug, pass);
    u32 i;

    if (dst == NULL || max < PASS_STORED) {
        return;
    }
    dst[0] = PASS_TAG[0];
    dst[1] = PASS_TAG[1];
    for (i = 0; i < PASS_HEX_LEN; ++i) {
        dst[PASS_TAG_LEN + i] =
            hex[(h >> (60u - 4u * i)) & 0xFu];
    }
    dst[PASS_TAG_LEN + PASS_HEX_LEN] = '\0';
}

static int pass_is_hash(const char *s)
{
    u32 i;

    if (s == NULL || s[0] != PASS_TAG[0] || s[1] != PASS_TAG[1]) {
        return 0;
    }
    for (i = 0; i < PASS_HEX_LEN; ++i) {
        char c = s[PASS_TAG_LEN + i];

        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
            return 0;
        }
    }
    return s[PASS_TAG_LEN + PASS_HEX_LEN] == '\0';
}

static int ct_eq_n(const char *a, const char *b, u32 n)
{
    u32 i;
    u8 d = 0;

    if (a == NULL || b == NULL) {
        return 0;
    }
    for (i = 0; i < n; ++i) {
        d |= (u8)a[i] ^ (u8)b[i];
    }
    return d == 0;
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

static void copy_z(char *dst, u32 max, const char *src)
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

static void make_slug(char *out, u32 max, const char *name)
{
    u32 i;
    u32 o = 0;

    if (out == NULL || max == 0) {
        return;
    }
    for (i = 0; name != NULL && name[i] != '\0' && o + 1u < max; ++i) {
        char c = name[i];

        if (c >= 'A' && c <= 'Z') {
            c = (char)(c + 32);
        }
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            out[o++] = c;
        }
    }
    if (o == 0) {
        copy_z(out, max, "user");
        return;
    }
    out[o] = '\0';
}

static void uniquify_slug(char *slug, u32 max)
{
    char base[USERDB_SLUG_MAX];
    u32 n;
    u32 k;

    copy_z(base, sizeof(base), slug);
    for (n = 0; n < 20u; ++n) {
        int clash = 0;
        for (k = 0; k < s_count; ++k) {
            if (streq(s_users[k].slug, slug)) {
                clash = 1;
                break;
            }
        }
        if (!clash) {
            return;
        }
        copy_z(slug, max, base);
        {
            u32 len = 0;
            while (slug[len] != '\0') {
                ++len;
            }
            if (len + 2u < max) {
                slug[len] = (char)('0' + (n % 10u));
                slug[len + 1u] = '\0';
            }
        }
    }
}

static int save_db(void)
{
    char buf[USERDB_MAX * (USERDB_NAME_MAX + USERDB_PASS_MAX + 4u)];
    u32 i;
    u32 o = 0;

    memset(buf, 0, sizeof(buf));
    for (i = 0; i < s_count; ++i) {
        const char *p;
        p = s_users[i].name;
        while (*p != '\0' && o + 3u < sizeof(buf)) {
            buf[o++] = *p++;
        }
        buf[o++] = '\n';
        p = s_users[i].pass;
        while (*p != '\0' && o + 3u < sizeof(buf)) {
            buf[o++] = *p++;
        }
        buf[o++] = '\n';
    }
    if (!nosfs_disk_ready()) {
        return -1;
    }
    return nosfs_disk_put(USERS_FILE, buf, o);
}

static void create_home(const char *slug, const char *name)
{
    char fname[32];
    char body[96];
    u32 o;
    const char *p;

    if (!nosfs_disk_ready() || slug == NULL || slug[0] == '\0') {
        return;
    }

    copy_z(fname, sizeof(fname), slug);
    o = 0;
    while (fname[o] != '\0') {
        ++o;
    }
    copy_z(fname + o, sizeof(fname) - o, "_home.txt");

    o = 0;
    p = name != NULL ? name : slug;
    while (*p != '\0' && o + 16u < sizeof(body)) {
        body[o++] = *p++;
    }
    body[o++] = '\n';
    p = "/home/";
    while (*p != '\0' && o + 2u < sizeof(body)) {
        body[o++] = *p++;
    }
    p = slug;
    while (*p != '\0' && o + 2u < sizeof(body)) {
        body[o++] = *p++;
    }
    body[o++] = '\n';
    body[o] = '\0';
    (void)nosfs_disk_put(fname, body, o);

    copy_z(fname, sizeof(fname), slug);
    o = 0;
    while (fname[o] != '\0') {
        ++o;
    }
    copy_z(fname + o, sizeof(fname) - o, "_docs.txt");
    copy_z(body, sizeof(body), "docs\n");
    (void)nosfs_disk_put(fname, body, 5u);
}

void userdb_load(void)
{
    char buf[1024];
    int n;
    u32 i;
    int want_name = 1;
    char line[USERDB_NAME_MAX];
    u32 lp;

    s_count = 0;
    s_cur = 0;
    memset(s_users, 0, sizeof(s_users));

    if (!nosfs_disk_ready()) {
        serial_write("userdb: no writable disk (accounts will not survive reboot)\n");
        return;
    }
    n = nosfs_disk_read(USERS_FILE, 0, buf, sizeof(buf) - 1);
    if (n <= 0) {
        serial_write("userdb: empty\n");
        return;
    }
    buf[n] = '\0';
    lp = 0;
    line[0] = '\0';
    {
        int migrated = 0;

        for (i = 0; i <= (u32)n && s_count < USERDB_MAX; ++i) {
            char c = (i == (u32)n) ? '\n' : buf[i];

            if (c == '\n' || c == '\r') {
                line[lp] = '\0';
                if (lp == 0) {
                    continue;
                }
                if (want_name) {
                    copy_z(s_users[s_count].name, USERDB_NAME_MAX, line);
                    make_slug(s_users[s_count].slug, USERDB_SLUG_MAX, line);
                    want_name = 0;
                } else {
                    copy_z(s_users[s_count].pass, USERDB_PASS_MAX, line);
                    if (!pass_is_hash(s_users[s_count].pass)) {
                        char plain[USERDB_PASS_MAX];

                        copy_z(plain, sizeof(plain), s_users[s_count].pass);
                        pass_store(s_users[s_count].pass, USERDB_PASS_MAX, plain,
                                   s_users[s_count].slug);
                        migrated = 1;
                    }
                    want_name = 1;
                    s_count++;
                }
                lp = 0;
                continue;
            }
            if (lp + 1u < sizeof(line)) {
                line[lp++] = c;
            }
        }
        if (migrated) {
            (void)save_db();
        }
    }
    serial_write("userdb: loaded ");
    serial_print_u32(s_count);
    serial_write("\n");
}

u32 userdb_count(void)
{
    return s_count;
}

const struct user_rec *userdb_get(u32 index)
{
    if (index >= s_count) {
        return NULL;
    }
    return &s_users[index];
}

const struct user_rec *userdb_current(void)
{
    return userdb_get(s_cur);
}

u32 userdb_current_index(void)
{
    return s_cur;
}

void userdb_select(u32 index)
{
    if (s_count == 0) {
        s_cur = 0;
        return;
    }
    s_cur = index % s_count;
}

void userdb_select_next(void)
{
    if (s_count == 0) {
        return;
    }
    s_cur = (s_cur + 1u) % s_count;
}

int userdb_add(const char *name, const char *pass)
{
    struct user_rec *u;

    if (name == NULL || name[0] == '\0' || pass == NULL || pass[0] == '\0') {
        return -1;
    }
    if (s_count >= USERDB_MAX) {
        return -1;
    }
    u = &s_users[s_count];
    memset(u, 0, sizeof(*u));
    copy_z(u->name, USERDB_NAME_MAX, name);
    make_slug(u->slug, USERDB_SLUG_MAX, name);
    uniquify_slug(u->slug, USERDB_SLUG_MAX);
    pass_store(u->pass, USERDB_PASS_MAX, pass, u->slug);
    s_count++;
    s_cur = s_count - 1u;
    create_home(u->slug, u->name);
    if (save_db() < 0) {
        serial_write("userdb: ram only (no disk)\n");
    } else {
        serial_write("userdb: saved ");
        serial_write(u->slug);
        serial_write("\n");
    }
    return 0;
}

int userdb_auth(const char *name, const char *pass)
{
    u32 i;
    char hashed[USERDB_PASS_MAX];

    if (name == NULL || pass == NULL) {
        return 0;
    }
    for (i = 0; i < s_count; ++i) {
        if (!(streq(name, s_users[i].name) || streq(name, s_users[i].slug))) {
            continue;
        }
        if (pass_is_hash(s_users[i].pass)) {
            pass_store(hashed, sizeof(hashed), pass, s_users[i].slug);
            if (ct_eq_n(hashed, s_users[i].pass, PASS_STORED - 1u)) {
                s_cur = i;
                return 1;
            }
        } else {
            char plain[USERDB_PASS_MAX];

            memset(plain, 0, sizeof(plain));
            copy_z(plain, sizeof(plain), pass);
            if (ct_eq_n(plain, s_users[i].pass, USERDB_PASS_MAX)) {
                pass_store(s_users[i].pass, USERDB_PASS_MAX, pass,
                           s_users[i].slug);
                (void)save_db();
                s_cur = i;
                return 1;
            }
        }
    }
    return 0;
}

int userdb_auth_current(const char *pass)
{
    const struct user_rec *u = userdb_current();
    char hashed[USERDB_PASS_MAX];

    if (u == NULL || pass == NULL) {
        return 0;
    }
    if (pass_is_hash(u->pass)) {
        pass_store(hashed, sizeof(hashed), pass, u->slug);
        return ct_eq_n(hashed, u->pass, PASS_STORED - 1u);
    }
    {
        char plain[USERDB_PASS_MAX];

        memset(plain, 0, sizeof(plain));
        copy_z(plain, sizeof(plain), pass);
        return ct_eq_n(plain, u->pass, USERDB_PASS_MAX);
    }
}
