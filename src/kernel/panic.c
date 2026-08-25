#include "panic.h"
#include "vga.h"

void halt_forever(void)
{
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

void panic(const char *message)
{
    vga_print("\nPANIC: ");
    vga_print(message);
    vga_print("\n");
    halt_forever();
}
