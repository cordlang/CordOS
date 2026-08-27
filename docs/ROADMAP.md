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
| Arranque | GRUB + Multiboot2 (default). BIOS MBR experimental | El default sigue siendo GRUB |
| CPU / IRQ | GDT, IDT, PIC, PIT, PS/2 teclado+ratón | APIC / IPI no |
| Memoria | PMM, VMM, heap, kselftest al boot | Sin COW ni demand paging |
| Tareas | `task` + `switch_context` + yield | **Preempt IRQ0 apagado** (congelaba el teclado) |
| Syscalls | `int 0x80`, números 0–7, copy user exige `PAGE_USER` | `mmap` es stub; read/write no multiplexan fds VFS |
| FS | VFS, NosFS, initrd, disco AHCI/IDE | Persistencia real, no truco de VBox |
| Shell | Kernel-mode, ES/EN, F1 emergencia | No es un proceso usuario |
| UI | Idioma → splash → onboarding/login → desktop | Apps del dock = código kernel |
| Red | PCI, virtio-net, WLAN host path | Detectar ≠ stack usable |
| SMP | `cpu_count_os = 1` | Stub a propósito |
| Marca | Logo y UI propios | `config.c` sigue en `temporal` (versión, autor, licencia) |

**Hecho y cerrado:** Fases 0–11 del plan original (cimientos → x86_64 → tasks →
syscalls → FS → shell → boot BIOS opcional → devices stub → SMP stub → FB +
docs). El contrato [`phases/PHASES_4_11_CONTRACT.md`](phases/PHASES_4_11_CONTRACT.md)
es histórico.

---

## 3. Prioridades (ahora)

Cuando haya conflicto de tiempo, este orden gana:

1. **P0 — No romper el producto.** `make` + `.\run-vbox.ps1` llega a login/desktop. Serial en `out/serial.log`.
2. **P1 — Userland de verdad.** Un programa ring 3 que escriba y salga; luego la Terminal del dock.
3. **P2 — El kernel como kernel.** Preempt que no mate el teclado; mmap real; spawn/exec.
4. **P3 — Disco y ABI cerrados.** read/write sobre fds VFS; loader ELF o `.nos`.
5. **P4 — Autonomía.** Bootloader propio como camino documentado (GRUB puede quedar de fallback).
6. **P5 — Escala.** SMP AP, red usable, W^X / SMAP, vsync / modo juego ([`VISUAL_ROADMAP.md`](VISUAL_ROADMAP.md)).

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
| 4 | Tasks + switch | Cerrada en cooperativo; preempt **no** |
| 5 | Syscalls 0–5 + ABI | Cerrada; 6–7 open/close VFS; mmap stub |
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
Dependencia: no saltar a 16 (SMP) sin 13 (preempt estable).

### Fase 12 — Marca y contrato de producto
**Prioridad:** P0 (barato) / deja de ser “temporal”  
**Dependencias:** ninguna

| Pieza | Qué hacer |
|---|---|
| `config.c` | `version_os`, `author_os`, `license_os` reales; semver `0.x` |
| Docs | Este archivo + [`UI_PLAN.md`](UI_PLAN.md) alineados con el desktop actual |
| Licencia | Un `LICENSE` en la raíz cuando el autor lo fije |

**Hecho cuando:** Acerca de y el serial muestran la misma versión; no queda la palabra `temporal` en `config.c`.

---

### Fase 13 — Un proceso ring 3 de verdad
**Prioridad:** P1  
**Dependencias:** syscalls actuales, VMM `PAGE_USER`, smoke `user_smoke()`

Hoy el escritorio y las “apps” son funciones del kernel. Ring 3 existe como
prueba (`user.c` imprime `r3` y hace `exit`). Eso no es userland.

