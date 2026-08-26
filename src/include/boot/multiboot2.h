#ifndef CORDOS_MULTIBOOT2_H
#define CORDOS_MULTIBOOT2_H

#include "types.h"

/* Bootloader magic in EAX at entry (after Multiboot2 handoff). */
#define MULTIBOOT2_BOOTLOADER_MAGIC 0x36d76289u

/* Header magic (embedded in the kernel image). */
#define MULTIBOOT2_HEADER_MAGIC 0xE85250D6u

/* Tag types (subset; enough to walk the info structure later). */
#define MULTIBOOT2_TAG_TYPE_END              0
#define MULTIBOOT2_TAG_TYPE_CMDLINE          1
#define MULTIBOOT2_TAG_TYPE_BOOT_LOADER_NAME 2
#define MULTIBOOT2_TAG_TYPE_MODULE           3
#define MULTIBOOT2_TAG_TYPE_BASIC_MEMINFO    4
#define MULTIBOOT2_TAG_TYPE_BOOTDEV          5
#define MULTIBOOT2_TAG_TYPE_MMAP             6
#define MULTIBOOT2_TAG_TYPE_FRAMEBUFFER      8
#define MULTIBOOT2_TAG_TYPE_EFI_MMAP        17

/* Memory map entry types */
#define MULTIBOOT2_MEMORY_AVAILABLE        1
#define MULTIBOOT2_MEMORY_RESERVED         2
#define MULTIBOOT2_MEMORY_ACPI_RECLAIMABLE 3
#define MULTIBOOT2_MEMORY_NVS              4
#define MULTIBOOT2_MEMORY_BADRAM           5

struct multiboot2_info_header {
    u32 total_size;
    u32 reserved;
};

struct multiboot2_tag {
    u32 type;
    u32 size;
};

struct multiboot2_tag_string {
    u32 type;
    u32 size;
    char string[];
};

struct multiboot2_tag_basic_meminfo {
    u32 type;
    u32 size;
    u32 mem_lower;
    u32 mem_upper;
};

struct multiboot2_tag_mmap {
    u32 type;
    u32 size;
    u32 entry_size;
    u32 entry_version;
    /* followed by multiboot2_mmap_entry entries */
};

struct multiboot2_mmap_entry {
    u64 addr;
    u64 len;
    u32 type;
    u32 zero;
};

struct multiboot2_tag_module {
    u32 type;
    u32 size;
    u32 mod_start;
    u32 mod_end;
    char cmdline[];
};

/* Align tag pointer to next 8-byte boundary after a tag of given size. */
static inline const struct multiboot2_tag *
multiboot2_tag_next(const struct multiboot2_tag *tag)
{
    u32 size = (tag->size + 7u) & ~7u;
    return (const struct multiboot2_tag *)((const u8 *)tag + size);
}

static inline bool multiboot2_is_valid(u32 magic)
{
    return magic == MULTIBOOT2_BOOTLOADER_MAGIC;
}

const struct multiboot2_tag *multiboot2_find_tag(const void *mb2_addr, u32 type);
void multiboot2_print_summary(const void *mb2_addr);

#endif
