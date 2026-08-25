#include "virtio_net.h"
#include "pci.h"
#include "serial.h"
#include "vga.h"

bool virtio_net_present_os = false;

bool virtio_net_init(void)
{
    struct pci_device dev;
    bool found = false;

    virtio_net_present_os = false;

    if (pci_find_device(VIRTIO_VENDOR_ID, VIRTIO_NET_DEVICE_LEGACY, &dev)) {
        found = true;
        serial_write("virtio-net: presente (legacy 0x1000) @ ");
    } else if (pci_find_device(VIRTIO_VENDOR_ID, VIRTIO_NET_DEVICE_MODERN, &dev)) {
        found = true;
        serial_write("virtio-net: presente (modern 0x1041) @ ");
    }

    if (!found) {
        serial_write("virtio-net: no detectado en bus 0\n");
        return false;
    }

    serial_print_u32(dev.bus);
    serial_putc(':');
    serial_print_u32(dev.slot);
    serial_putc('.');
    serial_print_u32(dev.func);
    serial_putc('\n');

    virtio_net_present_os = true;

    /* Full virtqueue / net stack is out of MVP scope. */
    return true;
}
