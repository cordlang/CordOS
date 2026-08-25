#include "net.h"
#include "nic.h"
#include "pci.h"
#include "serial.h"
#include "string.h"
#include "time.h"

#define ETH_IP   0x0800u
#define ETH_ARP  0x0806u
#define ARP_REQ  1u
#define ARP_REP  2u
#define IP_ICMP  1u
#define IP_UDP   17u
#define ICMP_ECHO 8u
#define ICMP_REPLY 0u
#define PKT_MAX  1514u

#define DHCP_CLIENT 68u
#define DHCP_SERVER 67u
#define DHCP_DISCOVER 1u
#define DHCP_OFFER    2u
#define DHCP_REQUEST  3u
#define DHCP_ACK      5u

static const u8 bcast_mac[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

static bool inited;
static bool configured;
static u32 ip_addr;
static u32 ip_mask;
static u32 ip_gw;
static u16 ip_id;
static u16 ping_seq;
static u16 ping_id;

static u32 arp_ip;
static u8 arp_mac[6];
static bool arp_ok;

static bool ping_got;
static u16 ping_want_seq;

static bool dhcp_got;
static u32 dhcp_xid;
static u32 dhcp_yiaddr;
static u32 dhcp_server;
static u32 dhcp_mask;
static u32 dhcp_gw;

static void delay_ms(u32 ms)
{
    u32 start = time_uptime_ms();

    while ((time_uptime_ms() - start) < ms) {
        __asm__ volatile("hlt");
    }
}

static u16 cksum(const void *data, u32 len)
{
    const u8 *p = (const u8 *)data;
    u32 sum = 0;

    while (len > 1u) {
        sum += ((u32)p[0] << 8) | (u32)p[1];
        p += 2;
        len -= 2u;
    }
    if (len != 0u) {
        sum += (u32)p[0] << 8;
    }
    while ((sum >> 16) != 0u) {
        sum = (sum & 0xFFFFu) + (sum >> 16);
    }
    return (u16)~sum;
}

static void put_u16(u8 *p, u16 v)
{
    p[0] = (u8)(v >> 8);
    p[1] = (u8)(v & 0xFFu);
}

static void put_u32(u8 *p, u32 v)
{
    p[0] = (u8)(v >> 24);
    p[1] = (u8)(v >> 16);
    p[2] = (u8)(v >> 8);
    p[3] = (u8)(v & 0xFFu);
}

static u16 get_u16(const u8 *p)
{
    return (u16)(((u16)p[0] << 8) | p[1]);
}

static u32 get_u32(const u8 *p)
{
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3];
}

bool net_parse_ip(const char *text, u32 *out)
{
    u32 oct[4];
    u32 i;
    u32 v;
    const char *p;

    if (text == NULL || out == NULL) {
        return false;
    }
    oct[0] = oct[1] = oct[2] = oct[3] = 0;
    i = 0;
    v = 0;
    p = text;
    if (*p == '\0') {
        return false;
    }
    while (*p != '\0') {
        if (*p >= '0' && *p <= '9') {
            v = v * 10u + (u32)(*p - '0');
            if (v > 255u) {
                return false;
            }
        } else if (*p == '.' && i < 3u) {
            oct[i++] = v;
            v = 0;
        } else {
            return false;
        }
        ++p;
    }
    if (i != 3u) {
        return false;
    }
    oct[3] = v;
    *out = (oct[0] << 24) | (oct[1] << 16) | (oct[2] << 8) | oct[3];
    return true;
}

void net_format_ip(u32 ip, char *out, u32 max)
{
    char tmp[4][4];
    u32 oct[4];
    u32 i;
    u32 n;
    u32 o;
    u32 v;

    if (out == NULL || max == 0u) {
        return;
    }
    oct[0] = (ip >> 24) & 0xFFu;
    oct[1] = (ip >> 16) & 0xFFu;
    oct[2] = (ip >> 8) & 0xFFu;
    oct[3] = ip & 0xFFu;
    for (i = 0; i < 4u; ++i) {
        v = oct[i];
        n = 0;
        if (v == 0u) {
            tmp[i][n++] = '0';
        } else {
            char d[4];
            u32 dn = 0;

            while (v > 0u && dn < 3u) {
                d[dn++] = (char)('0' + (v % 10u));
                v /= 10u;
            }
            while (dn > 0u) {
                tmp[i][n++] = d[--dn];
            }
        }
        tmp[i][n] = '\0';
    }
    o = 0;
    for (i = 0; i < 4u; ++i) {
        n = 0;
        while (tmp[i][n] != '\0' && o + 1u < max) {
            out[o++] = tmp[i][n++];
        }
        if (i + 1u < 4u && o + 1u < max) {
            out[o++] = '.';
        }
    }
    out[o] = '\0';
}

