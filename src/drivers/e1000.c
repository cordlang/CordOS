#include "e1000.h"
#include "pci.h"
#include "pmm.h"
#include "serial.h"
#include "string.h"
#include "time.h"
#include "vmm.h"

#define NIC_MMIO_BASE 0xFFFF800010000000ull

#define REG_CTRL   0x00000u
#define REG_STATUS 0x00008u
#define REG_EERD   0x00014u
#define REG_ICR    0x000C0u
#define REG_IMS    0x000D0u
#define REG_IMC    0x000D8u
#define REG_RCTL   0x00100u
#define REG_TCTL   0x00400u
#define REG_ITR    0x000C4u
#define REG_TIPG   0x00410u
#define REG_RDBAL  0x02800u
#define REG_RDBAH  0x02804u
#define REG_RDLEN  0x02808u
#define REG_RDH    0x02810u
#define REG_RDT    0x02818u
#define REG_RDTR   0x02820u
#define REG_RXDCTL 0x02828u
#define REG_RADV   0x0282Cu
#define REG_TDBAL  0x03800u
#define REG_TDBAH  0x03804u
#define REG_TDLEN  0x03808u
#define REG_TDH    0x03810u
#define REG_TDT    0x03818u
#define REG_TIDV   0x03820u
#define REG_TXDCTL 0x03828u
#define REG_TADV   0x0382Cu
#define REG_MTA    0x05200u
#define REG_RAL    0x05400u
#define REG_RAH    0x05404u

#define CTRL_FD    (1u << 0)
#define CTRL_SLU   (1u << 6)
#define CTRL_ASDE  (1u << 5)
#define CTRL_RST   (1u << 26)
#define STATUS_LU  (1u << 1)

#define RCTL_EN    (1u << 1)
#define RCTL_BAM   (1u << 15)
#define RCTL_SECRC (1u << 26)
#define RCTL_UPE   (1u << 3)
#define RCTL_MPE   (1u << 4)

#define TCTL_EN    (1u << 1)
#define TCTL_PSP   (1u << 3)

#define RX_DD  0x01u
#define RX_EOP 0x02u
#define TX_CMD (0x01u | 0x02u | 0x08u) /* EOP | IFCS | RS */
#define TX_DD  0x01u

#define E1000_RX   32u
#define E1000_TX   16u
#define E1000_BUF  2048u

struct rx_desc {
    volatile u64 addr;
    volatile u16 length;
    volatile u16 csum;
    volatile u8 status;
    volatile u8 errors;
    volatile u16 special;
} __attribute__((packed));

struct tx_desc {
    volatile u64 addr;
    volatile u16 length;
    volatile u8 cso;
    volatile u8 cmd;
    volatile u8 status;
    volatile u8 css;
    volatile u16 special;
} __attribute__((packed));

static volatile u32 *mmio;
static bool present;
static u8 mac[6];
static u32 rx_i;
static u32 tx_i;

static struct rx_desc rx_ring[E1000_RX] __attribute__((aligned(16)));
static struct tx_desc tx_ring[E1000_TX] __attribute__((aligned(16)));
static u8 rx_buf[E1000_RX][E1000_BUF] __attribute__((aligned(16)));
static u8 tx_buf[E1000_TX][E1000_BUF] __attribute__((aligned(16)));
static volatile u32 fence_word;

static void dma_fence(void)
{
    __asm__ volatile("lock addl $0, %0" : "+m"(fence_word) : : "memory");
}

static void delay_ms(u32 ms)
{
    u32 start = time_uptime_ms();

    while ((time_uptime_ms() - start) < ms) {
        __asm__ volatile("hlt");
    }
}

static u32 er32(u32 reg)
{
    return mmio[reg / 4u];
}

static void ew32(u32 reg, u32 value)
{
    mmio[reg / 4u] = value;
}

static bool map_bar(u64 phys, u32 size)
{
    u64 off;

    if (size < 4096u) {
        size = 4096u;
    }
    for (off = 0; off < size; off += PAGE_SIZE) {
        vmm_map_page(NIC_MMIO_BASE + off, (phys + off) & ~0xFFFull,
                     PAGE_PRESENT | PAGE_WRITE | PAGE_PCD);
    }
    mmio = (volatile u32 *)NIC_MMIO_BASE;
    return true;
}

static u16 eeprom_try(u8 addr, u32 shift)
{
    u32 i;
    u32 v;

    ew32(REG_EERD, ((u32)addr << shift) | 1u);
    for (i = 0; i < 10000u; ++i) {
        v = er32(REG_EERD);
        if ((v & (1u << 4)) != 0u) {
            return (u16)(v >> 16);
        }
        __asm__ volatile("pause");
    }
    return 0xFFFFu;
}

