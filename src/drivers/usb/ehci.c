#include "usb.h"
#include "pci.h"
#include "pmm.h"
#include "serial.h"
#include "string.h"
#include "time.h"
#include "vmm.h"

/*
 * Kernel MMIO windows (high canonical half). Keep these far apart and above
 * the framebuffer window, which starts at FB_VIRT_BASE 0xFFFF800000100000
 * and grows with the mode: a 1920x1080x32 surface already spans ~10 MiB, and
 * an 8K surface spans ~128 MiB.
 *
 *   0xFFFF800000100000  framebuffer  (fb.c, size depends on the video mode)
 *   0xFFFF800010000000  e1000 NIC    (e1000.c)
 *   0xFFFF800020000000  EHCI USB     (here)
 *   0xFFFF800030000000  AHCI SATA    (ahci.c)
 *
 * This used to be 0xFFFF800000800000, which is FB_VIRT_BASE + 0x700000 —
 * exactly inside the framebuffer window. Mapping the BAR there silently
 * replaced one framebuffer page-table entry, so every write to that 4 KiB
 * page of the screen went to USB registers instead of VRAM. The result was
 * a permanently stale 1px band (row 954/955 at 1920x1080) that no redraw or
 * repair pass could ever fix, because the pixels never reached VRAM.
 */
#define USB_MMIO_BASE 0xFFFF800020000000ull
#define USB_MAX_DEV   4u
#define QTD_TERM      0x1u

#define USBCMD     0x00u
#define USBSTS     0x04u
#define USBINTR    0x08u
#define FRINDEX    0x0Cu
#define CTRLDSSEG  0x10u
#define PERIODIC   0x14u
#define ASYNCLIST  0x18u
#define CONFIGFLAG 0x40u
#define PORTSC0    0x44u

#define CMD_RS      (1u << 0)
#define CMD_HCRESET (1u << 1)
#define CMD_ASE     (1u << 5)
#define STS_HCHALTED (1u << 12)

#define PORT_CCS    (1u << 0)
#define PORT_PED    (1u << 2)
#define PORT_PR     (1u << 8)
#define PORT_PP     (1u << 12)
#define PORT_OWNER  (1u << 13)

static volatile u32 *op;
static bool ehci_ok;
static u32 n_ports;
static u32 n_dev;
static struct usb_device devices[USB_MAX_DEV];

static u8 dma_page[4096] __attribute__((aligned(4096)));

static void delay_ms(u32 ms)
{
    u32 start = time_uptime_ms();

    while ((time_uptime_ms() - start) < ms) {
        __asm__ volatile("hlt");
    }
}

static u32 r32(u32 off)
{
    return op[off / 4u];
}

static void w32(u32 off, u32 value)
{
    op[off / 4u] = value;
}

static bool wait_clear(u32 off, u32 bit, u32 ms)
{
    u32 start = time_uptime_ms();

    while ((r32(off) & bit) != 0u) {
        if ((time_uptime_ms() - start) > ms) {
            return false;
        }
        __asm__ volatile("pause");
    }
    return true;
}

static bool wait_set(u32 off, u32 bit, u32 ms)
{
    u32 start = time_uptime_ms();

    while ((r32(off) & bit) == 0u) {
        if ((time_uptime_ms() - start) > ms) {
            return false;
        }
        __asm__ volatile("pause");
    }
    return true;
}

static u32 dma_phys(const void *p)
{
    return (u32)(u64)p;
}

struct qtd {
    volatile u32 next;
    volatile u32 alt;
    volatile u32 token;
    volatile u32 buf[5];
};

struct qh {
    volatile u32 horiz;
    volatile u32 epchar;
    volatile u32 epcap;
    volatile u32 current;
    volatile u32 overlay[8];
};

static struct qh *qh_head;
static struct qh *qh_xfer;
static struct qtd *qtd[3];
static u8 *ctrl_buf;

static void qtd_fill(struct qtd *t, u32 pid, const void *data, u16 len, u32 dt)
{
    u32 i;

    memset((void *)t, 0, sizeof(*t));
    t->next = QTD_TERM;
    t->alt = QTD_TERM;
    t->token = ((u32)len << 16) | (3u << 10) | (pid << 8) | (dt << 31) | 0x80u;
    if (data != NULL && len > 0u) {
        t->buf[0] = dma_phys(data);
        for (i = 1; i < 5u; ++i) {
            t->buf[i] = 0;
        }
    }
}

