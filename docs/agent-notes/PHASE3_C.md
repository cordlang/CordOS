# Agent C-cpu — Phase 3 interrupt stack (x86_64)

## Status

Prepared long-mode GDT / IDT / ISR without rewriting i386 `isr.s`.
i386 objects remain the default Makefile path; x86_64 sources are under
`src/arch/x86_64/` for Agent E to wire when `ARCH=x86_64`.

## Files added

| File | Role |
|---|---|
| `src/arch/x86_64/gdt64.c` | Final GDT64 + TSS placeholder, `gdt_init` |
| `src/arch/x86_64/idt64.c` | 256×16-byte gates, `idt_init` / `idt_set_gate` |
| `src/arch/x86_64/isr64.s` | Exception stubs 0–31, IRQ stubs 32–47 |
| `src/arch/x86_64/isr64.c` | C handlers, `isr_install`, IRQ table, STI/CLI |
| `src/include/arch/x86_64/{gdt,idt,isr}.h` | Pointers to public API / layout notes |

## Shared headers touched (dual-arch)

- `types.h` — `size_t` / `ssize_t` are 64-bit under `__x86_64__`
- `idt.h` — `idt_set_gate(..., u64 handler, ...)` on x86_64
- `isr.h` — 64-bit `interrupt_frame` (`rip` / `rflags` / `rsp`)
- `page_fault.c` — CR2 / rip as `u64` when `__x86_64__`

## Unchanged (compile on both arches)

`pic.c`, `pit.c`, `keyboard.c`, `time.c`, `io.c` — same I/O ports; no
logic change. They pick up the arch-specific `interrupt_frame` via `isr.h`.

## Symbols exported (link against these for x86_64)

Same names as i386 (swap objects, keep API):

| Symbol | From |
|---|---|
| `gdt_init` | gdt64.c |
| `idt_init`, `idt_set_gate` | idt64.c |
| `isr_install`, `isr_handler`, `irq_handler` | isr64.c |
| `irq_install_handler`, `irq_uninstall_handler` | isr64.c |
| `interrupts_enable`, `interrupts_disable` | isr64.c |
| `isr0`…`isr31`, `irq0`…`irq15` | isr64.s |
| `pic_*`, `pit_init`, `time_init`, `keyboard_init` | existing `.c` |

Selectors (unchanged): `GDT_KERNEL_CODE=0x08`, `GDT_KERNEL_DATA=0x10`,
`GDT_TSS=0x18`.

## GDT handoff with Agent B (boot64)

Aligned with `PHASE3_B.md`: B’s GDT in `.data` is **temporary** (code 0x08, data 0x10)
for entering long mode only.

| Who | What |
|---|---|
| **Agent B (`boot64.s`)** | Temporary GDT + identity map; jumps to `kmain64(RDI=mb2)`. Do not extend that GDT for TSS/IDT. |
| **Agent C (`gdt_init`)** | **Final** kernel GDT + TSS. Call from `kmain64` once long mode is stable. Reloads CS via `lretq`, DS/ES/SS, `ltr` on `0x18`. |

No symbol required from `boot64.s` for GDT reload. After `gdt_init`, B’s early GDT is unused.

## `kmain64` init order (expected)

Mirror i386 `init_interrupts()` in `kernel.c`:

```text
1. gdt_init()          /* final GDT + TSS; safe once in long mode */
2. idt_init()          /* lidt empty 256-gate table */
3. isr_install()       /* fill gates 0–47 */
4. pic_remap(0x20, 0x28)
5. pic_mask_all()
6. time_init(100)      /* pit + irq0 handler */
7. keyboard_init()     /* irq1 handler */
8. pic_clear_mask(0)
9. pic_clear_mask(1)
10. interrupts_enable() /* sti */
```

Page-fault path: `isr14` → `isr_handler` → `page_fault_handler` (Agent D may
extend recovery later; hook is live).

## Makefile hint for Agent E

When `ARCH=x86_64`, link **instead of** `gdt.o` / `idt.o` / `isr.o` / `isr_asm.o`:

```text
arch/x86_64/gdt64.c
arch/x86_64/idt64.c
arch/x86_64/isr64.c
arch/x86_64/isr64.s   → e.g. build/isr64_asm.o
```

Keep `pic.o pit.o time.o keyboard.o page_fault.o` (and `io.o`) for both arches.
Do **not** assemble `src/isr.s` for x86_64.

CFLAGS sketch: `-m64 -mcmodel=kernel` (or as chosen by E) plus `-Isrc/include`.

## Interrupt frame (x86_64)

Lowest address → `r15` … `rax`, `int_no`, `err_code`, then CPU-pushed
`rip`, `cs`, `rflags`, `rsp`, `ss`. Stub passes `%rsp` in `%rdi` (SysV).
