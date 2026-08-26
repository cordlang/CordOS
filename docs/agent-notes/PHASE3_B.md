# PHASE3 Agent B-boot — status

**Estado:** hecho  
**Alcance:** Multiboot2 + long mode entry → `kmain64`, sin romper i386.

## Archivos entregados

| Archivo | Rol |
|---|---|
| `src/boot64.s` | Header Multiboot2, stub 32-bit, PML4/PDPT/PD (1 GiB identity 2 MiB), long mode, `call kmain64` (RDI = mb2 info) |
| `linker64.ld` | `ENTRY(_start64)`, carga a 1M, `_kernel_start` / `_kernel_end` |
| `grub64.cfg` | `multiboot2 /boot/cordos.bin` |
| `src/kernel64.c` | `kmain64`: VGA clear, imprime globals `config`, resume mb2 `total_size`, `hlt` |
| `src/vga64.c` | VGA texto 64-bit; implementa API de `vga.h` (linkear en vez de `vga.o`) |
| `src/include/multiboot2.h` | Magics, tags, mmap entry, `multiboot2_tag_next`, `multiboot2_is_valid` |
| `src/config.c` | `arch_os` = `"x86_64"` si `__x86_64__`, si no `"i386"` |

**No tocado:** `src/boot.s`, `linker.ld`, `grub.cfg`, path i386 del Makefile.

## Verificación local (host `as`/`gcc`/`ld`, no cross aún)

Toolchain A (`~/opt/cross64`) aún no estaba instalada al verificar. Smoke con binutils host:

- `as --64` + link `elf_x86_64` → `out/cordos.bin`
- `grub-file --is-x86-multiboot2` → **OK**
- Símbolos: `_start64`, `kmain64`, `_kernel_start` @ 1M, `_kernel_end`

## Makefile rules para Agent E (no integradas aquí — evitar carrera)

Asumir `CROSS64 ?= $(HOME)/opt/cross64/bin` y `ARCH=x86_64` default.

```make
# --- x86_64 / Multiboot2 (Agent B) ---
CROSS64 ?= $(HOME)/opt/cross64/bin
CC64 := $(CROSS64)/x86_64-elf-gcc
LD64 := $(CROSS64)/x86_64-elf-ld
QEMU64 := qemu-system-x86_64

CFLAGS64 := -std=c11 -ffreestanding -fno-builtin -fno-stack-protector \
	-fno-pie -m64 -mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
	-Wall -Wextra -Werror -O2 -Isrc/include

KERNEL64_OBJS := \
	build/boot64.o \
	build/config64.o \
	build/vga64.o \
	build/kernel64.o

build/boot64.o: src/boot64.s | build
	$(CC64) -c $< -o $@

build/config64.o: src/config.c | build
	$(CC64) $(CFLAGS64) -c $< -o $@

build/vga64.o: src/vga64.c | build
	$(CC64) $(CFLAGS64) -c $< -o $@

build/kernel64.o: src/kernel64.c | build
	$(CC64) $(CFLAGS64) -c $< -o $@

out/cordos.bin: $(KERNEL64_OBJS) linker64.ld
	$(LD64) -T linker64.ld -m elf_x86_64 --build-id=none -o $@ $(KERNEL64_OBJS)

check64: out/cordos.bin
	$(GRUB_FILE) --is-x86-multiboot2 $<

out/cordos.iso: out/cordos.bin grub64.cfg | build
	mkdir -p iso64/boot/grub
	cp out/cordos.bin out/isoroot/boot/cordos.bin
	cp grub64.cfg iso64/boot/grub/grub.cfg
	$(GRUB_MKRESCUE) -o $@ iso64

run64: out/cordos.iso
	$(QEMU64) -cdrom $<
```

Notas para E:

1. Compilar `config.c` **con** `CC64` (para que `__x86_64__` fije `arch_os`).
2. Usar `vga64.o`, no `vga.o`, en el link 64.
3. Mantener `make ARCH=i386` / targets actuales intactos.
4. Cuando C/D aporten GDT/IDT/PMM 64, añadir sus `.o` a `KERNEL64_OBJS`.

## Detalles técnicos boot

- Magic header: `0xE85250D6`; magic bootloader (EAX): `0x36d76289` (validado en asm antes de C).
- Paginación: 4 niveles; solo PML4[0]→PDPT[0]→PD[0..511] con páginas 2 MiB (1 GiB identity).
- GDT temporal en `.data` (code 0x08, data 0x10); stack 16 KiB en `.bss`.
- ABI: SysV AMD64, `RDI` = puntero físico a info Multiboot2.

## Coordinación

- A: cuando exista `~/opt/cross64/bin/x86_64-elf-*`, re-link con esas tools.
- C: puede asumir long mode + identity map bajo; no pisa `boot64.s`.
- D: `_kernel_start` / `_kernel_end` listos; tipos `size_t` 64 los puede ampliar D en `types.h`.
- E: integrar rules de arriba; B no modificó el Makefile a propósito.
