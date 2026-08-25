#include "nic.h"
#include "e1000.h"
#include "pcnet.h"
#include "pci.h"
#include "serial.h"

static const u8 *active_mac(void)
{
    if (e1000_present()) {
        return e1000_mac();
    }
    if (pcnet_present()) {
        return pcnet_mac();
    }
    return NULL;
}

bool nic_init(void)
{
    struct pci_device dev;

    if (e1000_init()) {
        return true;
    }
    if (pcnet_init()) {
        return true;
    }

    if (pci_find_class(PCI_CLASS_NET, PCI_SUBCLASS_ETH, PCI_PROG_ANY, &dev)) {
        serial_write("nic: Ethernet PCI sin driver vend=");
        serial_print_hex(dev.vendor_id);
        serial_write(" dev=");
        serial_print_hex(dev.device_id);
        serial_putc('\n');
    } else if (pci_has_class(PCI_CLASS_NET)) {
        serial_write("nic: PCI class 02 presente, no es Ethernet clasico\n");
    } else {
        serial_write("nic: no hay adaptador PCI de red\n");
    }
    return false;
}

bool nic_present(void)
{
    return e1000_present() || pcnet_present();
}

const u8 *nic_mac(void)
{
    const u8 *m = active_mac();
    static const u8 z[6];

    return m != NULL ? m : z;
}

bool nic_send(const void *data, u16 length)
{
    if (e1000_present()) {
        return e1000_send(data, length);
    }
    if (pcnet_present()) {
        return pcnet_send(data, length);
    }
    return false;
}

u16 nic_recv(void *buf, u16 max)
{
    if (e1000_present()) {
        return e1000_recv(buf, max);
    }
    if (pcnet_present()) {
        return pcnet_recv(buf, max);
    }
    return 0;
}
