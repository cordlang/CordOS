#include "vmm.h"
#include "pmm.h"
#include "string.h"
#include "panic.h"
#include "serial.h"

/*
 * 4-level paging for long mode. boot64 already enabled paging with a
 * temporary 1 GiB identity map (2 MiB pages). vmm_init rebuilds a 4 KiB
 * identity map over PMM-managed frames and loads the new PML4 into CR3.
 */

volatile u64 pml4_os = 0;

static u64 *pml4;

#define PTE_ADDR_MASK 0x000FFFFFFFFFF000ull

static u64 *vmm_table_at(u64 entry)
{
    return (u64 *)(entry & PTE_ADDR_MASK);
}

static u64 *vmm_get_or_create(u64 *table, u32 index, bool create, u32 user_flag)
{
    if (!(table[index] & PAGE_PRESENT)) {
        void *frame;

        if (!create) {
            return NULL;
        }

        frame = pmm_alloc_page();
        if (frame == NULL) {
            panic("VMM64: sin frames para page table");
        }

        memset(frame, 0, PAGE_SIZE);
        table[index] = ((u64)frame) | PAGE_PRESENT | PAGE_WRITE | user_flag;
    } else if (user_flag) {
        /* U/S must be set at every paging level for CPL3 walks. */
        table[index] |= PAGE_USER;
    }

    return vmm_table_at(table[index]);
}

static u64 *vmm_walk_to_pt(u64 virtual_addr, bool create, u32 user_flag)
{
    u32 pml4_i = (u32)((virtual_addr >> 39) & 0x1FFu);
    u32 pdpt_i = (u32)((virtual_addr >> 30) & 0x1FFu);
    u32 pd_i = (u32)((virtual_addr >> 21) & 0x1FFu);
    u64 *pdpt;
    u64 *pd;
    u64 *pt;

    if (pml4 == NULL) {
        return NULL;
    }

    pdpt = vmm_get_or_create(pml4, pml4_i, create, user_flag);
    if (pdpt == NULL) {
        return NULL;
    }

    pd = vmm_get_or_create(pdpt, pdpt_i, create, user_flag);
    if (pd == NULL) {
        return NULL;
    }

    /* If boot left a 2 MiB page here, split is not supported — rebuild via init. */
    if ((pd[pd_i] & PAGE_PRESENT) && (pd[pd_i] & 0x80u)) {
        if (!create) {
            return NULL;
        }
        panic("VMM64: entrada PD con pagina 2MiB; reinicia identity-map");
    }

    pt = vmm_get_or_create(pd, pd_i, create, user_flag);
    return pt;
}

void vmm_map_page(u64 virtual_addr, u64 physical_addr, u32 flags)
{
    u32 user_flag = flags & PAGE_USER;
    u64 *pt = vmm_walk_to_pt(virtual_addr, true, user_flag);
    u32 pt_i = (u32)((virtual_addr >> 12) & 0x1FFu);

    if (pt == NULL) {
        panic("VMM64: no se pudo crear PT");
    }

    pt[pt_i] = (physical_addr & PTE_ADDR_MASK) | (flags & 0xFFFu) | PAGE_PRESENT;
    __asm__ volatile ("invlpg (%0)" : : "r"(virtual_addr) : "memory");
}

void vmm_unmap_page(u64 virtual_addr)
{
    u64 *pt = vmm_walk_to_pt(virtual_addr, false, 0);
    u32 pt_i;

    if (pt == NULL) {
        return;
    }

    pt_i = (u32)((virtual_addr >> 12) & 0x1FFu);
    pt[pt_i] = 0;
    __asm__ volatile ("invlpg (%0)" : : "r"(virtual_addr) : "memory");
}

u64 vmm_get_physical(u64 virtual_addr)
{
    u64 *pt = vmm_walk_to_pt(virtual_addr, false, 0);
    u32 pt_i;
    u64 entry;

    if (pt == NULL) {
        return 0;
    }

    pt_i = (u32)((virtual_addr >> 12) & 0x1FFu);
    entry = pt[pt_i];
    if (!(entry & PAGE_PRESENT)) {
        return 0;
    }

    return (entry & PTE_ADDR_MASK) | (virtual_addr & 0xFFFull);
}

void vmm_init(void)
{
    u64 addr;
    u64 limit = (u64)total_frames_os * PAGE_SIZE;
    void *pml4_page = pmm_alloc_page();

    if (pml4_page == NULL) {
        panic("VMM64: no hay frame para PML4");
    }

    if (limit == 0) {
        panic("VMM64: total_frames_os es 0");
    }

    pml4 = (u64 *)pml4_page;
    pml4_os = (u64)pml4_page;
    memset(pml4, 0, PAGE_SIZE);

    /* Identity-map managed physical memory (4 KiB pages). */
    for (addr = 0; addr < limit; addr += PAGE_SIZE) {
        vmm_map_page(addr, addr, PAGE_PRESENT | PAGE_WRITE);
    }

    __asm__ volatile (
        "mov %0, %%cr3\n"
        :
        : "r"(pml4)
        : "memory"
    );

    serial_write("VMM64 identity-map OK PML4=");
    serial_print_hex((u32)(pml4_os >> 32));
    serial_print_hex((u32)pml4_os);
    serial_write("\n");
}
