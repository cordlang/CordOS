#ifndef NUEVOOS_ARCH_X86_64_GDT_H
#define NUEVOOS_ARCH_X86_64_GDT_H

/*
 * Long-mode GDT implementation: src/arch/x86_64/gdt64.c
 * Public API: gdt_init() via gdt.h (selectors unchanged).
 *
 * Layout:
 *   0x00 null
 *   0x08 kernel code (L=1)
 *   0x10 kernel data
 *   0x18 TSS (16-byte system segment; occupies slots 3–4 / 0x18–0x20)
 *   0x28 user code (L=1, DPL=3) → selector 0x2B
 *   0x30 user data (DPL=3)      → selector 0x33
 */

#include "gdt.h"

#endif
