#include "idt.h"
#include "gdt.h"
#include "string.h"

struct idt_entry64 {
    u16 offset_low;
    u16 selector;
    u8 ist;
    u8 flags;
    u16 offset_mid;
    u32 offset_high;
    u32 zero;
} __attribute__((packed));

struct idt_ptr64 {
    u16 limit;
    u64 base;
} __attribute__((packed));

static struct idt_entry64 idt[256];
static struct idt_ptr64 idt_pointer;

void idt_set_gate(u8 index, u64 handler, u16 selector, u8 flags)
{
    idt[index].offset_low = (u16)(handler & 0xFFFF);
    idt[index].offset_mid = (u16)((handler >> 16) & 0xFFFF);
    idt[index].offset_high = (u32)((handler >> 32) & 0xFFFFFFFF);
    idt[index].selector = selector;
    idt[index].ist = 0;
    idt[index].flags = flags;
    idt[index].zero = 0;
}

void idt_init(void)
{
    idt_pointer.limit = (u16)(sizeof(idt) - 1);
    idt_pointer.base = (u64)&idt;

    memset(idt, 0, sizeof(idt));

    __asm__ volatile ("lidt (%0)" : : "r"(&idt_pointer) : "memory");
}
