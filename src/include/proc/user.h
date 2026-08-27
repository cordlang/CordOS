#ifndef CORDOS_USER_H
#define CORDOS_USER_H

#include "types.h"

/*
 * User image window sits at 1 GiB — above typical guest RAM identity maps,
 * below the 2 GiB cap. Syscall copies accept any PAGE_USER leaf in the low
 * half, not only identity RAM.
 */
#define USER_IMAGE_BASE     0x0000000040000000ULL
#define USER_IMAGE_MAX      0x0000000080000000ULL
#define USER_MMAP_BASE      0x0000000041000000ULL
#define USER_STACK_TOP_ELF  0x0000000041000000ULL
#define USER_MMAP_MAX       (16u * 1024u * 1024u)

/* Legacy trampoline (fallback if the embedded ELF fails to load). */
#define USER_TEXT_BASE  0x0000000010000000ULL
#define USER_STACK_BASE 0x0000000010001000ULL
#define USER_STACK_TOP  (USER_STACK_BASE + 0x1000ULL)

void user_enter(u64 rip, u64 rsp);
void user_smoke(void);

#endif