static void eth_send(const u8 *dst, u16 type, const void *payload, u16 plen)
{
    u8 frame[PKT_MAX];
    const u8 *mac;

    if (plen + 14u > PKT_MAX) {
        return;
    }
    mac = nic_mac();
    memcpy(frame, dst, 6);
    memcpy(frame + 6, mac, 6);
    put_u16(frame + 12, type);
    memcpy(frame + 14, payload, plen);
    (void)nic_send(frame, (u16)(plen + 14u));
}

static void arp_send(u16 op, u32 tip, const u8 *tmac)
{
    u8 p[28];
    const u8 *mac = nic_mac();

    put_u16(p + 0, 1);
    put_u16(p + 2, ETH_IP);
    p[4] = 6;
    p[5] = 4;
    put_u16(p + 6, op);
    memcpy(p + 8, mac, 6);
    put_u32(p + 14, ip_addr);
    if (tmac != NULL) {
        memcpy(p + 18, tmac, 6);
    } else {
        memset(p + 18, 0, 6);
    }
    put_u32(p + 24, tip);
    eth_send((op == ARP_REQ) ? bcast_mac : tmac, ETH_ARP, p, 28);
}

static void arp_in(const u8 *p, u16 len, const u8 *src_mac)
{
    u16 op;
    u32 sip;
    u32 tip;

    (void)src_mac;
    if (len < 28u) {
        return;
    }
    if (get_u16(p) != 1u || get_u16(p + 2) != ETH_IP) {
        return;
    }
    op = get_u16(p + 6);
    sip = get_u32(p + 14);
    tip = get_u32(p + 24);
    if (op == ARP_REQ && tip == ip_addr && ip_addr != 0u) {
        arp_send(ARP_REP, sip, p + 8);
    } else if (op == ARP_REP && tip == ip_addr) {
        arp_ip = sip;
        memcpy(arp_mac, p + 8, 6);
        arp_ok = true;
    }
}

static bool arp_resolve(u32 ip, u8 *mac_out, u32 timeout_ms)
{
    u32 start;
    u32 last;

    if (arp_ok && arp_ip == ip) {
        memcpy(mac_out, arp_mac, 6);
        return true;
    }
    arp_ok = false;
    arp_ip = ip;
    last = 0;
    start = time_uptime_ms();
    while ((time_uptime_ms() - start) < timeout_ms) {
        if ((time_uptime_ms() - last) >= 300u) {
            arp_send(ARP_REQ, ip, NULL);
            last = time_uptime_ms();
        }
        net_poll();
        if (arp_ok && arp_ip == ip) {
            memcpy(mac_out, arp_mac, 6);
            return true;
        }
        __asm__ volatile("pause");
    }
    return false;
}

static void ip_send(u32 dst, u8 proto, const void *payload, u16 plen)
{
    u8 pkt[PKT_MAX];
    u8 dmac[6];
    u32 next;
    u16 total;
    u16 c;

    if (!configured || plen + 20u + 14u > PKT_MAX) {
        return;
    }
    next = dst;
    if ((dst & ip_mask) != (ip_addr & ip_mask)) {
        next = ip_gw;
        if (next == 0u) {
            return;
        }
    }
    if (!arp_resolve(next, dmac, 400u)) {
        return;
    }

    total = (u16)(20u + plen);
    memset(pkt, 0, 20);
    pkt[0] = 0x45;
    put_u16(pkt + 2, total);
    put_u16(pkt + 4, ip_id++);
    pkt[8] = 64;
    pkt[9] = proto;
    put_u32(pkt + 12, ip_addr);
    put_u32(pkt + 16, dst);
    c = cksum(pkt, 20);
    put_u16(pkt + 10, c);
    memcpy(pkt + 20, payload, plen);
    eth_send(dmac, ETH_IP, pkt, total);
}

