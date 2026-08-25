#include "pci.h"
#include "io.h"
#include "serial.h"
#include "vga.h"

#define PCI_CONFIG_ADDR 0xCF8u
#define PCI_CONFIG_DATA 0xCFCu
#define PCI_BUS_MAX     8u

u32 pci_config_read32(u8 bus, u8 slot, u8 func, u8 offset)
{
    u32 address;

    address = (1u << 31)
        | ((u32)bus << 16)
        | ((u32)slot << 11)
        | ((u32)func << 8)
        | ((u32)offset & 0xFCu);
    outl(PCI_CONFIG_ADDR, address);
    return inl(PCI_CONFIG_DATA);
}

void pci_config_write32(u8 bus, u8 slot, u8 func, u8 offset, u32 value)
{
    u32 address;

    address = (1u << 31)
        | ((u32)bus << 16)
        | ((u32)slot << 11)
        | ((u32)func << 8)
        | ((u32)offset & 0xFCu);
    outl(PCI_CONFIG_ADDR, address);
    outl(PCI_CONFIG_DATA, value);
}

u16 pci_config_read16(u8 bus, u8 slot, u8 func, u8 offset)
{
    u32 value = pci_config_read32(bus, slot, func, offset & 0xFCu);
    return (u16)((value >> ((offset & 2u) * 8u)) & 0xFFFFu);
}

u8 pci_config_read8(u8 bus, u8 slot, u8 func, u8 offset)
{
    u32 value = pci_config_read32(bus, slot, func, offset & 0xFCu);
    return (u8)((value >> ((offset & 3u) * 8u)) & 0xFFu);
}

static void pci_log_device(const struct pci_device *dev)
{
    serial_write("pci: ");
    serial_print_u32(dev->bus);
    serial_putc(':');
    serial_print_u32(dev->slot);
    serial_putc('.');
    serial_print_u32(dev->func);
    serial_write(" vend=");
    serial_print_hex(dev->vendor_id);
    serial_write(" dev=");
    serial_print_hex(dev->device_id);
    serial_write(" class=");
    serial_print_hex(dev->class_code);
    serial_putc('/');
    serial_print_hex(dev->subclass);
    serial_putc('\n');
}

static void pci_fill(u8 bus, u8 slot, u8 func, struct pci_device *out)
{
    u32 id_reg;
    u32 class_reg;

    id_reg = pci_config_read32(bus, slot, func, 0x00);
    class_reg = pci_config_read32(bus, slot, func, 0x08);

    out->bus = bus;
    out->slot = slot;
    out->func = func;
    out->vendor_id = (u16)(id_reg & 0xFFFFu);
    out->device_id = (u16)((id_reg >> 16) & 0xFFFFu);
    out->revision = (u8)(class_reg & 0xFFu);
    out->prog_if = (u8)((class_reg >> 8) & 0xFFu);
    out->subclass = (u8)((class_reg >> 16) & 0xFFu);
    out->class_code = (u8)((class_reg >> 24) & 0xFFu);
}

typedef bool (*pci_cb)(const struct pci_device *dev, void *ctx);

static bool pci_walk(pci_cb cb, void *ctx)
{
    u8 bus;
    u8 slot;
    u8 func;
    struct pci_device dev;
    u8 header_type;
    u8 max_func;

    for (bus = 0; bus < PCI_BUS_MAX; ++bus) {
        for (slot = 0; slot < 32; ++slot) {
            pci_fill(bus, slot, 0, &dev);
            if (dev.vendor_id == PCI_VENDOR_INVALID) {
                continue;
            }
            if (cb(&dev, ctx)) {
                return true;
            }
            header_type = pci_config_read8(bus, slot, 0, 0x0E);
            max_func = (header_type & 0x80u) ? 8u : 1u;
            for (func = 1; func < max_func; ++func) {
                pci_fill(bus, slot, func, &dev);
                if (dev.vendor_id == PCI_VENDOR_INVALID) {
                    continue;
                }
                if (cb(&dev, ctx)) {
                    return true;
                }
            }
        }
    }
    return false;
}

static bool pci_cb_log(const struct pci_device *dev, void *ctx)
{
    u32 *count = (u32 *)ctx;

    pci_log_device(dev);
    ++(*count);
    return false;
}

u32 pci_init(void)
{
    u32 count = 0;

    serial_write("pci: enumerating buses 0-7\n");
    (void)pci_walk(pci_cb_log, &count);
    serial_write("pci: found ");
    serial_print_u32(count);
    serial_write(" device(s)\n");
    return count;
}

struct pci_id_query {
    u16 vendor_id;
    u16 device_id;
    struct pci_device *out;
};

static bool pci_cb_id(const struct pci_device *dev, void *ctx)
{
    struct pci_id_query *q = (struct pci_id_query *)ctx;

    if (dev->vendor_id == q->vendor_id && dev->device_id == q->device_id) {
        *q->out = *dev;
        return true;
    }
    return false;
}

bool pci_find_device(u16 vendor_id, u16 device_id, struct pci_device *out)
{
    struct pci_id_query q;

    if (out == NULL) {
        return false;
    }
    q.vendor_id = vendor_id;
    q.device_id = device_id;
    q.out = out;
    return pci_walk(pci_cb_id, &q);
}

static bool pci_cb_class_any(const struct pci_device *dev, void *ctx)
{
    u8 want = *(u8 *)ctx;

    return dev->class_code == want;
}

bool pci_has_class(u8 class_code)
{
    return pci_walk(pci_cb_class_any, &class_code);
}

struct pci_class_query {
    u8 class_code;
    u8 subclass;
    u8 prog_if;
    struct pci_device *out;
};

static bool pci_cb_class(const struct pci_device *dev, void *ctx)
{
    struct pci_class_query *q = (struct pci_class_query *)ctx;

    if (dev->class_code != q->class_code || dev->subclass != q->subclass) {
        return false;
    }
    if (q->prog_if != PCI_PROG_ANY && dev->prog_if != q->prog_if) {
        return false;
    }
    *q->out = *dev;
    return true;
}

bool pci_find_class(u8 class_code, u8 subclass, u8 prog_if, struct pci_device *out)
{
    struct pci_class_query q;

    if (out == NULL) {
        return false;
    }
    q.class_code = class_code;
    q.subclass = subclass;
    q.prog_if = prog_if;
    q.out = out;
    return pci_walk(pci_cb_class, &q);
}

u32 pci_read_bar(const struct pci_device *dev, u8 bar)
{
    u8 off;

    if (dev == NULL || bar > 5u) {
        return 0;
    }
    off = (u8)(0x10u + bar * 4u);
    return pci_config_read32(dev->bus, dev->slot, dev->func, off);
}

void pci_enable_mem_busmaster(const struct pci_device *dev)
{
    u32 reg;

    if (dev == NULL) {
        return;
    }
    /* Status is RW1C in the high 16 bits — write 0 there so we do not
     * clear sticky error bits. */
    reg = pci_config_read32(dev->bus, dev->slot, dev->func, 0x04);
    reg = (reg & 0xFFFFu) | 0x0006u;
    pci_config_write32(dev->bus, dev->slot, dev->func, 0x04, reg);
}

void pci_enable_io_busmaster(const struct pci_device *dev)
{
    u32 reg;

    if (dev == NULL) {
        return;
    }
    reg = pci_config_read32(dev->bus, dev->slot, dev->func, 0x04);
    reg = (reg & 0xFFFFu) | 0x0005u; /* IO Space + Bus Master */
    pci_config_write32(dev->bus, dev->slot, dev->func, 0x04, reg);
}
