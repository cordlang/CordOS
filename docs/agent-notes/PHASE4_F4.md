# Agent F4 — Fase 4 task / context / scheduler

**Estado:** hecho (MVP x86_64)  
**Alcance:** PCB, `switch_context`, round-robin preemptivo + yield, demo 2 tasks.

## API (contrato)

| Símbolo | Archivo |
|---|---|
| `struct task`, `task_init/create/current/yield/exit` | `src/include/task.h`, `src/task.c` |
| `switch_context(u64 **old_sp, u64 *new_sp)` | `src/arch/x86_64/switch.s` |
| `schedule`, `scheduler_on_tick`, `sched_start` | `src/include/sched.h`, `src/sched.c` |
| `phase4_init()` | `task.c` → llamado desde `kmain64` tras heap |

## Sync F5

`task_yield` / `task_current` are **strong** in `task.o` and override the weak no-ops in `syscall.c` for `SYS_YIELD` / `SYS_GETPID`. `struct task.pid` is the first field (layout-compatible with F5’s local `struct task { u32 pid; }`).

## Globales

- `current_task_os`, `task_table_os[]`, `tasks_ready_os`
- `scheduler_ticks_os`, `sched_enabled_os`

## Diseño

1. **Idle/bootstrap** = slot 0 (`"idle"`): stack actual de `kmain64` / shell. No stack propia.
2. **Nuevas tasks**: `kmalloc(8 KiB)` + frame de callee-saved + RIP → `task_bootstrap` → entry.
3. **RR**: `sched_pick_next` salta al siguiente `TASK_READY`; tick PIT llama `scheduler_on_tick`.
4. **EOI temprano** en `isr64.c` `irq_handler` para que el switch desde IRQ0 no deje el PIC sin EOI.
5. **`time.c`**: `#ifdef __x86_64__` → `scheduler_on_tick()` (i386 intacto).

## Demo

`demo_task_a` / `demo_task_b` escriben contadores en filas VGA 20/21; yield periódico + preempt por tick.

## Makefile

`KERNEL64_OBJS` += `task.o` `sched.o` `user.o` `switch.o` (+ rules). Ver también `MAKE_OBJS.md`.

## Ring 3 (user mode)

Real CPL=3 via `iretq`. Kernel demo tasks stay unused (they starved IRQs).

### GDT selectors

| Selector | Slot | Role |
|---|---|---|
| `0x08` | 1 | kernel code (unchanged) |
| `0x10` | 2 | kernel data (unchanged) |
| `0x18` | 3–4 | 16-byte TSS (unchanged; occupies 0x18 and 0x20) |
| `0x2B` | 5 | user code `0x28\|3` (L=1, DPL=3, access `0xFA`) |
| `0x33` | 6 | user data `0x30\|3` (DPL=3, access `0xF2`) |

`0x23`/`0x2B` is not possible with a 16-byte TSS at `0x18` (upper half is `0x20`).

### TSS.RSP0

`gdt_set_rsp0(u64)` / `gdt_idle_rsp0()`. `schedule()` loads the next task’s kernel stack top (`kstack_base + TASK_STACK_SIZE`); idle uses the GDT-local 16 KiB stack. CPL3 interrupts land on that stack.

### User addresses

Preferred (below F5 `USER_IDENTITY_END` = 1 GiB so `SYS_WRITE` `copy_from_user` accepts the buffer):

- `USER_TEXT_BASE  = 0x0000000010000000` (256 MiB)
- `USER_STACK_BASE = 0x0000000010001000` (stack top = base + 4 KiB)

If that VA is already identity-mapped, smoke falls back to the allocated frame’s own identity address and still sets `PAGE_USER` on every paging level (`vmm_map_page`).

### Enter ring 3

`user_enter(rip, rsp)` (`src/proc/user.c`): disable IRQs, set RSP0, `iretq` with CS=`0x2B`, SS=`0x33`, RFLAGS=`0x202` (IF).

`phase4_init` → `user_smoke()`:

1. Map text/stack, plant a byte trampoline (store CS, `int $0x80` SYS_WRITE, SYS_EXIT).
2. `task_create(user_smoke_task)` then idle `task_yield()`s until that pid is `TASK_DEAD`.
3. SYS_EXIT → `task_exit()` (F5) switches back to idle; `phase4_init` returns so GUI boot continues.
4. Serial: `phase4: user cpl=3` if the trampoline stored CS with RPL=3.

Gate `0x80` is already DPL=3 (`0xEE` in `syscall_init`).

## No hecho (fuera de MVP)

- Sleep / block real (solo `TASK_BLOCKED` enum)
- Prioridades
- Per-process address spaces / ELF loader
