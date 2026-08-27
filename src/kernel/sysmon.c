#include "sysmon.h"
#include "blk.h"
#include "e1000.h"
#include "fb.h"
#include "heap.h"
#include "net.h"
#include "nic.h"
#include "nosfs.h"
#include "pci.h"
#include "pcnet.h"
#include "persist.h"
#include "pmm.h"
#include "power.h"
#include "sched.h"
#include "smp.h"
#include "string.h"
#include "task.h"
#include "time.h"
#include "wlan.h"

#define SAMPLE_MS 250u

static struct sysmon_snap snap_os;
static u32 idle_ms_os;
static u32 idle_mark_os;
static bool idle_on_os;
static u32 last_ms_os;
static u32 last_ticks_os;
static bool primed_os;

static void put_dec(char *dst, u32 max, u32 v)
{
    char tmp[12];
    u32 n = 0;
    u32 i = 0;

    if (dst == NULL || max == 0) {
        return;
    }
    if (v == 0) {
        dst[0] = '0';
        dst[1] = '\0';
        return;
    }
    while (v > 0 && n < sizeof(tmp)) {
        tmp[n++] = (char)('0' + (v % 10u));
        v /= 10u;
    }
    while (n > 0 && i + 1u < max) {
        dst[i++] = tmp[--n];
    }
    dst[i] = '\0';
}

static void append(char *dst, u32 max, const char *src)
{
    u32 i = 0;
    u32 n;

    if (dst == NULL || max == 0) {
        return;
    }
    while (dst[i] != '\0' && i + 1u < max) {
        i++;
    }
    if (src == NULL) {
        return;
    }
    n = i;
    while (src[0] != '\0' && n + 1u < max) {
        dst[n++] = *src++;
    }
    dst[n] = '\0';
}

static void hist_push(u8 *hist, u8 value)
{
    u32 i;

    for (i = 0; i + 1u < SYSMON_HIST; i++) {
        hist[i] = hist[i + 1u];
    }
    hist[SYSMON_HIST - 1u] = value > 100u ? 100u : value;
}

static u32 task_live(void)
{
    u32 i;
    u32 n = 0;

    for (i = 0; i < TASK_MAX_OS; i++) {
        if (task_table_os[i].state != TASK_DEAD) {
            n++;
        }
    }
    return n;
}

static void fill_text(void)
{
    char nbuf[12];

    put_dec(snap_os.cpu_txt, sizeof(snap_os.cpu_txt), snap_os.cpu_pct);
    append(snap_os.cpu_txt, sizeof(snap_os.cpu_txt), "%");

    put_dec(snap_os.ram_txt, sizeof(snap_os.ram_txt), snap_os.ram_used_mib);
    append(snap_os.ram_txt, sizeof(snap_os.ram_txt), " / ");
    put_dec(nbuf, sizeof(nbuf), snap_os.ram_total_mib);
    append(snap_os.ram_txt, sizeof(snap_os.ram_txt), nbuf);
    append(snap_os.ram_txt, sizeof(snap_os.ram_txt), " MiB");

    put_dec(snap_os.fb_txt, sizeof(snap_os.fb_txt), snap_os.fb_w);
    append(snap_os.fb_txt, sizeof(snap_os.fb_txt), "x");
    put_dec(nbuf, sizeof(nbuf), snap_os.fb_h);
    append(snap_os.fb_txt, sizeof(snap_os.fb_txt), nbuf);

    {
        u32 s = snap_os.uptime_s;
        u32 h = s / 3600u;
        u32 m = (s / 60u) % 60u;
        u32 sec = s % 60u;

        snap_os.up_txt[0] = (char)('0' + (h / 10u) % 10u);
        snap_os.up_txt[1] = (char)('0' + (h % 10u));
        snap_os.up_txt[2] = ':';
        snap_os.up_txt[3] = (char)('0' + (m / 10u));
        snap_os.up_txt[4] = (char)('0' + (m % 10u));
        snap_os.up_txt[5] = ':';
        snap_os.up_txt[6] = (char)('0' + (sec / 10u));
        snap_os.up_txt[7] = (char)('0' + (sec % 10u));
        snap_os.up_txt[8] = '\0';
    }
}

void sysmon_idle_begin(void)
{
    if (idle_on_os) {
        return;
    }
    idle_on_os = true;
    idle_mark_os = time_uptime_ms();
}

void sysmon_idle_end(void)
{
    u32 now;
    u32 dt;

    if (!idle_on_os) {
        return;
    }
    idle_on_os = false;
    now = time_uptime_ms();
    dt = now - idle_mark_os;
    if (dt > 4000u) {
        dt = 4000u;
    }
    idle_ms_os += dt;
}

