#ifndef CORDOS_IDT_H
#define CORDOS_IDT_H

#include "types.h"

#ifdef __x86_64__
void idt_set_gate(u8 index, u64 handler, u16 selector, u8 flags);
#else
void idt_set_gate(u8 index, u32 handler, u16 selector, u8 flags);
#endif

void idt_init(void);

#endif