static void icmp_in(const u8 *ip, u16 iplen, const u8 *icmp, u16 icmplen)
{
    u32 src;
    u8 reply[PKT_MAX];

    src = get_u32(ip + 12);
    if (icmplen < 8u) {
        return;
    }
    if (icmp[0] == ICMP_REPLY && icmp[1] == 0u) {
        if (get_u16(icmp + 4) == ping_id && get_u16(icmp + 6) == ping_want_seq) {
            ping_got = true;
        }
        (void)src;
        return;
    }
    if (icmp[0] == ICMP_ECHO && icmp[1] == 0u && configured) {
        if (icmplen > PKT_MAX) {
            return;
        }
        memcpy(reply, icmp, icmplen);
        reply[0] = ICMP_REPLY;
        put_u16(reply + 2, 0);
        put_u16(reply + 2, cksum(reply, icmplen));
        ip_send(src, IP_ICMP, reply, icmplen);
    }
    (void)iplen;
}

static void dhcp_apply_options(const u8 *opt, u32 len)
{
    u32 i = 0;

    while (i < len) {
        u8 tag = opt[i];
        u8 n;

        if (tag == 0u) {
            ++i;
            continue;
        }
        if (tag == 255u) {
            break;
        }
        if (i + 1u >= len) {
            break;
        }
        n = opt[i + 1u];
        if (i + 2u + n > len) {
            break;
        }
        if (tag == 1u && n >= 4u) {
            dhcp_mask = get_u32(opt + i + 2u);
        } else if (tag == 3u && n >= 4u) {
            dhcp_gw = get_u32(opt + i + 2u);
        } else if (tag == 54u && n >= 4u) {
            dhcp_server = get_u32(opt + i + 2u);
        } else if (tag == 53u && n >= 1u) {
            if (opt[i + 2u] == DHCP_OFFER || opt[i + 2u] == DHCP_ACK) {
                dhcp_got = true;
            }
        }
        i = i + 2u + n;
    }
}

static void udp_in(const u8 *ip, const u8 *udp, u16 udplen)
{
    u16 dstport;
    const u8 *bootp;
    u16 blen;

    (void)ip;
    if (udplen < 8u) {
        return;
    }
    dstport = get_u16(udp + 2);
    if (dstport != DHCP_CLIENT) {
        return;
    }
    bootp = udp + 8;
    blen = (u16)(udplen - 8u);
    if (blen < 240u) {
        return;
    }
    if (get_u32(bootp + 4) != dhcp_xid) {
        return;
    }
    dhcp_yiaddr = get_u32(bootp + 16);
    if (dhcp_server == 0u) {
        dhcp_server = get_u32(bootp + 20);
    }
    if (blen > 240u) {
        dhcp_apply_options(bootp + 240, (u32)blen - 240u);
    }
}

static void ip_in(const u8 *p, u16 len)
{
    u16 ihl;
    u16 total;
    u8 proto;
    u32 dst;

    if (len < 20u || (p[0] >> 4) != 4u) {
        return;
    }
    ihl = (u16)((p[0] & 0x0Fu) * 4u);
    if (ihl < 20u || len < ihl) {
        return;
    }
    total = get_u16(p + 2);
    if (total > len) {
        total = len;
    }
    dst = get_u32(p + 16);
    proto = p[9];
    if (proto == IP_ICMP && (dst == ip_addr || dst == 0xFFFFFFFFu)) {
        icmp_in(p, total, p + ihl, (u16)(total - ihl));
    } else if (proto == IP_UDP) {
        udp_in(p, p + ihl, (u16)(total - ihl));
    }
}

void net_poll(void)
{
    u8 frame[PKT_MAX];
    u16 n;
    u16 type;

    if (!nic_present()) {
        return;
    }
    for (;;) {
        n = nic_recv(frame, PKT_MAX);
        if (n < 14u) {
            break;
        }
        type = get_u16(frame + 12);
        if (type == ETH_ARP) {
            arp_in(frame + 14, (u16)(n - 14u), frame + 6);
        } else if (type == ETH_IP) {
            ip_in(frame + 14, (u16)(n - 14u));
        }
    }
}

