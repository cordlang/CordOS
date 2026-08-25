# Objetos pendientes para Makefile (fases 4–11)

Orquestador: añadir a `KERNEL64_OBJS` cuando existan los `.c/.s`.

## Transversal

```text
build/utf864.o / build/utf8.o  # src/utf8.c; UTF-8 y mapeo CP437
```

```
# F4 — wired into KERNEL64_OBJS (see PHASE4_F4.md)
build/task.o               # src/task.c
build/sched.o              # src/sched.c
build/switch.o             # src/arch/x86_64/switch.s
# rules:
#   build/task.o / build/sched.o: $(CC64) $(CFLAGS64) -c
#   build/switch.o: $(CC64) -c src/arch/x86_64/switch.s
# wire: phase4_init() after heap_init / memory_self_test

# F5
build/syscall.o            # src/syscall.c
build/syscall_entry.o      # src/arch/x86_64/syscall_entry.s
# rules:
#   build/syscall.o: src/syscall.c | build
#   	$(CC64) $(CFLAGS64) -c $< -o $@
#   build/syscall_entry.o: src/arch/x86_64/syscall_entry.s | build
#   	$(CC64) -c $< -o $@
# wire: after isr_install / keyboard_init → phase5_init();

# F6 — see agent-notes/PHASE6_F6.md; call phase6_init() at splash stage 2
# sources: src/fs/{vfs,nosfs,nosfs_disk,initrd,persist}.c  src/drivers/ata.c
out/vfs.o
out/nosfs.o
out/nosfs_disk.o
out/initrd.o
out/persist.o
out/ata.o
# optional image: out/persist.img (16 MiB zeros); make run-persist

# F7
build/shell.o              # src/shell.c → shell_run(); wired in KERNEL64_OBJS

# F8 — usually separate binaries, not kernel objs
# boot/mbr.bin boot/stage2.bin  (NASM; make mbr / disk / run-bios)
# see agent-notes/PHASE8_F8.md + docs/boot_protocol.md

# F9 — see agent-notes/PHASE9_F9.md; call phase9_init() from kmain64
# sources: src/drivers/{serial,pci,virtio_net}.c  src/ipc.c
build/serial.o
build/pci.o
build/virtio_net.o
build/ipc.o
# rules:
#   build/serial.o: src/drivers/serial.c
#   build/pci.o: src/drivers/pci.c
#   build/virtio_net.o: src/drivers/virtio_net.c
#   build/ipc.o: src/ipc.c
#   $(CC64) $(CFLAGS64) -c $< -o $@
# QEMU: -serial stdio  [-device virtio-net-pci]

# F10 — see agent-notes/PHASE10_F10.md; call phase10_init() from kmain64
build/spinlock.o   # src/spinlock.c
build/smp.o        # src/smp.c

# F11
build/fb.o              # from src/drivers/fb.c  (header: src/drivers/fb.h)
# build/fb.o: src/drivers/fb.c | build
#	$(CC64) $(CFLAGS64) -c $< -o $@

# UI polish
build/animation.o       # src/ui/animation.c — short framebuffer fades
```
