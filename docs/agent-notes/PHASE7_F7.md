# Agent F7 — Fase 7 shell MVP

**Estado:** MVP listo  
**Alcance:** shell kernel-mode interactivo + builtins; VFS opcional vía weak / `__has_include`.

## Archivos

| Archivo | Rol |
|---|---|
| `src/shell.c` | Loop teclado, parseo, builtins |
| `src/include/shell.h` | `void shell_run(void);` |
| `src/kernel64.c` | `shell_echo_loop` → `shell_run()` |
| `Makefile` | `build/shell.o` en `KERNEL64_OBJS` (necesario para link) |

`user/shell/` **no** creado (userland cuando F5 esté listo).

## Builtins

| Cmd | Comportamiento |
|---|---|
| `help` | Lista comandos |
| `echo` | Imprime resto de línea |
| `clear` | `vga_clear` + status |
| `ls` | `vfs_ls("/")` si hay VFS; si no `(vfs pending)` |
| `cat <file>` | `vfs_open` / `read` / `close`; si no VFS `(vfs pending)` |
| `ticks` | `ticks_os`, `hz_os`, `uptime_ms` |
| `mem` | `heap_used_os` / `heap_free_os` + frames PMM |

## VFS (resiliente a F6 mid-flight)

1. Si existe `vfs.h` → `#include` (`__has_include`).
2. Si no → declaraciones **weak** de `vfs_open` / `vfs_read` / `vfs_close`.
3. `vfs_ls(const char *path)` siempre weak (API extra para listar).
4. Sin símbolos VFS: `ls`/`cat` imprimen `(vfs pending)`.

API esperada (alineada al contrato):

```c
int vfs_open(const char *path);
ssize_t vfs_read(int fd, void *buf, size_t len);
int vfs_close(int fd);
int vfs_ls(const char *path); /* opcional; <0 = fallo */
```

## Makefile

```
build/shell.o   # src/shell.c
```

También listado en `agent-notes/MAKE_OBJS.md` (# F7).

## Criterio MVP

- Prompt `>` responde a Enter con comandos anteriores.
- Barra inferior actualiza ticks.
- Sin F6: `ls` → `(vfs pending)`.