static bool read_mac(void)
{
    u16 w0;
    u16 w1;
    u16 w2;
    u32 ral;
    u32 rah;

    w0 = eeprom_try(0, 8);
    if (w0 == 0xFFFFu) {
        w0 = eeprom_try(0, 2);
    }
    if (w0 != 0xFFFFu) {
        w1 = eeprom_try(1, 8);
        if (w1 == 0xFFFFu) {
            w1 = eeprom_try(1, 2);
        }
        w2 = eeprom_try(2, 8);
        if (w2 == 0xFFFFu) {
            w2 = eeprom_try(2, 2);
        }
        mac[0] = (u8)(w0 & 0xFFu);
        mac[1] = (u8)(w0 >> 8);
        mac[2] = (u8)(w1 & 0xFFu);
        mac[3] = (u8)(w1 >> 8);
        mac[4] = (u8)(w2 & 0xFFu);
        mac[5] = (u8)(w2 >> 8);
        if (mac[0] != 0xFFu || mac[1] != 0xFFu) {
            return true;
        }
    }

    ral = er32(REG_RAL);
    rah = er32(REG_RAH);
    mac[0] = (u8)(ral & 0xFFu);
    mac[1] = (u8)((ral >> 8) & 0xFFu);
    mac[2] = (u8)((ral >> 16) & 0xFFu);
    mac[3] = (u8)((ral >> 24) & 0xFFu);
    mac[4] = (u8)(rah & 0xFFu);
    mac[5] = (u8)((rah >> 8) & 0xFFu);
    return mac[0] != 0u || mac[1] != 0u || mac[2] != 0u;
}

static bool probe(struct pci_device *out)
{
    static const u16 ids[] = {
        0x1004u, 0x100Eu, 0x100Fu, 0x1010u, 0x1026u, 0x107Cu, 0x109Au,
        0x10D3u, 0x10F5u, 0x1209u, 0x1502u, 0x1533u, 0
    };
    u32 i;

    for (i = 0; ids[i] != 0u; ++i) {
        if (pci_find_device(0x8086u, ids[i], out)) {
            return true;
        }
    }
    if (pci_find_class(PCI_CLASS_NET, PCI_SUBCLASS_ETH, PCI_PROG_ANY, out) &&
        out->vendor_id == 0x8086u) {
        return true;
    }
    return false;
}

