#ifndef CORDOS_BLK_H
#define CORDOS_BLK_H

#include "types.h"

#define BLK_SECTOR_SIZE 512u
#define BLK_NAME_MAX    16u
#define BLK_MAX_DEV     8u

/*
 * Writable disks only (HDD/SSD). Optical / ATAPI never register here.
 *
 * Persistence is the same idea as Windows C:, macOS the Macintosh HD, or
 * Linux the root filesystem: find a real disk, keep OS state on it.
 * VirtualBox SATA is just a stand-in for that internal drive.
 */
struct blk_dev {
    char name[BLK_NAME_MAX];
    u32 sectors;
    int (*read)(void *ctx, u32 lba, u32 count, void *buf);
    int (*write)(void *ctx, u32 lba, u32 count, const void *buf);
    void *ctx;
};

void blk_init(void);
int blk_register(const char *name, u32 sectors,
                 int (*read)(void *ctx, u32 lba, u32 count, void *buf),
                 int (*write)(void *ctx, u32 lba, u32 count, const void *buf),
                 void *ctx);
u32 blk_count(void);
const struct blk_dev *blk_get(u32 index);

#endif
