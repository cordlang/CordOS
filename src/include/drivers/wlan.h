#ifndef CORDOS_WLAN_H
#define CORDOS_WLAN_H

#include "types.h"

#define WLAN_SSID_MAX 32u
#define WLAN_MAX_BSS  12u
#define WLAN_PASS_MAX 64u

enum wlan_sec {
    WLAN_SEC_OPEN = 0,
    WLAN_SEC_WEP  = 1,
    WLAN_SEC_WPA  = 2
};

enum wlan_src {
    WLAN_SRC_RADIO = 0, /* USB/PCI 802.11 NIC */
    WLAN_SRC_HOST  = 1  /* paravirtual radio (host Wi-Fi via COM2) */
};

struct wlan_bss {
    char ssid[WLAN_SSID_MAX + 1u];
    u8 bssid[6];
    u8 quality; /* 0–100 */
    u8 sec;
    u8 channel;
    u8 src;
};

void wlan_init(void);
u32 wlan_scan(void);
u32 wlan_count(void);
const struct wlan_bss *wlan_get(u32 index);
bool wlan_connect(u32 index, const char *pass);
bool wlan_connected(void);
bool wlan_radio_present(void);
const char *wlan_connected_ssid(void);

/* Used by radio backends to publish a BSS. Returns false if the table is full. */
bool wlan_add_bss(const struct wlan_bss *bss);

#endif
