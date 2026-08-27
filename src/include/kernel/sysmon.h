#ifndef CORDOS_SYSMON_H
#define CORDOS_SYSMON_H

#include "types.h"

#define SYSMON_HIST 48u

struct sysmon_snap {
    u32 cpu_pct;
    u32 ram_pct;
    u32 ram_used_mib;
    u32 ram_total_mib;
    u32 heap_used_kib;
    u32 heap_free_kib;
    u32 tasks;
    u32 cpus;
    u32 ticks_hz;
    u32 uptime_s;
    u32 fb_w;
    u32 fb_h;
    u32 fb_bpp;
    u32 fb_mib;
    u32 pci_n;
    u32 blk_n;
    u32 bat_pct;
    bool preempt;
    bool persist;
    bool net_nic;
    bool net_link;
    bool net_cfg;
    bool wlan;
    bool ac;
    char cpu_txt[12];
    char ram_txt[28];
    char fb_txt[24];
    char up_txt[16];
    char ip_txt[16];
    char nic_txt[12];
    char ssid_txt[33];
    u8 cpu_hist[SYSMON_HIST];
    u8 ram_hist[SYSMON_HIST];
};

void sysmon_idle_begin(void);
void sysmon_idle_end(void);
/* True when a new 250 ms sample landed. */
bool sysmon_poll(void);
const struct sysmon_snap *sysmon_get(void);

#endif
