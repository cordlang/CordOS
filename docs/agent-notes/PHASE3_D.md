# Agent D-mm + E-build — Phase 3 memory + build

**Estado:** hecho  
**Alcance:** PMM/VMM/heap/page_fault para x86_64 + Makefile `ARCH` dual.

## Decisiones de memoria

| Tema | Elección |
|---|---|
| PMM | `pmm64.c` + Multiboot2 mmap (`multiboot2_find_tag`); bitmap hasta **1 GiB** (cubre identity-map de `boot64`) |
| VMM | `vmm64.c` 4 niveles; `vmm_init` **asume long mode ya activo**, reconstruye identity 4 KiB sobre frames PMM y carga CR3 |
| Heap | **Identity early:** `phys == virt`. `pmm_alloc_contiguous` + kmalloc en esas páginas. Sin HHDM / `0xD0000000` / `0xFFFF8…` hasta que haya map high de B/D |
| Page fault | `page_fault.c` con `#ifdef __x86_64__` (CR2/rip `u64`) — aportado/alineado con C |

## API pública (mismos nombres)

- `pmm_init(void *mb2_addr)` en x86_64; Multiboot1 en i386
- `vmm_init` / `vmm_map_page` / `kmalloc` / `kfree`
- `pmm_alloc_contiguous(u32 count)` solo x86_64 (heap identity)

## `kmain64` order (B + C + D)

```text
vga + banner + multiboot2_print_summary
gdt_init → idt_init → isr_install → PIC/PIT/kbd → STI   /* C */
pmm_init → vmm_init → heap_init → memory_self_test        /* D */
shell loop
```

No se edita `boot64.s`.

## Archivos D

| Archivo | Rol |
|---|---|
| `src/pmm64.c` | PMM 64 + mmap2 |
| `src/vmm64.c` | PML4 walk / identity rebuild |
| `src/multiboot2.c` | `find_tag` + summary |
| `src/heap.c` | rama `__x86_64__` identity |
| `src/include/pmm.h`, `vmm.h`, `multiboot2.h` | dual-arch |
| `src/kernel64.c` | init C luego memoria (extendido sobre stub B) |

## Build (E)

- `ARCH ?= x86_64` por defecto
- `make ARCH=i386` path antiguo intacto (`cordos32.iso`, Multiboot1)
- x86_64: `CC64`/`LD64` desde `$(HOME)/opt/cross64/bin` si existe, si no PATH
- Objetos B+C+D en `KERNEL64_OBJS`; ISO `out/cordos.iso` + `grub64.cfg`
- `build.ps1`: i386 en Windows; x86_64 → mensaje WSL/`make`
- README: nota corta `ARCH`

## Verificación

- `make ARCH=i386` → `out/cordos32.iso` OK
- `make ARCH=x86_64` → `out/cordos.iso` OK (host `gcc`/`ld` fallback mientras A instala `~/opt/cross64`)
- `grub-file --is-x86-multiboot2 out/cordos.bin` OK when checked after link
- No se modificó `boot64.s`
