# NuevoOS — Roadmap completo

Documento único de planificación. Define principios, estado actual, fases,
prioridades, módulos y criterios de aceptación. No se basa en Linux, macOS ni
Windows: se construye desde cero como sistema freestanding.

---

## 1. Principios

| Principio | Significado |
|---|---|
| Desde cero | Kernel propio, ABI propia, formato de ejecutables propio, FS propio. |
| Freestanding | Sin libc del anfitrión. Solo lo que nosotros implementemos. |
| Incremental | Cada fase deja algo arrancable y comprobable en QEMU. |
| Independencia | GRUB y tools de host son andamiaje de desarrollo; el producto final no los necesita. |
| Claridad | Preferir diseño simple y documentado antes que features prematuras. |

### Qué NO es NuevoOS

- No es un clone de Linux, ni un fork, ni un userspace sobre otro kernel.
- No reutiliza drivers, syscalls ni layouts de Windows/macOS/Linux.
- No depende de un sistema operativo anfitrión en tiempo de ejecución.

### Qué SÍ puede usar temporalmente

- GRUB / Multiboot (arranque) hasta tener bootloader propio.
- QEMU (pruebas).
- Toolchain cruzada `i686-elf` / luego `x86_64-elf` (compilación en el host).

---

## 2. Estado actual (baseline)

**Hecho (MVP usable)**

- [x] Fases 0–11 MVP en `ARCH=x86_64` (shell, VFS/initrd, syscalls, devices, boot BIOS opcional)
- [x] Consola usable: Enter, Backspace, scroll VGA, barra de estado limpia

**Futuro (backlog, no bloquea uso actual)**

- UI de producto: [`docs/UI_PLAN.md`](docs/UI_PLAN.md) — **Ola 0** (splash → login → Home) en `src/ui/session.c`
- Ring 3 + userland completo
- ATA + nosfs en disco
- Red / SMP AP / GUI avanzada (compositor)
- Bootloader nativo como default (hoy: `make run` = GRUB; `make run-bios` experimental)

---

## 3. Prioridades globales

Orden de decisión cuando haya conflicto de tiempo:

1. **P0 — Arranque estable y depurable** (no romper `make run`)
2. **P1 — Hardware mínimo usable** (timer, teclado, memoria)
3. **P2 — Aislamiento** (modo usuario, procesos, syscalls)
4. **P3 — Persistencia y shell** (disco, FS, comandos)
5. **P4 — Autonomía** (bootloader propio, red, GUI opcional)
6. **P5 — Pulido** (SMP, seguridad avanzada, portabilidad)

---

## 4. Fases del roadmap

Cada fase tiene: objetivo, prioridad, módulos/funciones, dependencias y
criterio de “hecho”.

### Fase 0 — Cimientos del kernel
**Prioridad:** P0  
**Estado:** hecha (mínimo viable)  
**Dependencias:** ninguna

| Módulo | Funciones / piezas | Notas |
|---|---|---|
| `types` | `u8/u16/u32/u64`, `size_t`, `bool`, `NULL` | Listo |
| `config` | `name_os`, `version_os`, … globales temporales | Listo (`temporal`) |
| `string` | `strlen`, `memcpy`, `memset`, `memcmp`, `strcpy` | Listo |
| `io` | `inb`, `outb`, `inw`, `outw`, `inl`, `outl` | Listo |
| `vga` | `clear`, `putc`, `print`, hex/u32 | Listo (printf completo después) |
| `panic` | `panic`, `halt_forever` | Listo (assert/stack dump después) |
| `multiboot` | flags, mem, cmdline, mmap summary | Listo (recorrer entradas mmap en Fase 2) |

**Criterio de hecho:** kernel limpio por módulos; resumen Multiboot en pantalla; `make run` sigue funcionando.

---

### Fase 1 — CPU: GDT, IDT e interrupciones
**Prioridad:** P0 → P1  
**Estado:** hecha (mínimo viable)  
**Dependencias:** Fase 0

| Módulo | Funciones / piezas | Notas |
|---|---|---|
| `gdt` | Descriptores planos + TSS placeholder + `ltr` | Listo |
| `idt` | 256 entradas, gates, `lidt` | Listo |
| `isr` | Stubs asm + handlers C | Excepciones 0–31 |
| `irq` | Handlers registrables IRQ 0–15 | Listo |
| `pic` | remap, EOI, mask/unmask | Listo (APIC después) |
| `pit` | Canal 0 programable | Listo |
| `keyboard` | PS/2 Set 1 + buffer circular | Listo (básico US) |
| `time` | `ticks_os`, `hz_os`, uptime_ms | Listo |

