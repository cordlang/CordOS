# SMP (Fase 10 MVP)

## Qué hay hoy

| Pieza | Estado |
|---|---|
| `spinlock_t` / `spin_lock` / `spin_unlock` | Listo (CLI + atomic TAS) |
| `smp_init()` | Detecta base LAPIC vía `IA32_APIC_BASE` (MSR `0x1B`) |
| `cpu_count_os` | Siempre **1** (solo BSP) |
| `percpu_cpu_id` | Siempre **0** |
| Arranque de APs (INIT-SIPI-SIPI) | **No implementado** |
| Timer LAPIC / IPI | **No implementado** |

El self-test corre en `phase10_init()` (lock → unlock dos veces).

## Probar con QEMU `-smp 2`

El kernel arranca en un solo núcleo aunque QEMU ofrezca más CPUs. Eso es
esperado en este MVP: sin trampolín AP ni SIPI, los APs quedan en el wait
loop del firmware/QEMU y CordOS no los usa.

```bash
# Path Multiboot/GRUB habitual (ajusta el target de make si cambia)
make ARCH=x86_64
qemu-system-x86_64 -cdrom cordos32.iso -smp 2 -serial stdio
```

En consola VGA deberías ver algo como:

```text
--- fase 10 SMP ---
smp: LAPIC base=........ (BSP)
smp: cpu_count_os=1 (AP bringup stubbed)
smp: spinlock self-test OK
--- fin fase 10 ---
```

`-smp 2` no cambia `cpu_count_os` hasta que exista bringup real.

## Límites actuales

1. **Uniprocessor lógico** — todo el kernel corre en el BSP.
2. **Spinlocks** — seguros en UP (IRQs off mientras se posee el lock) y listos
   para SMP vía `__sync_lock_test_and_set` + `pause`; aún no hay datos
   per-CPU ni `spin_lock_irqsave` anidado sofisticado.
3. **LAPIC** — solo se lee la base física del MSR; no se mapea MMIO ni se
   configura el timer (sigue el PIT vía PIC).
4. **Sin ACPI MADT / MP tables** — no se enumera el número real de cores.
5. **Wire-up** — el orquestador (INT) debe enlazar `build/spinlock.o` y
   `build/smp.o` y llamar `phase10_init()` desde `kmain64`.

## Próximos pasos (fuera de este MVP)

- Parsear MADT o tablas MP → `cpu_count_os`
- Trampolín AP + INIT-SIPI-SIPI
- IDT/GDT/stack per-CPU y `percpu_cpu_id`
- Timer LAPIC + IPIs para reschedule
