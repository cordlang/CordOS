#ifndef CORDOS_WLAN_HOST_H
#define CORDOS_WLAN_HOST_H

#include "types.h"

bool wlan_host_init(void);
bool wlan_host_present(void);
u32 wlan_host_scan(void);
bool wlan_host_connect(const char *ssid, const char *pass);

#endif
