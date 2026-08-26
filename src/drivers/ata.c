#include "ata.h"
#include "io.h"
#include "blk.h"
#include "serial.h"
#include "string.h"
#include "time.h"

/*
 * ATA PIO LBA28 — primary (0x1F0) and secondary (0x170), master + slave.
 * Polling only (nIEN set). Timeouts so a missing disk cannot hang boot.
 */

#define ATA_REG_DATA     0
#define ATA_REG_ERROR    1
#define ATA_REG_COUNT    2
#define ATA_REG_LBA0     3
#define ATA_REG_LBA1     4
#define ATA_REG_LBA2     5
#define ATA_REG_DRIVE    6
#define ATA_REG_STATUS   7
#define ATA_REG_CMD      7

#define ATA_SR_ERR  0x01u
#define ATA_SR_DRQ  0x08u
#define ATA_SR_DF   0x20u
#define ATA_SR_BSY  0x80u

#define ATA_CMD_READ    0x20u
#define ATA_CMD_WRITE   0x30u
#define ATA_CMD_FLUSH   0xE7u
#define ATA_CMD_IDENT   0xECu
#define ATA_CMD_IDENTP  0xA1u

#define ATA_NIEN  0x02u
#define ATA_SRST  0x04u

#define ATA_TIMEOUT_IDENT_MS 400u
#define ATA_TIMEOUT_IO_MS    2000u
#define ATA_TIMEOUT_RST_MS   800u

struct ata_chan {
    u16 io;
    u16 ctrl;
    u8 slave;
    const char *name;
};

static const struct ata_chan s_chan[4] = {
    { 0x1F0u, 0x3F6u, 0, "pri-master" },
    { 0x1F0u, 0x3F6u, 1, "pri-slave" },
    { 0x170u, 0x376u, 0, "sec-master" },
    { 0x170u, 0x376u, 1, "sec-slave" },
};

static int s_sel = -1;
static u32 s_lba_count;
static u16 s_bounce[256];
static int s_hdd[4];
static u32 s_hdd_lba[4];
static u32 s_nhdd;
static u32 s_blk_idx[4];

static void ata_pause(void)
{
    __asm__ volatile ("pause");
}

static void ata_io_wait(u16 ctrl)
{
    (void)inb(ctrl);
    (void)inb(ctrl);
    (void)inb(ctrl);
    (void)inb(ctrl);
}

static int ata_timed_out(u32 start, u32 timeout_ms, u32 spins)
{
    u32 hz = hz_os ? hz_os : 100u;
    u32 need = (timeout_ms * hz) / 1000u + 2u;
    u32 now = time_ticks();

    if (now - start >= need) {
        return 1;
    }
    /* Timer frozen (or called too early): absolute spin cap. */
    if (now == start && spins > (timeout_ms * 40000u + 200000u)) {
        return 1;
    }
    if (spins > 30000000u) {
        return 1;
    }
    return 0;
}

static int ata_wait_bsy(u16 io, u32 timeout_ms)
{
    u32 start = time_ticks();
    u32 spins = 0;

    for (;;) {
        u8 st = inb((u16)(io + ATA_REG_STATUS));

        if (st == 0xFFu) {
            return -1;
        }
        if ((st & ATA_SR_BSY) == 0) {
            return (int)st;
        }
        ata_pause();
        spins++;
        if (ata_timed_out(start, timeout_ms, spins)) {
            return -1;
        }
    }
}

static int ata_wait_drq(u16 io, u32 timeout_ms)
{
    u32 start = time_ticks();
    u32 spins = 0;

    for (;;) {
        u8 st = inb((u16)(io + ATA_REG_STATUS));

        if (st == 0xFFu) {
            return -1;
        }
        if ((st & ATA_SR_BSY) == 0) {
            if (st & (ATA_SR_ERR | ATA_SR_DF)) {
                return -1;
            }
            if (st & ATA_SR_DRQ) {
                return 0;
            }
        }
        ata_pause();
        spins++;
        if (ata_timed_out(start, timeout_ms, spins)) {
            return -1;
        }
    }
}

