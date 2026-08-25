#include "rtl8187.h"
#include "usb.h"
#include "wlan.h"
#include "serial.h"
#include "string.h"
#include "time.h"

#define RTL_VID     0x0BDAu
#define RTL_PID     0x8187u
#define RTL_PID_B   0x8189u
#define RTL_REQ     0x05u
#define RTL_READ    0xC0u
#define RTL_WRITE   0x40u

#define REG_CMD     0x37u
#define CMD_RST     0x10u
#define CMD_RX      0x08u
#define CMD_TX      0x04u
#define REG_RXCONF  0x44u
#define REG_TXCONF  0x40u
#define REG_MAR0    0x08u
#define REG_MAR4    0x0Cu
#define REG_RFINTF  0x90u

static bool present;
static u8 usb_addr;
static u8 ep_in;
static u8 ep_out;

static void delay_ms(u32 ms)
{
    u32 start = time_uptime_ms();

    while ((time_uptime_ms() - start) < ms) {
        __asm__ volatile("hlt");
    }
}

static bool rtl_id(u16 vid, u16 pid)
{
    return vid == RTL_VID && (pid == RTL_PID || pid == RTL_PID_B);
}

static int r8(u16 reg, u8 *val)
{
    return usb_ctrl(usb_addr, RTL_READ, RTL_REQ, reg, 0, val, 1);
}

static int w8(u16 reg, u8 val)
{
    return usb_ctrl(usb_addr, RTL_WRITE, RTL_REQ, reg, 0, &val, 1);
}

static int w32(u16 reg, u32 val)
{
    u8 buf[4];

    buf[0] = (u8)(val & 0xFFu);
    buf[1] = (u8)((val >> 8) & 0xFFu);
    buf[2] = (u8)((val >> 16) & 0xFFu);
    buf[3] = (u8)((val >> 24) & 0xFFu);
    return usb_ctrl(usb_addr, RTL_WRITE, RTL_REQ, reg, 0, buf, 4);
}

static int w16(u16 reg, u16 val)
{
    u8 buf[2];

    buf[0] = (u8)(val & 0xFFu);
    buf[1] = (u8)(val >> 8);
    return usb_ctrl(usb_addr, RTL_WRITE, RTL_REQ, reg, 0, buf, 2);
}

static bool hw_reset(void)
{
    u8 cmd;
    u32 i;

    if (r8(REG_CMD, &cmd) != 0) {
        return false;
    }
    cmd = (u8)((cmd & ~(CMD_TX | CMD_RX)) | CMD_RST);
    if (w8(REG_CMD, cmd) != 0) {
        return false;
    }
    for (i = 0; i < 20u; ++i) {
        delay_ms(2u);
        if (r8(REG_CMD, &cmd) != 0) {
            return false;
        }
        if ((cmd & CMD_RST) == 0u) {
            return true;
        }
    }
    return false;
}

static bool hw_rx_on(void)
{
    u8 cmd;
    u32 rx;

    /* Accept broadcast, multicast, management, data, CRC ok. */
    rx = (1u << 31) | (1u << 30) | (1u << 20) | (1u << 19) | (1u << 18) |
         (1u << 5) | (1u << 3) | (1u << 2) | (1u << 1) | (1u << 0);
    if (w32(REG_RXCONF, rx) != 0) {
        return false;
    }
    (void)w32(REG_MAR0, 0xFFFFFFFFu);
    (void)w32(REG_MAR4, 0xFFFFFFFFu);
    if (r8(REG_CMD, &cmd) != 0) {
        return false;
    }
    cmd = (u8)(cmd | CMD_RX | CMD_TX);
    return w8(REG_CMD, cmd) == 0;
}

static bool parse_beacon(const u8 *frame, u32 len, u8 quality)
{
    struct wlan_bss bss;
    u32 off;
    u16 fc;
    const u8 *tags;
    u32 tlen;

    if (len < 36u) {
        return false;
    }
    fc = (u16)frame[0] | ((u16)frame[1] << 8);
    /* management, subtype beacon (8) or probe resp (5) */
    if ((fc & 0x000Cu) != 0u) {
        return false;
    }
    if (((fc >> 4) & 0xFu) != 8u && ((fc >> 4) & 0xFu) != 5u) {
        return false;
    }

    memset(&bss, 0, sizeof(bss));
    memcpy(bss.bssid, frame + 16, 6);
    bss.quality = quality;
    bss.src = WLAN_SRC_RADIO;
    bss.sec = WLAN_SEC_OPEN;

    tags = frame + 36;
    tlen = len - 36u;
    off = 0;
    while (off + 2u <= tlen) {
        u8 id = tags[off];
        u8 elen = tags[off + 1u];

        if (off + 2u + elen > tlen) {
            break;
        }
        if (id == 0u && elen > 0u && elen <= WLAN_SSID_MAX) {
            memcpy(bss.ssid, tags + off + 2u, elen);
            bss.ssid[elen] = '\0';
        } else if (id == 3u && elen >= 1u) {
            bss.channel = tags[off + 2u];
        } else if (id == 48u) {
            bss.sec = WLAN_SEC_WPA; /* RSN / WPA2 */
        } else if (id == 221u && elen >= 4u &&
                   tags[off + 2u] == 0x00u && tags[off + 3u] == 0x50u &&
                   tags[off + 4u] == 0xF2u && tags[off + 5u] == 0x01u) {
            bss.sec = WLAN_SEC_WPA;
        }
        off = off + 2u + elen;
    }
    if (bss.ssid[0] == '\0') {
        return false;
    }
    return wlan_add_bss(&bss);
}

