#include "config.h"
#include "gdt.h"
#include "heap.h"
#include "idt.h"
#include "isr.h"
#include "keyboard.h"
#include "multiboot.h"
#include "panic.h"
#include "pic.h"
#include "pmm.h"
#include "time.h"
#include "vga.h"
#include "vmm.h"

static void print_banner(void)
{
    vga_print(name_os);
    vga_print(" ");
    vga_print(version_os);
    vga_print(" (");
    vga_print(codename_os);
    vga_print(")\n");

    vga_print("arch=");
    vga_print(arch_os);
    vga_print(" build=");
    vga_print(build_os);
    vga_print("\n\n");
}

static void update_status_line(void)
{
    vga_write_at(24, 0, "ticks=");
    vga_write_u32_at(24, 6, time_ticks(), 8);
    vga_write_at(24, 15, " free_fr=");
    vga_write_u32_at(24, 24, free_frames_os, 6);
    vga_write_at(24, 31, " | Fase 2 memoria");
}

static void init_interrupts(void)
{
    gdt_init();
    idt_init();
    isr_install();

    pic_remap(0x20, 0x28);
    pic_mask_all();

    time_init(100);
    keyboard_init();

    pic_clear_mask(0);
    pic_clear_mask(1);
    interrupts_enable();
}

static void memory_self_test(void)
{
    void *page_a;
    void *page_b;
    char *block;
    char *block2;
    u32 phys;

    vga_print("\n--- test memoria ---\n");

    page_a = pmm_alloc_page();
    page_b = pmm_alloc_page();
    if (page_a == NULL || page_b == NULL) {
        panic("test: pmm_alloc_page fallo");
    }

    vga_print("frames alloc: ");
    vga_print_hex((u32)page_a);
    vga_print(" y ");
    vga_print_hex((u32)page_b);
    vga_print("\n");

    pmm_free_page(page_b);
    pmm_free_page(page_a);

    block = (char *)kmalloc(64);
    block2 = (char *)kmalloc(128);
    if (block == NULL || block2 == NULL) {
        panic("test: kmalloc fallo");
    }

    block[0] = 'N';
    block[1] = 'O';
    block[2] = 'S';
    block[3] = '\0';
    vga_print("kmalloc OK: ");
    vga_print(block);
    vga_print(" @ ");
    vga_print_hex((u32)block);
    vga_print("\n");

    phys = vmm_get_physical((u32)block);
    vga_print("virt->phys: ");
    vga_print_hex(phys);
    vga_print("\n");

    kfree(block);
    kfree(block2);

    pmm_print_stats();
    heap_print_stats();
    vga_print("--- fin test ---\n\n");
}

static void shell_echo_loop(void)
{
    u32 last_ticks = 0;

    vga_print("Teclado activo. Siguiente fase: x86_64 o procesos.\n");
    vga_print("> ");

    for (;;) {
        if (time_ticks() != last_ticks) {
            last_ticks = time_ticks();
            update_status_line();
        }

        if (keyboard_has_char()) {
            char character = keyboard_getchar();
            vga_putc(character);
        }

        __asm__ volatile ("hlt");
    }
}

void kmain(u32 magic, u32 multiboot_info_address)
{
    const struct multiboot_info *info;

    vga_clear();
    print_banner();
    vga_print("Kernel freestanding en C iniciado.\n");
    vga_print("Fase 2: PMM, VMM, heap, page fault.\n\n");

    if (!multiboot_is_valid(magic)) {
        vga_print("Magic Multiboot invalido: ");
        vga_print_hex(magic);
        vga_print("\n");
        panic("arranque Multiboot fallido");
    }

    info = multiboot_get(multiboot_info_address);
    vga_print("Multiboot valido. Info en ");
    vga_print_hex(multiboot_info_address);
    vga_print("\n");
    multiboot_print_summary(info);
    vga_print("\n");

    pmm_init(info);
    pmm_print_stats();

    init_interrupts();
    vga_print("Interrupciones activas.\n");

    vmm_init();
    heap_init();
    memory_self_test();

    update_status_line();
    shell_echo_loop();
}