static void dhcp_build(u8 *b, u8 mtype, u32 req_ip, u32 srv)
{
    const u8 *mac = nic_mac();
    u32 i;

    memset(b, 0, 300);
    b[0] = 1;
    b[1] = 1;
    b[2] = 6;
    put_u32(b + 4, dhcp_xid);
    put_u16(b + 10, 0x8000);
    memcpy(b + 28, mac, 6);
    b[236] = 99;
    b[237] = 130;
    b[238] = 83;
    b[239] = 99;
    i = 240;
    b[i++] = 53;
    b[i++] = 1;
    b[i++] = mtype;
    if (req_ip != 0u) {
        b[i++] = 50;
        b[i++] = 4;
        put_u32(b + i, req_ip);
        i += 4u;
    }
    if (srv != 0u) {
        b[i++] = 54;
        b[i++] = 4;
        put_u32(b + i, srv);
        i += 4u;
    }
    b[i++] = 55;
    b[i++] = 3;
    b[i++] = 1;
    b[i++] = 3;
    b[i++] = 6;
    b[i++] = 12;
    b[i++] = 6;
    memcpy(b + i, "cordos", 6);
    i += 6u;
    b[i++] = 255;

    {
        u8 udp[8 + 300];
        u8 ip[20 + 8 + 300];
        u16 ulen = (u16)(8u + i);

        memset(udp, 0, sizeof(udp));
        put_u16(udp + 0, DHCP_CLIENT);
        put_u16(udp + 2, DHCP_SERVER);
        put_u16(udp + 4, ulen);
        memcpy(udp + 8, b, i);

        memset(ip, 0, 20);
        ip[0] = 0x45;
        put_u16(ip + 2, (u16)(20u + ulen));
        put_u16(ip + 4, ip_id++);
        ip[8] = 64;
        ip[9] = IP_UDP;
        put_u32(ip + 16, 0xFFFFFFFFu);
        put_u16(ip + 10, cksum(ip, 20));
        memcpy(ip + 20, udp, ulen);
        eth_send(bcast_mac, ETH_IP, ip, (u16)(20u + ulen));
    }
}

static bool dhcp_wait(u32 ms)
{
    u32 start = time_uptime_ms();

    dhcp_got = false;
    while ((time_uptime_ms() - start) < ms) {
        net_poll();
        if (dhcp_got && dhcp_yiaddr != 0u) {
            return true;
        }
        __asm__ volatile("pause");
    }
    return false;
}

static bool dhcp_go(void)
{
    u8 bootp[300];

    dhcp_xid = (time_ticks() << 16) ^ 0xC0D05E01u;
    dhcp_yiaddr = 0;
    dhcp_server = 0;
    dhcp_mask = 0;
    dhcp_gw = 0;

    dhcp_build(bootp, DHCP_DISCOVER, 0, 0);
    if (!dhcp_wait(1200u)) {
        dhcp_build(bootp, DHCP_DISCOVER, 0, 0);
        if (!dhcp_wait(1200u)) {
            return false;
        }
    }
    dhcp_build(bootp, DHCP_REQUEST, dhcp_yiaddr, dhcp_server);
    dhcp_got = false;
    if (!dhcp_wait(1200u)) {
        return false;
    }
    ip_addr = dhcp_yiaddr;
    ip_mask = dhcp_mask != 0u ? dhcp_mask : 0xFFFFFF00u;
    ip_gw = dhcp_gw != 0u ? dhcp_gw : ((ip_addr & 0xFFFFFF00u) | 2u);
    configured = true;
    serial_write("net: dhcp ip=");
    serial_print_hex(ip_addr);
    serial_putc('\n');
    return true;
}

static void net_static_vbox(void)
{
    ip_addr = 0x0A00020Fu; /* 10.0.2.15 */
    ip_mask = 0xFFFFFF00u;
    ip_gw = 0x0A000202u;   /* 10.0.2.2 */
    configured = true;
    serial_write("net: static 10.0.2.15/24 gw 10.0.2.2\n");
}

void net_init(void)
{
    inited = true;
    configured = false;
    ip_addr = 0;
    ip_mask = 0;
    ip_gw = 0;
    ip_id = 1;
    ping_seq = 1;
    ping_id = 0xC0D0u;
    arp_ok = false;
    time_tsc_calibrate();
    if (!nic_present()) {
        serial_write("net: sin NIC con driver\n");
        return;
    }
    serial_write("net: NIC lista (DHCP al usar ping)\n");
}

bool net_present(void)
{
    return nic_present();
}

bool net_configured(void)
{
    return configured && ip_addr != 0u;
}

u32 net_local_ip(void)
{
    return ip_addr;
}

