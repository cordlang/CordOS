#include "spinlock.h"

void spinlock_init(spinlock_t *lock)
{
    lock->locked = 0;
    lock->irq_flags = 0;
}

static u64 irq_save(void)
{
    u64 flags;

    __asm__ volatile(
        "pushfq\n\t"
        "popq %0\n\t"
        "cli"
        : "=r"(flags)
        :
        : "memory");
    return flags;
}

static void irq_restore(u64 flags)
{
    __asm__ volatile(
        "pushq %0\n\t"
        "popfq"
        :
        : "r"(flags)
        : "memory", "cc");
}

void spin_lock(spinlock_t *lock)
{
    u64 flags = irq_save();

    /*
     * Keep IRQs off while spinning so a uniprocessor IRQ handler cannot
     * re-enter the same lock. On SMP, pause avoids bus hammering; short
     * critical sections keep this acceptable until proper per-CPU irqsave.
     */
    while (__sync_lock_test_and_set(&lock->locked, 1)) {
        __asm__ volatile("pause");
    }

    lock->irq_flags = flags;
}

void spin_unlock(spinlock_t *lock)
{
    u64 flags = lock->irq_flags;

    lock->irq_flags = 0;
    __sync_lock_release(&lock->locked);
    irq_restore(flags);
}
