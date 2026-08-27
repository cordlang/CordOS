#include "brand.h"
#include "compositor.h"
#include "config.h"
#include "draw.h"
#include "fb.h"
#include "gdt.h"
#include "heap.h"
#include "i18n.h"
#include "idt.h"
#include "isr.h"
#include "keyboard.h"
#include "lang_select.h"
#include "mouse.h"
#include "multiboot2.h"
#include "onboarding.h"
#include "panic.h"
#include "phase9.h"
#include "pic.h"
#include "pmm.h"
#include "serial.h"
#include "session.h"
#include "smp.h"
#include "syscall.h"
#include "task.h"
#include "time.h"
#include "userdb.h"
#include "vga.h"
#include "vfs.h"
#include "vmm.h"

extern char _kernel_start[];
extern char _kernel_end[];

static void init_interrupts(void)
{
    gdt_init();
    idt_init();
    isr_install();

    pic_remap(0x20, 0x28);
    pic_mask_all();

    time_init(1000);
    keyboard_init();
    mouse_init();

    pic_clear_mask(0);
    pic_clear_mask(1);
    interrupts_enable();
}

void kmain64(void *mb2_addr)
{
    bool lang_from_cmdline;

    i18n_init();

    if (mb2_addr == NULL) {
        panic("Multiboot2 info nulo");
    }

    init_interrupts();
    phase5_init();

    serial_init();
    serial_write("\n=== CordOS boot ===\n");

    /* Memory + FB first so graphics UI can own the screen. */
    pmm_init(mb2_addr);
    vmm_init();
    heap_init();
    kselftest_run();
    (void)_kernel_start;
    (void)_kernel_end;
    fb_parse_cmdline(mb2_addr);
    fb_init(mb2_addr);
    if (fb_available()) {
        mouse_set_bounds(fb_width(), fb_height());
        draw_boot_splash(8u);
        draw_quality_init();
        ui_comp_init();
        draw_boot_splash(24u);
    }

    lang_from_cmdline = lang_try_cmdline(mb2_addr);
    pmm_release_boot_info();

    /* Disk + net before onboarding so accounts and Wi-Fi probe work. */
    phase9_init();
    phase6_init();
    userdb_load();

    if (userdb_count() == 0u) {
        if (fb_available()) {
            onboarding_run(!lang_from_cmdline);
        } else if (!lang_from_cmdline) {
            lang_select_run();
        }
    } else if (!lang_from_cmdline) {
        /* Language already restored from persist. */
    }

    session_splash_begin();

    session_splash_stage(0);
    session_splash_stage(1);
    session_splash_stage(2);
    session_splash_stage(3);
    phase4_init();
    phase10_init();
    session_splash_finish();

    serial_write("boot: session UI\n");
    session_run();
}
