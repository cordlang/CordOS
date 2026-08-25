# Handoff F9 COMPLETE

## Files
- `src/drivers/serial.c` + `src/include/serial.h`
- `src/drivers/pci.c` + `src/include/pci.h`
- `src/drivers/virtio_net.c` + `src/include/virtio_net.h`
- `src/ipc.c` + `src/include/ipc.h` + `src/include/phase9.h`

## Wire-up (INT)
1. KERNEL64_OBJS: `serial.o pci.o virtio_net.o ipc.o`
   - compile rule for `src/drivers/%.c` → `build/%.o`
2. `phase9_init()` after memory/VGA, before shell
3. QEMU optional: `-serial stdio` `-device virtio-net-pci`
