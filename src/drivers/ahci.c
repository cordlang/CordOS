#include "ahci.h"
#include "blk.h"
#include "pci.h"
#include "pmm.h"
#include "serial.h"
#include "string.h"
#include "time.h"
#include "vmm.h"

/*
 * AHCI host (SATA). Typical internal HDD/SSD on a real PC since ~2005.
 * MMIO at 0xFFFF800030000000 — after FB / e1000 / EHCI windows.
 */

#define AHCI_MMIO_BASE 0xFFFF800030000000ull
#define AHCI_MMIO_PAGES 4u

#define AHCI_MAX_SLOT 4u

#define GHC_IE   (1u << 1)
#define GHC_AE   (1u << 31)

#define CAP2_BOH 1u

#define BOHC_BOS (1u << 0)
#define BOHC_OOS (1u << 1)
#define BOHC_BB  (1u << 4)

#define PX_CLB   0x00u
#define PX_CLBU  0x04u
#define PX_FB    0x08u
#define PX_FBU   0x0Cu
#define PX_IS    0x10u
#define PX_IE    0x14u
#define PX_CMD   0x18u
#define PX_TFD   0x20u
#define PX_SIG   0x24u
#define PX_SSTS  0x28u
#define PX_SCTL  0x2Cu
#define PX_SERR  0x30u
#define PX_SACT  0x34u
#define PX_CI    0x38u

#define CMD_ST   (1u << 0)
#define CMD_SUD  (1u << 1)
#define CMD_POD  (1u << 2)
#define CMD_CLO  (1u << 3)
#define CMD_FRE  (1u << 4)
#define CMD_FR   (1u << 14)
#define CMD_CR   (1u << 15)

#define TFD_ERR  (1u << 0)
#define TFD_DRQ  (1u << 3)
#define TFD_BSY  (1u << 7)

#define IS_TFES  (1u << 30)

#define SSTS_DET 0x0Fu
#define DET_NONE 0u
#define DET_PRESENT 3u

#define SIG_ATAPI 0xEB14u

#define ATA_CMD_IDENT  0xECu
#define ATA_CMD_READ   0xC8u
#define ATA_CMD_WRITE  0xCAu
#define ATA_CMD_READ48 0x25u
#define ATA_CMD_WRITE48 0x35u
#define ATA_CMD_FLUSH  0xE7u
#define ATA_CMD_FLUSH48 0xEAu

struct ahci_slot {
    u32 index;
    u32 port;
    u32 sectors;
    int lba48;
};

static volatile u32 *hba;
static struct ahci_slot s_slot[AHCI_MAX_SLOT];
static u32 s_nslot;

static u8 s_cl[AHCI_MAX_SLOT][1024] __attribute__((aligned(1024)));
static u8 s_rfis[AHCI_MAX_SLOT][256] __attribute__((aligned(256)));
static u8 s_ct[AHCI_MAX_SLOT][256] __attribute__((aligned(128)));
static u8 s_dma[512] __attribute__((aligned(512)));

static void fence(void)
{
    __asm__ volatile("mfence" ::: "memory");
}

static void delay_ms(u32 ms)
{
    u32 start = time_uptime_ms();

    while ((time_uptime_ms() - start) < ms) {
        __asm__ volatile("pause");
    }
}

static u32 phys32(const void *p)
{
    u64 a = (u64)(u32)(u64)p;

    if (a == 0) {
        return 0;
    }
    return (u32)a;
}

static u32 hr32(u32 off)
{
    return hba[off / 4u];
}

static void hw32(u32 off, u32 value)
{
    hba[off / 4u] = value;
}

static volatile u32 *preg(u32 port)
{
    return (volatile u32 *)((u8 *)hba + 0x100u + port * 0x80u);
}

static u32 pr32(u32 port, u32 off)
{
    return preg(port)[off / 4u];
}

static void pw32(u32 port, u32 off, u32 value)
{
    preg(port)[off / 4u] = value;
}

static int wait_clear32(volatile u32 *reg, u32 bit, u32 ms)
{
    u32 start = time_uptime_ms();

    while ((*reg & bit) != 0u) {
        if ((time_uptime_ms() - start) > ms) {
            return -1;
        }
        __asm__ volatile("pause");
    }
    return 0;
}

static bool map_bar(u64 phys, u32 size)
{
    u64 off;

    if (size < 4096u) {
        size = 4096u;
    }
    for (off = 0; off < size; off += PAGE_SIZE) {
        vmm_map_page(AHCI_MMIO_BASE + off, (phys + off) & ~0xFFFull,
                     PAGE_PRESENT | PAGE_WRITE | PAGE_PCD);
    }
    hba = (volatile u32 *)AHCI_MMIO_BASE;
    return true;
}

