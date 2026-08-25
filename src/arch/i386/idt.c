#include "idt.h"
#include "gdt.h"
#include "string.h"

struct idt_entry {
    u16 offset_low;
    u16 selector;
    u8 zero;
    u8 flags;
    u16 offset_high;
} __attribute__((packed));

struct idt_ptr {
    u16 limit;
    u32 base;
} __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_ptr idt_pointer;

void idt_set_gate(u8 index, u32 handler, u16 selector, u8 flags)
{
    idt[index].offset_low = (u16)(handler & 0xFFFF);
    idt[index].offset_high = (u16)((handler >> 16) & 0xFFFF);
    idt[index].selector = selector;
    idt[index].zero = 0;
    idt[index].flags = flags;
}

void idt_init(void)
{
    idt_pointer.limit = (u16)(sizeof(idt) - 1);
    idt_pointer.base = (u32)&idt;

    memset(idt, 0, sizeof(idt));

    __asm__ volatile ("lidt (%0)" : : "r"(&idt_pointer) : "memory");
}
