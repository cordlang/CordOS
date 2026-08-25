#ifndef NUEVOOS_SPINLOCK_H
#define NUEVOOS_SPINLOCK_H

#include "types.h"

/*
 * UP-safe (CLI while held) and SMP-ready (atomic TAS + pause).
 * irq_flags is only valid while the lock is held by one CPU.
 */
typedef struct spinlock {
    volatile u32 locked;
    u64 irq_flags;
} spinlock_t;

#define SPINLOCK_INIT { 0, 0 }

void spinlock_init(spinlock_t *lock);
void spin_lock(spinlock_t *lock);
void spin_unlock(spinlock_t *lock);

#endif
