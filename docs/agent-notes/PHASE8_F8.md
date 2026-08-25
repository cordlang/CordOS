# PHASE8_F8 — BIOS MBR + stage2 (MVP)

**Agente:** F8  
**Estado:** MVP listo (experimental)

## Entregado

| Pieza | Ruta |
|---|---|
| Stage1 MBR | `boot/mbr.s` → `build/mbr.bin` (512 B, firma 0x55AA) |
| Stage2 | `boot/stage2.s` → `build/stage2.bin` (64 sectores / 32 KiB) |
| Disco | `scripts/mkdisk.sh` → `build/disk.img` |
| Protocolo | `docs/boot_protocol.md` |
| Make | `mbr`, `disk`, `run-bios` (`ARCH=x86_64`) |

## Dependencia

**NASM** (`nasm -f bin`). Debian/Ubuntu: `sudo apt install nasm`.  
El Makefile usa `nasm` del PATH; si falta, prueba `tools/nasm` (binario local opcional).

## Comportamiento

1. MBR carga stage2 desde LBA 1 → `0000:7E00`.
2. Stage2: A20, E820, INT 13h AH=42 carga ELF a `0x10000`, entra PM 32-bit.
3. Parsea ELF64 `PT_LOAD`, construye Multiboot2 mínimo (meminfo + mmap) en `0x2000`.
4. Salta a `e_entry` con `EAX=0x36d76289`, `EBX=0x2000` (shim Multiboot2).

`make run` (GRUB) sigue siendo el camino soportado. `make run-bios` es experimental.

## Make / orquestador

No añade objetos al kernel. Targets nuevos solo en rama `ARCH=x86_64`:

```make
mbr:       build/mbr.bin build/stage2.bin
disk:      build/disk.img
run-bios:  qemu -drive file=build/disk.img,format=raw,if=ide -boot order=c
```

## Smoke

Tras `make mbr disk`, QEMU muestra banner/kernel en VGA (long mode, HLT en idle). Helper: `scripts/smoke_bios.sh`.

## Límites MVP

- Kernel ≤ 256 KiB en buffer bajo.
- Sin FAT/GPT; LBA crudo.
- Sin tags Multiboot2 de módulos/framebuffer en path BIOS.
- Shim Multiboot2, no protocolo nativo NuevoOS todavía.
