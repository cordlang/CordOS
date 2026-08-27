# CordOS — Roadmap

Plan vivo del sistema. Fuente de verdad para **qué sigue**, no un diario de
agentes. Contratos viejos (Fases 3 y 4–11) están **cerrados**; no abrir trabajo
nuevo contra ellos.

Complementos: [`UI_PLAN.md`](UI_PLAN.md) (UX), [`VISUAL_ROADMAP.md`](VISUAL_ROADMAP.md)
(motion / frame time), [`abi.md`](abi.md) (syscalls), [`USER.md`](USER.md) (cómo arrancar).

---

## 1. Principios

| Principio | Significado |
|---|---|
| Desde cero | Kernel propio, ABI propia, FS propio. |
| Freestanding | Sin libc del anfitrión. CordOS **es** el anfitrión. |
| Incremental | Cada hito deja `.\run-vbox.ps1` arrancable. |
| Independencia | GRUB y el toolchain son andamiaje; el producto no los necesita para siempre. |
| Honesto | El escritorio es OS real y aún corre en ring 0. No fingir userland. |

**No es** un fork de Linux/Windows/macOS, ni userspace sobre otro kernel.

**Sí usa hoy:** GRUB/Multiboot2, QEMU/VirtualBox, `x86_64-elf-*` en el host.

---

## 2. Dónde estamos (agosto 2026)

CordOS es un **sistema operativo gráfico x86_64**. Arranca, pide sesión y deja
un escritorio con ventanas, dock y Spotlight. El kernel, el VFS y la UI viven
en el mismo `cordos.bin`.

| Capa | Estado | Caveat que importa |
|---|---|---|
| Arranque | GRUB + Multiboot2 (**default**). BIOS MBR experimental | El default **sigue siendo GRUB** |
| CPU / IRQ | GDT, IDT, PIC, PIT, PS/2 teclado+ratón | APIC / IPI no |
| Memoria | PMM, VMM, heap, kselftest al boot | Sin COW ni demand paging |
| Tareas | `task` + `switch_context` + yield; preempt ON en código (`schedule()` desde IRQ0) | **Falta confirmar PS/2 teclado/ratón en VirtualBox** |
| Syscalls | `int 0x80`, números 0–7, copy user exige `PAGE_USER` | `mmap` = bump anónimo; **`prot` se ignora** (páginas quedan W). `read`/`write` no multiplexan fds VFS (0/1 = teclado/VGA) |
| FS | VFS, NosFS, initrd, disco AHCI/IDE | Persistencia real, no truco de VBox |
| Shell | Kernel-mode, ES/EN, F1 emergencia | No es un proceso usuario |
| UI | Idioma → splash → onboarding/login → desktop | Apps del dock = código kernel (ring 0) |
| Red | PCI, virtio-net, WLAN host path | **Detectar ≠ stack usable** |
| SMP | `cpu_count_os = 1` | **Stub a propósito** |
| Claves | `users.db` con hash | **FNV-1a + pepper**, no un KDF |
| Marca | Logo y UI propios | Versión **0.1.0** / codename *instrumento*. Sin `LICENSE` en la raíz |

**Hecho y cerrado:** Fases 0–11 del plan original (cimientos → x86_64 → tasks →
syscalls → FS → shell → boot BIOS opcional → devices stub → SMP stub → FB +
docs). El contrato [`phases/PHASES_4_11_CONTRACT.md`](phases/PHASES_4_11_CONTRACT.md)
es histórico.

**Parcial:** Fase 12 (falta `LICENSE`). Fase 13 (ELF smoke hecho; `spawn` no;
`mmap` no honra `prot`). Fase 14 (preempt en código; falta VBox).

---

## 3. Prioridades (ahora)

Cuando haya conflicto de tiempo, este orden gana:

1. **P0 — No romper el producto.** `make` + `.\run-vbox.ps1` llega a login/desktop. Serial en `out/serial.log`.
2. **P1 — Userland de verdad.** `user_hello.elf` ya escribe y sale (lo lanza el kernel). Falta `spawn`/`exec` y la Terminal del dock en ring 3.
3. **P2 — El kernel como kernel.** Confirmar preempt + PS/2 en VirtualBox; `spawn`/`exec`; `read`/`write` sobre fds VFS.
4. **P3 — Disco y ABI sin inflar.** Persistencia y loader ELF ya existen. fds VFS siguen abiertos (Fase 15). **`mmap` no está “hecho”:** `prot` se ignora.
5. **P4 — Autonomía.** Bootloader propio como camino documentado. **Hoy el default sigue siendo GRUB.**
6. **P5 — Escala.** SMP AP, red usable, W^X / SMAP, vsync / modo juego ([`VISUAL_ROADMAP.md`](VISUAL_ROADMAP.md)).

**Seguridad (P2 de hecho, Fase 17 en el plan):** honrar `mmap` `prot` y W^X.
No perderlo detrás de “mmap anónimo real”. Hash FNV-1a: antes de “usuarios reales”.

La GUI **ya no es opcional**. El pulido visual (reloj `dt`, damage rects) es P5
respecto al aislamiento, no respecto a “¿tenemos desktop?”.

---

## 4. Fases 0–11 (archivo)

Resumen. El detalle de módulos está en el árbol `src/`.

| Fase | Qué era | Estado |
|---|---|---|
| 0 | types, string, io, vga, panic, multiboot | Cerrada |
| 1 | GDT, IDT, PIC, PIT, teclado | Cerrada |
| 2 | PMM/VMM/heap i386 | Cerrada (demo `ARCH=i386`) |
| 3 | Long mode, `ARCH=x86_64` default | Cerrada |
| 4 | Tasks + switch | Cerrada en cooperativo; preempt es Fase 14 |
| 5 | Syscalls 0–5 + ABI | Cerrada; 6–7 open/close VFS; mmap bump (Fase 13/17) |
| 6 | Initrd + VFS + NosFS + disco | Cerrada (criterio original superado) |
| 7 | Shell usable | Cerrada en kernel-mode |
| 8 | MBR + stage2 | Cerrada experimental (`make run-bios`) |
| 9 | Serial, PCI, virtio-net, pipes stub | Cerrada como detect; red no es producto |
| 10 | Spinlock + smp stub | Cerrada; 2 CPUs **no** |
| 11 | FB + docs + UI de producto | Cerrada y **ampliada**: compositor, login, desktop |

No reabrir 0–11 para features nuevas. Eso va a 12+.

---

## 5. Fases abiertas (el trabajo)

Cada fase: objetivo, prioridad, piezas, criterio de hecho. Una a la vez.
Dependencia: no saltar a 18 (SMP) sin 14 (preempt estable en VBox).

### Fase 12 — Marca y contrato de producto
**Prioridad:** P0  
**Estado:** hecha salvo `LICENSE` en la raíz

`config.c` es **0.1.0** / *instrumento* / Owell Andry. Acerca de usa `version_os`.

| Pieza | Estado |
|---|---|
| Versión / codename / autor | Hecho |
| `LICENSE` en la raíz | **Abierto.** El autor elige SPDX. Este archivo no inventa el texto ni el archivo. |

**Hecho cuando:** hay un `LICENSE` en la raíz con la licencia que elija el autor.

---

### Fase 13 — Un proceso ring 3 de verdad
**Prioridad:** P1  
**Estado:** hecho el criterio de ELF; `spawn`/`exec` sigue abierto (Fase 15). `mmap` no está cerrado (Fase 17).

`user_hello.elf` (linkado en `0x40000000`) se embebe en el kernel. Tras el
scheduler, `user_smoke()` lo carga, entra con `iretq`, hace `write`+`exit`.
El trampoline a mano queda de fallback.

`SYS_MMAP` asigna páginas `PAGE_USER` desde `0x41000000` (bump anónimo,
siempre `PAGE_WRITE`). **`prot` se ignora** (`(void)prot` en `sys_mmap`).
Eso no es “mmap hecho”: es un allocator. W^X / `prot` → Fase 17.

| Módulo | Piezas | Notas |
|---|---|---|
| `mmap` bump | `SYS_MMAP` anónimo, writable | Hecho como bump. **`prot` ignorado** |
| Loader | ELF64 ET_EXEC x86_64 | `src/proc/elf64.c` — PT_LOAD también se mapea W (`ph->flags` ignorado) |
| `spawn` / `exec` | Syscall nueva (8+) | Pendiente — hoy lo lanza el kernel (Fase 15) |
| libc user | `user/libnos` | `write`/`exit` |
| Aislamiento | Fault CPL3 mata la tarea | `page_fault.c` |

