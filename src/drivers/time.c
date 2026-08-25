#include "time.h"
#include "isr.h"
#include "pit.h"

#ifdef __x86_64__
#include "sched.h"
#endif

volatile u32 ticks_os = 0;
volatile u32 hz_os = 1000;

static u64 tsc_per_us;

static void timer_irq(struct interrupt_frame *frame)
{
    (void)frame;
    ++ticks_os;
#ifdef __x86_64__
    /* Preemptive RR: may switch before returning to irq_handler EOI path.
     * isr64 irq_handler sends EOI before this handler for that reason. */
    scheduler_on_tick();
#endif
}

void time_init(u32 frequency_hz)
{
    hz_os = frequency_hz;
    ticks_os = 0;
    pit_init(frequency_hz);
    irq_install_handler(0, timer_irq);
}

u32 time_ticks(void)
{
    return ticks_os;
}

u32 time_uptime_ms(void)
{
    if (hz_os == 0) {
        return 0;
    }
    return (ticks_os * 1000u) / hz_os;
}

u64 time_tsc(void)
{
    u32 lo;
    u32 hi;

    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((u64)hi << 32) | lo;
}

void time_tsc_calibrate(void)
{
    u32 start;
    u32 dt;
    u64 a;
    u64 b;
    u64 us;

    if (hz_os == 0) {
        return;
    }
    start = ticks_os;
    while (ticks_os == start) {
        __asm__ volatile("pause");
    }
    a = time_tsc();
    start = ticks_os;
    while ((ticks_os - start) < 10u) {
        __asm__ volatile("pause");
    }
    b = time_tsc();
    dt = ticks_os - start;
    if (b <= a || dt == 0u) {
        return;
    }
    us = ((u64)dt * 1000000ull) / (u64)hz_os;
    if (us == 0u) {
        return;
    }
    tsc_per_us = (b - a) / us;
}

u32 time_us_since(u64 tsc0)
{
    u64 now;
    u64 delta;

    if (tsc_per_us == 0u) {
        return time_uptime_ms() * 1000u;
    }
    now = time_tsc();
    if (now < tsc0) {
        return 0;
    }
    delta = now - tsc0;
    delta /= tsc_per_us;
    if (delta > 0xFFFFFFFFull) {
        return 0xFFFFFFFFu;
    }
    return (u32)delta;
}
