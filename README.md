# CordOS

Primer prototipo de un sistema operativo propio. El kernel no usa Windows,
Linux ni macOS: se compila como software `freestanding` y escribe directamente
en la memoria de video VGA.

## Árbol de módulos (`src/`)

| Carpeta | Qué hay |
|---|---|
| `src/boot` | Multiboot, `boot64.s` |
| `src/arch/x86_64` | GDT, IDT, ISR, PIC, PIT, context switch |
| `src/arch/i386` | Demo 32-bit |
| `src/kernel` | `kmain64`, panic, string, I/O |
| `src/mm` | PMM, VMM, heap |
| `src/proc` | tareas, scheduler, syscalls, SMP |
| `src/drivers` | framebuffer, teclado, ratón, PCI, power |
| `src/fs` | VFS, NosFS, initrd |
| `src/ui/gfx` | draw, iconos, fuente, wallpaper, marca |
| `src/ui/login` | pantalla de login |
| `src/ui/desktop` | escritorio, dock, ventanas |
| `src/ui/session` | splash, idioma, orquestación |
| `src/shell` | shell de emergencia, i18n |
| `src/include/<módulo>` | headers públicos |

La ISO queda en `dist/cordos.iso`; `out/` contiene solo los artefactos intermedios.

## Arquitectura (`ARCH`)

| Comando | Resultado |
|---|---|
| `make` / `make ARCH=x86_64` | Build principal: long mode, Multiboot2, `dist/cordos.iso` |
| `make ARCH=i386` | Demo congelada: protegido 32-bit, Multiboot1, `out/cordos32.iso` |

Toolchain 64: `x86_64-elf-gcc` / `x86_64-elf-ld` (recomendado en `~/opt/cross64`). En Windows, el path x86_64 va por WSL (`make`); `build.ps1` cubre solo i386.

## Alcance

- Kernel: C11 freestanding, sin libc ni SO anfitrión.
- Arranque temporal: GRUB (Multiboot2 en x86_64; Multiboot1 en i386).
- Prueba: la ISO (`dist/cordos.iso`) en **VirtualBox** (recomendado) o QEMU.

La primera instrucción no puede ser C puro: el procesador necesita una entrada
de bajo nivel (`src/boot/boot64.s` o `src/boot/boot.s`). Después, la lógica está en C.

## Requisitos de desarrollo

**x86_64 (default):** `x86_64-elf-gcc`, `x86_64-elf-ld`, `grub-file`, `grub-mkrescue`, `qemu-system-x86_64`.  
**BIOS path (opcional):** `nasm` — `make mbr` / `make disk` / `make run-bios` (experimental; ver `docs/boot_protocol.md`).

**i386:** `i686-elf-gcc`, `i686-elf-ld`, mismas herramientas GRUB, `qemu-system-i386`.

## Compilar y ejecutar

```text
make
.\run-vbox.ps1          # Windows + VirtualBox (recomendado para usar la ISO)
make run-vbox           # Linux con VBoxManage en PATH
make run                # QEMU (desarrollo / serial)
make mbr && make disk && make run-bios   # experimental BIOS (needs nasm)
make ARCH=i386
make ARCH=i386 run
```

Con PowerShell (ISO 64-bit en VirtualBox):

```powershell
.\run-vbox.ps1
```

Demo i386 (QEMU):

```powershell
.\build.ps1
.\run.ps1
```

## Documentación

| Doc | Contenido |
|---|---|
| [`docs/USER.md`](docs/USER.md) | Build/run en WSL, `make`, QEMU, idioma ES/EN |
| [`docs/DEV.md`](docs/DEV.md) | Mapa de módulos, syscalls y drivers |
| [`docs/UI_PLAN.md`](docs/UI_PLAN.md) | UI: arranque → Home → olas |
| [`docs/ROADMAP.md`](docs/ROADMAP.md) | Plan de fases completo |
| [`docs/phases/PHASE3_CONTRACT.md`](docs/phases/PHASE3_CONTRACT.md) | Coordinación Fase 3 (hecha) |
| [`docs/phases/PHASES_4_11_CONTRACT.md`](docs/phases/PHASES_4_11_CONTRACT.md) | Coordinación Fases 4–11 |
