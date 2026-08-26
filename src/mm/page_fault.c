#include "page_fault.h"
#include "vga.h"
#include "panic.h"
#include "serial.h"
#include "task.h"

#ifdef __x86_64__
static void print_u64_hex(u64 value)
{
    vga_print_hex((u32)(value >> 32));
    vga_print_hex((u32)value);
}
#endif

void page_fault_handler(struct interrupt_frame *frame)
{
#ifdef __x86_64__
    u64 fault_addr;

    __asm__ volatile ("mov %%cr2, %0" : "=r"(fault_addr));

    vga_print("\nPAGE FAULT\n");
    vga_print("  CR2=");
    print_u64_hex(fault_addr);
    vga_print(" err=");
    print_u64_hex(frame->err_code);
    vga_print("\n  rip=");
    print_u64_hex(frame->rip);
#else
    u32 fault_addr;

    __asm__ volatile ("mov %%cr2, %0" : "=r"(fault_addr));

    vga_print("\nPAGE FAULT\n");
    vga_print("  CR2=");
    vga_print_hex(fault_addr);
    vga_print(" err=");
    vga_print_hex(frame->err_code);
    vga_print("\n  eip=");
    vga_print_hex(frame->eip);
#endif
    vga_print("\n  bits:");

    if (frame->err_code & 0x1) {
        vga_print(" present");
    } else {
        vga_print(" not-present");
    }

    if (frame->err_code & 0x2) {
        vga_print(" write");
    } else {
        vga_print(" read");
    }

    if (frame->err_code & 0x4) {
        vga_print(" user");
    } else {
        vga_print(" kernel");
    }

    vga_print("\n");

    /*
     * No COW / demand paging. A CPL3 fault kills the task; a kernel fault
     * is still fatal (kernel bug or a user pointer we failed to reject).
     */
    if ((frame->err_code & 0x4) != 0 &&
        current_task_os != NULL &&
        current_task_os->pid != 0) {
        serial_write("page_fault: user — kill task\n");
        task_exit();
    }
    panic("page fault no recuperable");
}
