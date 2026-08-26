#ifndef CORDOS_MULTIBOOT_H
#define CORDOS_MULTIBOOT_H

#include "types.h"

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

#define MULTIBOOT_INFO_MEMORY 0x00000001
#define MULTIBOOT_INFO_BOOTDEV 0x00000002
#define MULTIBOOT_INFO_CMDLINE 0x00000004
#define MULTIBOOT_INFO_MODS 0x00000008
#define MULTIBOOT_INFO_MMAP 0x00000040

struct multiboot_info {
    u32 flags;
    u32 mem_lower;
    u32 mem_upper;
    u32 boot_device;
    u32 cmdline;
    u32 mods_count;
    u32 mods_addr;
    u32 syms[4];
    u32 mmap_length;
    u32 mmap_addr;
};

struct multiboot_mmap_entry {
    u32 size;
    u32 base_addr_low;
    u32 base_addr_high;
    u32 length_low;
    u32 length_high;
    u32 type;
};

bool multiboot_is_valid(u32 magic);
const struct multiboot_info *multiboot_get(u32 address);
void multiboot_print_summary(const struct multiboot_info *info);

#endif
