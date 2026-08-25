#include "shell.h"
#include "fb.h"
#include "fb_console.h"
#include "config.h"
#include "heap.h"
#include "i18n.h"
#include "keyboard.h"
#include "keycodes.h"
#include "pmm.h"
#include "time.h"
#include "utf8.h"
#include "vga.h"
#include "vfs.h"
#include "net.h"

#include "types.h"

#define SHELL_LINE_MAX 128
#define SHELL_CAT_BUF  256

static void shell_status_line(void)
{
    char line[80];
    u32 i;
    u32 ticks = time_ticks();
    u32 free_fr = free_frames_os;
    const char *prefix = "ticks=";
    const char *mid = "  free=";
    const char *suffix = i18n(MSG_STATUS_SUFFIX);

    for (i = 0; i < sizeof(line); ++i) {
        line[i] = ' ';
    }
    line[sizeof(line) - 1] = '\0';

    i = 0;
    while (*prefix && i < 70) {
        line[i++] = *prefix++;
    }
    {
        char tmp[12];
        u32 n = 0;
        u32 t = ticks;
        if (t == 0) {
            tmp[n++] = '0';
        } else {
            while (t > 0 && n < sizeof(tmp)) {
                tmp[n++] = (char)('0' + (t % 10));
                t /= 10;
            }
        }
        while (n > 0 && i < 70) {
            line[i++] = tmp[--n];
        }
    }
    while (*mid && i < 70) {
        line[i++] = *mid++;
    }
    {
        char tmp[12];
        u32 n = 0;
        u32 t = free_fr;
        if (t == 0) {
            tmp[n++] = '0';
        } else {
            while (t > 0 && n < sizeof(tmp)) {
                tmp[n++] = (char)('0' + (t % 10));
                t /= 10;
            }
        }
        while (n > 0 && i < 70) {
            line[i++] = tmp[--n];
        }
    }
    while (*suffix && i < 78) {
        line[i++] = *suffix++;
    }
    line[i] = '\0';

    vga_clear_row(24);
    vga_write_at(24, 0, line);
}

static int shell_streq(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b) {
        ++a;
        ++b;
    }
    return *a == *b;
}

static void shell_skip_spaces(const char **pp)
{
    while (**pp == ' ' || **pp == '\t') {
        ++(*pp);
    }
}

static const char *shell_next_token(const char **pp, char *out, size_t out_len)
{
    size_t n = 0;

    shell_skip_spaces(pp);
    if (**pp == '\0') {
        if (out_len > 0) {
            out[0] = '\0';
        }
        return NULL;
    }

    while (**pp != '\0' && **pp != ' ' && **pp != '\t') {
        if (n + 1 < out_len) {
            out[n++] = **pp;
        }
        ++(*pp);
    }
    if (out_len > 0) {
        out[n] = '\0';
    }
    return out;
}

static void cmd_help(void)
{
    vga_print(i18n(MSG_HELP_HEADER));
    vga_print("\n");
    vga_print(i18n(MSG_HELP_CMDS));
    vga_print("\n");
    vga_print(i18n(MSG_HELP_LANG));
    vga_print("\n");
    vga_print(i18n(MSG_HELP_UTF8));
    vga_print("\n");
    vga_print(i18n(MSG_HELP_ENTER));
    vga_print("\n");
}

static void cmd_echo(const char *args)
{
    shell_skip_spaces(&args);
    vga_print(args);
    vga_print("\n");
}

static void cmd_clear(void)
{
    vga_clear();
    shell_status_line();
}

static void cmd_ls(void)
{
    if (vfs_ls("/") < 0) {
        vga_print(i18n(MSG_LS_ERROR));
        vga_print("\n");
    }
}

static void cmd_cat(const char *args)
{
    char path[SHELL_LINE_MAX];
    char buf[SHELL_CAT_BUF];
    const char *p = args;
    int fd;
    ssize_t n;

    if (!shell_next_token(&p, path, sizeof(path))) {
        vga_print(i18n(MSG_CAT_USAGE));
        vga_print("\n");
        return;
    }

    fd = vfs_open(path);
    if (fd < 0) {
        vga_print(i18n(MSG_CAT_NOT_FOUND));
        vga_print("\n");
        return;
    }

    for (;;) {
        n = vfs_read(fd, buf, sizeof(buf) - 1);
        if (n <= 0) {
            break;
        }
        buf[n] = '\0';
        vga_print(buf);
    }
    vfs_close(fd);
    vga_print("\n");
}

static void cmd_ticks(void)
{
    vga_print("ticks=");
    vga_print_u32(time_ticks());
    vga_print("  uptime_ms=");
    vga_print_u32(time_uptime_ms());
    vga_print("\n");
}

static void cmd_mem(void)
{
    vga_print("heap used/free: ");
    vga_print_u32(heap_used_os);
    vga_print(" / ");
    vga_print_u32(heap_free_os);
    vga_print("\nframes free/total: ");
    vga_print_u32(free_frames_os);
    vga_print(" / ");
    vga_print_u32(total_frames_os);
    vga_print("\n");
}

static bool shell_should_exit;

