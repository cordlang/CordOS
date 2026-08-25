#include "isr.h"
#include "gdt.h"
#include "idt.h"
#include "page_fault.h"
#include "pic.h"
#include "vga.h"
#include "panic.h"

extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr15(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void isr21(void);
extern void isr22(void);
extern void isr23(void);
extern void isr24(void);
extern void isr25(void);
extern void isr26(void);
extern void isr27(void);
extern void isr28(void);
extern void isr29(void);
extern void isr30(void);
extern void isr31(void);

extern void irq0(void);
extern void irq1(void);
extern void irq2(void);
extern void irq3(void);
extern void irq4(void);
extern void irq5(void);
extern void irq6(void);
extern void irq7(void);
extern void irq8(void);
extern void irq9(void);
extern void irq10(void);
extern void irq11(void);
extern void irq12(void);
extern void irq13(void);
extern void irq14(void);
extern void irq15(void);

static irq_fn irq_handlers[16];

static const char *exception_messages[] = {
    "Division by zero",
    "Debug",
    "Non-maskable interrupt",
    "Breakpoint",
    "Overflow",
    "Bound range exceeded",
    "Invalid opcode",
    "Device not available",
    "Double fault",
    "Coprocessor segment overrun",
    "Invalid TSS",
    "Segment not present",
    "Stack-segment fault",
    "General protection fault",
    "Page fault",
    "Reserved",
    "x87 floating-point",
    "Alignment check",
    "Machine check",
    "SIMD floating-point",
    "Virtualization",
    "Control protection",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Hypervisor injection",
    "VMM communication",
    "Security exception",
    "Reserved"
};

void interrupts_enable(void)
{
    __asm__ volatile ("sti");
}

void interrupts_disable(void)
{
    __asm__ volatile ("cli");
}

void irq_install_handler(u8 irq, irq_fn handler)
{
    if (irq < 16) {
        irq_handlers[irq] = handler;
    }
}

void irq_uninstall_handler(u8 irq)
{
    if (irq < 16) {
        irq_handlers[irq] = 0;
    }
}

void isr_handler(struct interrupt_frame *frame)
{
    if (frame->int_no == 14) {
        page_fault_handler(frame);
        return;
    }

    vga_print("\nEXCEPCION #");
    vga_print_u32(frame->int_no);
    vga_print(": ");

    if (frame->int_no < 32) {
        vga_print(exception_messages[frame->int_no]);
    } else {
        vga_print("desconocida");
    }

    vga_print(" err=");
    vga_print_hex(frame->err_code);
    vga_print(" eip=");
    vga_print_hex(frame->eip);
    vga_print("\n");

    panic("excepcion de CPU");
}

void irq_handler(struct interrupt_frame *frame)
{
    u8 irq = (u8)(frame->int_no - 32);

    if (irq < 16 && irq_handlers[irq]) {
        irq_handlers[irq](frame);
    }

    pic_eoi(irq);
}

void isr_install(void)
{
    idt_set_gate(0, (u32)isr0, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(1, (u32)isr1, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(2, (u32)isr2, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(3, (u32)isr3, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(4, (u32)isr4, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(5, (u32)isr5, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(6, (u32)isr6, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(7, (u32)isr7, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(8, (u32)isr8, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(9, (u32)isr9, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(10, (u32)isr10, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(11, (u32)isr11, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(12, (u32)isr12, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(13, (u32)isr13, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(14, (u32)isr14, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(15, (u32)isr15, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(16, (u32)isr16, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(17, (u32)isr17, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(18, (u32)isr18, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(19, (u32)isr19, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(20, (u32)isr20, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(21, (u32)isr21, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(22, (u32)isr22, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(23, (u32)isr23, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(24, (u32)isr24, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(25, (u32)isr25, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(26, (u32)isr26, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(27, (u32)isr27, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(28, (u32)isr28, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(29, (u32)isr29, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(30, (u32)isr30, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(31, (u32)isr31, GDT_KERNEL_CODE, 0x8E);

    idt_set_gate(32, (u32)irq0, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(33, (u32)irq1, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(34, (u32)irq2, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(35, (u32)irq3, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(36, (u32)irq4, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(37, (u32)irq5, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(38, (u32)irq6, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(39, (u32)irq7, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(40, (u32)irq8, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(41, (u32)irq9, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(42, (u32)irq10, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(43, (u32)irq11, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(44, (u32)irq12, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(45, (u32)irq13, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(46, (u32)irq14, GDT_KERNEL_CODE, 0x8E);
    idt_set_gate(47, (u32)irq15, GDT_KERNEL_CODE, 0x8E);
}