u32 net_gateway(void)
{
    return ip_gw;
}

u32 net_netmask(void)
{
    return ip_mask;
}

const u8 *net_mac(void)
{
    return nic_mac();
}

bool net_ensure_up(void)
{
    if (!inited) {
        net_init();
    }
    if (!nic_present()) {
        return false;
    }
    if (configured) {
        return true;
    }
    if (dhcp_go()) {
        return true;
    }
    net_static_vbox();
    return true;
}

int net_ping(u32 ip, u32 timeout_ms)
{
    u8 icmp[40];
    u8 dmac[6];
    u32 i;
    u32 next;
    u32 limit_us;
    u64 t0;

    if (!net_ensure_up() || ip == 0u) {
        return -1;
    }

    next = ip;
    if ((ip & ip_mask) != (ip_addr & ip_mask)) {
        next = ip_gw;
        if (next == 0u) {
            return -1;
        }
    }
    if (!arp_resolve(next, dmac, 400u)) {
        return -1;
    }
    (void)dmac;

    memset(icmp, 0, sizeof(icmp));
    icmp[0] = ICMP_ECHO;
    icmp[1] = 0;
    ping_want_seq = ping_seq++;
    put_u16(icmp + 4, ping_id);
    put_u16(icmp + 6, ping_want_seq);
    put_u32(icmp + 8, time_uptime_ms());
    for (i = 12; i < 40u; ++i) {
        icmp[i] = (u8)(i * 3u);
    }
    put_u16(icmp + 2, cksum(icmp, 40));

    ping_got = false;
    limit_us = timeout_ms * 1000u;
    t0 = time_tsc();
    ip_send(ip, IP_ICMP, icmp, 40);

    for (;;) {
        u32 us;

        net_poll();
        us = time_us_since(t0);
        if (ping_got) {
            return (int)us;
        }
        if (us >= limit_us) {
            return -1;
        }
        __asm__ volatile("pause");
    }
}

static void emit(net_line_fn out, void *ctx, const char *s)
{
    if (out != NULL) {
        out(s, ctx);
    }
}

