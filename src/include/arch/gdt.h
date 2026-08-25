#ifndef NUEVOOS_GDT_H
#define NUEVOOS_GDT_H

#include "types.h"

#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10
#define GDT_TSS         0x18
/* 16-byte TSS occupies 0x18 and 0x20, so user segs follow it. RPL=3 baked in. */
#define GDT_USER_CODE   0x2B /* 0x28 | 3 */
#define GDT_USER_DATA   0x33 /* 0x30 | 3 */

void gdt_init(void);
void gdt_set_rsp0(u64 rsp);
u64 gdt_idle_rsp0(void);

#endif
