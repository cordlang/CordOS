#ifndef NUEVOOS_NOSFS_H
#define NUEVOOS_NOSFS_H

#include "types.h"

/* --- RAM NRD1 (initrd) --- */

/* Mount an NRD1 image as the nosfs RAM root. Returns 0 or -1. */
int nosfs_mount(const u8 *blob, u32 size);

/* Lookup basename; sets *out_off / *out_size (absolute blob offset). */
int nosfs_lookup(const char *name, u32 *out_off, u32 *out_size);

/* Invoke cb for each RAM file. Returns file count, or -1 if not mounted. */
int nosfs_list(void (*cb)(const char *name, u32 size, void *arg), void *arg);

const u8 *nosfs_data(void);
u32 nosfs_data_size(void);

/* --- On-disk NOSF (writable, reserved LBA range) --- */

#define NOSFS_VOL_LBA     2048u
#define NOSFS_VOL_SECTS   4096u
#define NOSFS_MAX_FILES   64u
#define NOSFS_NAME_LEN    32u

int nosfs_disk_mount(void);
bool nosfs_disk_ready(void);
int nosfs_disk_reload(void);
int nosfs_disk_lookup(const char *name, u32 *out_size);
int nosfs_disk_list(void (*cb)(const char *name, u32 size, void *arg), void *arg);
int nosfs_disk_read(const char *name, u32 off, void *buf, u32 len);
/* Create or overwrite a whole file. buf may be NULL if len == 0. */
int nosfs_disk_put(const char *name, const void *buf, u32 len);

#endif