static bool qh_run(struct qh *qh, u32 n_qtd, u32 ms)
{
    u32 start;
    u32 i;

    qh->current = 0;
    memset((void *)qh->overlay, 0, sizeof(qh->overlay));
    qh->overlay[0] = dma_phys(qtd[0]);
    w32(ASYNCLIST, dma_phys(qh_head));

    start = time_uptime_ms();
    for (;;) {
        bool active = false;
        bool halted = false;
        u32 last = 0;

        for (i = 0; i < n_qtd; ++i) {
            u32 token = qtd[i]->token;

            last = token;
            if ((token & 0x80u) != 0u) {
                active = true;
            }
            if ((token & 0x4Cu) != 0u) {
                halted = true;
            }
        }
        if (halted) {
            return false;
        }
        if (!active) {
            return (last & 0x7Cu) == 0u;
        }
        if ((time_uptime_ms() - start) > ms) {
            return false;
        }
        __asm__ volatile("pause");
    }
}

int usb_ctrl(u8 addr, u8 reqtype, u8 req, u16 value, u16 index, void *data,
             u16 length)
{
    u8 setup[8];
    u32 pid_data;
    u32 n = 0;
    u16 maxpkt = 64;
    u32 i;

    if (!ehci_ok) {
        return -1;
    }

    setup[0] = reqtype;
    setup[1] = req;
    setup[2] = (u8)(value & 0xFFu);
    setup[3] = (u8)(value >> 8);
    setup[4] = (u8)(index & 0xFFu);
    setup[5] = (u8)(index >> 8);
    setup[6] = (u8)(length & 0xFFu);
    setup[7] = (u8)(length >> 8);

    for (i = 0; i < n_dev; ++i) {
        if (devices[i].addr == addr && devices[i].maxpkt != 0u) {
            maxpkt = devices[i].maxpkt;
            break;
        }
    }

    memcpy(ctrl_buf, setup, 8);
    if (data != NULL && length > 0u && (reqtype & 0x80u) == 0u) {
        memcpy(ctrl_buf + 64, data, length);
    }

    qtd_fill(qtd[0], 2u, ctrl_buf, 8, 0); /* SETUP, DATA0 */
    if (length > 0u) {
        pid_data = (reqtype & 0x80u) ? 1u : 0u;
        qtd_fill(qtd[1], pid_data, ctrl_buf + 64, length, 1);
        qtd[0]->next = dma_phys(qtd[1]);
        qtd_fill(qtd[2], (reqtype & 0x80u) ? 0u : 1u, NULL, 0, 1);
        qtd[1]->next = dma_phys(qtd[2]);
        n = 3;
    } else {
        qtd_fill(qtd[1], 1u, NULL, 0, 1); /* STATUS IN */
        qtd[0]->next = dma_phys(qtd[1]);
        n = 2;
    }

    memset((void *)qh_xfer, 0, sizeof(*qh_xfer));
    qh_xfer->horiz = dma_phys(qh_head) | 0x2u;
    qh_xfer->epchar = ((u32)maxpkt << 16) | (2u << 12) | (1u << 14) | addr;
    qh_xfer->epcap = (1u << 30);
    qh_head->horiz = dma_phys(qh_xfer) | 0x2u;

    if (!qh_run(qh_xfer, n, 250u)) {
        qh_head->horiz = dma_phys(qh_head) | 0x2u;
        return -1;
    }
    qh_head->horiz = dma_phys(qh_head) | 0x2u;

    if (data != NULL && length > 0u && (reqtype & 0x80u) != 0u) {
        memcpy(data, ctrl_buf + 64, length);
    }
    return 0;
}

int usb_bulk(u8 addr, u8 ep, bool in, void *data, u16 length)
{
    u16 maxpkt = 512;
    u32 i;
    u8 *bounce = ctrl_buf + 128;

    if (!ehci_ok || data == NULL) {
        return -1;
    }
    for (i = 0; i < n_dev; ++i) {
        if (devices[i].addr == addr) {
            maxpkt = devices[i].high_speed ? 512u : 64u;
            break;
        }
    }
    if (length > 512u) {
        length = 512u;
    }
    if (!in) {
        memcpy(bounce, data, length);
    }

    qtd_fill(qtd[0], in ? 1u : 0u, bounce, length, 0);

    memset((void *)qh_xfer, 0, sizeof(*qh_xfer));
    qh_xfer->horiz = dma_phys(qh_head) | 0x2u;
    qh_xfer->epchar = ((u32)maxpkt << 16) | (2u << 12) | ((u32)ep << 8) | addr;
    qh_xfer->epcap = (1u << 30);
    qh_head->horiz = dma_phys(qh_xfer) | 0x2u;

    if (!qh_run(qh_xfer, 1u, 400u)) {
        qh_head->horiz = dma_phys(qh_head) | 0x2u;
        return -1;
    }
    qh_head->horiz = dma_phys(qh_head) | 0x2u;
    if (in) {
        u32 left = (qtd[0]->token >> 16) & 0x7FFFu;
        u16 got = (u16)(length - (u16)left);

        memcpy(data, bounce, got);
        return (int)got;
    }
    return (int)length;
}

