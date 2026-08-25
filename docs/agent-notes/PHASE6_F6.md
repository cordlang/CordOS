# Agent F6 — Fase 6 FS / initrd MVP

**Estado:** listo MVP (sin commit)  
**ATA PIO:** omitido (opcional; criterio mínimo = initrd)

## Archivos

| Archivo | Rol |
|---|---|
| `docs/nosfs.md` | Formato NRD1 + API VFS/nosfs |
| `src/include/initrd.h` | `initrd_blob` / `initrd_blob_size` |
| `src/include/nosfs.h` | mount / lookup / list |
| `src/include/vfs.h` | `vfs_open/read/close/ls` + `phase6_init` |
| `src/fs/initrd.c` | Blob NRD1 embebido (`hello.txt`, `motd`) |
| `src/fs/nosfs.c` | Parser NRD1 |
| `src/fs/vfs.c` | VFS + mount en `/` |
| `src/kernel64.c` | `phase6_init()` antes de `shell_run()` |

## Formato initrd (NRD1)

Magic `"NRD1"`, `u32 file_count`, records `(name[32], offset, size)`, luego payloads.  
Contenido MVP:

- `hello.txt` → `Hello from NuevoOS initrd!\n`
- `motd` → `Welcome to NuevoOS\n`

## API shell (compatible F7)

```c
int vfs_open(const char *path);
ssize_t vfs_read(int fd, void *buf, size_t len);
int vfs_close(int fd);
int vfs_ls(const char *path);  /* imprime basenames; solo "/" */
void phase6_init(void);
```

`ls` / `cat hello.txt` / `cat /motd` deben funcionar una vez linkeados los objs.

## Makefile (INT)

En `agent-notes/MAKE_OBJS.md`:

```
build/vfs.o      # src/fs/vfs.c
build/nosfs.o    # src/fs/nosfs.c
build/initrd.o   # src/fs/initrd.c
```

Añadir a `KERNEL64_OBJS` + reglas `-c` con `$(CFLAGS64)`.

## Wire `kmain64`

Ya aplicado:

```c
phase4_init();
phase6_init();   /* F6 */
update_status_line();
shell_run();     /* blocking — último */
```

## Notas

- Un solo mount `/`; sin subdirectorios.
- Magic de contrato “NOSF” → MVP usa archivo NRD1 documentado en `docs/nosfs.md`.
- Sin Multiboot2 module path aún (solo `.rodata`).

---

# Agent F6 — writable disk (ATA PIO + NOSF)

**Estado:** ATA + NosFS en disco; initrd sigue como fallback  
**ATA PIO:** implementado (no cuelga el boot)

## Qué se añadió

| Archivo | Rol |
|---|---|
| `src/drivers/ata.c` + `src/include/drivers/ata.h` | PIO LBA28, probe 0x1F0/0x170 master+slave |
| `src/fs/nosfs_disk.c` | Superblock `NOSF` @ LBA 2048, 4096 sectores |
| `src/fs/persist.c` + `src/include/fs/persist.h` | `lang` / `login_wp` / `icon_style` → `config.txt` |
| `src/fs/vfs.c` | overlay disco+initrd; `vfs_write` / `vfs_create` / `vfs_list` |
| `out/persist.img` | 16 MiB raw; `make run-persist` |
| `scripts/mkpersist.sh` | helper para el raw disk |

Initrd NRD1 **no se toca** (`hello.txt`, `motd`). Sin HDD → `phase6: root=initrd`.

## LBA layout (NOSF)

```
LBA 0–2047     reservado (MBR/stage2/kernel si make disk)
LBA 2048       superblock "NOSF"
LBA 2049–2056  directorio (64 entradas × 64 B)
LBA 2057–6143  datos (bump)
```

Formato automático si el magic falta. Preferencia: disco que ya tenga `NOSF`.

## Overlay

Disco es `/` writable. Initrd se lista y se abre en RO si el nombre no existe en disco.
`vfs_write` solo al disco (pos 0 = replace). Self-test: escribe `p6test`, `nosfs_disk_reload()`, lee.

## Persist API

```c
void persist_init(void);          /* desde phase6_init */
bool persist_available(void);
int persist_set_u32(const char *key, u32 value);
int persist_get_u32(const char *key, u32 *value);
```

Hooks mínimos: `wallpaper_set_login` / `icon_set_style` (draw.c) y lang en Settings.

## QEMU / VirtualBox

```bash
make ARCH=x86_64 out/nuevoos64.bin
make ARCH=x86_64 out/persist.img
qemu-system-x86_64 -cdrom out/nuevoos64.iso \
  -drive file=out/persist.img,format=raw,if=ide -boot order=d
```

VirtualBox: ISO = CD de arranque; adjuntar VDI/raw en **IDE** (port 1 / slave).
ATAPI se salta; se prueban master/slave primary y secondary.

## Serial esperado

```
ata: probe 0x1F0/0x170
ata: pri-master ATA LBA=32768   (o ATAPI skip / no HDD)
phase6: disk writeback OK
phase6: root=disk (initrd overlay RO)
persist: ready (config.txt)
```

Sin disco:

```
ata: no HDD
phase6: root=initrd
persist: no disk
```

