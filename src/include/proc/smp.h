#ifndef CORDOS_SMP_H
#define CORDOS_SMP_H

#include "types.h"

/* Always >= 1. MVP reports 1 until AP bringup exists. */
extern u32 cpu_count_os;

/* Logical CPU id for this core; BSP = 0 until per-CPU data exists. */
extern u32 percpu_cpu_id;

/* Physical MMIO base of the local APIC, or 0 if disabled / unknown. */
extern u64 lapic_base_os;

void smp_init(void);

/* Spinlock self-test + smp_init; call from kmain64 when wired by INT. */
void phase10_init(void);

#endif
