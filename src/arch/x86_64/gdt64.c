#include "gdt.h"
#include "string.h"

/* Long-mode GDT: null | kcode | kdata | TSS (16-byte) | ucode | udata. */

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
    u64 base;
} __attribute__((packed));

/* System V AMD64 TSS (IST-capable; IST entries left zero for now). */
struct tss64 {
    u32 reserved0;
    u64 rsp0;
    u64 rsp1;
    u64 rsp2;
    u64 reserved1;
    u64 ist1;
    u64 ist2;
    u64 ist3;
    u64 ist4;
    u64 ist5;
    u64 ist6;
    u64 ist7;
    u64 reserved2;
    u16 reserved3;
    u16 iomap_base;
} __attribute__((packed));

/*
 * Slots: 0 null, 1 kcode, 2 kdata, 3–4 TSS, 5 ucode, 6 udata.
 * Slot 4 is the upper half of the 16-byte TSS descriptor — do not reuse it.
 */
static struct gdt_entry gdt[7];
static struct gdt_ptr gdt_pointer;
static struct tss64 tss;
static u8 kernel_stack[16384] __attribute__((aligned(16)));
static u64 idle_rsp0;

static void gdt_set_gate(u32 index, u32 base, u32 limit, u8 access, u8 gran)
{
    gdt[index].base_low = (u16)(base & 0xFFFF);
    gdt[index].base_middle = (u8)((base >> 16) & 0xFF);
    gdt[index].base_high = (u8)((base >> 24) & 0xFF);
    gdt[index].limit_low = (u16)(limit & 0xFFFF);
    gdt[index].granularity = (u8)((limit >> 16) & 0x0F);
    gdt[index].granularity |= (u8)(gran & 0xF0);
    gdt[index].access = access;
}

static void gdt_set_tss(u64 base, u32 limit)
{
    gdt[3].limit_low = (u16)(limit & 0xFFFF);
    gdt[3].base_low = (u16)(base & 0xFFFF);
    gdt[3].base_middle = (u8)((base >> 16) & 0xFF);
    gdt[3].base_high = (u8)((base >> 24) & 0xFF);
    gdt[3].access = 0x89; /* present, available 64-bit TSS */
    gdt[3].granularity = (u8)((limit >> 16) & 0x0F);

    /* Upper 8 bytes of the TSS descriptor occupy GDT index 4. */
    {
        u32 *high = (u32 *)&gdt[4];
        high[0] = (u32)(base >> 32);
        high[1] = 0;
    }
}

static void tss_init(void)
{
    memset(&tss, 0, sizeof(tss));
    idle_rsp0 = (u64)(kernel_stack + sizeof(kernel_stack));
    tss.rsp0 = idle_rsp0;
    tss.iomap_base = sizeof(tss);

    gdt_set_tss((u64)&tss, (u32)(sizeof(tss) - 1));
}

void gdt_set_rsp0(u64 rsp)
{
    tss.rsp0 = rsp;
}

u64 gdt_idle_rsp0(void)
{
    return idle_rsp0;
}

/*
 * Reload CS via far return (lretq). Boot may already have a temporary GDT
 * for entering long mode; this installs the final kernel GDT + TSS.
 */
static void gdt_load_and_reload(void)
{
    __asm__ volatile (
        "lgdt (%0)\n"
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%ss\n"
        "xor %%eax, %%eax\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "pushq $0x08\n"
        "leaq 1f(%%rip), %%rax\n"
        "pushq %%rax\n"
        "lretq\n"
        "1:\n"
        "mov $0x18, %%ax\n"
        "ltr %%ax\n"
        :
        : "r"(&gdt_pointer)
        : "rax", "memory"
    );
}

void gdt_init(void)
{
    gdt_pointer.limit = (u16)(sizeof(gdt) - 1);
    gdt_pointer.base = (u64)&gdt;

    memset(gdt, 0, sizeof(gdt));

    gdt_set_gate(0, 0, 0, 0, 0);
    /* Kernel code: access=0x9A, L=1 (long mode), D=0 → gran high nibble 0x2 */
    gdt_set_gate(1, 0, 0, 0x9A, 0x20);
    /* Kernel data */
    gdt_set_gate(2, 0, 0, 0x92, 0x00);
    tss_init();
    /* User code: DPL=3, L=1. Selector 0x28 | RPL3 = 0x2B. */
    gdt_set_gate(5, 0, 0, 0xFA, 0x20);
    /* User data: DPL=3. Selector 0x30 | RPL3 = 0x33. */
    gdt_set_gate(6, 0, 0, 0xF2, 0x00);

    gdt_load_and_reload();
}