static void ata_select(const struct ata_chan *ch, u8 extra)
{
    outb((u16)(ch->io + ATA_REG_DRIVE),
         (u8)(extra | (ch->slave ? 0x10u : 0u)));
    ata_io_wait(ch->ctrl);
}

static void ata_soft_reset(u16 ctrl)
{
    outb(ctrl, (u8)(ATA_NIEN | ATA_SRST));
    ata_io_wait(ctrl);
    outb(ctrl, ATA_NIEN);
    ata_io_wait(ctrl);
}

static void ata_insw(u16 io, u16 *buf)
{
    u32 i;

    for (i = 0; i < 256u; ++i) {
        buf[i] = inw(io);
    }
}

static void ata_outsw(u16 io, const u16 *buf)
{
    u32 i;

    for (i = 0; i < 256u; ++i) {
        outw(io, buf[i]);
    }
}

static void ata_drain_drq(u16 io)
{
    u32 i;

    if ((inb((u16)(io + ATA_REG_STATUS)) & ATA_SR_DRQ) == 0) {
        return;
    }
    for (i = 0; i < 256u; ++i) {
        (void)inw(io);
    }
}

static int ata_identify(const struct ata_chan *ch, u32 *out_lba)
{
    u16 ident[256];
    u8 st;
    u8 lba1;
    u8 lba2;
    u32 lba28;

    ata_select(ch, 0xA0u);
    st = inb((u16)(ch->io + ATA_REG_STATUS));
    if (st == 0xFFu || st == 0x00u) {
        serial_write("ata: ");
        serial_write(ch->name);
        serial_write(" empty\n");
        return -1;
    }

    outb((u16)(ch->io + ATA_REG_ERROR), 0);
    outb((u16)(ch->io + ATA_REG_COUNT), 0);
    outb((u16)(ch->io + ATA_REG_LBA0), 0);
    outb((u16)(ch->io + ATA_REG_LBA1), 0);
    outb((u16)(ch->io + ATA_REG_LBA2), 0);
    outb((u16)(ch->io + ATA_REG_CMD), ATA_CMD_IDENT);

    if (ata_wait_bsy(ch->io, ATA_TIMEOUT_IDENT_MS) < 0) {
        return -1;
    }

    st = inb((u16)(ch->io + ATA_REG_STATUS));
    if (st == 0x00u || st == 0xFFu) {
        return -1;
    }

    lba1 = inb((u16)(ch->io + ATA_REG_LBA1));
    lba2 = inb((u16)(ch->io + ATA_REG_LBA2));
    /* ATAPI / SATAPI signatures — not a block disk we can PIO. */
    if ((lba1 == 0x14u && lba2 == 0xEBu) ||
        (lba1 == 0x69u && lba2 == 0x96u)) {
        serial_write("ata: ");
        serial_write(ch->name);
        serial_write(" ATAPI skip\n");
        ata_drain_drq(ch->io);
        return -1;
    }

    if (st & (ATA_SR_ERR | ATA_SR_DF)) {
        /* Some ATAPI devices abort IDENTIFY; drain and skip. */
        outb((u16)(ch->io + ATA_REG_CMD), ATA_CMD_IDENTP);
        (void)ata_wait_bsy(ch->io, ATA_TIMEOUT_IDENT_MS);
        ata_drain_drq(ch->io);
        serial_write("ata: ");
        serial_write(ch->name);
        serial_write(" IDENT err skip\n");
        return -1;
    }

    if (ata_wait_drq(ch->io, ATA_TIMEOUT_IDENT_MS) < 0) {
        return -1;
    }

    ata_insw(ch->io, ident);

    /* Word 0 bit 15 set => ATA packet (ATAPI). */
    if (ident[0] & 0x8000u) {
        serial_write("ata: ");
        serial_write(ch->name);
        serial_write(" packet skip\n");
        return -1;
    }

    lba28 = (u32)ident[60] | ((u32)ident[61] << 16);
    if (lba28 == 0) {
        /* Fall back to LBA48 words if LBA28 is empty; still PIO28 later. */
        lba28 = (u32)ident[100] | ((u32)ident[101] << 16);
    }
    if (lba28 == 0) {
        return -1;
    }
    if (lba28 > 0x0FFFFFFFu) {
        lba28 = 0x0FFFFFFFu;
    }

    /* Consume any leftover DRQ (should be idle). */
    (void)inb((u16)(ch->io + ATA_REG_STATUS));

    *out_lba = lba28;
    serial_write("ata: ");
    serial_write(ch->name);
    serial_write(" ATA LBA=");
    serial_print_u32(lba28);
    serial_putc('\n');
    return 0;
}

