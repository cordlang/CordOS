# CordOS — Guía de desarrollador

Mapa del árbol, convenciones y cómo añadir syscalls o drivers. Arquitectura objetivo: **x86_64** (`ARCH=x86_64`). i386 se mantiene como demo congelada.

**Qué sigue no se decide aquí.** Fuente de verdad: [`ROADMAP.md`](ROADMAP.md).
ABI implementada: [`abi.md`](abi.md). Este mapa de rutas está atrasado respecto
al árbol (`src/kernel/`, `src/proc/`, …).

Notas honestas (agosto 2026), sin inventar ABI:

- `mmap` no es stub: bump anónimo `PAGE_USER`, siempre writable. **`prot` se ignora.**
- `read`/`write` no multiplexan fds VFS (0 = teclado, 1 = VGA). `open`/`close` sí hablan con VFS.
- No hay `spawn`/`exec`. El ELF de smoke lo lanza el kernel.
- Hash de claves: FNV-1a + pepper, no un KDF.

## Mapa de módulos (actual)

```text
src/
  boot64.s / boot.s     # entrada Multiboot2 (64) / Multiboot1 (32)
  kernel64.c / kernel.c # kmain
  arch/x86_64/          # GDT, IDT, ISR 64-bit; switch.s / syscall_entry.s (F4/F5)
  config.c, string.c, io.c, panic.c
  utf8.c                # decode/encode UTF-8 y mapeo CP437
  vga64.c / vga.c       # consola texto 80×25
  multiboot2.c          # tags Multiboot2
  pic.c, pit.c, time.c, keyboard.c
  pmm64.c, vmm64.c, heap.c, page_fault.c
  sched.c, syscall.c, shell.c, spinlock.c, smp.c, ipc.c   # F4–F10 (wire-up INT)
  drivers/
    fb.c / fb.h         # stub framebuffer Multiboot2 (F11)
    serial.c, pci.c, virtio_net.c
  include/              # -Isrc/include (fb.h wrapper → drivers/fb.h)
docs/
  USER.md, DEV.md, abi.md, smp.md
agent-notes/
  PHASE*_*.md, MAKE_OBJS.md
boot/
  mbr.s                 # F8 Stage1 (en curso)
```

Lista de `.o` pendientes de enlace: [`agent-notes/MAKE_OBJS.md`](../agent-notes/MAKE_OBJS.md).

## Convenciones

- Freestanding C11, sin libc host. Tipos en `types.h` (`u8`…`u64`, `bool`).
- Headers públicos en `src/include/`. Drivers nuevos preferidos bajo `src/drivers/`.
- Extender `kernel64.c` lo mínimo: preferir `foo_init(...)` y una sola llamada al final (orquestador / INT).
- **Makefile:** no editar a ciegas en paralelo; documentar objetos nuevos en `agent-notes/MAKE_OBJS.md` para que INT los enlace.
- Compilar mentalmente: `-m64 -ffreestanding -Isrc/include`.

## Añadir un driver

1. Crear `src/drivers/mi_drv.c` y `src/include/mi_drv.h` (o `src/drivers/mi_drv.h` si documentas el include path).
2. API típica: `void mi_drv_init(...);` + operaciones mínimas.
3. Añadir `build/mi_drv.o` a `MAKE_OBJS.md` (y regla de compilación si no cae en un patrón genérico).
4. Llamar `mi_drv_init` desde `kmain64` (o un `phaseN_init`) **solo** cuando INT integre.
5. Probar con `make` + `make run` en WSL.

Ejemplo actual: framebuffer — `fb_init(mb2_addr)` busca el tag Multiboot2 tipo 8; si existe y el addr es usable, pinta un pixel; si no, mensaje no-op.

## Añadir un syscall

Cuando exista la tabla (Fase 5 / contrato):

1. Reservar número en la tabla documentada (`docs/abi.md`) y en `src/syscall.c`.
2. Convención: `int 0x80`, `rax` = número, args en `rdi` / `rsi` / `rdx`.
3. Validar punteros de usuario cuando haya ring 3; hasta entonces el shell kernel puede llamar `syscall_dispatch` directo.
4. Stub user (si aplica): wrappers en `user/libnos/` o equivalente.
5. Criterio: un programa puede ejercer el syscall sin corromper el kernel.

Números: 0 `exit`, 1 `write`, 2 `read`, 3 `yield`, 4 `getpid`, 5 `mmap` (bump anónimo; `prot` ignorado), 6 `open`, 7 `close`. Detalle y límites: [`abi.md`](abi.md).

## Multiboot2 y framebuffer

- Tag type `8` (`MULTIBOOT2_TAG_TYPE_FRAMEBUFFER`): ver `multiboot2.h` + `fb.h`.
- Sin request en el header Multiboot2 / `gfxpayload`, GRUB suele no entregar el tag → `fb_init` es no-op con mensaje (esperado).
- El identity map inicial (~1 GiB) puede no cubrir un FB en MMIO alto; el stub lo reporta y no escribe.

## Pruebas rápidas

```bash
make ARCH=x86_64
make run
```

Ver también [USER.md](USER.md).
