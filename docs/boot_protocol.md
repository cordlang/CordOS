# CordOS BIOS boot protocol (Fase 8 MVP)

**Status:** experimental. Default development path remains **GRUB** (`make run`).
BIOS disk boot is available via `make disk` / `make run-bios`.

## Disk layout (`build/disk.img`)

| LBA | Contents | Size |
|---|---|---|
| 0 | Stage1 MBR (`boot/mbr.s` → `build/mbr.bin`) | 512 B |
| 1–64 | Stage2 (`boot/stage2.s` → `build/stage2.bin`) | 32 KiB |
| 65+ | Kernel ELF (`out/cordos.bin`) | variable (≤ 256 KiB for stage2 buffer) |

Built by `scripts/mkdisk.sh` (patches stage2 `NOSP` fields with kernel LBA + sector count).

## Handoff (compatibility shim)

Stage2 does **not** implement a full Multiboot2 bootloader. For MVP it:

1. Collects an E820 memory map (BIOS).
2. Loads the ELF64 kernel into a low buffer, applies `PT_LOAD` segments.
3. Builds a **minimal Multiboot2 info** structure (basic meminfo + mmap + end).
4. Enters 32-bit protected mode with a flat GDT.
5. Jumps to ELF `e_entry` (`_start64`) with:
   - `EAX` = `0x36d76289` (Multiboot2 bootloader magic)
   - `EBX` = physical address of the info structure (`0x2000`)

So the existing `boot64.s` / PMM path keeps working without a separate CordOS kernel entry yet.

## Future CordOS-native protocol

A dedicated magic (e.g. `EAX = 'NOSB'`) and info blob may replace the Multiboot2 shim later. Until then, treat Multiboot2 register handoff as a **compatibility layer**, not a commitment to recreate GRUB.

## Toolchain

| Tool | Role |
|---|---|
| **NASM** | Assemble `boot/mbr.s` and `boot/stage2.s` (`nasm -f bin`) |
| `x86_64-elf-gcc` / `ld` | Kernel (unchanged) |
| QEMU | `qemu-system-x86_64 -drive file=build/disk.img,format=raw`

Install NASM on Debian/Ubuntu: `sudo apt install nasm`.

## Make targets (`ARCH=x86_64`)

```text
make mbr          # build/mbr.bin + build/stage2.bin
make disk         # build/disk.img (needs kernel + mbr)
make run-bios     # QEMU boot from disk.img (experimental)
make run          # GRUB ISO (supported default)
```

## Limits (MVP)

- Kernel must fit in 256 KiB temporary buffer at `0x10000`.
- No FAT/GPT; raw LBA packing only.
- No Multiboot2 modules / framebuffer tags from BIOS path.
- A20 via fast gate only; QEMU-oriented.
