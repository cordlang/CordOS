<p align="center">
  <img src="assets/logo/cordos_logo_black.svg#gh-light-mode-only" width="148" alt="CordOS">
  <img src="assets/logo/cordos_logo_white.svg#gh-dark-mode-only" width="148" alt="CordOS">
</p>

<h1 align="center">CordOS</h1>

<p align="center">
  <strong>Un sistema operativo propio, de cero, con escritorio gráfico.</strong><br>
  Kernel freestanding en C11. Sin Linux, sin Windows, sin macOS.
</p>

<p align="center">
  <code>x86_64</code>
  &nbsp;·&nbsp;
  <code>C11</code>
  &nbsp;·&nbsp;
  <code>Multiboot2</code>
  &nbsp;·&nbsp;
  <code>ES / EN</code>
</p>

<p align="center">
  <a href="#arrancar">Arrancar</a>
  ·
  <a href="#qué-hay-dentro">Módulos</a>
  ·
  <a href="#documentación">Docs</a>
</p>

---

## Qué es

CordOS es un kernel **x86_64** compilado como software `freestanding`: escribe el framebuffer, atiende el teclado y el ratón, y pinta su propia interfaz. Arranca con GRUB (Multiboot2), elige idioma, muestra un splash, pide sesión y abre un escritorio con ventanas, dock y Spotlight.

No es un fork. No reutiliza drivers ni syscalls de otro sistema. Lo que corre en la máquina es código de este árbol.

## Lo que ya corre

| Superficie | Qué ves |
|---|---|
| **Sesión** | Idioma → splash → login → escritorio |
| **Escritorio** | Dock, ventanas, menús, Spotlight (`Win`/`Ctrl` + `Space`) |
| **Apps** | Archivos, Terminal, Ajustes, Acerca de |
| **Kernel** | PMM, VMM, heap, tareas, syscalls, VFS / NosFS |
| **Disco** | Persistencia en AHCI / IDE (no es un truco de VirtualBox) |
| **Idioma** | Español e inglés en UI y shell |

Cuenta de desarrollo: usuario `admin`, contraseña `admin`.

## Arrancar

En Windows, con WSL y VirtualBox, basta un comando. Reconstruye la ISO y abre la VM:

```powershell
.\run-vbox.ps1
```

La ISO queda en `dist/cordos.iso`. El serial del kernel, en `out/serial.log`.

<details>
<summary><strong>Compilar a mano, QEMU y demo i386</strong></summary>

<br>

Toolchain 64: `x86_64-elf-gcc` / `x86_64-elf-ld` (recomendado en `~/opt/cross64`). En Windows el build x86_64 va por WSL.

```bash
make                          # x86_64 → dist/cordos.iso
make check                    # Multiboot2 + tests de host
make run                      # QEMU (ciclos rápidos / serial)
make run-vbox                 # Linux con VBoxManage en PATH
make ARCH=i386                # demo 32-bit congelada
```

PowerShell solo para la demo i386: `.\build.ps1` y `.\run.ps1`.

BIOS experimental: `make mbr && make disk && make run-bios` (hace falta `nasm`). Detalle en [`docs/boot_protocol.md`](docs/boot_protocol.md).

**VirtualBox:** firmware BIOS, VBoxSVGA, ≥128 MiB VRAM, ratón PS/2. Clic para capturar el puntero; **Right Ctrl** lo suelta. Si la imagen se ve pequeña, **Host+F** a pantalla completa.

Guía completa: [`docs/USER.md`](docs/USER.md).

</details>

## Qué hay dentro

```text
src/
  boot/          Multiboot, entrada 64-bit
  arch/x86_64/   GDT, IDT, ISR, PIC, PIT, context switch
  arch/i386/     demo 32-bit (congelada)
  kernel/        kmain, panic, string, selftest
  mm/            PMM, VMM, heap
  proc/          tareas, scheduler, syscalls
  drivers/       framebuffer, teclado, ratón, PCI, disco, red
  fs/            VFS, NosFS, initrd, usuarios
  ui/            draw, login, desktop, splash, onboarding
  shell/         shell de emergencia, i18n
  include/       headers públicos por módulo
```

`out/` son artefactos intermedios. El entregable es `dist/cordos.iso`.

## Documentación

| | |
|---|---|
| [`docs/USER.md`](docs/USER.md) | Compilar, VirtualBox, QEMU, idioma |
| [`docs/DEV.md`](docs/DEV.md) | Mapa de módulos, syscalls, drivers |
| [`docs/UI_PLAN.md`](docs/UI_PLAN.md) | UX: arranque → login → Home |
| [`docs/ROADMAP.md`](docs/ROADMAP.md) | Plan vivo (canónico). 0–11 cerradas. 13 ELF + spawn/fds. Falta confirmar preempt en VBox |
| [`docs/abi.md`](docs/abi.md) | ABI de syscalls |
| [`docs/nosfs.md`](docs/nosfs.md) | Sistema de archivos |

## Principios

1. **Desde cero** — kernel propio, ABI propia, FS propio.
2. **Freestanding** — sin libc del anfitrión; solo lo que implementamos.
3. **Incremental** — cada fase deja algo arrancable.
4. **Honesto** — GRUB y el toolchain son andamiaje; el producto no es Linux.

---

<p align="center">
  <img src="assets/logo/cordos_logo_black.svg#gh-light-mode-only" width="40" alt="">
  <img src="assets/logo/cordos_logo_white.svg#gh-dark-mode-only" width="40" alt="">
  <br>
  <sub>CordOS — construido desde el metal.</sub>
</p>
