#include "desktop_priv.h"
#include "string.h"

i32 z_top(void)
{
    if (s_zcount == 0) {
        return -1;
    }
    return (i32)s_z[s_zcount - 1u];
}

void z_raise(u32 id)
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

void win_close(u32 id)
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

void files_reload(void)
{
    s_files.count = 0;
    (void)vfs_list("/", file_cb, &s_files);
}

void files_preview(struct window *w, u32 index)
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

void term_exec(struct window *w)
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

i32 win_open(enum win_kind kind)
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
    w->w = kind == WIN_POWER ? 440u : (kind == WIN_ACTIVITY ? 1040u : 760u);
    if (kind == WIN_POWER) {
        w->h = 220u;
    } else if (kind == WIN_SETTINGS) {
        w->h = 620u;
    } else if (kind == WIN_ACTIVITY) {
        w->h = 640u;
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
    w->file_sel = kind == WIN_ACTIVITY ? 0u : 0xFFFFFFFFu;
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

void action_open(u32 item)
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
        (void)win_open(WIN_ACTIVITY);
    } else if (item == 5) {
        s_logout = true;
    } else if (item == 6) {
        (void)win_open(WIN_POWER);
    }
}

void settings_layout(const struct window *w, u32 *lang_y, u32 *wp_y,
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

void settings_lang_btn(u32 bx, u32 lang_y, u32 i, u32 *x, u32 *y)
{
    *x = bx + 24u + (i % 2u) * 156u;
    *y = lang_y + (i / 2u) * 48u;
}

void act_layout(const struct window *w, u32 *lx, u32 *ly, u32 *lw, u32 *rh,
                u32 *rgap, u32 *dx, u32 *dy, u32 *dw, u32 *dh)
{
    u32 pad = 16u;
    u32 gap = 16u;
    u32 bx = (u32)w->x;
    u32 by = (u32)w->y;
    u32 body_y = by + TITLE_H + 14u;
    u32 body_h = w->h > TITLE_H + 32u ? w->h - TITLE_H - 28u : 80u;
    u32 list_w = 250u;
    u32 row_gap = 8u;
    u32 row_h;

    if (w->w < pad * 2u + gap + 420u) {
        list_w = w->w / 3u;
        if (list_w < 168u) {
            list_w = 168u;
        }
    }
    if (pad + list_w + gap + 220u + pad > w->w) {
        list_w = w->w > pad * 2u + gap + 220u
                     ? w->w - pad * 2u - gap - 220u
                     : w->w / 3u;
    }
    row_h = (body_h - (ACT_N - 1u) * row_gap) / ACT_N;
    if (row_h < 44u) {
        row_h = 44u;
    }
    *lx = bx + pad;
    *ly = body_y;
    *lw = list_w;
    *rh = row_h;
    *rgap = row_gap;
    *dx = *lx + *lw + gap;
    *dy = body_y;
    *dw = bx + w->w > *dx + pad ? bx + w->w - *dx - pad : 120u;
    *dh = body_h;
}

static void cat(char *dst, u32 max, const char *src)
{
    u32 n = 0;

    if (dst == NULL || max == 0u) {
        return;
    }
    while (dst[n] != '\0' && n + 1u < max) {
        n++;
    }
    if (src == NULL) {
        return;
    }
    while (*src != '\0' && n + 1u < max) {
        dst[n++] = *src++;
    }
    dst[n] = '\0';
}

static void draw_segments(u32 x, u32 y, u32 w, u32 h, u32 pct, struct rgb fill)
{
    u32 filled;

    if (w < 12u || h < 4u) {
        return;
    }
    if (pct > 100u) {
        pct = 100u;
    }
    draw_round_fill_hard(x, y, w, h, h / 2u, THEME_GRID, 255u);
    if (pct == 0u) {
        return;
    }
    filled = (pct * w) / 100u;
    if (filled < h) {
        filled = h;
    }
    if (filled > w) {
        filled = w;
    }
    draw_round_fill_hard(x, y, filled, h, h / 2u, fill, 255u);
}

static void draw_spark(u32 x, u32 y, u32 w, u32 h, const u8 *hist, struct rgb col)
{
    u32 i;
    u32 cw;

    if (w < SYSMON_HIST || h < 8u || hist == NULL) {
        return;
    }
    draw_round_fill_hard(x, y, w, h, 5u, THEME_BG2, 255u);
    cw = w / SYSMON_HIST;
    if (cw < 1u) {
        cw = 1u;
    }
    for (i = 0; i < SYSMON_HIST; i++) {
        u32 bh = ((u32)hist[i] * (h - 4u)) / 100u;
        u32 bx = x + i * cw;
        u32 by;

        if (bh == 0u) {
            continue;
        }
        by = y + h - 2u - bh;
        fb_blend_rect(bx, by, cw > 1u ? cw - 1u : cw, bh,
                      col.r, col.g, col.b, 120u);
        fb_fill_rect(bx, by, cw > 1u ? cw - 1u : cw, 1u, col.r, col.g, col.b);
    }
}

static void draw_area(u32 x, u32 y, u32 w, u32 h, const u8 *hist, struct rgb col)
{
    u32 i;
    u32 cw;
    u32 inset = 14u;
    u32 base;

    if (w < 48u || h < 48u || hist == NULL) {
        return;
    }
    draw_round_fill_hard(x, y, w, h, 12u, THEME_BG2, 255u);
    fb_blend_rect(x + 1u, y + 1u, w > 2u ? w - 2u : w, 1u,
                  col.r, col.g, col.b, 170u);
    for (i = 1u; i < 4u; i++) {
        u32 gy = y + (h * i) / 4u;

        fb_blend_rect(x + inset, gy, w > inset * 2u ? w - inset * 2u : w, 1u,
                      THEME_BORDER.r,
                      THEME_BORDER.g, THEME_BORDER.b, 70u);
    }
    cw = (w > inset * 2u) ? (w - inset * 2u) / SYSMON_HIST : 1u;
    if (cw < 1u) {
        cw = 1u;
    }
    base = y + h - 8u;
    for (i = 0; i < SYSMON_HIST; i++) {
        u32 bh = ((u32)hist[i] * (h - 16u)) / 100u;
        u32 bx = x + inset + i * cw;
        u32 bw = cw > 1u ? cw - 1u : cw;
        u32 by;

        if (bx + bw > x + w - inset) {
            if (x + w <= bx + inset) {
                break;
            }
            bw = x + w - inset - bx;
        }
        if (bh == 0u) {
            continue;
        }
        by = base - bh;
        fb_blend_rect(bx, by, bw, bh, col.r, col.g, col.b, 105u);
        fb_fill_rect(bx, by, bw, 2u, col.r, col.g, col.b);
    }
    {
        u32 bh = ((u32)hist[SYSMON_HIST - 1u] * (h - 16u)) / 100u;
        u32 px = x + inset + (SYSMON_HIST - 1u) * cw;
        u32 py = base - bh;

        if (px + 2u < x + w) {
            fb_fill_rect(px, y + 8u, 2u, base - y - 8u, col.r, col.g, col.b);
            draw_round_fill_hard(px > 2u ? px - 2u : 0u, py > 2u ? py - 2u : 0u,
                                 6u, 6u, 3u, col, 255u);
        }
    }
}

static void draw_pill(u32 x, u32 y, u32 w, u32 h, const char *text, struct rgb dot)
{
    u32 ty;

    if (w < 20u || h < 16u) {
        return;
    }
    draw_round_fill_hard(x, y, w, h, h / 2u, THEME_BG2, 255u);
    fb_blend_rect(x + 1u, y + 1u, w > 2u ? w - 2u : w, 1u,
                  dot.r, dot.g, dot.b, 130u);
    draw_round_fill_hard(x + 8u, y + (h / 2u) - 3u, 6u, 6u, 3u, dot, 255u);
    ty = y + (h > FONT_HEIGHT ? (h - FONT_HEIGHT) / 2u : 0u);
    draw_text_clip(x + 22u, ty, x + w - 8u, text, THEME_FG, 1);
}

static void draw_kv(u32 x, u32 y, u32 w, u32 h, const char *k, const char *v,
                    struct rgb col)
{
    if (w < 24u || h < FONT_LINE) {
        return;
    }
    draw_round_fill_hard(x, y, w, h, 10u, THEME_BG2, 255u);
    fb_fill_rect(x + 10u, y + 8u, 3u, h > 16u ? h - 16u : h, col.r, col.g, col.b);
    draw_text_clip(x + 20u, y + 8u, x + w - 8u, k, THEME_FG_DIM, 1);
    draw_text_clip(x + 20u, y + h - FONT_HEIGHT - 6u, x + w - 8u, v, THEME_FG, 1);
}

static const char *task_state_txt(enum task_state st)
{
    if (st == TASK_RUNNING) {
        return i18n(MSG_ACT_RUN);
    }
    if (st == TASK_BLOCKED) {
        return i18n(MSG_ACT_BLOCK);
    }
    return i18n(MSG_ACT_READY);
}

static void draw_task_rows(u32 x, u32 y, u32 w, u32 max_rows, struct rgb col)
{
    u32 i;
    u32 shown = 0;
    u32 row_h = FONT_LINE + 2u;

    draw_text(x, y, i18n(MSG_ACT_KTASKS), THEME_FG_DIM, 1);
    fb_blend_rect(x, y + FONT_LINE - 3u, w, 1u, col.r, col.g, col.b, 120u);
    y += FONT_LINE + 4u;
    for (i = 0; i < TASK_MAX_OS && shown < max_rows; i++) {
        const char *name;
        u32 ny;

        if (task_table_os[i].state == TASK_DEAD) {
            continue;
        }
        name = task_table_os[i].name;
        if (name == NULL || name[0] == '\0') {
            name = "?";
        }
        ny = y + shown * row_h;
        draw_round_fill_hard(x, ny, w, FONT_LINE - 2u, 8u, THEME_BG2, 255u);
        draw_round_fill_hard(x + 10u, ny + 9u, 6u, 6u, 3u, col, 255u);
        draw_text_clip(x + 24u, ny + 2u, x + w / 2u, name, THEME_FG, 1);
        draw_text_clip(x + w / 2u, ny + 2u, x + w - 10u,
                       task_state_txt(task_table_os[i].state), THEME_FG_DIM, 1);
        shown++;
    }
}

static struct rgb act_color(u32 i)
{
    static const struct rgb cols[ACT_N] = {
        { 0x55, 0xDE, 0xB5 },
        { 0x4B, 0x9A, 0xFF },
        { 0xC4, 0xA2, 0x65 },
        { 0xEE, 0x77, 0x78 },
        { 0x6B, 0xC9, 0x8A },
        { 0xE8, 0xA2, 0x3A }
    };

    return i < ACT_N ? cols[i] : THEME_ACCENT;
}

static void draw_monitor(struct window *win)
{
    const struct sysmon_snap *s;
    u32 lx;
    u32 ly;
    u32 lw;
    u32 rh;
    u32 rgap;
    u32 dx;
    u32 dy;
    u32 dw;
    u32 dh;
    u32 i;
    u32 sel;
    struct rgb dim = THEME_FG_DIM;
    char gpu_sub[36];
    char disk_sub[28];
    char sys_sub[36];
    char core_txt[28];
    char hz_txt[20];
    char heap_txt[24];
    char free_txt[24];
    char ram_pct_txt[8];
    char bat_txt[16];
    char nbuf[12];
    u8 fb_hist[SYSMON_HIST];
    const char *title[ACT_N];
    const char *hero[ACT_N];
    const char *sub[ACT_N];
    const u8 *hist[ACT_N];
    u32 pct[ACT_N];

    s = sysmon_get();
    act_layout(win, &lx, &ly, &lw, &rh, &rgap, &dx, &dy, &dw, &dh);
    sel = win->file_sel;
    if (sel >= ACT_N) {
        sel = 0u;
        win->file_sel = 0u;
    }

    gpu_sub[0] = '\0';
    u32_to_dec(s->fb_bpp, nbuf, sizeof(nbuf));
    cat(gpu_sub, sizeof(gpu_sub), nbuf);
    cat(gpu_sub, sizeof(gpu_sub), " bpp  ");
    u32_to_dec(s->fb_mib, nbuf, sizeof(nbuf));
    cat(gpu_sub, sizeof(gpu_sub), nbuf);
    cat(gpu_sub, sizeof(gpu_sub), " MiB");

    disk_sub[0] = '\0';
    u32_to_dec(s->blk_n, nbuf, sizeof(nbuf));
    cat(disk_sub, sizeof(disk_sub), nbuf);
    cat(disk_sub, sizeof(disk_sub), " ");
    cat(disk_sub, sizeof(disk_sub), i18n(MSG_ACT_BLK));

    sys_sub[0] = '\0';
    u32_to_dec(s->tasks, nbuf, sizeof(nbuf));
    cat(sys_sub, sizeof(sys_sub), nbuf);
    cat(sys_sub, sizeof(sys_sub), " ");
    cat(sys_sub, sizeof(sys_sub), i18n(MSG_ACT_TASKS));
    cat(sys_sub, sizeof(sys_sub), " · ");
    u32_to_dec(s->pci_n, nbuf, sizeof(nbuf));
    cat(sys_sub, sizeof(sys_sub), nbuf);
    cat(sys_sub, sizeof(sys_sub), " PCI");

    core_txt[0] = '\0';
    u32_to_dec(s->cpus, nbuf, sizeof(nbuf));
    cat(core_txt, sizeof(core_txt), nbuf);
    cat(core_txt, sizeof(core_txt), " ");
    cat(core_txt, sizeof(core_txt), i18n(MSG_ACT_CORE));

    hz_txt[0] = '\0';
    u32_to_dec(s->ticks_hz, nbuf, sizeof(nbuf));
    cat(hz_txt, sizeof(hz_txt), nbuf);
    cat(hz_txt, sizeof(hz_txt), " Hz");

    heap_txt[0] = '\0';
    u32_to_dec(s->heap_used_kib, nbuf, sizeof(nbuf));
    cat(heap_txt, sizeof(heap_txt), nbuf);
    cat(heap_txt, sizeof(heap_txt), " KiB");

    free_txt[0] = '\0';
    u32_to_dec(s->heap_free_kib, nbuf, sizeof(nbuf));
    cat(free_txt, sizeof(free_txt), nbuf);
    cat(free_txt, sizeof(free_txt), " KiB");

    ram_pct_txt[0] = '\0';
    u32_to_dec(s->ram_pct, nbuf, sizeof(nbuf));
    cat(ram_pct_txt, sizeof(ram_pct_txt), nbuf);
    cat(ram_pct_txt, sizeof(ram_pct_txt), "%");

    bat_txt[0] = '\0';
    if (s->ac) {
        cat(bat_txt, sizeof(bat_txt), i18n(MSG_ACT_AC));
    } else {
        u32_to_dec(s->bat_pct, nbuf, sizeof(nbuf));
        cat(bat_txt, sizeof(bat_txt), nbuf);
        cat(bat_txt, sizeof(bat_txt), "%");
    }

    for (i = 0; i < SYSMON_HIST; i++) {
        fb_hist[i] = fb_available() ? 88u : 8u;
    }

    title[0] = i18n(MSG_ACT_CPU);
    hero[0] = s->cpu_txt;
    sub[0] = i18n(s->preempt ? MSG_ACT_PREEMPT_ON : MSG_ACT_PREEMPT_OFF);
    hist[0] = s->cpu_hist;
    pct[0] = s->cpu_pct;

    title[1] = i18n(MSG_ACT_RAM);
    hero[1] = s->ram_txt;
    sub[1] = i18n(MSG_ACT_HEAP);
    hist[1] = s->ram_hist;
    pct[1] = s->ram_pct;

    title[2] = i18n(MSG_ACT_GPU);
    hero[2] = s->fb_txt;
    sub[2] = gpu_sub;
    hist[2] = fb_hist;
    pct[2] = fb_available() ? 100u : 0u;

    title[3] = i18n(MSG_ACT_NET);
    if (!s->net_nic) {
        hero[3] = i18n(MSG_ACT_NO_NIC);
        sub[3] = i18n(MSG_ACT_DOWN);
    } else if (s->net_cfg && s->ip_txt[0] != '\0') {
        hero[3] = s->ip_txt;
        sub[3] = s->net_link ? s->nic_txt : i18n(MSG_ACT_DOWN);
    } else {
        hero[3] = s->nic_txt[0] != '\0' ? s->nic_txt : i18n(MSG_ACT_DOWN);
        sub[3] = s->net_link ? i18n(MSG_ACT_LINK) : i18n(MSG_ACT_DOWN);
    }
    hist[3] = s->net_hist;
    pct[3] = s->net_link ? 100u : (s->net_nic ? 12u : 0u);

    title[4] = i18n(MSG_ACT_DISK);
    hero[4] = s->persist ? i18n(MSG_ACT_PERSIST) : i18n(MSG_ACT_INITRD);
    sub[4] = disk_sub;
    hist[4] = s->disk_hist;
    pct[4] = s->persist ? 100u : 20u;

    title[5] = i18n(MSG_ACT_SYS);
    hero[5] = s->up_txt;
    sub[5] = sys_sub;
    hist[5] = s->cpu_hist;
    pct[5] = s->cpu_pct;

    for (i = 0; i < ACT_N; i++) {
        u32 rx = lx;
        u32 ry = ly + i * (rh + rgap);
        u32 spark_w = lw > 116u ? 66u : 42u;
        u32 spark_x;
        u32 tx;
        u32 ty;
        bool on = (sel == i) || (s_hover == HIT_ACT + i);
        struct rgb col = act_color(i);

        if (lw > spark_w + 36u) {
            spark_x = rx + lw - 10u - spark_w;
        } else {
            spark_w = 0u;
            spark_x = rx + lw;
        }
        draw_round_fill_hard(rx, ry, lw, rh, 12u,
                             sel == i ? THEME_BG2 : (on ? THEME_TITLE : THEME_FIELD),
                             255u);
        if (sel == i) {
            fb_fill_rect(rx, ry + 10u, 4u, rh > 20u ? rh - 20u : rh / 2u,
                         col.r, col.g, col.b);
            fb_blend_rect(rx + 8u, ry + 1u, lw > 16u ? lw - 16u : lw, 1u,
                          col.r, col.g, col.b, 180u);
        } else if (on) {
            fb_blend_rect(rx + 10u, ry + 1u, lw > 20u ? lw - 20u : lw, 1u,
                          col.r, col.g, col.b, 90u);
        }
        draw_round_fill_hard(rx + 14u, ry + rh / 2u - 4u, 8u, 8u, 4u, col, 255u);
        tx = rx + 30u;
        ty = ry + (rh > 56u ? 10u : 4u);
        if (rh >= 58u) {
            draw_text_clip(tx, ty, spark_x - 8u, title[i], dim, 1);
            draw_text_clip(tx, ty + FONT_LINE - 10u, spark_x - 8u, hero[i],
                           sel == i ? col : THEME_FG, 1);
        } else {
            draw_text_clip(tx, ry + (rh > FONT_HEIGHT ? (rh - FONT_HEIGHT) / 2u : 0u),
                           spark_x - 8u, hero[i], sel == i ? col : THEME_FG, 1);
        }
        if (spark_w >= SYSMON_HIST && rh > 28u) {
            u32 sh = rh > 40u ? rh - 24u : 16u;

            draw_spark(spark_x, ry + (rh - sh) / 2u, spark_w, sh, hist[i], col);
        }
    }

    {
        struct rgb col = act_color(sel);
        u32 px = dx + 18u;
        u32 py = dy + 14u;
        u32 ts = dw > 460u ? 2u : 1u;
        u32 hero_w = draw_text_width(hero[sel], ts);
        u32 hero_x = px + 38u;
        u32 hero_y = py + FONT_LINE - 4u;
        u32 hero_h = ts == 2u ? FONT_TITLE_H : FONT_HEIGHT;
        u32 sub_y = hero_y + hero_h + 2u;
        u32 meter_y = sub_y + FONT_LINE - 6u;
        u32 chart_y;
        u32 chart_h;
        u32 stat_y;
        u32 stat_h = 62u;
        u32 stat_w;
        u32 stat_gap = 8u;
        u32 task_y;
        u32 live_w;
        u32 span_w;
        u32 chip_x;

        draw_round_fill_hard(dx, dy, dw, dh, 16u, THEME_BG0, 255u);
        fb_blend_rect(dx + 14u, dy + 1u, dw > 28u ? dw - 28u : dw, 2u,
                      col.r, col.g, col.b, 190u);
        draw_round_fill_hard(px, py, 28u, 28u, 9u, col, 255u);
        draw_icon(px + 5u, py + 5u, 18u, UI_ICON_ACTIVITY, THEME_BG0);

        live_w = draw_text_width(i18n(MSG_ACT_LIVE), 1) + 42u;
        span_w = draw_text_width(i18n(MSG_ACT_SPAN), 1) + 42u;
        chip_x = dx + dw > live_w + 16u ? dx + dw - live_w - 16u : px;
        draw_pill(chip_x, py, live_w, 28u, i18n(MSG_ACT_LIVE), col);
        if (chip_x > px + span_w + 44u) {
            draw_pill(chip_x - span_w - 8u, py, span_w, 28u,
                      i18n(MSG_ACT_SPAN), THEME_FG_DIM);
        }

        draw_text_clip(hero_x, py, chip_x > hero_x + 8u ? chip_x - 8u : dx + dw - 16u,
                       title[sel], THEME_FG_DIM, 1);
        if (hero_w > 0u) {
            draw_text_clip(hero_x, hero_y, dx + dw - 16u, hero[sel], col, ts);
        }
        draw_text_clip(hero_x, sub_y, dx + dw - 16u, sub[sel], dim, 1);
        draw_segments(px, meter_y, dw > 36u ? dw - 36u : dw, 10u, pct[sel], col);

        chart_y = meter_y + 20u;
        chart_h = dh > 520u ? 190u : dh / 3u;
        if (chart_h > 190u) {
            chart_h = 190u;
        }
        if (chart_h < 88u) {
            chart_h = 88u;
        }
        if (chart_y + chart_h + stat_h + 24u > dy + dh) {
            u32 room = dy + dh > chart_y + stat_h + 24u
                           ? dy + dh - chart_y - stat_h - 24u : 48u;

            if (room < chart_h) {
                chart_h = room;
            }
        }
        draw_area(px, chart_y, dw > 36u ? dw - 36u : dw, chart_h, hist[sel], col);
        if (chart_h >= FONT_LINE + 12u) {
            draw_text(px + 12u, chart_y + chart_h - FONT_LINE + 4u,
                      i18n(MSG_ACT_SPAN), dim, 1);
        }

        stat_y = chart_y + chart_h + 12u;
        stat_w = dw > 52u ? (dw - 36u - stat_gap * 2u) / 3u : 80u;
        if (stat_y + stat_h < dy + dh && stat_w > 24u) {
            if (sel == 0u) {
                draw_kv(px, stat_y, stat_w, stat_h, i18n(MSG_ACT_UTIL), s->cpu_txt, col);
                draw_kv(px + stat_w + stat_gap, stat_y, stat_w, stat_h,
                        i18n(MSG_ACT_CORE), core_txt, col);
                draw_kv(px + (stat_w + stat_gap) * 2u, stat_y, stat_w, stat_h,
                        i18n(MSG_ACT_TASKS), sys_sub, col);
            } else if (sel == 1u) {
                draw_kv(px, stat_y, stat_w, stat_h, i18n(MSG_ACT_USED), s->ram_txt, col);
                draw_kv(px + stat_w + stat_gap, stat_y, stat_w, stat_h,
                        i18n(MSG_ACT_HEAP), heap_txt, col);
                draw_kv(px + (stat_w + stat_gap) * 2u, stat_y, stat_w, stat_h,
                        i18n(MSG_ACT_UTIL), ram_pct_txt, col);
            } else if (sel == 2u) {
                draw_kv(px, stat_y, stat_w, stat_h, i18n(MSG_ACT_GPU), s->fb_txt, col);
                draw_kv(px + stat_w + stat_gap, stat_y, stat_w, stat_h,
                        i18n(MSG_ACT_UTIL), gpu_sub, col);
                draw_kv(px + (stat_w + stat_gap) * 2u, stat_y, stat_w, stat_h,
                        i18n(MSG_ACT_NO_3D), i18n(MSG_ACT_LIVE), col);
            } else if (sel == 3u) {
                draw_kv(px, stat_y, stat_w, stat_h, i18n(MSG_ACT_NET), hero[3], col);
                draw_kv(px + stat_w + stat_gap, stat_y, stat_w, stat_h,
                        i18n(MSG_ACT_LINK), s->net_link ? i18n(MSG_ACT_LINK)
                                                        : i18n(MSG_ACT_DOWN), col);
                draw_kv(px + (stat_w + stat_gap) * 2u, stat_y, stat_w, stat_h,
                        i18n(MSG_ACT_LIVE), s->wlan && s->ssid_txt[0] != '\0'
                                                ? s->ssid_txt : s->nic_txt, col);
            } else if (sel == 4u) {
                draw_kv(px, stat_y, stat_w, stat_h, i18n(MSG_ACT_DISK), hero[4], col);
                draw_kv(px + stat_w + stat_gap, stat_y, stat_w, stat_h,
                        i18n(MSG_ACT_BLK), disk_sub, col);
                draw_kv(px + (stat_w + stat_gap) * 2u, stat_y, stat_w, stat_h,
                        i18n(MSG_ACT_PERSIST),
                        s->persist ? i18n(MSG_ACT_PERSIST) : i18n(MSG_ACT_INITRD), col);
            } else {
                draw_kv(px, stat_y, stat_w, stat_h, i18n(MSG_ACT_SYS), s->up_txt, col);
                draw_kv(px + stat_w + stat_gap, stat_y, stat_w, stat_h,
                        i18n(MSG_ACT_TASKS), sys_sub, col);
                draw_kv(px + (stat_w + stat_gap) * 2u, stat_y, stat_w, stat_h,
                        i18n(s->ac ? MSG_ACT_AC : MSG_ACT_BAT), bat_txt, col);
            }
        }

        task_y = stat_y + stat_h + 14u;
        if (task_y + FONT_LINE * 3u < dy + dh &&
            (sel == 0u || sel == 5u)) {
            u32 rows = (dy + dh - task_y) / (FONT_LINE + 2u);

            if (rows > 4u) {
                rows = 4u;
            }
            if (rows > 0u) {
                draw_task_rows(px, task_y, dw > 36u ? dw - 36u : dw, rows, col);
            }
        }
    }
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
    } else if (w->kind == WIN_ACTIVITY) {
        draw_monitor(w);
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

void clamp_win(struct window *w)
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

void paint_windows(void)
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
