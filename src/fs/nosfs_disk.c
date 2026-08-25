#include "nosfs.h"
#include "ata.h"
#include "serial.h"
#include "string.h"

/*
 * On-disk NosFS (magic NOSF). Lives in a reserved LBA window so MBR / stage2 /
 * kernel at LBA 0..~65+ are never overwritten. See docs/nosfs.md.
 *
 *   LBA 2048            superblock (512 B)
 *   LBA 2049..2056      directory (64 × 64 B)
 *   LBA 2057..6143      file payloads (bump allocator)
 */

#define NOSF_VERSION    1u
#define NOSF_DIR_LBA    1u
#define NOSF_DIR_SECTS  8u
#define NOSF_DATA_LBA   9u
#define NOSF_ENT_USED   1u

struct __attribute__((packed)) nosf_sb {
    char magic[4];
    u32 version;
    u32 vol_lba;
    u32 vol_sectors;
    u32 file_count;
    u32 max_files;
    u32 dir_lba;
    u32 dir_sectors;
    u32 data_lba;
    u32 next_data;
    u8 reserved[512 - 40];
};

struct __attribute__((packed)) nosf_dirent {
    char name[32];
    u32 size;
    u32 data_lba;
    u32 data_sectors;
    u32 flags;
    u8 pad[16];
};

static struct nosf_sb s_sb;
static struct nosf_dirent s_dir[NOSFS_MAX_FILES];
static int s_ready;
static u8 s_sec[ATA_SECTOR_SIZE];

static int name_eq(const char *a, const char *b32)
{
    u32 i;

    for (i = 0; i < 32u; ++i) {
        char ca = a[i];
        char cb = b32[i];

        if (ca != cb) {
            return 0;
        }
        if (ca == '\0') {
            return 1;
        }
    }
    return a[32] == '\0';
}

static int valid_name(const char *name)
{
    u32 i;

    if (name == NULL || name[0] == '\0') {
        return 0;
    }
    for (i = 0; i < 32u; ++i) {
        char c = name[i];

        if (c == '\0') {
            return 1;
        }
        if (c == '/' || c == '\\') {
            return 0;
        }
    }
    return name[32] == '\0';
}

static void copy_name(char *dst, const char *src)
{
    u32 i;

    memset(dst, 0, 32);
    for (i = 0; i < 31u && src[i] != '\0'; ++i) {
        dst[i] = src[i];
    }
}

static int vol_read(u32 rel, u32 count, void *buf)
{
    return ata_read(NOSFS_VOL_LBA + rel, count, buf);
}

static int vol_write(u32 rel, u32 count, const void *buf)
{
    return ata_write(NOSFS_VOL_LBA + rel, count, buf);
}

static int load_meta(void)
{
    u32 i;

    if (vol_read(0, 1, &s_sb) < 0) {
        return -1;
    }
    if (s_sb.magic[0] != 'N' || s_sb.magic[1] != 'O' ||
        s_sb.magic[2] != 'S' || s_sb.magic[3] != 'F') {
        return -1;
    }
    if (s_sb.version != NOSF_VERSION) {
        return -1;
    }
    if (s_sb.vol_sectors != NOSFS_VOL_SECTS ||
        s_sb.max_files != NOSFS_MAX_FILES ||
        s_sb.dir_lba != NOSF_DIR_LBA ||
        s_sb.dir_sectors != NOSF_DIR_SECTS) {
        return -1;
    }
    if (s_sb.next_data < NOSF_DATA_LBA || s_sb.next_data > NOSFS_VOL_SECTS) {
        return -1;
    }

    memset(s_dir, 0, sizeof(s_dir));
    for (i = 0; i < NOSF_DIR_SECTS; ++i) {
        if (vol_read(NOSF_DIR_LBA + i, 1, s_sec) < 0) {
            return -1;
        }
        memcpy((u8 *)s_dir + i * ATA_SECTOR_SIZE, s_sec, ATA_SECTOR_SIZE);
    }
    return 0;
}

static int flush_sb(void)
{
    memset(s_sec, 0, sizeof(s_sec));
    memcpy(s_sec, &s_sb, sizeof(s_sb));
    return vol_write(0, 1, s_sec);
}

