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
    w->w = kind == WIN_POWER ? 440u : (kind == WIN_ACTIVITY ? 900u : 760u);
    if (kind == WIN_POWER) {
        w->h = 220u;
    } else if (kind == WIN_SETTINGS) {
        w->h = 620u;
    } else if (kind == WIN_ACTIVITY) {
        w->h = 580u;
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

static void draw_meter(u32 x, u32 y, u32 w, u32 h, u32 pct, struct rgb fill)
{
    u32 inner;

    if (w < 8u || h < 4u) {
        return;
    }
    if (pct > 100u) {
        pct = 100u;
    }
    draw_round_fill_hard(x, y, w, h, h / 2u, THEME_GRID, 255u);
    inner = (w * pct) / 100u;
    if (inner >= 4u) {
        draw_round_fill_hard(x, y, inner, h, h / 2u, fill, 255u);
    }
}

static void draw_spark(u32 x, u32 y, u32 w, u32 h, const u8 *hist, struct rgb col)
{
    u32 i;
    u32 cw;

    if (w < SYSMON_HIST || h < 8u || hist == NULL) {
        return;
    }
    cw = w / SYSMON_HIST;
    if (cw < 1u) {
        cw = 1u;
    }
    for (i = 0; i < SYSMON_HIST; i++) {
        u32 bh = ((u32)hist[i] * (h - 2u)) / 100u;
        u32 bx = x + i * cw;
        u32 by;

        if (bh < 2u) {
            bh = 2u;
        }
        by = y + h - bh;
        fb_fill_rect(bx, by, cw > 1u ? cw - 1u : cw, bh, col.r, col.g, col.b);
    }
}

static void draw_card(u32 x, u32 y, u32 w, u32 h)
{
    draw_round_fill_hard(x, y, w, h, 14u, THEME_FIELD, 255u);
}

static void draw_monitor(struct window *win)
{
    const struct sysmon_snap *s;
    u32 bx = (u32)win->x;
    u32 by = (u32)win->y;
    u32 pad = 18u;
    u32 gap = 12u;
    u32 inner_w;
    u32 inner_h;
    u32 cw;
    u32 ch;
    u32 i;
    struct rgb cpu_c = THEME_ACCENT;
    struct rgb ram_c = { 0x4B, 0x9A, 0xFF };
    struct rgb ok_c = { 0x6B, 0xC9, 0x8A };
    struct rgb dim = THEME_FG_DIM;
    char gpu_sub[36];
    char disk_sub[28];
    char sys_sub[36];
    char nbuf[12];

    s = sysmon_get();

    gpu_sub[0] = '\0';
    u32_to_dec(s->fb_bpp, nbuf, sizeof(nbuf));
    copy_n(gpu_sub, sizeof(gpu_sub), nbuf);
    copy_n(gpu_sub + strlen(gpu_sub), sizeof(gpu_sub) - strlen(gpu_sub), " bpp  ");
    u32_to_dec(s->fb_mib, nbuf, sizeof(nbuf));
    copy_n(gpu_sub + strlen(gpu_sub), sizeof(gpu_sub) - strlen(gpu_sub), nbuf);
    copy_n(gpu_sub + strlen(gpu_sub), sizeof(gpu_sub) - strlen(gpu_sub), " MiB");

    disk_sub[0] = '\0';
    u32_to_dec(s->blk_n, nbuf, sizeof(nbuf));
    copy_n(disk_sub, sizeof(disk_sub), nbuf);
    copy_n(disk_sub + strlen(disk_sub), sizeof(disk_sub) - strlen(disk_sub), " ");
    copy_n(disk_sub + strlen(disk_sub), sizeof(disk_sub) - strlen(disk_sub),
           i18n(MSG_ACT_BLK));

    sys_sub[0] = '\0';
    u32_to_dec(s->tasks, nbuf, sizeof(nbuf));
    copy_n(sys_sub, sizeof(sys_sub), nbuf);
    copy_n(sys_sub + strlen(sys_sub), sizeof(sys_sub) - strlen(sys_sub), " ");
    copy_n(sys_sub + strlen(sys_sub), sizeof(sys_sub) - strlen(sys_sub),
           i18n(MSG_ACT_TASKS));
    copy_n(sys_sub + strlen(sys_sub), sizeof(sys_sub) - strlen(sys_sub), " · ");
    u32_to_dec(s->pci_n, nbuf, sizeof(nbuf));
    copy_n(sys_sub + strlen(sys_sub), sizeof(sys_sub) - strlen(sys_sub), nbuf);
    copy_n(sys_sub + strlen(sys_sub), sizeof(sys_sub) - strlen(sys_sub), " PCI");

    inner_w = win->w > pad * 2u + gap ? win->w - pad * 2u - gap : win->w / 2u;
    inner_h = win->h > TITLE_H + pad * 2u + gap * 2u
                  ? win->h - TITLE_H - pad * 2u - gap * 2u
                  : 120u;
    cw = inner_w / 2u;
    ch = inner_h / 3u;
    if (cw < 160u) {
        cw = 160u;
    }
    if (ch < 110u) {
        ch = 110u;
    }

    for (i = 0; i < 6u; i++) {
        u32 row = i / 2u;
        u32 col = i % 2u;
        u32 cx = bx + pad + col * (cw + gap);
        u32 cy = by + TITLE_H + pad + row * (ch + gap);
        u32 tx = cx + 16u;
        u32 ty = cy + 12u;
        const char *title = "";
        const char *hero = "";
        const char *sub = "";
        u32 pct = 0;
        struct rgb bar_c = cpu_c;
        const u8 *hist = NULL;
        u32 bar_w = cw > 40u ? cw - 32u : cw;

        draw_card(cx, cy, cw, ch);

        if (i == 0u) {
            title = i18n(MSG_ACT_CPU);
            hero = s->cpu_txt;
            sub = i18n(s->preempt ? MSG_ACT_PREEMPT_ON : MSG_ACT_PREEMPT_OFF);
            pct = s->cpu_pct;
            hist = s->cpu_hist;
        } else if (i == 1u) {
            title = i18n(MSG_ACT_RAM);
            hero = s->ram_txt;
            sub = i18n(MSG_ACT_HEAP);
            pct = s->ram_pct;
            bar_c = ram_c;
            hist = s->ram_hist;
        } else if (i == 2u) {
            title = i18n(MSG_ACT_GPU);
            hero = s->fb_txt;
            sub = gpu_sub;
        } else if (i == 3u) {
            title = i18n(MSG_ACT_NET);
            if (!s->net_nic) {
                hero = i18n(MSG_ACT_NO_NIC);
                sub = i18n(MSG_ACT_DOWN);
            } else if (s->net_cfg && s->ip_txt[0] != '\0') {
                hero = s->ip_txt;
                sub = s->net_link ? s->nic_txt : i18n(MSG_ACT_DOWN);
            } else {
                hero = s->nic_txt[0] != '\0' ? s->nic_txt : i18n(MSG_ACT_DOWN);
                sub = s->net_link ? i18n(MSG_ACT_LINK) : i18n(MSG_ACT_DOWN);
            }
        } else if (i == 4u) {
            title = i18n(MSG_ACT_DISK);
            hero = s->persist ? i18n(MSG_ACT_PERSIST) : i18n(MSG_ACT_INITRD);
            sub = disk_sub;
        } else {
            title = i18n(MSG_ACT_SYS);
            hero = s->up_txt;
            sub = sys_sub;
        }

        draw_text(tx, ty, title, dim, 1);
        draw_text_clip(tx, ty + FONT_LINE - 6u, cx + cw - 12u, hero, THEME_FG, 2);

        if (hist != NULL) {
            draw_meter(tx, ty + FONT_LINE + FONT_HEIGHT + 4u, bar_w, 10u, pct,
                       bar_c);
            draw_spark(tx, ty + FONT_LINE + FONT_HEIGHT + 22u, bar_w,
                       ch > 96u ? ch - 96u : 20u, hist, bar_c);
        } else if (i == 2u) {
            draw_text_clip(tx, ty + FONT_LINE + FONT_HEIGHT + 6u, cx + cw - 12u,
                           i18n(MSG_ACT_NO_3D), dim, 1);
            draw_text_clip(tx, cy + ch - FONT_LINE, cx + cw - 12u, sub, dim, 1);
        } else if (i == 3u) {
            draw_text_clip(tx, ty + FONT_LINE + FONT_HEIGHT + 6u, cx + cw - 12u,
                           sub, s->net_link ? ok_c : dim, 1);
            if (s->wlan && s->ssid_txt[0] != '\0') {
                draw_text_clip(tx, ty + FONT_LINE * 2u + FONT_HEIGHT,
                               cx + cw - 12u, s->ssid_txt, THEME_ACCENT, 1);
            }
        } else {
            draw_text_clip(tx, ty + FONT_LINE + FONT_HEIGHT + 6u, cx + cw - 12u,
                           sub, dim, 1);
        }
        if (hist != NULL) {
            draw_text_clip(tx, cy + ch - FONT_LINE + 4u, cx + cw - 12u, sub, dim,
                           1);
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