void net_ping_run(u32 ip, u32 count, net_line_fn out, void *ctx)
{
    char line[64];
    char ips[20];
    u32 i;
    u32 ok;
    u32 n;

    if (count == 0u) {
        count = 4u;
    }
    if (count > 8u) {
        count = 8u;
    }
    if (!net_present()) {
        if (pci_has_class(PCI_CLASS_NET)) {
            emit(out, ctx, "NIC PCI sin driver (usa 82540EM)");
        } else {
            emit(out, ctx, "sin NIC. NAT 82540EM en VirtualBox");
        }
        return;
    }
    if (!net_ensure_up()) {
        emit(out, ctx, "sin enlace");
        return;
    }
    if (ip == 0u) {
        ip = ip_gw != 0u ? ip_gw : 0x0A000202u;
    }

    net_format_ip(ip, ips, sizeof(ips));
    line[0] = 'P';
    line[1] = 'I';
    line[2] = 'N';
    line[3] = 'G';
    line[4] = ' ';
    n = 5;
    i = 0;
    while (ips[i] != '\0' && n + 1u < sizeof(line)) {
        line[n++] = ips[i++];
    }
    line[n] = '\0';
    emit(out, ctx, line);

    ok = 0;
    for (i = 0; i < count; ++i) {
        int rtt = net_ping(ip, 1000u);
        n = 0;
        {
            u32 k = 0;

            while (ips[k] != '\0' && n + 1u < sizeof(line)) {
                line[n++] = ips[k++];
            }
        }
        if (rtt >= 0) {
            const char *mid = "  ";
            u32 us = (u32)rtt;
            char num[12];
            u32 nn = 0;

            while (*mid && n + 1u < sizeof(line)) {
                line[n++] = *mid++;
            }
            if (us < 1000u) {
                u32 tenth = us / 100u;

                if (n + 5u < sizeof(line)) {
                    line[n++] = '0';
                    line[n++] = '.';
                    line[n++] = (char)('0' + tenth);
                    line[n++] = 'm';
                    line[n++] = 's';
                }
            } else {
                u32 ms = (us + 500u) / 1000u;
                char dig[12];
                u32 dn = 0;

                if (ms == 0u) {
                    ms = 1u;
                }
                while (ms > 0u && dn < sizeof(dig)) {
                    dig[dn++] = (char)('0' + (ms % 10u));
                    ms /= 10u;
                }
                while (dn > 0u && nn + 1u < sizeof(num)) {
                    num[nn++] = dig[--dn];
                }
                num[nn] = '\0';
                nn = 0;
                while (num[nn] != '\0' && n + 1u < sizeof(line)) {
                    line[n++] = num[nn++];
                }
                if (n + 2u < sizeof(line)) {
                    line[n++] = 'm';
                    line[n++] = 's';
                }
            }
            line[n] = '\0';
            emit(out, ctx, line);
            ++ok;
        } else {
            const char *to = "  timeout";

            while (*to && n + 1u < sizeof(line)) {
                line[n++] = *to++;
            }
            line[n] = '\0';
            emit(out, ctx, line);
        }
        if (i + 1u < count) {
            delay_ms(8u);
        }
    }

    {
        char a[4];
        char b[4];
        u32 an = 0;
        u32 bn = 0;
        u32 v;
        char d[4];
        u32 dn;

        v = count;
        dn = 0;
        if (v == 0u) {
            a[an++] = '0';
        } else {
            while (v > 0u && dn < 3u) {
                d[dn++] = (char)('0' + (v % 10u));
                v /= 10u;
            }
            while (dn > 0u) {
                a[an++] = d[--dn];
            }
        }
        a[an] = '\0';
        v = ok;
        dn = 0;
        if (v == 0u) {
            b[bn++] = '0';
        } else {
            while (v > 0u && dn < 3u) {
                d[dn++] = (char)('0' + (v % 10u));
                v /= 10u;
            }
            while (dn > 0u) {
                b[bn++] = d[--dn];
            }
        }
        b[bn] = '\0';
        n = 0;
        {
            u32 k = 0;

            while (b[k] != '\0' && n + 1u < sizeof(line)) {
                line[n++] = b[k++];
            }
        }
        {
            const char *s = "/";

            while (*s && n + 1u < sizeof(line)) {
                line[n++] = *s++;
            }
        }
        {
            u32 k = 0;

            while (a[k] != '\0' && n + 1u < sizeof(line)) {
                line[n++] = a[k++];
            }
        }
        {
            const char *s = " ok";

            while (*s && n + 1u < sizeof(line)) {
                line[n++] = *s++;
            }
        }
        line[n] = '\0';
        emit(out, ctx, line);
    }
}

void net_status_run(net_line_fn out, void *ctx)
{
    char line[64];
    char ips[20];
    const u8 *mac;
    u32 i;
    u32 n;

    if (!net_present()) {
        if (pci_has_class(PCI_CLASS_NET)) {
            emit(out, ctx, "NIC PCI sin driver (usa 82540EM)");
        } else {
            emit(out, ctx, "sin NIC. NAT 82540EM en VirtualBox");
        }
        return;
    }
    (void)net_ensure_up();

    mac = nic_mac();
    n = 0;
    {
        const char *s = "mac ";

        while (*s && n + 1u < sizeof(line)) {
            line[n++] = *s++;
        }
    }
    for (i = 0; i < 6u; ++i) {
        static const char hex[] = "0123456789abcdef";

        if (n + 2u >= sizeof(line)) {
            break;
        }
        line[n++] = hex[(mac[i] >> 4) & 0xFu];
        line[n++] = hex[mac[i] & 0xFu];
        if (i + 1u < 6u && n + 1u < sizeof(line)) {
            line[n++] = ':';
        }
    }
    line[n] = '\0';
    emit(out, ctx, line);

    net_format_ip(ip_addr, ips, sizeof(ips));
    n = 0;
    {
        const char *s = "ip  ";

        while (*s && n + 1u < sizeof(line)) {
            line[n++] = *s++;
        }
    }
    i = 0;
    while (ips[i] != '\0' && n + 1u < sizeof(line)) {
        line[n++] = ips[i++];
    }
    line[n] = '\0';
    emit(out, ctx, line);

    net_format_ip(ip_gw, ips, sizeof(ips));
    n = 0;
    {
        const char *s = "gw  ";

        while (*s && n + 1u < sizeof(line)) {
            line[n++] = *s++;
        }
    }
    i = 0;
    while (ips[i] != '\0' && n + 1u < sizeof(line)) {
        line[n++] = ips[i++];
    }
    line[n] = '\0';
    emit(out, ctx, line);
}
