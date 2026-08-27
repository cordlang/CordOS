#ifndef CORDOS_PCI_H
#define CORDOS_PCI_H

#include "types.h"

#define PCI_VENDOR_INVALID 0xFFFFu
#define PCI_CLASS_STORAGE  0x01u
#define PCI_CLASS_NET      0x02u
#define PCI_CLASS_SERIAL   0x0Cu
#define PCI_SUBCLASS_IDE   0x01u
#define PCI_SUBCLASS_SATA  0x06u
#define PCI_PROG_AHCI      0x01u
#define PCI_SUBCLASS_USB   0x03u
#define PCI_SUBCLASS_ETH   0x00u
#define PCI_SUBCLASS_WIFI  0x80u
#define PCI_PROG_UHCI      0x00u
#define PCI_PROG_OHCI      0x10u
#define PCI_PROG_EHCI      0x20u
#define PCI_PROG_XHCI      0x30u
#define PCI_PROG_ANY       0xFFu

struct pci_device {
    u8 bus;
    u8 slot;
    u8 func;
    u16 vendor_id;
    u16 device_id;
    u8 class_code;
    u8 subclass;
    u8 prog_if;
    u8 revision;
};

u32 pci_config_read32(u8 bus, u8 slot, u8 func, u8 offset);
u16 pci_config_read16(u8 bus, u8 slot, u8 func, u8 offset);
u8 pci_config_read8(u8 bus, u8 slot, u8 func, u8 offset);
void pci_config_write32(u8 bus, u8 slot, u8 func, u8 offset, u32 value);

/* Enumerate buses 0–7; log devices on serial. Returns count found. */
u32 pci_init(void);
/* Last pci_init() count (0 before init). */
u32 pci_device_count(void);

/* Optional lookup helper used by virtio-net. */
bool pci_find_device(u16 vendor_id, u16 device_id, struct pci_device *out);

/* True if any bus has a device of this PCI class (0x02 = network). */
bool pci_has_class(u8 class_code);

/* prog_if == PCI_PROG_ANY matches any programming interface. */
bool pci_find_class(u8 class_code, u8 subclass, u8 prog_if, struct pci_device *out);

u32 pci_read_bar(const struct pci_device *dev, u8 bar);
void pci_enable_mem_busmaster(const struct pci_device *dev);
void pci_enable_io_busmaster(const struct pci_device *dev);

#endif