static int ata_pio1(u32 lba, int write, u16 *sec)
{
    const struct ata_chan *ch;
    u8 drv;

    if (s_sel < 0 || s_sel > 3) {
        return -1;
    }
    ch = &s_chan[s_sel];

    drv = (u8)(0xE0u | (ch->slave ? 0x10u : 0u) | ((lba >> 24) & 0x0Fu));
    ata_select(ch, drv);

    if (ata_wait_bsy(ch->io, ATA_TIMEOUT_IO_MS) < 0) {
        return -1;
    }

    outb((u16)(ch->io + ATA_REG_ERROR), 0);
    outb((u16)(ch->io + ATA_REG_COUNT), 1);
    outb((u16)(ch->io + ATA_REG_LBA0), (u8)lba);
    outb((u16)(ch->io + ATA_REG_LBA1), (u8)(lba >> 8));
    outb((u16)(ch->io + ATA_REG_LBA2), (u8)(lba >> 16));
    outb((u16)(ch->io + ATA_REG_DRIVE), drv);
    outb((u16)(ch->io + ATA_REG_CMD), write ? ATA_CMD_WRITE : ATA_CMD_READ);

    if (ata_wait_drq(ch->io, ATA_TIMEOUT_IO_MS) < 0) {
        return -1;
    }

    if (write) {
        ata_outsw(ch->io, sec);
        if (ata_wait_bsy(ch->io, ATA_TIMEOUT_IO_MS) < 0) {
            return -1;
        }
        if (inb((u16)(ch->io + ATA_REG_STATUS)) & (ATA_SR_ERR | ATA_SR_DF)) {
            return -1;
        }
    } else {
        ata_insw(ch->io, sec);
        if (inb((u16)(ch->io + ATA_REG_STATUS)) & (ATA_SR_ERR | ATA_SR_DF)) {
            return -1;
        }
    }
    return 0;
}

static int ata_flush(void)
{
    const struct ata_chan *ch;
    int st;

    if (s_sel < 0) {
        return -1;
    }
    ch = &s_chan[s_sel];
    ata_select(ch, (u8)(0xE0u | (ch->slave ? 0x10u : 0u)));
    if (ata_wait_bsy(ch->io, ATA_TIMEOUT_IO_MS) < 0) {
        return -1;
    }
    outb((u16)(ch->io + ATA_REG_CMD), ATA_CMD_FLUSH);
    st = ata_wait_bsy(ch->io, ATA_TIMEOUT_IO_MS);
    if (st < 0) {
        return -1;
    }
    /* Flush is optional on some disks; ERR is ignored. */
    return 0;
}

static int ata_blk_read(void *ctx, u32 lba, u32 count, void *buf)
{
    u32 idx;

    if (ctx == NULL) {
        return -1;
    }
    idx = *(const u32 *)ctx;
    if (ata_use_hdd(idx) < 0) {
        return -1;
    }
    return ata_read(lba, count, buf);
}

