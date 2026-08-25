#ifndef NUEVOOS_NET_H
#define NUEVOOS_NET_H

#include "types.h"

void net_init(void);
bool net_present(void);
bool net_ensure_up(void);
bool net_configured(void);
void net_poll(void);

u32 net_local_ip(void);
u32 net_gateway(void);
u32 net_netmask(void);
const u8 *net_mac(void);

bool net_parse_ip(const char *text, u32 *out);
void net_format_ip(u32 ip, char *out, u32 max);

/* ICMP echo. Returns RTT in milliseconds, or -1 on timeout/error. */
int net_ping(u32 ip, u32 timeout_ms);

typedef void (*net_line_fn)(const char *line, void *ctx);

/* Four echoes by default when count is 0. Prints one line at a time. */
void net_ping_run(u32 ip, u32 count, net_line_fn out, void *ctx);
void net_status_run(net_line_fn out, void *ctx);

#endif
