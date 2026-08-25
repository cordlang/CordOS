#include "multiboot.h"
#include "vga.h"

bool multiboot_is_valid(u32 magic)
{
    return magic == MULTIBOOT_BOOTLOADER_MAGIC;
}

const struct multiboot_info *multiboot_get(u32 address)
{
    return (const struct multiboot_info *)address;
}

void multiboot_print_summary(const struct multiboot_info *info)
{
    vga_print("Multiboot flags: ");
    vga_print_hex(info->flags);
    vga_print("\n");

    if (info->flags & MULTIBOOT_INFO_MEMORY) {
        vga_print("Memoria lower/upper (KiB): ");
        vga_print_u32(info->mem_lower);
        vga_print(" / ");
        vga_print_u32(info->mem_upper);
        vga_print("\n");
    }

    if (info->flags & MULTIBOOT_INFO_CMDLINE) {
        vga_print("Cmdline: ");
        vga_print((const char *)info->cmdline);
        vga_print("\n");
    }

    if (info->flags & MULTIBOOT_INFO_MMAP) {
        vga_print("MMAP en ");
        vga_print_hex(info->mmap_addr);
        vga_print(" (");
        vga_print_u32(info->mmap_length);
        vga_print(" bytes)\n");
    }
}