static void shell_net_line(const char *line, void *ctx)
{
    (void)ctx;
    vga_print(line);
    vga_print("\n");
}

static void cmd_ping(const char *args)
{
    char tok[32];
    const char *p = args;
    u32 ip = 0;

    if (shell_next_token(&p, tok, sizeof(tok))) {
        if (!net_parse_ip(tok, &ip)) {
            vga_print("uso: ping [ip]   ej. ping 10.0.2.2\n");
            return;
        }
    }
    net_ping_run(ip, 4u, shell_net_line, NULL);
}

static void cmd_net(void)
{
    net_status_run(shell_net_line, NULL);
}

static void cmd_lang(const char *args)
{
    char code[8];
    const char *p = args;

    if (!shell_next_token(&p, code, sizeof(code))) {
        vga_print(i18n_lang_name(i18n_lang()));
        vga_print(" (lang es | lang en)\n");
        return;
    }

    if (!i18n_set_lang_code(code)) {
        vga_print("lang es | lang en\n");
        return;
    }

    vga_print(i18n_lang_name(i18n_lang()));
    vga_print("\n");
}

static bool shell_append_codepoint(char *line, u32 *length, u32 codepoint)
{
    char encoded[4];
    u32 encoded_length;
    u32 i;

    encoded_length = utf8_encode(codepoint, encoded);
    if (encoded_length == 0 || *length + encoded_length >= SHELL_LINE_MAX) {
        return false;
    }

    for (i = 0; i < encoded_length; ++i) {
        line[*length + i] = encoded[i];
    }
    *length += encoded_length;
    line[*length] = '\0';
    return true;
}

static void shell_remove_last_codepoint(char *line, u32 *length)
{
    if (*length == 0) {
        return;
    }

    --(*length);
    while (*length > 0 &&
           (((u8)line[*length] & 0xC0u) == 0x80u)) {
        --(*length);
    }
    line[*length] = '\0';
    vga_putc('\b');
}

static void shell_dispatch(char *line)
{
    char cmd[32];
    const char *p = line;
    const char *rest;

    shell_skip_spaces(&p);
    if (*p == '\0') {
        return;
    }

    if (!shell_next_token(&p, cmd, sizeof(cmd))) {
        return;
    }
    rest = p;
    shell_skip_spaces(&rest);

    if (shell_streq(cmd, "help")) {
        cmd_help();
    } else if (shell_streq(cmd, "echo")) {
        cmd_echo(rest);
    } else if (shell_streq(cmd, "clear")) {
        cmd_clear();
    } else if (shell_streq(cmd, "ls")) {
        cmd_ls();
    } else if (shell_streq(cmd, "cat")) {
        cmd_cat(rest);
    } else if (shell_streq(cmd, "ticks")) {
        cmd_ticks();
    } else if (shell_streq(cmd, "mem")) {
        cmd_mem();
    } else if (shell_streq(cmd, "lang")) {
        cmd_lang(rest);
    } else if (shell_streq(cmd, "ping")) {
        cmd_ping(rest);
    } else if (shell_streq(cmd, "net") || shell_streq(cmd, "ifconfig")) {
        cmd_net();
    } else if (shell_streq(cmd, "exit")) {
        shell_should_exit = true;
    } else {
        vga_print("?");
        vga_print(cmd);
        vga_print(i18n(MSG_UNKNOWN_CMD));
        vga_print("\n");
    }
}

void shell_run(void)
{
    char line[SHELL_LINE_MAX];
    u32 len = 0;
    u32 last_ticks = 0;
    bool gfx_term = fb_available();

    shell_should_exit = false;

    if (gfx_term) {
        fb_console_enable();
    }

    vga_clear();
    vga_print(name_os);
    vga_print(" ");
    vga_print(version_os);
    vga_print("\n");
    vga_print(i18n(MSG_SHELL_BANNER));
    vga_print(" ");
    vga_print(i18n(MSG_SHELL_PROMPT_HELP));
    vga_print("\n");
    vga_print(i18n(MSG_SHELL_EXIT_HINT));
    vga_print("\n\n");
    vga_print("> ");
    shell_status_line();

    for (;;) {
        if (shell_should_exit) {
            if (gfx_term) {
                fb_console_disable();
            }
            return;
        }

        if (time_ticks() != last_ticks) {
            last_ticks = time_ticks();
            shell_status_line();
        }

        if (!keyboard_has_char()) {
            __asm__ volatile("hlt");
            continue;
        }

        {
            u32 codepoint = keyboard_get_codepoint();

            if (key_is_special(codepoint)) {
                continue;
            }

            if (codepoint == '\n' || codepoint == '\r') {
                vga_putc('\n');
                line[len] = '\0';
                shell_dispatch(line);
                if (shell_should_exit) {
                    if (gfx_term) {
                        fb_console_disable();
                    }
                    return;
                }
                len = 0;
                vga_print("> ");
                shell_status_line();
            } else if (codepoint == '\b' || codepoint == 127) {
                shell_remove_last_codepoint(line, &len);
            } else if (codepoint >= 32 && codepoint != 127 &&
                       shell_append_codepoint(line, &len, codepoint)) {
                vga_put_codepoint(codepoint);
            }
        }
    }
}