| Módulo | Piezas | Notas |
|---|---|---|
| `mmap` | `SYS_MMAP` deja de ser `-1` | Páginas user, no identity kernel |
| Loader | ELF64 mínimo **o** formato `.nos` | ELF subset primero ([§8](#8-decisiones)) |
| `spawn` / `exec` | Syscall nueva (8+) | Un `user_hello.elf` ya se construye (`make userland`) |
| libc user | `user/libnos` | `write`/`exit` contra ABI real |
| Aislamiento | Fault CPL3 mata la tarea, no el kernel | Ya hay esbozo en `page_fault.c` |

**Hecho cuando:** desde el kernel (o un menú de debug) se lanza un ELF que hace
`write` + `exit` y el desktop **sigue vivo**. No hace falta mover el dock aún.

---

### Fase 14 — Preempt y teclado a la vez
**Prioridad:** P2  
**Dependencias:** Fase 4 (switch), comprensión del bug IF=0

`scheduler_on_tick` está vacío a propósito: un switch en IRQ0 dejaba `IF=0` y
mataba el PS/2.

| Pieza | Qué hacer |
|---|---|
| IRQ0 | Switch con iret / trampolín que restaure IF |
| Idle | `hlt` en la tarea idle (ya hay loop) |
| Prueba | Teclear durante dos tareas que hacen yield **y** durante preempt |

**Hecho cuando:** preempt ON, teclado y ratón del desktop siguen funcionando.

---

### Fase 15 — Apps del dock en userland
**Prioridad:** P2  
**Dependencias:** 13 + 14

| Pieza | Qué hacer |
|---|---|
| Terminal | El builtin del dock lanza un proceso, no `shell_run()` en ring 0 |
| Syscalls | `read`/`write` sobre fds VFS (hoy 0/1 son teclado/VGA sueltos) |
| Compositor | Superficie por proceso **o** un servidor gráfico mínimo | Un paso; no Wayland |
| Fallback | F1 sigue siendo shell kernel | Válvula de escape |

**Hecho cuando:** Archivos o Terminal sobreviven a un kill del proceso; logout
no requiere reboot.

---

### Fase 16 — Boot autónomo
**Prioridad:** P4  
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
**Prioridad:** P5  
**Dependencias:** 13 (user mappings reales)

| Pieza | Notas |
|---|---|
| W^X | Páginas no W+X en kernel y user |
| Demand paging / COW | Hoy un fault CPL3 mata la tarea |
| SMAP / UMIP | Si el CPU guest lo ofrece |
| Hash de claves | Hoy FNV-1a + pepper; no es Argon2 — documentar o reemplazar |

**Hecho cuando:** un programa user no puede leer identity kernel; `docs/abi.md`
describe el modelo.

---

### Fase 18 — SMP y red (producto)
**Prioridad:** P5  
**Dependencias:** 14 (preempt), IPI TLB en `vmm64.c`

| Pieza | Criterio |
|---|---|
| AP bring-up | `cpu_count_os > 1` con QEMU/VBox `-smp 2` |
| TLB shootdown | Dejar de ser “local only” |
| Red | Ping o TCP mínimo documentado; WLAN host no cuenta como stack guest |

**Hecho cuando:** dos núcleos ejecutan tareas; **o** (si se parte la fase) hay
un camino de red que un usuario puede seguir en [`USER.md`](USER.md).

---

### Fase 19 — Motion y frame time
**Prioridad:** P5 (producto se siente OS; no bloquea 13)  
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
| 1 | Fijar `config.c` + dejar de mentir en docs de fase | 12 | P0 |
| 2 | `SYS_MMAP` real (páginas user) | 13 | P1 |
| 3 | Cargar y correr `user_hello.elf` desde el kernel | 13 | P1 |
| 4 | Preempt IRQ0 sin matar PS/2 | 14 | P2 |
| 5 | `read`/`write` en fds VFS; spawn como syscall | 13–15 | P2 |
| 6 | Terminal del dock = proceso ring 3 | 15 | P2 |
| 7 | V1: reloj `dt` en login/onboarding/desktop | 19 | P5 |
| 8 | Boot sin GRUB como camino documentado **o** `-smp 2` real | 16 / 18 | P4–P5 |

Si solo hay tiempo para **uno**: el 3. Sin un ELF en ring 3, CordOS sigue siendo
un kernel con UI, no un OS que ejecuta programas.

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
| Boot de desarrollo | GRUB + VBox | ¿BIOS o UEFI como default de producto? |
| FS | NosFS + initrd overlay | Layout on-disk a largo plazo |
| Ejecutables | — | ELF64 subset vs `.nos` — **ELF primero** |
| GUI | Compositor 2D propio; no GTK/Qt | Servidor gráfico vs blit in-process |
| Red | Detectar virtio / WLAN host | Stack IP en guest |
| SMP | 1 CPU de verdad | AP + IPI |
| Claves | Hash débil documentable | Sustituir antes de “usuarios reales” |

Actualizar esta tabla al cerrar una fila.

---

## 9. Listo por capa (definición actual)

| Capa | Listo cuando… | ¿Ya? |
|---|---|---|
| Kernel mínimo | Arranca freestanding | Sí |
| Kernel interactivo | Teclado + timer + excepciones | Sí |
| Kernel con memoria | Heap + paginación | Sí |
| Producto gráfico | Login → desktop en VBox | Sí |
| Kernel multiproceso | Switch **y** preempt usable | Switch sí, preempt no |
| Sistema usable | Programas usuario + archivos | Archivos sí, programas no |
| Sistema autónomo | Arranque propio + instalable | No |
| Sistema serio | SMP + red + W^X | No |

---

## 10. Cómo usar este archivo

1. Un hito de la [§6](#6-próximos-8-hitos-orden-de-ataque) por tanda.
2. Nombrar la fase (`Fase 13`, …) en el cambio.
3. No reabrir 0–11. No tratar [`UI_PLAN.md`](UI_PLAN.md) “Ola 1” como trabajo
   de framebuffer: eso ya arranca.
4. Trabajo visual nuevo → [`VISUAL_ROADMAP.md`](VISUAL_ROADMAP.md) (V1, V3, …).
5. Si aparece un frente nuevo, **añadirlo aquí con prioridad** antes de
   implementarlo.

---

## 11. Resumen

**0–11 están hechas.** El hueco que define el siguiente año de CordOS no es
“¿pintamos un desktop?” sino **¿puede un programa que no es el kernel correr,
romperse y dejar el escritorio en pie?**

Eso es Fase 13. El resto (preempt, boot propio, SMP, motion) se apoya en eso.
