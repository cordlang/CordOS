#include "initrd.h"

#ifndef offsetof
#define offsetof(type, member) ((u32)(u64) & (((type *)0)->member))
#endif

struct __attribute__((packed)) nrd1_rec {
    char name[32];
    u32 offset;
    u32 size;
};

/* Contiguous NRD1 image in .rodata — see docs/nosfs.md */
struct __attribute__((packed)) initrd_image {
    char magic[4];
    u32 file_count;
    struct nrd1_rec rec[2];
    char hello[32];
    char motd[32];
};

static const struct initrd_image g_initrd = {
    .magic = { 'N', 'R', 'D', '1' },
    .file_count = 2,
    .rec = {
        {
            .name = "hello.txt",
            .offset = offsetof(struct initrd_image, hello),
            .size = 26,
        },
        {
            .name = "motd",
            .offset = offsetof(struct initrd_image, motd),
            .size = 18,
        },
    },
    .hello = "Hello from CordOS initrd!\n",
    .motd = "Welcome to CordOS\n",
};

const u8 *initrd_blob = (const u8 *)&g_initrd;
const u32 initrd_blob_size = (u32)sizeof(g_initrd);
