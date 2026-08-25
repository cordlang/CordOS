# Agent F10 — Fase 10 SMP MVP

## Status

**Listo MVP** (sin commit). AP bringup no implementado; stub honesto.

## Files

| File | Role |
|---|---|
| `src/include/spinlock.h` | `spinlock_t`, `SPINLOCK_INIT`, lock/unlock API |
| `src/spinlock.c` | CLI + `__sync_lock_test_and_set` + `pause` |
| `src/include/smp.h` | `cpu_count_os`, `percpu_cpu_id`, `lapic_base_os`, `smp_init`, `phase10_init` |
| `src/smp.c` | MSR `IA32_APIC_BASE` detect + self-test |
| `docs/smp.md` | QEMU `-smp 2` y límites |

## Globals (`*_os`)

- `cpu_count_os` — always `1` until AP bringup
- `percpu_cpu_id` — always `0` (BSP)
- `lapic_base_os` — phys base from MSR `0x1B`, or `0` if APIC disabled

## Makefile (for INT)

Already listed in `agent-notes/MAKE_OBJS.md`:

```
build/spinlock.o
build/smp.o
```

Suggested rule (same pattern as other freestanding C):

```make
build/spinlock.o: src/spinlock.c
	$(CC) $(CFLAGS64) -c $< -o $@

build/smp.o: src/smp.c
	$(CC) $(CFLAGS64) -c $< -o $@
```

## `kmain64` wire-up (INT)

After memory init / before shell:

```c
#include "smp.h"
/* ... */
phase10_init();
```

Do **not** call before `vga_init`-equivalent / VGA usable (uses `vga_print*`).

## Self-test

`phase10_init()` → `smp_init()` → lock/unlock twice; prints OK/FAIL.

## Out of scope (honest stubs)

- INIT-SIPI-SIPI / AP trampoline
- LAPIC timer (PIT remains)
- MADT / MP table CPU count
- Per-CPU stacks / GDT / IDT