static bool map_bar(u64 phys, u32 size)
{
    u64 off;

    if (size < 4096u) {
        size = 4096u;
    }
    for (off = 0; off < size; off += PAGE_SIZE) {
        vmm_map_page(USB_MMIO_BASE + off, (phys + off) & ~0xFFFull,
                     PAGE_PRESENT | PAGE_WRITE | PAGE_PCD);
    }
    return true;
}

static bool port_reset(u32 port)
{
    u32 sc = r32(PORTSC0 + port * 4u);

    if ((sc & PORT_CCS) == 0u) {
        return false;
    }
    w32(PORTSC0 + port * 4u, (sc | PORT_PP) & ~PORT_OWNER);
    delay_ms(10u);
    sc = r32(PORTSC0 + port * 4u);
    w32(PORTSC0 + port * 4u, (sc | PORT_PR) & ~PORT_PED);
    delay_ms(50u);
    sc = r32(PORTSC0 + port * 4u);
    w32(PORTSC0 + port * 4u, sc & ~PORT_PR);
    if (!wait_clear(PORTSC0 + port * 4u, PORT_PR, 50u)) {
        return false;
    }
    delay_ms(10u);
    sc = r32(PORTSC0 + port * 4u);
    if ((sc & PORT_PED) == 0u) {
        /* Low/full speed: release to companion. */
        w32(PORTSC0 + port * 4u, sc | PORT_OWNER);
        return false;
    }
    return true;
}

static bool parse_config(const u8 *cfg, u16 len, struct usb_device *dev)
{
    u16 off = 0;
    bool want = false;

    dev->ep_in = 0;
    dev->ep_out = 0;
    while (off + 2u <= len) {
        u8 dlen = cfg[off];
        u8 dtype;

        if (dlen < 2u) {
            break;
        }
        if ((u16)off + dlen > len) {
            break;
        }
        dtype = cfg[off + 1u];
        if (dtype == 4u && dlen >= 9u) {
            /* interface: wireless 0xE0 or vendor 0xFF */
            want = (cfg[off + 5u] == 0xE0u || cfg[off + 5u] == 0xFFu);
        } else if (dtype == 5u && dlen >= 7u && want) {
            u8 addr = cfg[off + 2u];
            u8 attr = cfg[off + 3u];

            if ((attr & 0x03u) == 0x02u) {
                if ((addr & 0x80u) != 0u) {
                    dev->ep_in = (u8)(addr & 0x0Fu);
                } else {
                    dev->ep_out = (u8)(addr & 0x0Fu);
                }
            }
        }
        off = (u16)(off + dlen);
    }
    return dev->ep_in != 0u;
}

static void enum_device(u32 port)
{
    u8 desc[18];
    u8 cfg[64];
    u8 addr;
    struct usb_device *dev;

    (void)port;
    if (n_dev >= USB_MAX_DEV) {
        return;
    }
    memset(desc, 0, sizeof(desc));
    if (usb_ctrl(0, 0x80u, 6u, 0x0100u, 0, desc, 8u) != 0) {
        serial_write("usb: GET_DESC8 fail\n");
        return;
    }
    addr = (u8)(n_dev + 1u);
    if (usb_ctrl(0, 0x00u, 5u, addr, 0, NULL, 0) != 0) {
        serial_write("usb: SET_ADDR fail\n");
        return;
    }
    delay_ms(2u);
    if (usb_ctrl(addr, 0x80u, 6u, 0x0100u, 0, desc, 18u) != 0) {
        serial_write("usb: GET_DESC18 fail\n");
        return;
    }
    if (usb_ctrl(addr, 0x80u, 6u, 0x0200u, 0, cfg, 9u) != 0) {
        serial_write("usb: GET_CFG9 fail\n");
        return;
    }
    {
        u16 total = (u16)cfg[2] | ((u16)cfg[3] << 8);
        u8 cfgval = cfg[5];

        if (total > 64u) {
            total = 64u;
        }
        if (total > 9u) {
            (void)usb_ctrl(addr, 0x80u, 6u, 0x0200u, 0, cfg, total);
        }
        if (usb_ctrl(addr, 0x00u, 9u, cfgval, 0, NULL, 0) != 0) {
            serial_write("usb: SET_CFG fail\n");
            return;
        }
        dev = &devices[n_dev];
        memset(dev, 0, sizeof(*dev));
        dev->addr = addr;
        dev->vendor = (u16)desc[8] | ((u16)desc[9] << 8);
        dev->product = (u16)desc[10] | ((u16)desc[11] << 8);
        dev->maxpkt = desc[7] != 0u ? desc[7] : 64u;
        dev->high_speed = true;
        (void)parse_config(cfg, total, dev);
        serial_write("usb: device vend=");
        serial_print_hex(dev->vendor);
        serial_write(" prod=");
        serial_print_hex(dev->product);
        serial_putc('\n');
        ++n_dev;
    }
}

