#include "nosfs.h"
#include "string.h"

struct __attribute__((packed)) nrd1_rec {
    char name[32];
    u32 offset;
    u32 size;
};

struct __attribute__((packed)) nrd1_hdr {
    char magic[4];
    u32 file_count;
};

static const u8 *s_blob;
static u32 s_size;
static u32 s_count;
static const struct nrd1_rec *s_recs;
static int s_mounted;

static int name_eq(const char *a, const char *b32)
{
    u32 i;

    for (i = 0; i < 32; ++i) {
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

int nosfs_mount(const u8 *blob, u32 size)
{
    const struct nrd1_hdr *hdr;
    u32 table_bytes;
    u32 i;

    s_mounted = 0;
    s_blob = NULL;
    s_size = 0;
    s_count = 0;
    s_recs = NULL;

    if (blob == NULL || size < sizeof(struct nrd1_hdr)) {
        return -1;
    }

    hdr = (const struct nrd1_hdr *)blob;
    if (hdr->magic[0] != 'N' || hdr->magic[1] != 'R' ||
        hdr->magic[2] != 'D' || hdr->magic[3] != '1') {
        return -1;
    }

    table_bytes = hdr->file_count * (u32)sizeof(struct nrd1_rec);
    if (size < sizeof(struct nrd1_hdr) + table_bytes) {
        return -1;
    }

    s_recs = (const struct nrd1_rec *)(blob + sizeof(struct nrd1_hdr));
    for (i = 0; i < hdr->file_count; ++i) {
        u32 end;

        if (s_recs[i].offset > size) {
            return -1;
        }
        end = s_recs[i].offset + s_recs[i].size;
        if (end < s_recs[i].offset || end > size) {
            return -1;
        }
    }

    s_blob = blob;
    s_size = size;
    s_count = hdr->file_count;
    s_mounted = 1;
    return 0;
}

int nosfs_lookup(const char *name, u32 *out_off, u32 *out_size)
{
    u32 i;

    if (!s_mounted || name == NULL || out_off == NULL || out_size == NULL) {
        return -1;
    }

    for (i = 0; i < s_count; ++i) {
        if (name_eq(name, s_recs[i].name)) {
            *out_off = s_recs[i].offset;
            *out_size = s_recs[i].size;
            return 0;
        }
    }
    return -1;
}

int nosfs_list(void (*cb)(const char *name, u32 size, void *arg), void *arg)
{
    u32 i;
    char name[33];

    if (!s_mounted) {
        return -1;
    }
    if (cb == NULL) {
        return (int)s_count;
    }

    for (i = 0; i < s_count; ++i) {
        memcpy(name, s_recs[i].name, 32);
        name[32] = '\0';
        cb(name, s_recs[i].size, arg);
    }
    return (int)s_count;
}

const u8 *nosfs_data(void)
{
    return s_blob;
}

u32 nosfs_data_size(void)
{
    return s_size;
}
