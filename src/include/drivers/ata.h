#ifndef NUEVOOS_ATA_H
#define NUEVOOS_ATA_H

#include "types.h"

#define ATA_SECTOR_SIZE 512u

/* Probe primary/secondary master+slave. Safe to call with no disk. */
void ata_init(void);

/* True if an ATA HDD was selected (not ATAPI / empty bus). */
bool ata_present(void);

/* LBA28 sector count of the selected drive, or 0. */
u32 ata_sectors(void);

/* HDDs found during probe (excludes ATAPI). */
u32 ata_hdd_count(void);

/* Select HDD index 0 .. ata_hdd_count()-1. Returns 0 or -1. */
int ata_use_hdd(u32 index);

/*
 * Read/write 512-byte sectors on the selected drive.
 * count is the number of sectors. Returns 0 or -1.
 */
int ata_read(u32 lba, u32 count, void *buf);
int ata_write(u32 lba, u32 count, const void *buf);

#endif
