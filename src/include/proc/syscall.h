#ifndef CORDOS_SYSCALL_H
#define CORDOS_SYSCALL_H

#include "types.h"
#include "isr.h"

#define SYS_EXIT   0
#define SYS_WRITE  1
#define SYS_READ   2
#define SYS_YIELD  3
#define SYS_GETPID 4
#define SYS_MMAP   5
#define SYS_OPEN   6
#define SYS_CLOSE  7
#define SYS_SPAWN  8
#define SYS_EXEC   9

#define PROT_READ  1ull
#define PROT_WRITE 2ull
#define PROT_EXEC  4ull

/*
 * User pointers must be PAGE_USER leaves in the low half (bit 63 clear).
 * The identity RAM window is not a stand-in for "user memory".
 */
#define USER_IDENTITY_END 0x0000000040000000ull /* 1 GiB identity (kernel) */

/* Kernel-callable dispatch (shell / tests). Returns value in rax sense. */
i64 syscall_dispatch(u64 num, u64 a0, u64 a1, u64 a2);

/* IDT 0x80 handler path (from syscall_entry.s). */
void syscall_interrupt(struct interrupt_frame *frame);

/* Register gate 0x80 → syscall_entry. Call after isr_install(). */
void syscall_init(void);

/* Fase 5 bring-up: syscall_init(). */
void phase5_init(void);

#endif