static int flush_dir(void)
{
    u32 i;

    for (i = 0; i < NOSF_DIR_SECTS; ++i) {
        memcpy(s_sec, (u8 *)s_dir + i * ATA_SECTOR_SIZE, ATA_SECTOR_SIZE);
        if (vol_write(NOSF_DIR_LBA + i, 1, s_sec) < 0) {
            return -1;
        }
    }
    return 0;
}

static int format_vol(void)
{
    u32 i;

    memset(&s_sb, 0, sizeof(s_sb));
    s_sb.magic[0] = 'N';
    s_sb.magic[1] = 'O';
    s_sb.magic[2] = 'S';
    s_sb.magic[3] = 'F';
    s_sb.version = NOSF_VERSION;
    s_sb.vol_lba = NOSFS_VOL_LBA;
    s_sb.vol_sectors = NOSFS_VOL_SECTS;
    s_sb.file_count = 0;
    s_sb.max_files = NOSFS_MAX_FILES;
    s_sb.dir_lba = NOSF_DIR_LBA;
    s_sb.dir_sectors = NOSF_DIR_SECTS;
    s_sb.data_lba = NOSF_DATA_LBA;
    s_sb.next_data = NOSF_DATA_LBA;

    memset(s_dir, 0, sizeof(s_dir));
    memset(s_sec, 0, sizeof(s_sec));

    if (flush_sb() < 0) {
        return -1;
    }
    for (i = 0; i < NOSF_DIR_SECTS; ++i) {
        if (vol_write(NOSF_DIR_LBA + i, 1, s_sec) < 0) {
            return -1;
        }
    }
    serial_write("nosfs: formatted NOSF @ LBA ");
    serial_print_u32(NOSFS_VOL_LBA);
    serial_putc('\n');
    return 0;
}

static int find_ent(const char *name)
{
    u32 i;

    for (i = 0; i < NOSFS_MAX_FILES; ++i) {
        if ((s_dir[i].flags & NOSF_ENT_USED) &&
            name_eq(name, s_dir[i].name)) {
            return (int)i;
        }
    }
    return -1;
}

static int find_free(void)
{
    u32 i;

    for (i = 0; i < NOSFS_MAX_FILES; ++i) {
        if ((s_dir[i].flags & NOSF_ENT_USED) == 0) {
            return (int)i;
        }
    }
    return -1;
}

int nosfs_disk_mount(void)
{
    u32 need = NOSFS_VOL_LBA + NOSFS_VOL_SECTS;
    u32 n;
    u32 i;
    int blank = -1;

    s_ready = 0;
    if (!ata_present()) {
        return -1;
    }

    n = ata_hdd_count();
    for (i = 0; i < n; ++i) {
        if (ata_use_hdd(i) < 0) {
            continue;
        }
        if (ata_sectors() < need) {
            serial_write("nosfs: ");
            serial_write("hdd too small, skip\n");
            continue;
        }
        if (load_meta() == 0) {
            s_ready = 1;
            serial_write("nosfs: mounted NOSF files=");
            serial_print_u32(s_sb.file_count);
            serial_putc('\n');
            return 0;
        }
        if (blank < 0) {
            blank = (int)i;
        }
    }

    if (blank < 0) {
        serial_write("nosfs: no usable HDD\n");
        return -1;
    }

    if (ata_use_hdd((u32)blank) < 0) {
        return -1;
    }
    if (format_vol() < 0) {
        serial_write("nosfs: format failed\n");
        return -1;
    }
    if (load_meta() < 0) {
        serial_write("nosfs: remount after format failed\n");
        return -1;
    }
    s_ready = 1;
    return 0;
}

bool nosfs_disk_ready(void)
{
    return s_ready != 0;
}

int nosfs_disk_reload(void)
{
    if (!s_ready) {
        return -1;
    }
    if (load_meta() < 0) {
        s_ready = 0;
        return -1;
    }
    return 0;
}

int nosfs_disk_lookup(const char *name, u32 *out_size)
{
    int idx;

    if (!s_ready || name == NULL || out_size == NULL) {
        return -1;
    }
    idx = find_ent(name);
    if (idx < 0) {
        return -1;
    }
    *out_size = s_dir[idx].size;
    return 0;
}

