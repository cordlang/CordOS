#include "smp.h"
#include "spinlock.h"
#include "serial.h"

#define IA32_APIC_BASE_MSR 0x1Bu
#define APIC_BASE_ENABLE   (1u << 11)
#define APIC_BASE_BSP      (1u << 8)
#define APIC_BASE_ADDR_MASK 0xFFFFFFFFFFFFF000ULL

u32 cpu_count_os = 1;
u32 percpu_cpu_id = 0;
u64 lapic_base_os = 0;

static u64 rdmsr(u32 msr)
{
    u32 lo;
    u32 hi;

    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((u64)hi << 32) | (u64)lo;
}

static void detect_lapic(void)
{
    u64 apic_base;

    apic_base = rdmsr(IA32_APIC_BASE_MSR);
    if ((apic_base & APIC_BASE_ENABLE) == 0) {
        lapic_base_os = 0;
        return;
    }

    lapic_base_os = apic_base & APIC_BASE_ADDR_MASK;
}

void smp_init(void)
{
    /* Honest MVP: BSP only. No SIPI / AP trampoline yet. */
    cpu_count_os = 1;
    percpu_cpu_id = 0;
    detect_lapic();
}

static void spinlock_self_test(void)
{
    spinlock_t lock = SPINLOCK_INIT;
    volatile u32 counter = 0;

    spin_lock(&lock);
    counter++;
    spin_unlock(&lock);

    spin_lock(&lock);
    counter++;
    spin_unlock(&lock);

    if (counter != 2) {
        serial_write("smp: spinlock FAIL\n");
    }
}

void phase10_init(void)
{
    smp_init();
    spinlock_self_test();
}
