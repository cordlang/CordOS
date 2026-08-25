# Fase 11 — Agent F11

## Entregado

| Pieza | Ruta | Notas |
|---|---|---|
| Guía usuario | `docs/USER.md` | WSL, `make`, QEMU, FB opcional |
| Guía dev | `docs/DEV.md` | Mapa módulos, syscall/driver |
| Framebuffer stub | `src/drivers/fb.c`, `src/drivers/fb.h` (+ wrapper `src/include/fb.h`) | Tag MB2 tipo 8 → pixel; si no, mensaje |
| Roadmap | `ROADMAP.md` | Estados 4–11: solo MVP confirmado / notas agent target |
| README | enlaces a `docs/` | |
| Contrato | fila F11 → `MVP hecho (docs+fb stub)` | |

## API

```c
void fb_init(const void *mb2_addr);
bool fb_set_pixel(u32 x, u32 y, u8 r, u8 g, u8 b);
bool fb_available(void);
```

## Wire-up (orquestador / INT)

1. Añadir a `KERNEL64_OBJS`: `build/fb.o`
2. Regla sugerida:

```make
build/fb.o: src/drivers/fb.c | build
	$(CC64) $(CFLAGS64) -c $< -o $@
```

3. En `kmain64`, tras `multiboot2_print_summary` (o al final de inits):

```c
#include "fb.h"
/* ... */
fb_init(mb2_addr);
```

**No** editado Makefile ni `kernel64.c` desde F11 (anti-conflicto).

## Objeto Makefile

Ya listado en `MAKE_OBJS.md` como `# F11` → `build/fb.o`.

## Límites conocidos

- Sin framebuffer *request* en `boot64.s` / `gfxpayload`, GRUB suele no dar tag 8 → no-op esperado.
- Addr ≥ 1 GiB: no escrito (fuera del identity map de boot).
- Indexed color: detectado, sin palette → no pinta.