static const u8 chan_rf[] = {
    0x85, 0x8D, 0x8E, 0x8B, 0x89, 0x91, 0x92, 0x93,
    0x9A, 0x9B, 0x99, 0xA2, 0xA3, 0xA0
};

static void set_channel(u8 ch)
{
    if (ch < 1u || ch > 14u) {
        ch = 1u;
    }
    /* RF register 0x07 selects the 8225/8256 channel. */
    {
        u8 rf = chan_rf[ch - 1u];

        (void)w16(REG_RFINTF, 0x0080u);
        (void)usb_ctrl(usb_addr, RTL_WRITE, RTL_REQ, 0x7u, 0x82u, &rf, 1);
        delay_ms(5u);
    }
}

bool rtl8187_init(void)
{
    u32 i;
    u32 n;

    present = false;
    usb_addr = 0;
    ep_in = 0;
    ep_out = 0;

    n = usb_device_count();
    for (i = 0; i < n; ++i) {
        const struct usb_device *d = usb_device_get(i);

        if (d == NULL) {
            continue;
        }
        if (!rtl_id(d->vendor, d->product)) {
            if (d->vendor == RTL_VID) {
                serial_write("wifi: Realtek USB ");
                serial_print_hex(d->product);
                serial_write(" (sin firmware en este driver)\n");
            }
            continue;
        }
        usb_addr = d->addr;
        ep_in = d->ep_in != 0u ? d->ep_in : 1u;
        ep_out = d->ep_out != 0u ? d->ep_out : 2u;
        present = true;
        break;
    }

    if (!present) {
        return false;
    }

    serial_write("wifi: RTL8187 addr=");
    serial_print_u32(usb_addr);
    serial_putc('\n');

    if (!hw_reset()) {
        serial_write("wifi: RTL8187 reset fallido\n");
        present = false;
        return false;
    }
    if (!hw_rx_on()) {
        serial_write("wifi: RTL8187 RX no arranco\n");
        present = false;
        return false;
    }
    return true;
}

bool rtl8187_present(void)
{
    return present;
}

u32 rtl8187_scan(void)
{
    u8 buf[512];
    u8 ch;
    u32 added_before;

    if (!present) {
        return 0;
    }

    added_before = wlan_count();
    for (ch = 1; ch <= 13u; ++ch) {
        u32 t0;

        set_channel(ch);
        t0 = time_uptime_ms();
        while ((time_uptime_ms() - t0) < 40u) {
            int n = usb_bulk(usb_addr, ep_in, true, buf, 512);

            if (n < 16) {
                continue;
            }
            /* rtl8187 RX header is 16 bytes then 802.11. */
            {
                u8 agc = buf[6];
                u8 qual = agc > 100u ? 100u : agc;
                u8 *frame = buf + 16;
                u32 flen = (u32)n - 16u;

                (void)parse_beacon(frame, flen, qual);
            }
        }
    }
    serial_write("wifi: RTL8187 scan=");
    serial_print_u32(wlan_count() - added_before);
    serial_putc('\n');
    return wlan_count() - added_before;
}

bool rtl8187_associate(const char *ssid, const char *pass, u8 sec)
{
    u8 frame[128];
    u32 n;

    if (!present || ssid == NULL) {
        return false;
    }
    (void)pass;
    /* Open authentication (subtype 11) + association (0) on the air.
     * WPA2 4-way is handled by the host radio path; here we only
     * attempt open networks on a real RTL8187. */
    if (sec != WLAN_SEC_OPEN) {
        serial_write("wifi: RTL8187 WPA requiere CCMP (usa radio host)\n");
        return false;
    }

    memset(frame, 0, sizeof(frame));
    /* Frame control: mgmt, subtype auth (11). */
    frame[0] = 0xB0;
    frame[1] = 0x00;
    memset(frame + 4, 0xFF, 6);  /* DA broadcast */
    /* SA / BSSID left 0; chip fills MAC from registers. */
    n = 30;
    /* Auth algorithm 0, seq 1, status 0. */
    frame[24] = 0;
    frame[26] = 1;
    if (usb_bulk(usb_addr, ep_out, false, frame, (u16)n) < 0) {
        return false;
    }
    delay_ms(20u);
    /* Assoc request. */
    frame[0] = 0x00;
    n = 28;
    {
        u32 sl = 0;

        while (ssid[sl] != '\0' && sl < WLAN_SSID_MAX) {
            ++sl;
        }
        frame[n++] = 0; /* SSID tag */
        frame[n++] = (u8)sl;
        memcpy(frame + n, ssid, sl);
        n += sl;
        frame[n++] = 1; /* rates */
        frame[n++] = 4;
        frame[n++] = 0x82;
        frame[n++] = 0x84;
        frame[n++] = 0x8B;
        frame[n++] = 0x96;
    }
    if (usb_bulk(usb_addr, ep_out, false, frame, (u16)n) < 0) {
        return false;
    }
    return true;
}