**Hecho cuando (criterio ELF, ya cumplido):** `user_hello.elf` hace `write`+`exit`
y el desktop sigue, lanzado por el kernel.

**No es criterio de esta fase:** honrar `prot`, W^X, ni `spawn` como syscall.

---

### Fase 14 — Preempt y teclado a la vez
**Prioridad:** P2  
**Estado:** código ON (`schedule()` desde IRQ0 + tarea `tick`); **falta confirmar PS/2 teclado y ratón en VirtualBox**  
**Dependencias:** Fase 4 (switch). EOI del PIC va **antes** del handler; `schedule()` hace `sti` al retomar.

El freeze histórico: switch a mitad de IRQ0 sin `iret` dejaba `IF=0` (interrupt gate) y el teclado moría. Hoy el PIC ya no queda enmascarado esperando el EOI, y la tarea retomada vuelve con IF=1. Eso está en el árbol. **No cuenta como hecho** hasta que alguien teclee y mueva el ratón en VBox con preempt ON.

| Pieza | Estado |
|---|---|
| IRQ0 | `scheduler_on_tick` → `schedule()` — en código |
| Compañera | tarea `tick` (`hlt` + yield) para que el RR tenga a quién saltar — en código |
| Prueba VBox | **Abierta.** Teclear y mover el ratón en login/desktop con preempt ON |

**Hecho cuando:** login `admin`/`admin` y el desktop responden al teclado y al
ratón PS/2 en VirtualBox con preempt ON. QEMU serial no sustituye esa prueba.

---

### Fase 15 — Apps del dock en userland
**Prioridad:** P2  
**Estado:** abierta  
**Dependencias:** 13 (ELF) + 14 (preempt usable)

Hoy: `open`/`close` hablan con la tabla VFS; `read`/`write` **no**. fd 0 es
teclado, fd 1 es VGA+serial. No hay syscall `spawn`/`exec`. Archivos, Terminal,
Ajustes y Acerca de son funciones del kernel (ring 0).

| Pieza | Qué hacer | Hoy |
|---|---|---|
| `read` / `write` | Multiplexar fds VFS (los que devuelve `open`) | Solo 0/1 = teclado/VGA; otro fd → `-1` |
| `spawn` / `exec` | Syscall nueva (8+) que cargue un ELF y lo deje ring 3 | `user_smoke()` en el kernel |
| Terminal | El builtin del dock lanza un proceso, no `shell_run()` en ring 0 | Ring 0 |
| Compositor | Superficie por proceso **o** un servidor gráfico mínimo | Un paso; no Wayland |
| Fallback | F1 sigue siendo shell kernel | Válvula de escape |

**Hecho cuando:** un `spawn`/`exec` lanza un ELF; `read`/`write` operan sobre
fds VFS (no solo 0/1); Archivos o Terminal sobreviven a un kill del proceso;
logout no requiere reboot. El dock deja de ser “todo ring 0” para esa app.

---

### Fase 16 — Boot autónomo
**Prioridad:** P4  
**Estado:** abierta. **GRUB sigue siendo el boot default** (`make` / `.\run-vbox.ps1`).  
**Dependencias:** kernel estable (12–14 preferible)

| Pieza | Qué hacer |
|---|---|
| Default | Un `make run-bios` (o UEFI) documentado como camino de producto |
| Protocolo | [`boot_protocol.md`](boot_protocol.md) = lo que Stage2 entrega |
| GRUB | Fallback de desarrollo, no la historia del README |

**Hecho cuando:** se puede decir “arranca sin GRUB” en [`USER.md`](USER.md) sin
la palabra experimental, y el desktop es el mismo.

---

### Fase 17 — Memoria y seguridad de kernel
**Prioridad:** P5 (fase); **P2 de hecho** para `prot` / W^X  
**Estado:** abierta. El bump de `mmap` **no** cierra esta fase.  
**Dependencias:** 13 (user mappings reales)

