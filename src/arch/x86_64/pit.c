#include "pit.h"
#include "io.h"

#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43
#define PIT_BASE_HZ  1193182u

void pit_init(u32 frequency_hz)
{
    u32 divisor;

    if (frequency_hz == 0) {
        frequency_hz = 100;
    }

    divisor = PIT_BASE_HZ / frequency_hz;
    if (divisor == 0) {
        divisor = 1;
    }

    outb(PIT_COMMAND, 0x36);
    outb(PIT_CHANNEL0, (u8)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (u8)((divisor >> 8) & 0xFF));
}
