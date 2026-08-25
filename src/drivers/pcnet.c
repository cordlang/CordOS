#include "pcnet.h"
#include "io.h"
#include "pci.h"
#include "serial.h"
#include "string.h"
#include "time.h"

#define PCNET_VID  0x1022u
#define PCNET_DID  0x2000u
#define PCNET_RX   8u
#define PCNET_TX   8u
#define PCNET_BUF  1544u

struct pcnet_desc {
    volatile u32 addr;
    volatile i16 length;
    volatile u16 status;
    volatile u32 misc;
    volatile u32 reserved;
} __attribute__((packed));

struct pcnet_init {
    u16 mode;
    u8 rlen;
    u8 tlen;
    u8 mac[6];
    u16 reserved;
    u16 ladr[4];
    u32 rx_ring;
    u32 tx_ring;
} __attribute__((packed));

static bool present;
static u16 io;
static u8 mac[6];
static u32 rx_i;
static u32 tx_i;
static struct pcnet_desc rx_ring[PCNET_RX] __attribute__((aligned(16)));
static struct pcnet_desc tx_ring[PCNET_TX] __attribute__((aligned(16)));
static u8 rx_buf[PCNET_RX][PCNET_BUF] __attribute__((aligned(16)));
static u8 tx_buf[PCNET_TX][PCNET_BUF] __attribute__((aligned(16)));
static struct pcnet_init init_block __attribute__((aligned(16)));

static void delay_ms(u32 ms)
{
    u32 start = time_uptime_ms();

    while ((time_uptime_ms() - start) < ms) {
        __asm__ volatile("hlt");
    }
}

static void rap(u32 v)
{
    outl(io + 0x14u, v);
}

static void csr_write(u32 csr, u32 v)
{
    rap(csr);
    outl(io + 0x10u, v);
}

static u32 csr_read(u32 csr)
{
    rap(csr);
    return inl(io + 0x10u);
}

static void bcr_write(u32 bcr, u32 v)
{
    rap(bcr);
    outl(io + 0x1Cu, v);
}

bool pcnet_init(void)
{
    struct pci_device dev;
    u32 bar;
    u32 i;
    u32 spins;

    present = false;
    io = 0;
    rx_i = 0;
    tx_i = 0;

    if (!pci_find_device(PCNET_VID, PCNET_DID, &dev)) {
        return false;
    }
    bar = pci_read_bar(&dev, 0);
    if ((bar & 1u) == 0u) {
        serial_write("pcnet: BAR0 no es IO\n");
        return false;
    }
    io = (u16)(bar & ~3u);
    pci_enable_io_busmaster(&dev);

    (void)inw(io + 0x14u); /* reset */
    delay_ms(1u);
    outl(io + 0x10u, 0);   /* 32-bit mode */
    csr_write(0, 4u);      /* STOP */

    for (i = 0; i < 6u; ++i) {
        mac[i] = inb((u16)(io + (u16)i));
    }

    memset((void *)rx_ring, 0, sizeof(rx_ring));
    memset((void *)tx_ring, 0, sizeof(tx_ring));
    for (i = 0; i < PCNET_RX; ++i) {
        rx_ring[i].addr = (u32)(u64)&rx_buf[i][0];
        rx_ring[i].length = (i16)(-((i16)PCNET_BUF));
        rx_ring[i].status = 0x8000u; /* OWN */
        rx_ring[i].misc = 0;
    }
    for (i = 0; i < PCNET_TX; ++i) {
        tx_ring[i].addr = (u32)(u64)&tx_buf[i][0];
        tx_ring[i].length = 0;
        tx_ring[i].status = 0;
        tx_ring[i].misc = 0;
    }

    memset(&init_block, 0, sizeof(init_block));
    init_block.mode = 0;
    init_block.rlen = (u8)(3u << 4); /* log2(8) */
    init_block.tlen = (u8)(3u << 4);
    memcpy(init_block.mac, mac, 6);
    init_block.rx_ring = (u32)(u64)rx_ring;
    init_block.tx_ring = (u32)(u64)tx_ring;

    bcr_write(20u, 2u); /* SWSTYLE 2 */
    bcr_write(2u, 2u);  /* ASEL */

    csr_write(1u, (u32)(u64)&init_block & 0xFFFFu);
    csr_write(2u, ((u32)(u64)&init_block >> 16) & 0xFFFFu);
    csr_write(4u, 0x0915u);
    csr_write(0u, 0x0001u); /* INIT */

    spins = 100000u;
    while ((csr_read(0) & 0x0100u) == 0u && spins > 0u) {
        --spins;
        __asm__ volatile("pause");
    }
    csr_write(0u, 0x0002u); /* STRT */

    present = true;
    serial_write("pcnet: mac=");
    for (i = 0; i < 6u; ++i) {
        serial_print_hex(mac[i]);
        if (i + 1u < 6u) {
            serial_putc(':');
        }
    }
    serial_putc('\n');
    return true;
}

bool pcnet_present(void)
{
    return present;
}

const u8 *pcnet_mac(void)
{
    return mac;
}

bool pcnet_send(const void *data, u16 length)
{
    struct pcnet_desc *d;
    u32 spins;

    if (!present || data == NULL || length == 0u) {
        return false;
    }
    if (length > 1514u) {
        length = 1514u;
    }
    d = &tx_ring[tx_i];
    spins = 100000u;
    while ((d->status & 0x8000u) != 0u && spins > 0u) {
        --spins;
        __asm__ volatile("pause");
    }
    if ((d->status & 0x8000u) != 0u) {
        return false;
    }
    memcpy(tx_buf[tx_i], data, length);
    if (length < 60u) {
        memset(tx_buf[tx_i] + length, 0, 60u - length);
        length = 60u;
    }
    d->addr = (u32)(u64)&tx_buf[tx_i][0];
    d->length = (i16)(-((i16)length));
    d->misc = 0;
    d->status = 0x8300u; /* OWN | STP | ENP */
    __asm__ volatile("" ::: "memory");
    csr_write(0u, 0x000Au); /* STRT | TDMD */
    tx_i = (tx_i + 1u) % PCNET_TX;
    return true;
}

u16 pcnet_recv(void *buf, u16 max)
{
    struct pcnet_desc *d;
    u16 n;

    if (!present || buf == NULL) {
        return 0;
    }
    d = &rx_ring[rx_i];
    if ((d->status & 0x8000u) != 0u) {
        return 0;
    }
    n = (u16)(d->misc & 0xFFFu);
    if (n > max) {
        n = max;
    }
    if (n > 0u) {
        memcpy(buf, rx_buf[rx_i], n);
    }
    d->length = (i16)(-((i16)PCNET_BUF));
    d->misc = 0;
    d->status = 0x8000u;
    rx_i = (rx_i + 1u) % PCNET_RX;
    return n;
}