bool sysmon_poll(void)
{
    u32 now = time_uptime_ms();
    u32 dt;
    u32 ticks;
    u32 ram_pages;
    u64 fb_bytes;
    const char *ssid;

    if (!primed_os) {
        last_ms_os = now;
        last_ticks_os = time_ticks();
        primed_os = true;
        return false;
    }
    dt = now - last_ms_os;
    if (dt < SAMPLE_MS) {
        return false;
    }

    if (idle_on_os) {
        sysmon_idle_end();
        sysmon_idle_begin();
    }

    snap_os.cpu_pct = 0;
    if (dt > 0) {
        u32 busy;

        if (idle_ms_os > dt) {
            idle_ms_os = dt;
        }
        busy = dt - idle_ms_os;
        snap_os.cpu_pct = (busy * 100u) / dt;
        if (snap_os.cpu_pct > 100u) {
            snap_os.cpu_pct = 100u;
        }
    }
    idle_ms_os = 0;

    ticks = time_ticks();
    snap_os.ticks_hz = dt > 0 ? ((ticks - last_ticks_os) * 1000u) / dt : 0;
    last_ticks_os = ticks;
    last_ms_os = now;

    ram_pages = total_frames_os;
    snap_os.ram_total_mib = (ram_pages * (PAGE_SIZE / 1024u)) / 1024u;
    snap_os.ram_used_mib =
        ((used_frames_os * (PAGE_SIZE / 1024u)) / 1024u);
    snap_os.ram_pct = ram_pages == 0
                          ? 0
                          : (used_frames_os * 100u) / ram_pages;
    if (snap_os.ram_pct > 100u) {
        snap_os.ram_pct = 100u;
    }

    snap_os.heap_used_kib = heap_used_os / 1024u;
    snap_os.heap_free_kib = heap_free_os / 1024u;
    snap_os.tasks = task_live();
    snap_os.cpus = cpu_count_os == 0u ? 1u : cpu_count_os;
    snap_os.preempt = sched_enabled_os != 0u;
    snap_os.uptime_s = time_uptime_ms() / 1000u;
    snap_os.pci_n = pci_device_count();
    snap_os.blk_n = blk_count();
    snap_os.persist = persist_available() || nosfs_disk_ready();
    snap_os.bat_pct = power_battery_percent();
    snap_os.ac = power_on_ac();

    snap_os.fb_w = fb_width();
    snap_os.fb_h = fb_height();
    snap_os.fb_bpp = fb_bpp();
    fb_bytes = (u64)fb_pitch() * (u64)fb_height();
    snap_os.fb_mib = (u32)((fb_bytes + 1024u * 1024u - 1u) / (1024u * 1024u));

    snap_os.net_nic = nic_present();
    snap_os.net_cfg = net_configured();
    snap_os.net_link = false;
    snap_os.nic_txt[0] = '\0';
    snap_os.ip_txt[0] = '\0';
    if (e1000_present()) {
        snap_os.net_link = e1000_link_up();
        memcpy(snap_os.nic_txt, "e1000", 6);
    } else if (pcnet_present()) {
        snap_os.net_link = true;
        memcpy(snap_os.nic_txt, "pcnet", 6);
    }
    if (snap_os.net_cfg) {
        net_format_ip(net_local_ip(), snap_os.ip_txt, sizeof(snap_os.ip_txt));
    }

    snap_os.wlan = wlan_connected();
    snap_os.ssid_txt[0] = '\0';
    ssid = wlan_connected_ssid();
    if (snap_os.wlan && ssid != NULL) {
        u32 i = 0;

        while (ssid[i] != '\0' && i + 1u < sizeof(snap_os.ssid_txt)) {
            snap_os.ssid_txt[i] = ssid[i];
            i++;
        }
        snap_os.ssid_txt[i] = '\0';
    }

    hist_push(snap_os.cpu_hist, (u8)snap_os.cpu_pct);
    hist_push(snap_os.ram_hist, (u8)snap_os.ram_pct);
    {
        u8 net_lv = 0;
        u8 disk_lv = snap_os.persist ? 40u : 12u;

        if (snap_os.net_nic) {
            net_lv = 10u;
        }
        if (snap_os.net_link) {
            net_lv = 62u;
        }
        if (snap_os.wlan) {
            net_lv = 78u;
        }
        hist_push(snap_os.net_hist, net_lv);
        hist_push(snap_os.disk_hist, disk_lv);
    }
    fill_text();
    return true;
}

const struct sysmon_snap *sysmon_get(void)
{
    return &snap_os;
}