| Pieza | Hoy | Notas |
|---|---|---|
| `mmap` `prot` | **Ignorado.** Páginas siempre `PAGE_WRITE` | Hueco de seguridad. No es “mmap hecho” |
| W^X | ELF PT_LOAD también se mapea W (`ph->flags` ignorado) | Páginas no W+X en kernel y user |
| Demand paging / COW | Un fault CPL3 mata la tarea | — |
| SMAP / UMIP | No | Si el CPU guest lo ofrece |
| Hash de claves | **FNV-1a + pepper** (`src/fs/userdb.c`) | No es un KDF. Antes de “usuarios reales” |

**Hecho cuando:**

- `mmap(..., prot)` deja páginas según `prot` (R/W/X); no quedan W por defecto.
- Un programa user no puede ejecutar páginas W ni escribir páginas X (W^X).
- Un programa user no puede leer identity kernel; [`abi.md`](abi.md) describe el modelo.
- Nadie llama “usuarios reales” al login mientras el secreto sea FNV-1a + pepper.
  El reemplazo es un KDF de verdad (el autor elige cuál). Este roadmap no finge
  que el hash actual sirva.

---

### Fase 18 — SMP y red (producto)
**Prioridad:** P5  
**Estado:** abierta. SMP es **stub** (`cpu_count_os = 1`). Red: detectar ≠ stack.  
**Dependencias:** 14 (preempt estable), IPI TLB en `vmm64.c`

| Pieza | Criterio |
|---|---|
| AP bring-up | `cpu_count_os > 1` con QEMU/VBox `-smp 2` |
| TLB shootdown | Dejar de ser “local only” |
| Red | Ping o TCP mínimo documentado como camino de usuario; WLAN host **no** cuenta como stack guest |

**Hecho cuando:** dos núcleos ejecutan tareas; **o** (si se parte la fase) hay
un camino de red que un usuario puede seguir en [`USER.md`](USER.md).

---

### Fase 19 — Motion y frame time
**Prioridad:** P5 (producto se siente OS; no bloquea 13–15)  
**Dependencias:** desktop actual  
**Plan detallado:** [`VISUAL_ROADMAP.md`](VISUAL_ROADMAP.md)

Orden allí: **V1** (`ui_tick` / hover `dt`) → **V3** (wallpaper frozen +
`present_rect`) → **V2** (páginas / open window) → V4 vsync / modo juego.

Page-flip en el LFB de VBox **no** es el camino; ya se intentó.

**Hecho cuando:** V1 cumple el criterio de ese doc (hover aceite, F1 intacto).

---

## 6. Próximos 8 hitos (orden de ataque)

| # | Hito | Fase | Pri |
|---|---|---|---|
| 1 | `LICENSE` en la raíz (el autor elige SPDX; no inventar el archivo) | 12 | P0 — **abierto** |
| 2 | Cargar y correr `user_hello.elf` desde el kernel | 13 | P1 — **hecho** |
| 3 | `SYS_MMAP` bump anónimo `PAGE_USER` | 13 | P1 — **hecho el bump**; `prot` ignorado → hito 6 |
| 4 | Confirmar preempt IRQ0 + PS/2 teclado/ratón en **VirtualBox** | 14 | P2 — código ON; **falta VBox** |
| 5 | `read`/`write` sobre fds VFS; `spawn`/`exec` como syscall | 15 | P2 |
| 6 | Honrar `mmap` `prot` + W^X (páginas no quedan W) | 17 | P2 seguridad / P5 fase |
| 7 | Terminal del dock = proceso ring 3 | 15 | P2 |
| 8 | V1: reloj `dt` **o** boot sin GRUB como default documentado **o** `-smp 2` real | 19 / 16 / 18 | P4–P5 |

Siguiente código: **hito 4** (confirmar preempt en VBox: teclado y ratón PS/2),
luego **hito 5** (`spawn` + fds VFS). El **hito 6** (W^X / `prot`) es el hueco
de seguridad de mmap: no se trata el bump como “mmap hecho”. El desktop sigue
siendo ring 0 hasta la Fase 15.

---

## 7. Árbol real (`src/`)