static int stop_port(u32 port)
{
    u32 cmd = pr32(port, PX_CMD);

    cmd &= ~(CMD_ST);
    pw32(port, PX_CMD, cmd);
    if (wait_clear32(&preg(port)[PX_CMD / 4u], CMD_CR, 500u) < 0) {
        return -1;
    }
    cmd = pr32(port, PX_CMD);
    cmd &= ~CMD_FRE;
    pw32(port, PX_CMD, cmd);
    if (wait_clear32(&preg(port)[PX_CMD / 4u], CMD_FR, 500u) < 0) {
        return -1;
    }
    return 0;
}

static int start_port(u32 slot, u32 port)
{
    u32 cl = phys32(s_cl[slot]);
    u32 fis = phys32(s_rfis[slot]);
    u32 cmd;
    u32 tfd;
    u32 start;

    if (cl == 0 || fis == 0) {
        return -1;
    }

    memset(s_cl[slot], 0, sizeof(s_cl[slot]));
    memset(s_rfis[slot], 0, sizeof(s_rfis[slot]));
    memset(s_ct[slot], 0, sizeof(s_ct[slot]));

    pw32(port, PX_CLB, cl);
    pw32(port, PX_CLBU, 0);
    pw32(port, PX_FB, fis);
    pw32(port, PX_FBU, 0);
    pw32(port, PX_IE, 0);
    pw32(port, PX_IS, 0xFFFFFFFFu);
    pw32(port, PX_SERR, 0xFFFFFFFFu);

    tfd = pr32(port, PX_TFD);
    if ((tfd & (TFD_BSY | TFD_DRQ)) != 0u) {
        cmd = pr32(port, PX_CMD) | CMD_CLO;
        pw32(port, PX_CMD, cmd);
        if (wait_clear32(&preg(port)[PX_CMD / 4u], CMD_CLO, 500u) < 0) {
            return -1;
        }
    }

    cmd = pr32(port, PX_CMD);
    cmd |= CMD_FRE | CMD_POD | CMD_SUD;
    pw32(port, PX_CMD, cmd);
    delay_ms(1);
    cmd = pr32(port, PX_CMD) | CMD_ST;
    pw32(port, PX_CMD, cmd);

    start = time_uptime_ms();
    while ((pr32(port, PX_TFD) & TFD_BSY) != 0u) {
        if ((time_uptime_ms() - start) > 500u) {
            return -1;
        }
        __asm__ volatile("pause");
    }
    return 0;
}

static void fill_h2d(u8 *fis, u8 cmd, u64 lba, u16 count, int lba48)
{
    memset(fis, 0, 64);
    fis[0] = 0x27u;
    fis[1] = 0x80u;
    fis[2] = cmd;
    fis[4] = (u8)(lba & 0xFFu);
    fis[5] = (u8)((lba >> 8) & 0xFFu);
    fis[6] = (u8)((lba >> 16) & 0xFFu);
    fis[7] = 0x40u;
    fis[12] = (u8)(count & 0xFFu);
    if (lba48) {
        fis[8] = (u8)((lba >> 24) & 0xFFu);
        fis[9] = (u8)((lba >> 32) & 0xFFu);
        fis[10] = (u8)((lba >> 40) & 0xFFu);
        fis[13] = (u8)((count >> 8) & 0xFFu);
    } else {
        fis[7] = (u8)(0x40u | ((lba >> 24) & 0x0Fu));
    }
}

static int issue(u32 slot, u8 cmd, u64 lba, u16 count, int write, int data, int lba48)
{
    u32 port = s_slot[slot].port;
    u32 ct = phys32(s_ct[slot]);
    u32 dma = phys32(s_dma);
    u32 *hdr;
    u32 *prdt;
    u32 start;
    u32 dw0;

    if (ct == 0 || (data && dma == 0)) {
        return -1;
    }

    memset(s_ct[slot], 0, sizeof(s_ct[slot]));
    fill_h2d(s_ct[slot], cmd, lba, count, lba48);

    if (data) {
        prdt = (u32 *)(s_ct[slot] + 0x80);
        prdt[0] = dma;
        prdt[1] = 0;
        prdt[2] = 0;
        prdt[3] = (511u) | (1u << 31);
    }

    memset(s_cl[slot], 0, 32);
    hdr = (u32 *)s_cl[slot];
    dw0 = 5u;
    if (write) {
        dw0 |= (1u << 6);
    }
    if (data) {
        dw0 |= (1u << 16);
    }
    hdr[0] = dw0;
    hdr[2] = ct;
    hdr[3] = 0;
    fence();

    pw32(port, PX_IS, 0xFFFFFFFFu);
    pw32(port, PX_CI, 1u);

    start = time_uptime_ms();
    for (;;) {
        u32 ci = pr32(port, PX_CI);
        u32 is = pr32(port, PX_IS);
        u32 tfd = pr32(port, PX_TFD);

        if ((is & IS_TFES) != 0u || (tfd & TFD_ERR) != 0u) {
            pw32(port, PX_IS, 0xFFFFFFFFu);
            return -1;
        }
        if ((ci & 1u) == 0u) {
            break;
        }
        if ((time_uptime_ms() - start) > 2000u) {
            return -1;
        }
        __asm__ volatile("pause");
    }
    fence();
    return 0;
}

