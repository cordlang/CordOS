#include "wlan.h"
#include "wlan_host.h"
#include "rtl8187.h"
#include "usb.h"
#include "pci.h"
#include "serial.h"
#include "string.h"

static struct wlan_bss table[WLAN_MAX_BSS];
static u32 count;
static bool inited;
static bool connected;
static bool radio;
static char connected_ssid[WLAN_SSID_MAX + 1u];

static bool same_ssid(const char *a, const char *b)
{
    u32 i;

    for (i = 0; i <= WLAN_SSID_MAX; ++i) {
        if (a[i] != b[i]) {
            return false;
        }
        if (a[i] == '\0') {
            return true;
        }
    }
    return true;
}

bool wlan_add_bss(const struct wlan_bss *bss)
{
    u32 i;

    if (bss == NULL || bss->ssid[0] == '\0') {
        return false;
    }
    for (i = 0; i < count; ++i) {
        if (same_ssid(table[i].ssid, bss->ssid)) {
            if (bss->quality > table[i].quality) {
                table[i].quality = bss->quality;
            }
            if (bss->src == WLAN_SRC_RADIO) {
                table[i].src = WLAN_SRC_RADIO;
                memcpy(table[i].bssid, bss->bssid, 6);
            }
            if (bss->sec > table[i].sec) {
                table[i].sec = bss->sec;
            }
            return true;
        }
    }
    if (count >= WLAN_MAX_BSS) {
        return false;
    }
    table[count] = *bss;
    ++count;
    return true;
}

void wlan_init(void)
{
    struct pci_device dev;

    count = 0;
    connected = false;
    radio = false;
    connected_ssid[0] = '\0';
    inited = true;

    (void)wlan_host_init();
    (void)usb_ehci_init();
    if (rtl8187_init()) {
        radio = true;
    }

    if (pci_find_class(PCI_CLASS_NET, PCI_SUBCLASS_WIFI, PCI_PROG_ANY, &dev)) {
        serial_write("wlan: NIC 802.11 PCI vend=");
        serial_print_hex(dev.vendor_id);
        serial_write(" dev=");
        serial_print_hex(dev.device_id);
        serial_putc('\n');
        radio = true;
    }

    serial_write("wlan: radio=");
    serial_putc(radio ? '1' : '0');
    serial_write(" host=");
    serial_putc(wlan_host_present() ? '1' : '0');
    serial_putc('\n');
}

u32 wlan_scan(void)
{
    count = 0;
    connected = false;

    if (!inited) {
        wlan_init();
    }

    if (rtl8187_present()) {
        (void)rtl8187_scan();
    }
    /* Host radio fills in the SSIDs the VM cannot see on its own
     * (VirtualBox has no 802.11 NIC). Prefer it when the USB radio
     * found nothing. */
    if (count == 0u) {
        (void)wlan_host_scan();
    }

    serial_write("wlan: scan total=");
    serial_print_u32(count);
    serial_putc('\n');
    return count;
}

u32 wlan_count(void)
{
    return count;
}

const struct wlan_bss *wlan_get(u32 index)
{
    if (index >= count) {
        return NULL;
    }
    return &table[index];
}

bool wlan_connect(u32 index, const char *pass)
{
    const struct wlan_bss *b;

    connected = false;
    connected_ssid[0] = '\0';
    b = wlan_get(index);
    if (b == NULL) {
        return false;
    }
    if (pass == NULL) {
        pass = "";
    }

    if (b->src == WLAN_SRC_RADIO) {
        if (!rtl8187_associate(b->ssid, pass, b->sec)) {
            return false;
        }
    } else if (b->src == WLAN_SRC_HOST) {
        if (!wlan_host_connect(b->ssid, pass)) {
            return false;
        }
    } else {
        return false;
    }

    {
        u32 i;

        for (i = 0; b->ssid[i] != '\0' && i < WLAN_SSID_MAX; ++i) {
            connected_ssid[i] = b->ssid[i];
        }
        connected_ssid[i] = '\0';
    }
    connected = true;
    serial_write("wlan: asociado a ");
    serial_write(connected_ssid);
    serial_putc('\n');
    return true;
}

bool wlan_connected(void)
{
    return connected;
}

bool wlan_radio_present(void)
{
    return radio || wlan_host_present() || rtl8187_present();
}

const char *wlan_connected_ssid(void)
{
    return connected_ssid;
}