bool usb_ehci_init(void)
{
    struct pci_device dev;
    u32 bar;
    u64 phys;
    u8 cap;
    u32 hcs;
    u32 i;
    u8 *p;

    ehci_ok = false;
    n_dev = 0;
    n_ports = 0;
    op = NULL;

    if (!pci_find_class(PCI_CLASS_SERIAL, PCI_SUBCLASS_USB, PCI_PROG_EHCI,
                        &dev)) {
        if (pci_find_class(PCI_CLASS_SERIAL, PCI_SUBCLASS_USB, PCI_PROG_XHCI,
                           &dev)) {
            serial_write("usb: xHCI presente (hace falta USB 2.0 EHCI)\n");
        } else {
            serial_write("usb: EHCI no detectado\n");
        }
        return false;
    }

    bar = pci_read_bar(&dev, 0);
    if ((bar & 1u) != 0u) {
        serial_write("usb: BAR0 IO, ignorado\n");
        return false;
    }
    phys = (u64)(bar & ~0xFul);
    if ((bar & 0x6u) == 0x4u) {
        phys |= ((u64)pci_read_bar(&dev, 1)) << 32;
    }
    pci_enable_mem_busmaster(&dev);
    if (!map_bar(phys, 4096u)) {
        return false;
    }

    cap = *(volatile u8 *)USB_MMIO_BASE;
    op = (volatile u32 *)(USB_MMIO_BASE + cap);
    hcs = *(volatile u32 *)(USB_MMIO_BASE + 4u);
    n_ports = hcs & 0xFu;
    if (n_ports == 0u) {
        n_ports = 8u;
    }

    /* Halt, reset, async dummy QH. */
    w32(USBINTR, 0);
    w32(USBCMD, r32(USBCMD) & ~CMD_RS);
    (void)wait_set(USBSTS, STS_HCHALTED, 50u);
    w32(USBCMD, CMD_HCRESET);
    if (!wait_clear(USBCMD, CMD_HCRESET, 50u)) {
        serial_write("usb: EHCI reset timeout\n");
        return false;
    }

    p = dma_page;
    memset(p, 0, sizeof(dma_page));
    qh_head = (struct qh *)(p + 0);
    qh_xfer = (struct qh *)(p + 64);
    qtd[0] = (struct qtd *)(p + 128);
    qtd[1] = (struct qtd *)(p + 160);
    qtd[2] = (struct qtd *)(p + 192);
    ctrl_buf = p + 256;

    memset((void *)qh_head, 0, sizeof(*qh_head));
    qh_head->horiz = dma_phys(qh_head) | 0x2u;
    qh_head->epchar = (1u << 15) | (1u << 14) | (8u << 16); /* H=1, DTC, 8 */
    qh_head->overlay[0] = QTD_TERM;
    qh_head->overlay[1] = QTD_TERM;

    w32(CTRLDSSEG, 0);
    w32(USBINTR, 0);
    w32(FRINDEX, 0);
    w32(PERIODIC, 0);
    w32(ASYNCLIST, dma_phys(qh_head));
    w32(USBSTS, 0x3Fu);
    w32(USBCMD, CMD_RS | CMD_ASE | (8u << 16));
    w32(CONFIGFLAG, 1);

    if (!wait_clear(USBSTS, STS_HCHALTED, 50u)) {
        serial_write("usb: EHCI no arranca\n");
        return false;
    }

    ehci_ok = true;
    serial_write("usb: EHCI puertos=");
    serial_print_u32(n_ports);
    serial_putc('\n');

    delay_ms(20u);
    for (i = 0; i < n_ports; ++i) {
        if (port_reset(i)) {
            enum_device(i);
        }
    }
    return true;
}

bool usb_ehci_present(void)
{
    return ehci_ok;
}

u32 usb_device_count(void)
{
    return n_dev;
}

const struct usb_device *usb_device_get(u32 index)
{
    if (index >= n_dev) {
        return NULL;
    }
    return &devices[index];
}