static int identify(u32 slot, u32 *out_sectors, int *out_lba48)
{
    u16 ident[256];
    u32 lba28;
    u64 lba48;
    u16 w83;

    memset(s_dma, 0, sizeof(s_dma));
    fence();
    if (issue(slot, ATA_CMD_IDENT, 0, 1, 0, 1, 0) < 0) {
        return -1;
    }
    memcpy(ident, s_dma, sizeof(ident));

    if ((ident[0] & 0x8000u) != 0u) {
        return -1;
    }

    lba28 = (u32)ident[60] | ((u32)ident[61] << 16);
    w83 = ident[83];
    *out_lba48 = 0;
    if ((w83 & 0xC000u) == 0x4000u && (w83 & 0x0400u) != 0u) {
        *out_lba48 = 1;
    }
    lba48 = (u64)ident[100] | ((u64)ident[101] << 16) |
            ((u64)ident[102] << 32) | ((u64)ident[103] << 48);

    if (*out_lba48 && lba48 != 0) {
        if (lba48 > 0xFFFFFFFFull) {
            *out_sectors = 0xFFFFFFFFu;
        } else {
            *out_sectors = (u32)lba48;
        }
    } else {
        *out_sectors = lba28;
    }
    if (*out_sectors == 0) {
        return -1;
    }
    return 0;
}

static int ahci_rw(u32 slot, u32 lba, u32 count, void *buf, int write)
{
    u8 *p = (u8 *)buf;
    u32 i;
    int lba48 = s_slot[slot].lba48;
    u8 cmd;

    if (count == 0) {
        return 0;
    }
    if (lba + count < lba || lba + count > s_slot[slot].sectors) {
        return -1;
    }

    for (i = 0; i < count; ++i) {
        if (write) {
            memcpy(s_dma, p + i * BLK_SECTOR_SIZE, BLK_SECTOR_SIZE);
            fence();
            cmd = lba48 ? ATA_CMD_WRITE48 : ATA_CMD_WRITE;
        } else {
            memset(s_dma, 0, BLK_SECTOR_SIZE);
            fence();
            cmd = lba48 ? ATA_CMD_READ48 : ATA_CMD_READ;
        }
        if (issue(slot, cmd, (u64)lba + i, 1, write, 1, lba48) < 0) {
            return -1;
        }
        if (!write) {
            memcpy(p + i * BLK_SECTOR_SIZE, s_dma, BLK_SECTOR_SIZE);
        }
    }
    if (write) {
        cmd = lba48 ? ATA_CMD_FLUSH48 : ATA_CMD_FLUSH;
        if (issue(slot, cmd, 0, 0, 0, 0, lba48) < 0) {
            return -1;
        }
    }
    return 0;
}

static int ahci_blk_read(void *ctx, u32 lba, u32 count, void *buf)
{
    const struct ahci_slot *s = (const struct ahci_slot *)ctx;

    if (s == NULL || s->index >= s_nslot || buf == NULL) {
        return -1;
    }
    return ahci_rw(s->index, lba, count, buf, 0);
}

static int ahci_blk_write(void *ctx, u32 lba, u32 count, const void *buf)
{
    const struct ahci_slot *s = (const struct ahci_slot *)ctx;

    if (s == NULL || s->index >= s_nslot || buf == NULL) {
        return -1;
    }
    return ahci_rw(s->index, lba, count, (void *)buf, 1);
}

static void comreset(u32 port)
{
    u32 sctl = pr32(port, PX_SCTL);

    pw32(port, PX_SCTL, (sctl & ~0x0Fu) | 1u);
    delay_ms(2);
    pw32(port, PX_SCTL, sctl & ~0x0Fu);
    delay_ms(2);
}

