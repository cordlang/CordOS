#include "wlan_host.h"
#include "wlan.h"
#include "io.h"
#include "serial.h"
#include "string.h"
#include "time.h"

#define COM2          0x2F8u
#define UART_DATA     0
#define UART_IER      1
#define UART_FCR      2
#define UART_LCR      3
#define UART_MCR      4
#define UART_LSR      5
#define UART_SCR      7
#define LSR_DR        0x01u
#define LSR_THRE      0x20u

static bool host_ok;

static bool lsr_ok(void)
{
    return inb(COM2 + UART_LSR) != 0xFFu;
}

static void com2_putc(char c)
{
    u32 spins = 100000u;

    while ((inb(COM2 + UART_LSR) & LSR_THRE) == 0u && spins > 0u) {
        --spins;
        __asm__ volatile("pause");
    }
    if (spins > 0u) {
        outb(COM2 + UART_DATA, (u8)c);
    }
}

static void com2_write(const char *s)
{
    while (s != NULL && *s != '\0') {
        com2_putc(*s);
        ++s;
    }
}

static void com2_flush_rx(void)
{
    u32 n = 0;

    while ((inb(COM2 + UART_LSR) & LSR_DR) != 0u && n < 1024u) {
        (void)inb(COM2 + UART_DATA);
        ++n;
    }
}

static int com2_getc(u32 timeout_ms)
{
    u32 start = time_uptime_ms();

    while ((inb(COM2 + UART_LSR) & LSR_DR) == 0u) {
        if ((time_uptime_ms() - start) > timeout_ms) {
            return -1;
        }
        __asm__ volatile("pause");
    }
    return (int)inb(COM2 + UART_DATA);
}

static bool com2_readline(char *buf, u32 max, u32 timeout_ms)
{
    u32 n = 0;
    u32 start = time_uptime_ms();

    if (max == 0u) {
        return false;
    }
    buf[0] = '\0';
    for (;;) {
        int c;
        u32 left;

        if ((time_uptime_ms() - start) > timeout_ms) {
            buf[n] = '\0';
            return n > 0u;
        }
        left = timeout_ms - (time_uptime_ms() - start);
        c = com2_getc(left > 50u ? 50u : (left == 0u ? 1u : left));
        if (c < 0) {
            continue;
        }
        if (c == '\r') {
            continue;
        }
        if (c == '\n') {
            buf[n] = '\0';
            return true;
        }
        if (n + 1u < max) {
            buf[n++] = (char)c;
        }
    }
}

bool wlan_host_init(void)
{
    host_ok = false;

    outb(COM2 + UART_IER, 0x00);
    outb(COM2 + UART_LCR, 0x80);
    outb(COM2 + UART_DATA, 0x01); /* 115200 */
    outb(COM2 + UART_IER, 0x00);
    outb(COM2 + UART_LCR, 0x03);
    outb(COM2 + UART_FCR, 0xC7);
    outb(COM2 + UART_MCR, 0x0B);
    outb(COM2 + UART_SCR, 0x5A);
    if (inb(COM2 + UART_SCR) != 0x5A) {
        serial_write("wlan-host: COM2 ausente\n");
        return false;
    }
    if (!lsr_ok()) {
        serial_write("wlan-host: COM2 LSR invalido\n");
        return false;
    }
    host_ok = true;
    serial_write("wlan-host: COM2 listo (radio del anfitrion)\n");
    return true;
}

bool wlan_host_present(void)
{
    return host_ok;
}

u32 wlan_host_scan(void)
{
    char line[96];
    u32 added = 0;
    u32 start;

    if (!host_ok) {
        return 0;
    }

    memset(line, 0, sizeof(line));
    com2_flush_rx();
    com2_write("S\r\n");

    start = time_uptime_ms();
    while (!com2_readline(line, sizeof(line), 80u)) {
        if ((time_uptime_ms() - start) > 400u) {
            break;
        }
    }
    if (line[0] != 'W' || line[1] != '1') {
        /* Wait for header; host scan can take a couple of seconds. */
        while ((time_uptime_ms() - start) < 2800u) {
            if (com2_readline(line, sizeof(line), 120u) &&
                line[0] == 'W' && line[1] == '1') {
                break;
            }
        }
    }
    if (line[0] != 'W' || line[1] != '1') {
        serial_write("wlan-host: sin respuesta de scan\n");
        return 0;
    }

    while (com2_readline(line, sizeof(line), 400u)) {
        struct wlan_bss bss;
        u32 i;
        u32 field;
        const char *parts[3];
        u32 nparts = 0;

        if (line[0] == '.' && line[1] == '\0') {
            break;
        }
        if (line[0] == '\0') {
            continue;
        }

        parts[0] = line;
        nparts = 1;
        for (i = 0; line[i] != '\0' && nparts < 3u; ++i) {
            if (line[i] == '\t') {
                line[i] = '\0';
                parts[nparts++] = &line[i + 1u];
            }
        }
        if (nparts < 3u || parts[0][0] == '\0') {
            continue;
        }

        memset(&bss, 0, sizeof(bss));
        for (i = 0; parts[0][i] != '\0' && i < WLAN_SSID_MAX; ++i) {
            bss.ssid[i] = parts[0][i];
        }
        bss.ssid[i] = '\0';
        bss.quality = 0;
        for (i = 0; parts[1][i] >= '0' && parts[1][i] <= '9'; ++i) {
            bss.quality = (u8)(bss.quality * 10u + (u8)(parts[1][i] - '0'));
        }
        if (bss.quality > 100u) {
            bss.quality = 100u;
        }
        field = (u8)(parts[2][0] - '0');
        bss.sec = (field <= 2u) ? (u8)field : (u8)WLAN_SEC_WPA;
        bss.src = WLAN_SRC_HOST;
        bss.channel = 0;
        if (wlan_add_bss(&bss)) {
            ++added;
        }
    }

    serial_write("wlan-host: redes=");
    serial_print_u32(added);
    serial_putc('\n');
    return added;
}

bool wlan_host_connect(const char *ssid, const char *pass)
{
    char line[32];
    u32 i;

    if (!host_ok || ssid == NULL) {
        return false;
    }
    if (pass == NULL) {
        pass = "";
    }
    com2_flush_rx();
    com2_putc('C');
    com2_putc('\t');
    for (i = 0; ssid[i] != '\0'; ++i) {
        char c = ssid[i];

        if (c == '\t' || c == '\n') {
            c = ' ';
        }
        com2_putc(c);
    }
    com2_putc('\t');
    for (i = 0; pass[i] != '\0'; ++i) {
        char c = pass[i];

        if (c == '\t' || c == '\n') {
            c = ' ';
        }
        com2_putc(c);
    }
    com2_write("\r\n");
    if (!com2_readline(line, sizeof(line), 1500u)) {
        return false;
    }
    return line[0] == 'K';
}