**Criterio de hecho:** excepciones visibles; teclas se imprimen; ticks en barra inferior.

---

### Fase 2 — Memoria física y virtual (i386)
**Prioridad:** P1  
**Estado:** hecha (mínimo viable)  
**Dependencias:** Fase 0 (mmap), Fase 1 (excepciones de page fault útiles)

| Módulo | Funciones / piezas | Notas |
|---|---|---|
| `pmm` | Bitmap de frames 4 KiB + stats globales | Desde Multiboot mmap |
| `vmm` | PD/PT, map/unmap, identity-map + CR0.PG | Listo |
| `heap` | `kmalloc` / `kfree` en `0xD0000000` | 1 MiB inicial |
| `page_fault` | Diagnóstico CR2 + bits de error | Panic controlado |

**Criterio de hecho:** alloc/free de frames; mapear región; page fault diagnosticado; heap usable.

---

### Fase 3 — Migración a x86_64 (long mode)
**Prioridad:** P1  
**Estado:** hecha (mínimo viable)  
**Dependencias:** Fase 0; lecciones de Fases 1–2

| Módulo | Funciones / piezas | Notas |
|---|---|---|
| Boot 64 | Multiboot2 + long mode + identity 1 GiB | `boot64.s` |
| Toolchain | `x86_64-elf-gcc` 13.2.0 en `~/opt/cross64` | Listo |
| Multiboot2 | Tags + summary | Listo |
| Port Fases 1–2 | GDT/IDT/ISR64, PMM/VMM/heap identity | Listo |

**Decisión:** `ARCH ?= x86_64` por defecto; `make ARCH=i386` conserva el demo 32-bit.

**Criterio de hecho:** ISO x86_64 arranca en QEMU; C en long mode; interrupciones y memoria básicas portadas.

---

### Fase 4 — Procesos, scheduler y modo usuario
**Prioridad:** P2  
**Estado:** MVP en curso — *confirmado en árbol:* `src/sched.c`, `src/include/task.h`/`sched.h`, `src/arch/x86_64/switch.s`; *falta confirmar:* `task.c` completo + preemptivo cableado en IRQ0  
**Dependencias:** Fase 1–2 (o 3 si ya en 64-bit)

| Módulo | Funciones / piezas | Notas |
|---|---|---|
| `task` | PCB/TCB, stacks kernel/user | MVP target F4 |
| `context` | Save/restore registros, switch | Asm + C (`switch.s`) |
| `scheduler` | Round-robin cooperativo → preemptivo | Usar PIT/APIC timer |
| `user_mode` | Ring 3, TSS/IST, iret/sysret | Aislamiento real (post-MVP OK) |
| `elf_loader` | Cargar ELF propio o formato NuevoOS | Ver Fase 5 |

**Criterio de hecho:** dos tareas kernel conmutan; luego un programa en ring 3 corre sin corromper el kernel.

---

### Fase 5 — Syscalls, ABI y formato de ejecutables
**Prioridad:** P2  
**Estado:** MVP en curso — *confirmado:* `src/syscall.c`, `docs/abi.md`, `user/libnos/syscall.*`, entry asm; *pendiente INT:* wire `phase5_init` / IDT 0x80  
**Dependencias:** Fase 4

| Módulo | Funciones / piezas | Notas |
|---|---|---|
| `syscall` | Tabla, dispatch, validación de punteros | MVP: `int 0x80` |
| ABI | Números, registros, errno propio | Target: `docs/abi.md` |
| `nos` format | Header + segmentos + entry | Alternativa a ELF, o ELF restringido |
| libc mínima user | `write`, `exit`, `mmap`, `open`… | Stub mínimo en MVP F5 |

**Syscalls iniciales (mínimo viable)**

| # | Nombre | Función |
|---|---|---|
| 0 | `sys_exit` | Terminar proceso |
| 1 | `sys_write` | Escribir a fd (consola primero) |
| 2 | `sys_read` | Leer de fd (teclado/consola) |
| 3 | `sys_yield` | Ceder CPU |
| 4 | `sys_getpid` | Id de proceso |
| 5 | `sys_mmap` | Mapear memoria |
| 6 | `sys_open` / `sys_close` | Tras tener FS |
| 7 | `sys_spawn` / `sys_exec` | Lanzar programa |

**Criterio de hecho:** programa usuario imprime con `sys_write` y termina con `sys_exit`.

---

### Fase 6 — Almacenamiento y sistema de archivos
**Prioridad:** P3  
**Estado:** pendiente — *MVP agent target (F6)*: initrd + VFS + nosfs; **aún no hay `src/fs/`** en árbol al momento F11  
**Dependencias:** heap, IRQ, DMA/PIO según driver

