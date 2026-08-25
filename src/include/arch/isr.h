#ifndef NUEVOOS_ISR_H
#define NUEVOOS_ISR_H

#include "types.h"

#ifdef __x86_64__
/* Built by isr64.s: GPRs, then int_no/err, then CPU frame. */
struct interrupt_frame {
    u64 r15, r14, r13, r12, r11, r10, r9, r8;
    u64 rbp, rdi, rsi, rdx, rcx, rbx, rax;
    u64 int_no, err_code;
    u64 rip, cs, rflags, rsp, ss;
};
#else
struct interrupt_frame {
    u32 gs, fs, es, ds;
    u32 edi, esi, ebp, esp_dummy;
    u32 ebx, edx, ecx, eax;
    u32 int_no, err_code;
    u32 eip, cs, eflags, useresp, ss;
};
#endif

typedef void (*irq_fn)(struct interrupt_frame *frame);

void isr_install(void);
void irq_install_handler(u8 irq, irq_fn handler);
void irq_uninstall_handler(u8 irq);
void interrupts_enable(void);
void interrupts_disable(void);

void isr_handler(struct interrupt_frame *frame);
void irq_handler(struct interrupt_frame *frame);

#endif
