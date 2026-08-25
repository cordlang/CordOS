# PHASE5 / F5 — Syscalls MVP

## Entregado

| Artefacto | Rol |
|-----------|-----|
| `docs/abi.md` | ABI `int 0x80`, nums 0–5, init order |
| `src/include/syscall.h` | `syscall_dispatch` / `syscall_init` / `phase5_init` |
| `src/syscall.c` | exit/write/read/yield/getpid/mmap-stub |
| `src/arch/x86_64/syscall_entry.s` | IDT stub → `interrupt_frame` → `syscall_interrupt` |
| `user/libnos/syscall.S` + `.h` | stubs userland (`nos_write`, `nos_exit`, …) |
| `user/test_write.c` | smoke test — **no** link al kernel |

## Comportamiento

- `write(1, …)` → `vga_putc`
- `read(0, …)` → buffer teclado (bloqueante con `hlt`)
- `yield` / `getpid` → weak `task_yield` / `task_current` (F4 override)
- `mmap` → `-1`
- Gate `0x80` flags `0x8E` (DPL=0); subir a `0xEE` con ring3

## Call order (kernel64)

Tras `isr_install()` (y teclado si se usa `read`):

```c
#include "syscall.h"
/* … */
isr_install();
/* pic / time / keyboard … */
phase5_init();   /* = syscall_init → idt_set_gate(0x80, syscall_entry) */
```

**No se llamó desde `kernel64.c` aún** — hace falta linkear objs (orquestador).
Sin esos objs, un call rompería el build.

## Makefile (orquestador)

Añadir a `KERNEL64_OBJS` + reglas (ver `MAKE_OBJS.md`):

```
build/syscall.o         # src/syscall.c
build/syscall_entry.o   # src/arch/x86_64/syscall_entry.s
```

## Conflictos / notas

- `struct task` mínimo local en `syscall.c` (solo `pid`); F4 debe poner `pid` primero.
- No toca `isr64.c` — gate separado vía `syscall_init`.
- Userland no entra en ISO/kernel.

---

## F5 ring-3 ABI (this pass)

### Hecho

- IDT `0x80` flags **`0xEE` (DPL=3)**, selector still `GDT_KERNEL_CODE` (`0x08`).
- User pointer checks for `write` / `read` / `mmap` / `open`:
  - reject NULL, wrap, bit 63 (kernel canonical high), and anything at/above
    `USER_IDENTITY_END` (`0x40000000`, 1 GiB identity window)
  - copy in/out through a 256-byte kernel bounce buffer; path copy NUL-terminated, max 256
- `SYS_EXIT`: `task_exit()` when `task_current()` is non-NULL (TASK_DEAD + schedule);
  halt fallback if no task. Weak `task_exit` in `syscall.c` if F4 not linked.
- `SYS_OPEN=6` → `vfs_open` (path copied first). `SYS_CLOSE=7` → `vfs_close`.
  Unmounted vfs / bad fd / bad path → `-1`. No ATA.
- `SYS_WRITE` `fd!=1` → `-1` (no `vfs_write` this phase).
- `syscall_entry.s` left as-is (long-mode frame already has SS/RSP for `iretq`).
- Userland: `nos_open` / `nos_close` in `user/libnos`. `make userland` →
  `out/user_hello.elf` from `user/test_write.c` (not linked into the kernel).
- `docs/abi.md` matches numbers 0–7, DPL=3, error model, allowed VA range.

### Verify

```
make ARCH=x86_64 out/nuevoos64.bin
make ARCH=x86_64 userland
```

GDT / `gdt64.c`, `src/fs/*`, `src/ui/**` not touched.
