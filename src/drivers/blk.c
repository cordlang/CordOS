#include "blk.h"
#include "serial.h"
#include "string.h"

static struct blk_dev s_dev[BLK_MAX_DEV];
static u32 s_n;

void blk_init(void)
{
    s_n = 0;
    memset(s_dev, 0, sizeof(s_dev));
}

int blk_register(const char *name, u32 sectors,
                 int (*read)(void *ctx, u32 lba, u32 count, void *buf),
                 int (*write)(void *ctx, u32 lba, u32 count, const void *buf),
                 void *ctx)
{
    struct blk_dev *d;
    u32 i;

    if (s_n >= BLK_MAX_DEV || name == NULL || sectors == 0 ||
        read == NULL || write == NULL) {
        return -1;
    }

    d = &s_dev[s_n];
    memset(d, 0, sizeof(*d));
    for (i = 0; i < (BLK_NAME_MAX - 1u) && name[i] != '\0'; ++i) {
        d->name[i] = name[i];
    }
    d->sectors = sectors;
    d->read = read;
    d->write = write;
    d->ctx = ctx;
    s_n++;

    serial_write("blk: ");
    serial_write(d->name);
    serial_write(" ");
    serial_print_u32(sectors);
    serial_write(" sectors\n");
    return 0;
}

u32 blk_count(void)
{
    return s_n;
}

const struct blk_dev *blk_get(u32 index)
{
    if (index >= s_n) {
        return NULL;
    }
    return &s_dev[index];
}
