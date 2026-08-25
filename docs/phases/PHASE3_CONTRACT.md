# Fase 3 — Contrato compartido entre agentes

Documento de coordinación. Todos los agentes deben leerlo y actualizar la
sección **Estado por agente** al terminar su parte.

## Objetivo

Migrar NuevoOS a **x86_64 long mode** según `ROADMAP.md` Fase 3.
Congelar i386 como demo (`ARCH=i386`) y hacer **x86_64 el build principal**.

## Decisiones cerradas (no reinventar)

| Tema | Decisión |
|---|---|
| Arch principal | `x86_64` |
| i386 | Se mantiene con `make ARCH=i386` |
| Boot temporal | Multiboot2 (GRUB) |
| Toolchain | `x86_64-elf-gcc` / `x86_64-elf-ld` en `~/opt/cross64` |
| Paginación | 4 niveles, identity-map inicial |
| ABI C | System V AMD64 donde aplique en kernel freestanding |
| Globales | Seguir patrón `name_os`, `arch_os="x86_64"`, etc. |

## Layout de archivos acordado

```text
src/
  boot.s                 # i386 Multiboot1 (existente, no romper)
  boot64.s               # x86_64 Multiboot2 + long mode entry
  include/
    types.h              # tipos con ifdef o tipos anchos u64-first
  arch/
    x86_64/              # gdt64, idt64, isr64 (si un agente lo crea)
  ... módulos existentes adaptados o con #ifdef __x86_64__
linker64.ld
Makefile                 # ARCH=x86_64 por defecto
```

## Interfaces mínimas (contratos de API)

- `void kmain64(void *mb2_addr);` o `kmain` unificado según arch
- PMM/VMM: mismas funciones públicas (`pmm_init`, `vmm_init`, `kmalloc`)
- Interrupts: `gdt_init`, `idt_init`, `isr_install`, `interrupts_enable`
- No borrar el path i386 en esta fase

## Orden de dependencias

1. **Toolchain** (bloquea link)
2. **boot64 + linker64 + Multiboot2** (bloquea ejecución)
3. **GDT64/IDT64/ISR** (puede prepararse en paralelo al boot)
4. **PMM/VMM/heap 64** (después de boot + tipos)

## Estado por agente

| Agente | Alcance | Estado | Notas |
|---|---|---|---|
| A-toolchain | Instalar `x86_64-elf-*`, verificar versiones | **hecho** | gcc 13.2.0 / binutils 2.42 in `~/opt/cross64`; see `agent-notes/PHASE3_A.md` |
| B-boot | `boot64.s`, Multiboot2, `linker64.ld`, entry a C | hecho | Ver `agent-notes/PHASE3_B.md`; Makefile rules para E |
| C-cpu | GDT64, IDT64, ISR stubs 64, PIC/PIT/kbd smoke | listo | `src/arch/x86_64/*` + `agent-notes/PHASE3_C.md`; i386 OK |
| D-mm | PMM/VMM/heap/page_fault para 64-bit | hecho | Identity heap; `agent-notes/PHASE3_D.md` |
| E-build | Makefile/build.ps1 `ARCH=x86_64` default + README | hecho | `ARCH=i386` OK; 64 vía Make/WSL |

## Reglas anti-conflicto

1. No reescribir módulos i386 enteros sin `#if` / archivos `*64*`.
2. Si tocas `kernel.c`, preferir `kernel64.c` o ramas claras por `ARCH`.
3. Al terminar: actualizar esta tabla y dejar el árbol compilable.
4. No hacer commit (el usuario no lo pidió).
