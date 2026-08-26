#ifndef CORDOS_PMM_H
#define CORDOS_PMM_H

#include "types.h"

#define PAGE_SIZE 4096u

extern volatile u32 total_frames_os;
extern volatile u32 used_frames_os;
extern volatile u32 free_frames_os;

#ifdef __x86_64__
void pmm_init(void *mb2_addr);
/* Contiguous physical pages for early identity heap; 0 on failure. */
u64 pmm_alloc_contiguous(u32 count);
/* After the last Multiboot2 consumer (FB / cmdline). Safe no-op if unused. */
void pmm_release_boot_info(void);
#else
#include "multiboot.h"
void pmm_init(const struct multiboot_info *info);
#endif

u32 pmm_alloc_frame(void);
void pmm_free_frame(u32 frame);
void *pmm_alloc_page(void);
void pmm_free_page(void *page);
void pmm_print_stats(void);

#endif
