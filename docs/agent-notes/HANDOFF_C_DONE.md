# Handoff: C-cpu COMPLETE

## Init order for kmain64

```
gdt_init → idt_init → isr_install → pic_remap → pic_mask_all
→ time_init → keyboard_init → unmask 0,1 → interrupts_enable
```

Call `gdt_init` first (replaces B temp GDT). Do not edit `boot64.s`.

## Objects to add to KERNEL64_OBJS

```
build/gdt64.o      from src/arch/x86_64/gdt64.c
build/idt64.o      from src/arch/x86_64/idt64.c
build/isr64.o      from src/arch/x86_64/isr64.c
build/isr64_asm.o  from src/arch/x86_64/isr64.s
```

Plus shared (CC64): pic, pit, time, keyboard, io, string, panic, page_fault.

Do NOT link i386 `gdt.o`/`idt.o`/`isr.o`/`isr_asm.o` on x86_64.

Full notes: `PHASE3_C.md`
