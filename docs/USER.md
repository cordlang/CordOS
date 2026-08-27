# CordOS — Guía de usuario (desarrollo)

Cómo construir el kernel y arrancarlo. Compilas en **WSL (Ubuntu)** con `x86_64-elf`; la ISO se usa como un CD en **VirtualBox** (recomendado) o en QEMU.

## Requisitos

| Pieza | Notas |
|---|---|
| WSL2 + Ubuntu | En Windows: `wsl -d Ubuntu` desde el repo (`/mnt/d/os` o tu mount) |
| `x86_64-elf-gcc` / `x86_64-elf-ld` | Recomendado en `~/opt/cross64/bin` (Fase 3) |
| `grub-file`, `grub-mkrescue` | Paquete `grub-pc-bin` / `grub2-common` según distro |
| `xorriso` | Lo usa `grub-mkrescue` para la ISO |
| VirtualBox | Arranque de la ISO en una app de escritorio (recomendado) |
| `qemu-system-x86_64` | Pruebas rápidas / serial |

Demo i386 (opcional): `i686-elf-gcc`, `qemu-system-i386`. PowerShell (`build.ps1` / `run.ps1`) solo cubre i386.

## Compilar (x86_64, default)

Desde la raíz del repo en WSL:

```bash
cd /mnt/d/os   # ajusta si tu mount es otro
make
# o explícito:
make ARCH=x86_64
```

Salida: `dist/cordos.iso` y `out/cordos.bin`.

Comprobar Multiboot2:

```bash
make check
```

## Ejecutar la ISO (VirtualBox)

La ISO es un CD arrancable (`dist/cordos.iso`). Lo cómodo es abrirla en VirtualBox, no en QEMU.

### Windows (lo habitual)

1. Compila en WSL:

```bash
cd /mnt/d/os
make ARCH=x86_64
```

2. En PowerShell, en la raíz del repo:

```powershell
.\run-vbox.ps1
```

Eso crea (una vez) una VM `CordOS` y monta la ISO:

| Ajuste | Valor (necesario) |
|---|---|
| Tipo | Other / Other 64-bit |
| Firmware | **BIOS** (EFI apagado) |
| I/O APIC | activado |
| Gráficos | **VBoxSVGA**, ≥128 MiB VRAM |
| Ratón | **PS/2** (no USB tablet) |
| Disco | no hace falta; arranque desde CD |

Si la ventana se ve **diminuta o pixelada** (p.ej. el selector muestra `(640x480)`):

1. Remonta la ISO nueva (`make` → `dist\cordos.iso`).
2. Fondo embebido: **exactamente 1920×1080** RGB; si el guest también es 1920×1080, el blit es 1:1 (sin pixelar).
3. El kernel sube hacia **1920×1080** con Bochs VBE (1920 → 1600 → 1280 → 1024).
4. En GRUB elige **CordOS**.
5. En tu VM `nuevaos` (apagada):

```powershell
& "C:\Program Files\Oracle\VirtualBox\VBoxManage.exe" setextradata nuevaos CustomVideoMode1 1920x1080x32
```

Tras arrancar, opcional:

```powershell
& "C:\Program Files\Oracle\VirtualBox\VBoxManage.exe" controlvm nuevaos setvideomodehint 1920 1080 32
```

En el selector deberías ver **`(1920x1080)`**.

**Si se ve pixelado / borroso con `(1920x1080)`:** no es que el OS baje la calidad — VirtualBox está **comprimiendo** el framebuffer 1080p en una ventana chica. Solución:

1. Clic en la ventana de la VM
2. **Host+F** (Right Ctrl + F) → pantalla completa, 1:1  
   o **Ver → Pantalla completa**
3. Alternativa: **Ver → Ajustar a tamaño de ventana** y agranda la ventana hasta ~1920×1080

Con pantalla completa el fondo y el texto se ven nítidos.

Deberías ver el menú GRUB y luego idioma / splash / login / escritorio.

Login de desarrollo: usuario `admin`, contraseña `admin`.

Clic en la ventana de VirtualBox para capturar el ratón; **Right Ctrl** (tecla Host) lo suelta.

Si reconstruyes el kernel, vuelve a ejecutar `.\run-vbox.ps1` (remonta la ISO).

### Linux con VirtualBox

```bash
make ARCH=x86_64 run-vbox
```

## Ejecutar en QEMU (desarrollo)

Útil para serial y ciclos rápidos:

```bash
make run
```

Equivalente:

```bash
qemu-system-x86_64 -cdrom dist/cordos.iso
```

Deberías ver el selector de idioma (si aplica), el **splash**, el **login** y el **escritorio**.

### Sesión (escritorio gráfico)

Flujo: idioma → splash → **login** → **escritorio** (iconos, ventanas, barra de tareas).

Con GRUB `gfxpayload=1920x1080x32` hay framebuffer Full HD. Si el visor no puede, cae a 1280×720 o 1024×768. Sin FB, se usa el login de texto.

| Pantalla | Controles |
|---|---|
| Login | Clic en campos/botón; Tab / Enter; F1 = shell emergencia |
| Escritorio | Clic iconos y Menu; arrastrar ventanas; clic derecho = menú |
| Terminal (ventana) | `help` `ls` `cat` `exit`; Esc cierra |
| Shell emergencia | F1; `exit` vuelve al escritorio |

Credenciales de desarrollo:

- Usuario: `admin`
- Contraseña: `admin`

El escritorio ofrece: Archivos, Terminal, Ajustes, Acerca de, Cerrar sesión, Apagar.

### Idioma (ES / EN)

Al arrancar puedes elegir **español** o **inglés** de tres formas:

1. **Pantalla del kernel** (por defecto): si no hay `lang=` en la línea de comandos, aparece el selector. Usa `1`/`2`, `Tab`, `w`/`s` o `a`/`e` y **Enter**.
2. **Menú GRUB** (5 s): entradas *Espanol*, *English*, o *elegir idioma* (abre el selector).
3. **Shell**: `lang es` o `lang en` (también `lang` sin args muestra el actual).

Forzar idioma al lanzar QEMU sin menú GRUB, editando `grub64.cfg` o pasando cmdline en la entrada Multiboot2:

```text
multiboot2 /boot/cordos.bin lang=en
```

### UTF-8 en la shell

La shell conserva las líneas en UTF-8 y traduce los caracteres latinos que
VGA texto puede representar mediante su tabla CP437. Backspace elimina el
carácter completo, incluidos sus bytes UTF-8.

Con un teclado PS/2, usa **AltGr** junto con estas teclas para escribir
caracteres especiales sin cambiar la distribución ASCII base:

| Combinación | Carácter |
|---|---|
| AltGr + a/e/i/o/u | á/é/í/ó/ú |
| AltGr + y | ü |
| AltGr + n | ñ |
| AltGr + c | ç |
| AltGr + Shift + a/e/i/o/u/y/n/c | Mayúscula correspondiente |
| AltGr + 1 o ! | ¡ |
| AltGr + Shift + / | ¿ |

### Demo 32-bit

```bash
make ARCH=i386
make ARCH=i386 run
```

## Framebuffer

La ISO pide **1920×1080×32** (header Multiboot2 + `gfxpayload` en GRUB). En VirtualBox usa **VBoxSVGA** y al menos 64 MiB de VRAM. Si no hay FB, el kernel sigue en VGA texto.

## Limpieza

```bash
make clean
```

## Documentación relacionada

- [DEV.md](DEV.md) — mapa de módulos y cómo extender el kernel
- [ROADMAP.md](ROADMAP.md) — plan vivo (canónico; 0–11 cerradas)
- [../README.md](../README.md) — visión general