static int probe_port(u32 port)
{
    u32 det;
    u32 sig;
    u32 sectors = 0;
    int lba48 = 0;
    u32 slot;
    char name[8];

    det = pr32(port, PX_SSTS) & SSTS_DET;
    if (det == DET_NONE) {
        return -1;
    }
    if (det != DET_PRESENT) {
        comreset(port);
        det = pr32(port, PX_SSTS) & SSTS_DET;
        if (det != DET_PRESENT) {
            return -1;
        }
    }

    sig = pr32(port, PX_SIG);
    if ((sig >> 16) == SIG_ATAPI) {
        serial_write("ahci: p");
        serial_print_u32(port);
        serial_write(" ATAPI skip\n");
        return -1;
    }

    if (s_nslot >= AHCI_MAX_SLOT) {
        return -1;
    }
    slot = s_nslot;
    s_slot[slot].index = slot;
    s_slot[slot].port = port;
    s_slot[slot].sectors = 0;
    s_slot[slot].lba48 = 0;

    if (stop_port(port) < 0 || start_port(slot, port) < 0) {
        serial_write("ahci: p");
        serial_print_u32(port);
        serial_write(" start fail\n");
        return -1;
    }
    if (identify(slot, &sectors, &lba48) < 0) {
        (void)stop_port(port);
        serial_write("ahci: p");
        serial_print_u32(port);
        serial_write(" IDENTIFY fail\n");
        return -1;
    }

    s_slot[slot].sectors = sectors;
    s_slot[slot].lba48 = lba48;
    s_nslot++;

    name[0] = 's';
    name[1] = 'a';
    name[2] = 't';
    name[3] = 'a';
    name[4] = (char)('0' + (int)slot);
    name[5] = '\0';
    if (blk_register(name, sectors, ahci_blk_read, ahci_blk_write,
                     &s_slot[slot]) < 0) {
        s_nslot--;
        (void)stop_port(port);
        return -1;
    }
    return 0;
}

static void bios_handoff(void)
{
    u32 cap2;
    u32 start;

    cap2 = hr32(0x24u);
    if ((cap2 & CAP2_BOH) == 0u) {
        return;
    }
    hw32(0x28u, hr32(0x28u) | BOHC_OOS);
    start = time_uptime_ms();
    while ((hr32(0x28u) & (BOHC_BOS | BOHC_BB)) != 0u) {
        if ((time_uptime_ms() - start) > 2000u) {
            serial_write("ahci: BIOS handoff timeout\n");
            return;
        }
        __asm__ volatile("pause");
    }
}

void ahci_init(void)
{
    struct pci_device dev;
    u32 bar;
    u64 phys;
    u32 pi;
    u32 ghc;
    u32 p;

    s_nslot = 0;
    hba = NULL;
    memset(s_slot, 0, sizeof(s_slot));

    if (!pci_find_class(PCI_CLASS_STORAGE, PCI_SUBCLASS_SATA, PCI_PROG_AHCI,
                        &dev)) {
        if (!pci_find_class(PCI_CLASS_STORAGE, PCI_SUBCLASS_SATA, PCI_PROG_ANY,
                            &dev) ||
            dev.prog_if != PCI_PROG_AHCI) {
            serial_write("ahci: no HBA\n");
            return;
        }
    }

    pci_enable_mem_busmaster(&dev);
    bar = pci_read_bar(&dev, 5);
    if ((bar & 1u) != 0u) {
        serial_write("ahci: BAR5 I/O\n");
        return;
    }
    phys = (u64)(bar & ~0xFu);
    if ((bar & 4u) != 0u) {
        phys |= ((u64)pci_config_read32(dev.bus, dev.slot, dev.func, 0x28u)) << 32;
    }
    if (phys == 0) {
        serial_write("ahci: BAR5 empty\n");
        return;
    }
    if (!map_bar(phys, AHCI_MMIO_PAGES * PAGE_SIZE)) {
        return;
    }

    serial_write("ahci: ");
    serial_print_hex(dev.vendor_id);
    serial_putc(':');
    serial_print_hex(dev.device_id);
    serial_write(" ABAR=");
    serial_print_hex((u32)phys);
    serial_putc('\n');

    bios_handoff();

    ghc = hr32(0x04u);
    ghc |= GHC_AE;
    ghc &= ~GHC_IE;
    hw32(0x04u, ghc);

    pi = hr32(0x0Cu);

    for (p = 0; p < 32u && s_nslot < AHCI_MAX_SLOT; ++p) {
        if ((pi & (1u << p)) == 0u) {
            continue;
        }
        (void)probe_port(p);
    }

    if (s_nslot == 0) {
        serial_write("ahci: no HDD\n");
    }
}
