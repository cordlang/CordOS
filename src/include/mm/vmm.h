#ifndef CORDOS_VMM_H
#define CORDOS_VMM_H

#include "types.h"

#define PAGE_PRESENT 0x001u
#define PAGE_WRITE   0x002u
#define PAGE_USER    0x004u
#define PAGE_PWT     0x008u
#define PAGE_PCD     0x010u
#define PAGE_PS      0x080u

#ifdef __x86_64__
extern volatile u64 pml4_os;

void vmm_init(void);
void vmm_map_page(u64 virtual_addr, u64 physical_addr, u32 flags);
void vmm_unmap_page(u64 virtual_addr);
u64 vmm_get_physical(u64 virtual_addr);
int vmm_page_mapped(u64 virtual_addr);
/* Present and PAGE_USER at the leaf (and 2MiB PDE). Syscall copies. */
int vmm_page_user(u64 virtual_addr);
/* Map `len` bytes at `va` (page-aligned) as user. Fails if any page is
 * already a kernel mapping. extra_flags is PAGE_WRITE or 0. */
int vmm_map_user_anon(u64 va, u64 len, u32 extra_flags);
void vmm_unmap_user_anon(u64 va, u64 len);
#else
extern volatile u32 page_directory_os;

void vmm_init(void);
void vmm_map_page(u32 virtual_addr, u32 physical_addr, u32 flags);
void vmm_unmap_page(u32 virtual_addr);
u32 vmm_get_physical(u32 virtual_addr);
#endif

#endif
