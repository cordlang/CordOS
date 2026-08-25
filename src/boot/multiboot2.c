#include "multiboot2.h"
#include "vga.h"

const struct multiboot2_tag *multiboot2_find_tag(const void *mb2_addr, u32 type)
{
    const struct multiboot2_info_header *header;
    const struct multiboot2_tag *tag;
    const u8 *end;
    u32 aligned_size;
    u32 remaining;

    if (mb2_addr == NULL) {
        return NULL;
    }

    header = (const struct multiboot2_info_header *)mb2_addr;
    if (header->total_size < sizeof(*header) + sizeof(*tag)) {
        return NULL;
    }

    tag = (const struct multiboot2_tag *)(header + 1);
    end = (const u8 *)mb2_addr + header->total_size;

    while ((const u8 *)tag + sizeof(*tag) <= end) {
        remaining = (u32)(end - (const u8 *)tag);
        if (tag->size < sizeof(*tag) || tag->size > remaining) {
            return NULL;
        }

        if (tag->type == MULTIBOOT2_TAG_TYPE_END) {
            break;
        }

        if (tag->type == type) {
            return tag;
        }

        aligned_size = (tag->size + 7u) & ~7u;
        if (aligned_size < tag->size || aligned_size > remaining) {
            return NULL;
        }
        tag = (const struct multiboot2_tag *)((const u8 *)tag + aligned_size);
    }

    return NULL;
}

void multiboot2_print_summary(const void *mb2_addr)
{
    const struct multiboot2_info_header *header;
    const struct multiboot2_tag *mem;
    const struct multiboot2_tag *mmap;

    if (mb2_addr == NULL) {
        vga_print("Multiboot2: null\n");
        return;
    }

    header = (const struct multiboot2_info_header *)mb2_addr;
    vga_print("Multiboot2 total_size: ");
    vga_print_u32(header->total_size);
    vga_print("\n");

    mem = multiboot2_find_tag(mb2_addr, MULTIBOOT2_TAG_TYPE_BASIC_MEMINFO);
    if (mem != NULL) {
        const struct multiboot2_tag_basic_meminfo *info =
            (const struct multiboot2_tag_basic_meminfo *)mem;
        vga_print("Memoria lower/upper (KiB): ");
        vga_print_u32(info->mem_lower);
        vga_print(" / ");
        vga_print_u32(info->mem_upper);
        vga_print("\n");
    }

    mmap = multiboot2_find_tag(mb2_addr, MULTIBOOT2_TAG_TYPE_MMAP);
    if (mmap != NULL) {
        const struct multiboot2_tag_mmap *tag =
            (const struct multiboot2_tag_mmap *)mmap;
        vga_print("MMAP2 tag size=");
        vga_print_u32(tag->size);
        vga_print(" entry_size=");
        vga_print_u32(tag->entry_size);
        vga_print("\n");
    }
}
