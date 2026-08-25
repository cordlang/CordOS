# Agent F9 — Fase 9 devices / IPC MVP

## Status

**Listo MVP** (sin commit). Virtio-net: solo detección PCI; sin virtqueues ni stack IP.

## Files

| File | Role |
|---|---|
| `src/include/serial.h` + `src/drivers/serial.c` | COM1 `0x3F8` init / putc / write |
| `src/include/pci.h` + `src/drivers/pci.c` | Config space mech #1; enum bus 0 |
| `src/include/virtio_net.h` + `src/drivers/virtio_net.c` | Detect `1AF4:1000` / `1AF4:1041` |
| `src/include/ipc.h` + `src/ipc.c` | In-kernel pipe create/read/write stub |
| `src/include/phase9.h` | `phase9_init()` (impl in `ipc.c`) |

## Globals (`*_os`)

- `virtio_net_present_os` — `true` si PCI encontró virtio-net
- `pipes_os[]` — estático interno en `ipc.c`

## API

```c
void serial_init(void);
void serial_putc(char c);
void serial_write(const char *text);

u32 pci_init(void);                    /* bus 0, log serial+VGA */
bool pci_find_device(u16 vend, u16 dev, struct pci_device *out);

bool virtio_net_init(void);            /* status only */

i32 pipe_create(void);
ssize_t pipe_read(i32 id, void *buf, size_t len);
ssize_t pipe_write(i32 id, const void *buf, size_t len);
void pipe_close(i32 id);

void phase9_init(void);
```

## Makefile (for INT)

Listed in `agent-notes/MAKE_OBJS.md`. Suggested rules:

```make
build/serial.o: src/drivers/serial.c
	$(CC64) $(CFLAGS64) -c $< -o $@

build/pci.o: src/drivers/pci.c
	$(CC64) $(CFLAGS64) -c $< -o $@

build/virtio_net.o: src/drivers/virtio_net.c
	$(CC64) $(CFLAGS64) -c $< -o $@

build/ipc.o: src/ipc.c
	$(CC64) $(CFLAGS64) -c $< -o $@
```

Add to `KERNEL64_OBJS`: `build/serial.o build/pci.o build/virtio_net.o build/ipc.o`

QEMU tip for serial: `-serial stdio` (or `file:serial.log`).

Virtio-net in QEMU (optional, for detect): `-device virtio-net-pci`.

## `kmain64` wire-up (INT)

After memory init / before shell (VGA must work):

```c
#include "phase9.h"
/* ... */
phase9_init();
```

## Self-test

`phase9_init()` → serial → `pci_init` → `virtio_net_init` → pipe write/read `"NOS"`.

## Out of scope

- Virtio virtqueues / RX-TX / IP/ICMP
- PCI buses > 0 / MSI
- User-facing pipe FDs / syscalls (F5 can wire later)