| Módulo | Funciones / piezas | Notas |
|---|---|---|
| `ata` / `ahci` | Leer/escribir sectores | Opcional post-MVP |
| `block` | Capa de bloques, cache simple | |
| `vfs` | `mount`, `open`, `read`, `write`, `readdir` | MVP: open/read/close |
| `nosfs` | FS propio (superblock, inodos, bitmap) | Magic `NOSF` |
| initrd | Ramdisk inicial embebido en ISO | Criterio mínimo MVP |

**Criterio de hecho:** leer archivo desde initrd; luego persistir en imagen de disco QEMU.

---

### Fase 7 — Shell y userland mínimo
**Prioridad:** P3  
**Estado:** MVP en curso — *confirmado:* `src/shell.c` + `shell.h` (kernel-mode builtins per F7); *pendiente INT:* llamar `shell_run` desde `kmain64`  
**Dependencias:** Fase 5–6

| Módulo | Funciones / piezas | Notas |
|---|---|---|
| `init` | Primer proceso usuario | Tras F5 user |
| `shell` | Prompt, parseo, builtins | MVP: help/echo/clear/ls/cat |
| utils | Pequeños programas `.nos` | Demostrar exec |
| tty | Línea canónica, echo, Ctrl+C básico | |

**Criterio de hecho:** sesión interactiva usable en QEMU sin depurador.

---

### Fase 8 — Bootloader propio (reemplazar GRUB)
**Prioridad:** P4  
**Estado:** MVP en curso — *confirmado parcial:* `boot/mbr.s`; Stage2/disco/`run-bios` no verificados como MVP completo  
**Dependencias:** FS básico o etapa 2 embebida; kernel estable

| Módulo | Funciones / piezas | Notas |
|---|---|---|
| Stage 1 | MBR/GPT + BIOS o UEFI | MVP: BIOS MBR 512B |
| Stage 2 | Cargar kernel, setup memoria/video | Carga `nuevoos64.bin` |
| Protocolo | Header NuevoOS (dejar Multiboot) | Documentar |
| Installer | Escribir bootloader a imagen disco | |

**Criterio de hecho:** ISO o disco arranca **sin GRUB**; mismo kernel.

---

### Fase 9 — Dispositivos, IPC y red
**Prioridad:** P4  
**Estado:** MVP en curso — *confirmado:* `drivers/serial.c`, `pci.c`, `virtio_net.c`, `src/ipc.c`; *pendiente INT:* objs + inits en `kmain64`. Framebuffer stub: Fase 11.  
**Dependencias:** VFS, IRQ, heap, procesos

| Módulo | Funciones / piezas | Notas |
|---|---|---|
| `pci` | Enumeración de dispositivos | MVP: bus 0 |
| `serial` | COM1 debug | `0x3F8` |
| `framebuffer` | Modo gráfico básico | Stub MVP en Fase 11 (`drivers/fb`) |
| `ipc` | Pipes, mensajes, shared memory | MVP: `pipe_create` stub |
| `net` | NIC virtio-net, stack IP mínimo | MVP: detectar presente |
| `virtio` | Preferir virtio en QEMU | Menos dolor que hardware legacy |

**Criterio de hecho:** debug por serial; al menos ping o socket local documentado.

---

### Fase 10 — SMP, seguridad y endurecimiento
**Prioridad:** P5  
**Estado:** MVP en curso — *confirmado:* `spinlock.c`, `smp.c`, `docs/smp.md` (stub sin AP bringup); *pendiente INT:* `phase10_init`  
**Dependencias:** APIC, scheduler, VMM

| Módulo | Funciones / piezas | Notas |
|---|---|---|
| SMP | Arranque APs, per-cpu data | MVP: stub `smp_init`, cpu_id=0 |
| sync | Spinlocks, mutex, atomics | MVP: `spinlock_t` |
| capabilities / ACL | Modelo de permisos propio | Post-MVP |
| ASLR / KASLR | Aleatorizar layouts | Post-MVP |
| W^X | Páginas no writable+executable | Post-MVP |

**Criterio de hecho:** 2+ CPUs en QEMU `-smp 2` con scheduler; pruebas de aislamiento básicas.

---

### Fase 11 — Experiencia de producto (opcional largo plazo)
**Prioridad:** P5  
**Estado:** MVP en curso/hecho (parcial) — *confirmado:* `docs/USER.md`, `docs/DEV.md`, stub `src/drivers/fb.c` + `fb.h`; *pendiente INT:* enlace `build/fb.o` y `fb_init` en `kmain64`  
**Dependencias:** casi todo lo anterior (GUI/audio/ports = largo plazo)

