#include "gdt.h"
#include "string.h"

struct gdt_entry {
    u16 limit_low;
    u16 base_low;
    u8 base_middle;
    u8 access;
    u8 granularity;
    u8 base_high;
} __attribute__((packed));

struct gdt_ptr {
    u16 limit;
    u32 base;
} __attribute__((packed));

struct tss_entry {
    u32 prev_tss;
    u32 esp0;
    u32 ss0;
    u32 esp1;
    u32 ss1;
    u32 esp2;
    u32 ss2;
    u32 cr3;
    u32 eip;
    u32 eflags;
    u32 eax;
    u32 ecx;
    u32 edx;
    u32 ebx;
    u32 esp;
    u32 ebp;
    u32 esi;
    u32 edi;
    u32 es;
    u32 cs;
    u32 ss;
    u32 ds;
    u32 fs;
    u32 gs;
    u32 ldt;
    u16 trap;
    u16 iomap_base;
} __attribute__((packed));

static struct gdt_entry gdt[6];
static struct gdt_ptr gdt_pointer;
static struct tss_entry tss;
static u8 kernel_stack[16384];

static void gdt_set_gate(u32 index, u32 base, u32 limit, u8 access, u8 gran)
{
    gdt[index].base_low = (u16)(base & 0xFFFF);
    gdt[index].base_middle = (u8)((base >> 16) & 0xFF);
    gdt[index].base_high = (u8)((base >> 24) & 0xFF);
    gdt[index].limit_low = (u16)(limit & 0xFFFF);
    gdt[index].granularity = (u8)((limit >> 16) & 0x0F);
    gdt[index].granularity |= gran & 0xF0;
    gdt[index].access = access;
}

static void tss_init(void)
{
    u32 base = (u32)&tss;
    u32 limit = sizeof(tss) - 1;

    memset(&tss, 0, sizeof(tss));
    tss.ss0 = GDT_KERNEL_DATA;
    tss.esp0 = (u32)(kernel_stack + sizeof(kernel_stack));
    tss.iomap_base = sizeof(tss);

    gdt_set_gate(3, base, limit, 0x89, 0x00);
}

void gdt_init(void)
{
    gdt_pointer.limit = (u16)(sizeof(gdt) - 1);
    gdt_pointer.base = (u32)&gdt;

    gdt_set_gate(0, 0, 0, 0, 0);
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); /* kernel code */
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF); /* kernel data */
    tss_init();
    gdt_set_gate(4, 0, 0, 0, 0); /* reserved */
    gdt_set_gate(5, 0, 0, 0, 0); /* reserved */

    __asm__ volatile (
        "lgdt (%0)\n"
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "mov %%ax, %%ss\n"
        "ljmp $0x08, $1f\n"
        "1:\n"
        "mov $0x18, %%ax\n"
        "ltr %%ax\n"
        :
        : "r"(&gdt_pointer)
        : "ax", "memory"
    );
}
