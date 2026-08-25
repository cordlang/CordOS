#include "vmm.h"
#include "pmm.h"
#include "string.h"
#include "panic.h"
#include "vga.h"

volatile u32 page_directory_os = 0;

static u32 *page_directory;

static u32 *vmm_get_table(u32 virtual_addr, bool create)
{
    u32 pd_index = virtual_addr >> 22;
    u32 *table;

    if (!(page_directory[pd_index] & PAGE_PRESENT)) {
        void *frame;

        if (!create) {
            return NULL;
        }

        frame = pmm_alloc_page();
        if (frame == NULL) {
            panic("VMM: sin frames para page table");
        }

        memset(frame, 0, PAGE_SIZE);
        page_directory[pd_index] =
            ((u32)frame) | PAGE_PRESENT | PAGE_WRITE;
    }

    table = (u32 *)(page_directory[pd_index] & ~0xFFFu);
    return table;
}

void vmm_map_page(u32 virtual_addr, u32 physical_addr, u32 flags)
{
    u32 *table = vmm_get_table(virtual_addr, true);
    u32 pt_index = (virtual_addr >> 12) & 0x3FFu;

    table[pt_index] = (physical_addr & ~0xFFFu) | (flags & 0xFFFu) | PAGE_PRESENT;

    __asm__ volatile ("invlpg (%0)" : : "r"(virtual_addr) : "memory");
}

void vmm_unmap_page(u32 virtual_addr)
{
    u32 *table = vmm_get_table(virtual_addr, false);
    u32 pt_index;

    if (table == NULL) {
        return;
    }

    pt_index = (virtual_addr >> 12) & 0x3FFu;
    table[pt_index] = 0;
    __asm__ volatile ("invlpg (%0)" : : "r"(virtual_addr) : "memory");
}

u32 vmm_get_physical(u32 virtual_addr)
{
    u32 *table = vmm_get_table(virtual_addr, false);
    u32 pt_index;
    u32 entry;

    if (table == NULL) {
        return 0;
    }

    pt_index = (virtual_addr >> 12) & 0x3FFu;
    entry = table[pt_index];
    if (!(entry & PAGE_PRESENT)) {
        return 0;
    }

    return (entry & ~0xFFFu) | (virtual_addr & 0xFFFu);
}

void vmm_init(void)
{
    u32 addr;
    u32 limit = total_frames_os * PAGE_SIZE;
    void *pd_page = pmm_alloc_page();

    if (pd_page == NULL) {
        panic("VMM: no hay frame para page directory");
    }

    page_directory = (u32 *)pd_page;
    page_directory_os = (u32)pd_page;
    memset(page_directory, 0, PAGE_SIZE);

    if (limit == 0) {
        panic("VMM: total_frames_os es 0");
    }

    /* Identity map de toda la memoria fisica gestionada (incluye VGA y kernel). */
    for (addr = 0; addr < limit; addr += PAGE_SIZE) {
        vmm_map_page(addr, addr, PAGE_PRESENT | PAGE_WRITE);
    }

    __asm__ volatile (
        "mov %0, %%cr3\n"
        "mov %%cr0, %%eax\n"
        "or $0x80000000, %%eax\n"
        "mov %%eax, %%cr0\n"
        :
        : "r"(page_directory)
        : "eax", "memory"
    );

    vga_print("Paginacion identity-map activa. PD en ");
    vga_print_hex(page_directory_os);
    vga_print("\n");
}