bool e1000_init(void)
{
    struct pci_device dev;
    u32 bar;
    u64 phys;
    u32 i;
    u32 rah;

    present = false;
    mmio = NULL;
    rx_i = 0;
    tx_i = 0;
    memset(mac, 0, sizeof(mac));

    if (!probe(&dev)) {
        serial_write("e1000: no detectado\n");
        return false;
    }

    bar = pci_read_bar(&dev, 0);
    if ((bar & 1u) != 0u) {
        serial_write("e1000: BAR0 IO\n");
        return false;
    }
    phys = (u64)(bar & ~0xFul);
    if ((bar & 0x6u) == 0x4u) {
        phys |= ((u64)pci_read_bar(&dev, 1)) << 32;
    }
    pci_enable_mem_busmaster(&dev);
    {
        u32 cmd = pci_config_read32(dev.bus, dev.slot, dev.func, 0x04);

        cmd = (cmd & 0xFFFFu) | 0x0406u; /* MEM + BM + INTX disable */
        pci_config_write32(dev.bus, dev.slot, dev.func, 0x04, cmd);
    }
    if (!map_bar(phys, 128u * 1024u)) {
        return false;
    }

    ew32(REG_IMC, 0xFFFFFFFFu);
    (void)er32(REG_ICR);
    ew32(REG_CTRL, er32(REG_CTRL) | CTRL_RST);
    delay_ms(4u);
    ew32(REG_IMC, 0xFFFFFFFFu);
    ew32(REG_ITR, 0);
    ew32(REG_CTRL, er32(REG_CTRL) | CTRL_SLU | CTRL_ASDE | CTRL_FD);

    if (!read_mac()) {
        mac[0] = 0x02u;
        mac[1] = 0x00u;
        mac[2] = 0x00u;
        mac[3] = 0xC0u;
        mac[4] = 0xD0u;
        mac[5] = 0x01u;
    }

    ew32(REG_RAL, (u32)mac[0] | ((u32)mac[1] << 8) | ((u32)mac[2] << 16) |
                      ((u32)mac[3] << 24));
    rah = (u32)mac[4] | ((u32)mac[5] << 8) | (1u << 31);
    ew32(REG_RAH, rah);

    for (i = 0; i < 128u; ++i) {
        ew32(REG_MTA + i * 4u, 0);
    }

    memset((void *)rx_ring, 0, sizeof(rx_ring));
    memset((void *)tx_ring, 0, sizeof(tx_ring));
    for (i = 0; i < E1000_RX; ++i) {
        rx_ring[i].addr = (u64)(u32)(u64)&rx_buf[i][0];
        rx_ring[i].status = 0;
    }
    for (i = 0; i < E1000_TX; ++i) {
        tx_ring[i].addr = (u64)(u32)(u64)&tx_buf[i][0];
        tx_ring[i].status = TX_DD;
    }

    ew32(REG_RDBAL, (u32)(u64)rx_ring);
    ew32(REG_RDBAH, 0);
    ew32(REG_RDLEN, E1000_RX * 16u);
    ew32(REG_RDH, 0);
    ew32(REG_RDT, E1000_RX - 1u);

    ew32(REG_TDBAL, (u32)(u64)tx_ring);
    ew32(REG_TDBAH, 0);
    ew32(REG_TDLEN, E1000_TX * 16u);
    ew32(REG_TDH, 0);
    ew32(REG_TDT, 0);

    ew32(REG_RDTR, 0);
    ew32(REG_RADV, 0);
    ew32(REG_TIDV, 0);
    ew32(REG_TADV, 0);
    /* Write back each descriptor immediately (GRAN + WTHRESH=1). */
    ew32(REG_RXDCTL, (1u << 24) | (1u << 16));
    ew32(REG_TXDCTL, (1u << 24) | (1u << 16));
    ew32(REG_TIPG, 8u | (8u << 10) | (6u << 20));
    ew32(REG_TCTL, TCTL_EN | TCTL_PSP | (0x10u << 4) | (0x40u << 12));
    /* Unicast + broadcast only. Promiscuous mode wasted RX slots. */
    ew32(REG_RCTL, RCTL_EN | RCTL_BAM | RCTL_SECRC);

    delay_ms(8u);
    present = true;

    serial_write("e1000: mac=");
    for (i = 0; i < 6u; ++i) {
        serial_print_hex(mac[i]);
        if (i + 1u < 6u) {
            serial_putc(':');
        }
    }
    serial_write(" link=");
    serial_putc(e1000_link_up() ? '1' : '0');
    serial_putc('\n');
    return true;
}

bool e1000_present(void)
{
    return present;
}

const u8 *e1000_mac(void)
{
    return mac;
}

bool e1000_link_up(void)
{
    if (!present || mmio == NULL) {
        return false;
    }
    return (er32(REG_STATUS) & STATUS_LU) != 0u;
}

bool e1000_send(const void *data, u16 length)
{
    struct tx_desc *d;
    u32 spins;

    if (!present || data == NULL || length == 0u) {
        return false;
    }
    if (length > 1514u) {
        length = 1514u;
    }

    d = &tx_ring[tx_i];
    spins = 100000u;
    while ((d->status & TX_DD) == 0u && spins > 0u) {
        --spins;
        __asm__ volatile("pause");
    }
    if ((d->status & TX_DD) == 0u) {
        return false;
    }

    memcpy(tx_buf[tx_i], data, length);
    if (length < 60u) {
        memset(tx_buf[tx_i] + length, 0, 60u - length);
        length = 60u;
    }
    d->length = length;
    d->cso = 0;
    d->css = 0;
    d->special = 0;
    d->status = 0;
    d->cmd = TX_CMD;
    dma_fence();
    tx_i = (tx_i + 1u) % E1000_TX;
    ew32(REG_TDT, tx_i);
    return true;
}

u16 e1000_recv(void *buf, u16 max)
{
    struct rx_desc *d;
    u16 n;

    if (!present || buf == NULL || max == 0u) {
        return 0;
    }

    d = &rx_ring[rx_i];
    if ((d->status & RX_DD) == 0u) {
        return 0;
    }
    dma_fence();
    n = d->length;
    if (n > max) {
        n = max;
    }
    if (n > 0u && d->errors == 0u) {
        memcpy(buf, rx_buf[rx_i], n);
    } else {
        n = 0;
    }
    d->status = 0;
    __asm__ volatile("" ::: "memory");
    ew32(REG_RDT, rx_i);
    rx_i = (rx_i + 1u) % E1000_RX;
    return n;
}
