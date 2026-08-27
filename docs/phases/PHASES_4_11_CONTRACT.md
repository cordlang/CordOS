# CordOS — Contrato fases 4–11 (histórico)

**Cerrado.** El plan vivo es [`docs/ROADMAP.md`](../ROADMAP.md) a partir de la
Fase 12. No asignar trabajo nuevo contra esta tabla.

**Arquitectura objetivo:** solo ampliar `ARCH=x86_64`. No romper i386.
**No commits** salvo que el usuario lo pida.

## Estado

| Fase | Agente | Alcance MVP | Estado |
|---|---|---|---|
| 4 | F4 | task/context/scheduler preemptivo (+ camino a ring3 si da tiempo) | hecho |
| 5 | F5 | syscalls 0–5 + docs/abi.md + stub user `sys_write`/`exit` | listo (objs en MAKE_OBJS; wire phase5_init orquestador) |
| 6 | F6 | initrd embebido + VFS + nosfs mínimo (read file) | listo MVP |
| 7 | F7 | shell kernel-mode o user-mode con builtins help/echo/clear/ls/cat | MVP listo |
| 8 | F8 | Stage1 BIOS MBR + Stage2 carga kernel (QEMU disco) | MVP listo (experimental) |
| 9 | F9 | serial COM1 + PCI enum + virtio-net detect (+ IPC pipes stub) | listo MVP |
| 10 | F10 | spinlock/atomics + LAPIC timer stub + doc SMP `-smp 2` | listo MVP |
| 11 | F11 | framebuffer stub + docs producto + roadmap “hecho MVP” | MVP hecho (docs+fb stub); wire-up INT |
| INT | orquestador | Makefile + kmain64 wire-up al final | hecho |

## Decisiones cerradas (API compartida)

### Task (Fase 4)

```c
enum task_state { TASK_READY, TASK_RUNNING, TASK_BLOCKED, TASK_DEAD };

struct task {
    u32 pid;
    enum task_state state;
    u64 *kstack_top;
    void *kstack_base;
    /* context guardado en switch */
};

void task_init(void);
u32 task_create(void (*entry)(void), const char *name);
void task_yield(void);
void scheduler_on_tick(void); /* desde IRQ0 */
struct task *task_current(void);
```

Context switch asm: `src/arch/x86_64/switch.s` — `void switch_context(u64 **old_sp, u64 *new_sp);`

### Syscalls (Fase 5) — `int 0x80`, rax=num, args rdi/rsi/rdx

| # | Nombre |
|---|---|
| 0 | exit(code) |
| 1 | write(fd, buf, len) |
| 2 | read(fd, buf, len) |
| 3 | yield() |
| 4 | getpid() |
| 5 | mmap(hint, len, prot) stub OK |

Handlers en `src/syscall.c`. Vector IDT 0x80 DPL=3 cuando exista user. Hasta entonces DPL=0 y shell kernel puede llamar `syscall_dispatch` directo.

Documentar en `docs/abi.md`.

### FS (Fase 6)

- Initrd: archivo embebido o módulo Multiboot2; si no hay módulo, **tar/cpio mínimo embebido en `.rodata`** (`initrd_blob`).
- VFS: `vfs_open/read/close`, mount único `/`
- `nosfs`: superblock magic `NOSF`, inodos simples, leer archivo por nombre
- ATA PIO opcional si da tiempo; initrd es el criterio mínimo

### Shell (Fase 7)

Builtins: `help`, `echo`, `clear`, `ls`, `cat <file>`, `ticks`/`mem` info.
Puede vivir en kernel (`shell_run`) leyendo teclado hasta que F5/user existan; si F5 listo, preferir userland en `user/shell/`.

### Bootloader (Fase 8)

BIOS MBR 512B + stage2 en sectores siguientes; carga `cordos.bin` ELF desde LBA crudo.
Target QEMU: `make disk` / `make run-bios` (`-drive file=disk.img -boot order=c`).
Mantener GRUB como default `make run`. Detalle: `docs/boot_protocol.md`, nota `agent-notes/PHASE8_F8.md`.
Requiere **NASM**.

### Devices (Fase 9)

- `serial_init` / `serial_write` COM1 0x3F8
- `pci_init` enum bus 0
- virtio-net: detectar vendor/device, status “presente”
- `pipe_create` stub IPC

### SMP (Fase 10)

- `spinlock_t`, `spin_lock`/`unlock` (cli+ticket o atomic)
- Detectar LAPIC; documentar límites; si no hay SMP real completo, stub `smp_init` + percpu_cpu_id=0 y test de spinlock

### Producto (Fase 11)

- `docs/USER.md`, `docs/DEV.md`
- Framebuffer: si Multiboot2 framebuffer tag existe, plot pixel; si no, documentar
- Actualizar ROADMAP checkboxes MVP

## Reglas anti-conflicto

1. Archivos nuevos preferidos: `src/task.c`, `src/sched.c`, `src/syscall.c`, `src/fs/*`, `src/drivers/serial.c`, etc.
2. Extender `kernel64.c` lo mínimo: preferir `void phaseN_init(void)` llamadas al final.
3. **Makefile:** cada agente documenta objs en su nota; solo INT/orquestador o F6 si es crítico puede editar Makefile — preferir append en `agent-notes/MAKE_OBJS.md` lista de objetos.
4. Globales temporales OK (`name_os` pattern): `scheduler_ticks_os`, etc.
5. Compilar mentalmente con `-m64 -ffreestanding -Isrc/include`.

## Criterio global “fases completas MVP”

`make ARCH=x86_64` produce ISO que: muestra banner, corre scheduler (2 tasks), shell responde, lee archivo initrd, serial imprime, syscalls documentadas, docs presentes. Bootloader BIOS y SMP pueden ser paths secundarios (`make disk` / doc).