**MVP producto (hecho en árbol)**

- [x] `docs/USER.md` — build/run WSL + make + QEMU
- [x] `docs/DEV.md` — mapa de módulos, añadir syscall/driver
- [x] Framebuffer stub: si tag Multiboot2 tipo 8 usable → pixel de prueba; si no → mensaje no-op
- [ ] Wire-up Makefile + `kmain64` (orquestador INT)

**Largo plazo (no MVP)**

- GUI / compositor simple
- Audio
- Package/runtime propio
- Documentación de desarrollador de apps
- Port a otra arquitectura (ej. RISC-V) como prueba de diseño limpio

---

## 5. Orden de ejecución recomendado (próximos 8 hitos)

| # | Hito | Fase | Prioridad |
|---|---|---|---|
| 1 | Separar `vga`, `io`, `types`, `string`, `panic`, parse Multiboot + `config` globales | 0 | P0 — hecho |
| 2 | GDT + IDT + excepciones + PIC | 1 | P0 — hecho |
| 3 | PIT + teclado PS/2 | 1 | P1 — hecho |
| 4 | PMM + VMM + heap + page fault | 2 | P1 — hecho |
| 5 | Decidir y migrar a x86_64 | 3 | P1 — hecho |
| 6 | Tasks + scheduler preemptivo | 4 | P2 — MVP hecho |
| 7 | Ring 3 + syscalls mínimas | 4–5 | P2 — syscalls MVP (ring3 pendiente) |
| 8 | Initrd + VFS + shell | 6–7 | P3 — MVP hecho |

Bootloader propio (Fase 8) **después** de shell usable, no antes.

---

## 6. Estructura de código objetivo

```text
src/
  boot/          # entry asm, multiboot, long mode
  arch/x86/      # gdt, idt, isr, irq, io, cpu
  kernel/        # kmain, panic, scheduler, syscall
  mm/            # pmm, vmm, heap
  drivers/       # vga, keyboard, pit, serial, ata, …
  fs/            # vfs, nosfs, initrd
  lib/           # string, printf freestanding
user/
  init/
  shell/
  libnos/        # libc mínima de usuario
docs/            # abi, fs layout, boot protocol (cuando existan)
iso/             # staging GRUB temporal
```

Ir creando carpetas solo cuando la fase correspondiente lo exija (evitar vacío prematuro).

---

## 7. Definiciones de “listo” por capa

| Capa | Listo cuando… |
|---|---|
| Kernel mínimo | Arranca, imprime, no depende de libc host |
| Kernel interactivo | Teclado + timer + excepciones |
| Kernel con memoria | Alloc dinámico + paginación |
| Kernel multiproceso | Switch de tareas fiable |
| Sistema usable | Shell + archivos + programas usuario |
| Sistema autónomo | Bootloader propio + instalable en disco |
| Sistema serio | SMP + red + modelo de seguridad |

---

## 8. Riesgos y decisiones abiertas

| Tema | Opciones | Recomendación provisional |
|---|---|---|
| Arquitectura principal | Quedarse en i386 vs pasar a x86_64 | **x86_64** tras Fase 2 |
| Formato de ejecutables | ELF subset vs formato `.nos` propio | ELF subset primero; `.nos` si aporta simplicidad |
| Boot | BIOS+MBR vs UEFI | BIOS/QEMU primero; UEFI después |
| FS | Solo initrd vs FS en disco pronto | Initrd → luego `nosfs` |
| Drivers | Legacy PIC/ATA vs APIC/virtio | Legacy para aprender; virtio para avanzar rápido en QEMU |

Actualizar esta sección cuando se cierre cada decisión.

---

## 9. Cómo usar este archivo

1. Trabajar **una fase / un hito** a la vez.
2. Marcar checkboxes del estado actual al completar.
3. No saltar a shell/GUI sin memoria + interrupciones.
4. Cada PR/cambio grande debe nombrar la fase (`Fase 1`, `Fase 2`, …).
5. Si surge trabajo nuevo, añadirlo aquí con prioridad antes de implementarlo.

---

## 10. Resumen ejecutivo

NuevoOS ya demostró arranque freestanding. El camino es:

**cimientos → interrupciones/entrada → memoria → (x86_64) → procesos/syscalls → FS/shell → bootloader propio → red/SMP/seguridad.**

Todo diseño de ABI, FS, boot y userland es **propio**. Linux/macOS/Windows no son base; solo el hardware y los estándares necesarios (Multiboot temporal, VGA, PC legacy, luego virtio/UEFI según decidamos).