int nosfs_disk_list(void (*cb)(const char *name, u32 size, void *arg), void *arg)
{
    u32 i;
    u32 n = 0;
    char name[33];

    if (!s_ready) {
        return -1;
    }
    if (cb == NULL) {
        return (int)s_sb.file_count;
    }

    for (i = 0; i < NOSFS_MAX_FILES; ++i) {
        if ((s_dir[i].flags & NOSF_ENT_USED) == 0) {
            continue;
        }
        memcpy(name, s_dir[i].name, 32);
        name[32] = '\0';
        cb(name, s_dir[i].size, arg);
        n++;
    }
    return (int)n;
}

int nosfs_disk_read(const char *name, u32 off, void *buf, u32 len)
{
    int idx;
    u32 remain;
    u32 n;
    u32 pos;
    u8 *dst;
    u32 lba;
    u32 in_sec;
    u32 chunk;

    if (!s_ready || name == NULL || buf == NULL) {
        return -1;
    }
    idx = find_ent(name);
    if (idx < 0) {
        return -1;
    }
    if (off >= s_dir[idx].size) {
        return 0;
    }

    remain = s_dir[idx].size - off;
    n = len;
    if (n > remain) {
        n = remain;
    }

    dst = (u8 *)buf;
    pos = 0;
    while (pos < n) {
        lba = s_dir[idx].data_lba + ((off + pos) / ATA_SECTOR_SIZE);
        in_sec = (off + pos) % ATA_SECTOR_SIZE;
        chunk = ATA_SECTOR_SIZE - in_sec;
        if (chunk > n - pos) {
            chunk = n - pos;
        }
        if (vol_read(lba, 1, s_sec) < 0) {
            return -1;
        }
        memcpy(dst + pos, s_sec + in_sec, chunk);
        pos += chunk;
    }
    return (int)n;
}

int nosfs_disk_put(const char *name, const void *buf, u32 len)
{
    int idx;
    u32 need;
    u32 i;
    const u8 *src;
    u32 left;
    u32 chunk;

    if (!s_ready || !valid_name(name)) {
        return -1;
    }
    if (len > 0 && buf == NULL) {
        return -1;
    }

    need = (len + (ATA_SECTOR_SIZE - 1u)) / ATA_SECTOR_SIZE;
    idx = find_ent(name);

    if (idx < 0) {
        idx = find_free();
        if (idx < 0) {
            return -1;
        }
        memset(&s_dir[idx], 0, sizeof(s_dir[idx]));
        copy_name(s_dir[idx].name, name);
        s_dir[idx].flags = NOSF_ENT_USED;
        s_sb.file_count++;
    }

    if (need > s_dir[idx].data_sectors) {
        if (s_sb.next_data + need > NOSFS_VOL_SECTS) {
            if ((s_dir[idx].flags & NOSF_ENT_USED) && s_dir[idx].size == 0 &&
                s_dir[idx].data_sectors == 0) {
                /* New empty slot we just claimed — roll back. */
                memset(&s_dir[idx], 0, sizeof(s_dir[idx]));
                if (s_sb.file_count > 0) {
                    s_sb.file_count--;
                }
            }
            return -1;
        }
        s_dir[idx].data_lba = s_sb.next_data;
        s_dir[idx].data_sectors = need;
        s_sb.next_data += need;
    }

    src = (const u8 *)buf;
    left = len;
    for (i = 0; i < s_dir[idx].data_sectors; ++i) {
        memset(s_sec, 0, sizeof(s_sec));
        chunk = left > ATA_SECTOR_SIZE ? ATA_SECTOR_SIZE : left;
        if (chunk > 0) {
            memcpy(s_sec, src, chunk);
            src += chunk;
            left -= chunk;
        }
        if (vol_write(s_dir[idx].data_lba + i, 1, s_sec) < 0) {
            return -1;
        }
    }

    s_dir[idx].size = len;
    if (flush_dir() < 0 || flush_sb() < 0) {
        return -1;
    }
    return 0;
}