```text
src/
  boot/            Multiboot, boot64.s
  arch/x86_64/     GDT, IDT, ISR, PIC, PIT, switch, syscall_entry
  arch/i386/       demo congelada
  kernel/          kmain64, config, string, panic, kselftest
  mm/              PMM, VMM, heap, page_fault
  proc/            task, sched, syscall, smp, ipc, user smoke
  drivers/         fb, teclado, ratón, PCI, AHCI, ATA, serial, net, USB/EHCI
  fs/              vfs, nosfs, initrd, persist, userdb
  ui/              gfx, login, desktop, session, widgets
  shell/           emergencia + i18n
  include/<mod>/   headers públicos
user/
  libnos/          stubs syscall usuario
  test_write.c     / user_hello
docs/              este archivo, USER, DEV, ABI, UI, visual
```

ISO: `dist/cordos.iso`. Intermedios: `out/`.

---

## 8. Decisiones

| Tema | Cerrado | Abierto |
|---|---|---|
| Arch principal | **x86_64**. i386 = demo | — |
| Boot de desarrollo | GRUB + VBox (**sigue siendo el default**) | ¿BIOS o UEFI como default de producto? |
| FS | NosFS + initrd overlay | Layout on-disk a largo plazo |
| Ejecutables | ELF64 subset primero (`user_hello.elf`) | `spawn`/`exec` syscall; `.nos` después |
| `mmap` | Bump anónimo `PAGE_USER` | **`prot` ignorado** — W^X (Fase 17) |
| GUI | Compositor 2D propio; no GTK/Qt | Servidor gráfico vs blit in-process; dock aún ring 0 |
| Red | Detectar virtio / WLAN host | Stack IP en guest (detectar ≠ producto) |
| SMP | 1 CPU de verdad (stub) | AP + IPI |
| Claves | FNV-1a + pepper, documentado | Sustituir por un KDF **antes** de “usuarios reales” |
| Licencia | — | `LICENSE` en la raíz; el autor elige SPDX |

Actualizar esta tabla al cerrar una fila.

---

## 9. Listo por capa (definición actual)

| Capa | Listo cuando… | ¿Ya? |
|---|---|---|
| Kernel mínimo | Arranca freestanding | Sí |
| Kernel interactivo | Teclado + timer + excepciones | Sí |
| Kernel con memoria | Heap + paginación | Sí |
| Producto gráfico | Login → desktop en VBox | Sí |
| Un ELF ring 3 | `write`+`exit` y el desktop sigue | Sí (lanzado por el kernel; sin `spawn`) |
| `mmap` usable | Anónimo **y** `prot` honrado / W^X | Bump sí; **`prot` no** |
| Kernel multiproceso | Switch **y** preempt usable en VBox | Switch sí; preempt ON en código (**confirmar PS/2**) |
| Sistema usable | Programas usuario + archivos por fds | Archivos sí; spawn/fds/dock no |
| Usuarios reales | KDF, no FNV-1a + pepper | No |
| Sistema autónomo | Arranque propio + instalable | No (GRUB default) |
| Sistema serio | SMP + red + W^X | No |

---

## 10. Cómo usar este archivo

1. Un hito de la [§6](#6-próximos-8-hitos-orden-de-ataque) por tanda.
2. Nombrar la fase (`Fase 14`, …) en el cambio.
3. No reabrir 0–11. No tratar [`UI_PLAN.md`](UI_PLAN.md) “Ola 1” como trabajo
   de framebuffer: eso ya arranca.
4. Trabajo visual nuevo → [`VISUAL_ROADMAP.md`](VISUAL_ROADMAP.md) (V1, V3, …).
5. **Reparto:** kernel (`spawn`, fds VFS, `prot`/W^X, KDF) en C; UI y motion
   aparte. Este archivo es el contrato; no diluir el hueco en otro doc.
6. Si aparece un frente nuevo, **añadirlo aquí con prioridad** antes de
   implementarlo.

---

## 11. Resumen

**0–11 están hechas.** Fase 13 dejó un ELF ring 3 que el **kernel** lanza;
eso no es userland de producto. El hueco que sigue es: **¿preempt convive con
PS/2 en VirtualBox?** y después **¿puede un programa que no es el kernel
nacer por `spawn`, hablar por fds VFS, romperse y dejar el escritorio en pie?**

`mmap` anónimo no cierra memoria: **`prot` se ignora**. FNV-1a no cierra
usuarios. GRUB sigue siendo el default. SMP y red siguen en stub / detect.