static int ata_blk_write(void *ctx, u32 lba, u32 count, const void *buf)
{
    u32 idx;

    if (ctx == NULL) {
        return -1;
    }
    idx = *(const u32 *)ctx;
    if (ata_use_hdd(idx) < 0) {
        return -1;
    }
    return ata_write(lba, count, buf);
}

void ata_init(void)
{
    int i;

    s_sel = -1;
    s_lba_count = 0;
    s_nhdd = 0;

    serial_write("ata: probe 0x1F0/0x170\n");

    ata_soft_reset(0x3F6u);
    (void)ata_wait_bsy(0x1F0u, ATA_TIMEOUT_RST_MS);
    outb(0x3F6u, ATA_NIEN);

    ata_soft_reset(0x376u);
    (void)ata_wait_bsy(0x170u, ATA_TIMEOUT_RST_MS);
    outb(0x376u, ATA_NIEN);

    for (i = 0; i < 4; ++i) {
        u32 lba = 0;

        if (ata_identify(&s_chan[i], &lba) == 0 && s_nhdd < 4u) {
            s_hdd[s_nhdd] = i;
            s_hdd_lba[s_nhdd] = lba;
            s_nhdd++;
        }
    }

    if (s_nhdd == 0) {
        serial_write("ata: no HDD\n");
        return;
    }

    s_sel = s_hdd[0];
    s_lba_count = s_hdd_lba[0];
    serial_write("ata: using ");
    serial_write(s_chan[s_sel].name);
    serial_putc('\n');

    for (i = 0; i < (int)s_nhdd; ++i) {
        char name[8];

        s_blk_idx[i] = (u32)i;
        name[0] = 'i';
        name[1] = 'd';
        name[2] = 'e';
        name[3] = (char)('0' + i);
        name[4] = '\0';
        (void)blk_register(name, s_hdd_lba[i], ata_blk_read, ata_blk_write,
                           &s_blk_idx[i]);
    }
}

bool ata_present(void)
{
    return s_sel >= 0;
}

u32 ata_sectors(void)
{
    return s_lba_count;
}

u32 ata_hdd_count(void)
{
    return s_nhdd;
}

int ata_use_hdd(u32 index)
{
    int next;

    if (index >= s_nhdd) {
        return -1;
    }
    next = s_hdd[index];
    s_lba_count = s_hdd_lba[index];
    if (s_sel == next) {
        return 0;
    }
    s_sel = next;
    serial_write("ata: using ");
    serial_write(s_chan[s_sel].name);
    serial_putc('\n');
    return 0;
}

int ata_read(u32 lba, u32 count, void *buf)
{
    u8 *dst;
    u32 i;

    if (!ata_present() || buf == NULL) {
        return -1;
    }
    if (count == 0) {
        return 0;
    }
    if (lba + count < lba || lba + count > s_lba_count) {
        return -1;
    }

    dst = (u8 *)buf;
    for (i = 0; i < count; ++i) {
        if (ata_pio1(lba + i, 0, s_bounce) < 0) {
            return -1;
        }
        memcpy(dst + i * ATA_SECTOR_SIZE, s_bounce, ATA_SECTOR_SIZE);
    }
    return 0;
}

int ata_write(u32 lba, u32 count, const void *buf)
{
    const u8 *src;
    u32 i;

    if (!ata_present() || buf == NULL) {
        return -1;
    }
    if (count == 0) {
        return 0;
    }
    if (lba + count < lba || lba + count > s_lba_count) {
        return -1;
    }

    src = (const u8 *)buf;
    for (i = 0; i < count; ++i) {
        memcpy(s_bounce, src + i * ATA_SECTOR_SIZE, ATA_SECTOR_SIZE);
        if (ata_pio1(lba + i, 1, s_bounce) < 0) {
            return -1;
        }
    }
    return ata_flush();
}
