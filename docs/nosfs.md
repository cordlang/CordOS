# CordOS FS — initrd (NRD1) + on-disk NosFS (NOSF)

Phase 6 has two images:

| Image | Magic | Media | Access |
|-------|-------|--------|--------|
| Initrd | `NRD1` | Kernel `.rodata` | Read-only |
| Disk volume | `NOSF` | First writable HDD/SSD (AHCI SATA or IDE) at LBA 2048..6143 | Read/write |

`phase6_init()` probes IDE (legacy ATA) and AHCI SATA. Optical/ATAPI is skipped.
If a writable disk is large enough, it mounts (or formats) the NOSF volume as
the system volume — the same idea as Windows `C:`, macOS the Macintosh HD, or
Linux `/` — and keeps initrd as a **read-only overlay**. ISO-only (no internal
disk) is a live session: settings and accounts stay in RAM.

VirtualBox is only a stand-in for a real PC: attach a SATA (AHCI) disk, not
because CordOS is a VM OS, but because that is how a real machine's SSD shows
up.

---

## Initrd format: NRD1

Minimal ramdisk archive embedded in kernel `.rodata` as `initrd_blob`.

| Offset | Size | Field |
|--------|------|--------|
| 0 | 4 | Magic `"NRD1"` |
| 4 | 4 | `file_count` (u32 LE) |
| 8 | `file_count * 40` | Directory records |
| … | … | File payloads (any order; offsets absolute from blob start) |

### Directory record (40 bytes)

| Offset | Size | Field |
|--------|------|--------|
| 0 | 32 | `name` — NUL-padded ASCII basename |
| 32 | 4 | `offset` — byte offset of payload from start of blob |
| 36 | 4 | `size` — payload length in bytes |

### Embedded contents

| Name | Content |
|------|---------|
| `hello.txt` | `Hello from CordOS initrd!\n` |
| `motd` | `Welcome to CordOS\n` |

---

## On-disk format: NOSF

Reserved so BIOS `make disk` (MBR LBA 0, stage2 LBA 1–64, kernel LBA 65+) is
never overwritten.

| Absolute LBA | Relative | Contents | Size |
|--------------|----------|----------|------|
| 0–2047 | — | Untouched (MBR / boot / kernel / gap) | 1 MiB |
| **2048** | 0 | Superblock, magic `"NOSF"` | 512 B |
| 2049–2056 | 1–8 | Directory (64 × 64 B) | 4 KiB |
| 2057–6143 | 9–4095 | File payloads (bump allocator) | ~2 MiB |

Volume length is fixed at **4096 sectors (2 MiB)** starting at **LBA 2048**.
The backing image should be larger (default `out/persist.img` is **16 MiB**).

### Superblock (512 bytes)

| Offset | Size | Field |
|--------|------|--------|
| 0 | 4 | Magic `"NOSF"` |
| 4 | 4 | `version` (1) |
| 8 | 4 | `vol_lba` (2048) |
| 12 | 4 | `vol_sectors` (4096) |
| 16 | 4 | `file_count` |
| 20 | 4 | `max_files` (64) |
| 24 | 4 | `dir_lba` (1, relative) |
| 28 | 4 | `dir_sectors` (8) |
| 32 | 4 | `data_lba` (9, relative) |
| 36 | 4 | `next_data` — next free relative LBA |
| 40 | 472 | reserved |

### Directory entry (64 bytes)

| Offset | Size | Field |
|--------|------|--------|
| 0 | 32 | `name` — NUL-padded basename |
| 32 | 4 | `size` (bytes) |
| 36 | 4 | `data_lba` (relative to volume start) |
| 40 | 4 | `data_sectors` |
| 44 | 4 | `flags` (bit 0 = in use) |
| 48 | 16 | pad |

Empty disk (no `"NOSF"` magic at LBA 2048) is formatted on first mount.

Seed files written by the kernel:

| Name | Role |
|------|------|
| `p6test` | Write-then-remount self-test payload |
| `config.txt` | Persist keys (`lang`, `login_wp`, `icon_style`) |

---

## VFS

Single mount at `/` (overlay when disk is up):

- `vfs_open(path)` → fd (`hello.txt` or `/hello.txt`); disk first, then initrd
- `vfs_read` / `vfs_close`
- `vfs_write` — disk only; write at pos 0 replaces the whole file
- `vfs_create(path)` — empty file on disk
- `vfs_ls("/")` / `vfs_list` — union, disk names hide initrd duplicates

`phase6_init()`: ATA + AHCI probe → NOSF on the first usable disk (or initrd) → `persist_init()`.

Serial: `phase6: root=disk (initrd overlay RO)` or `phase6: root=initrd`.

---

## Persist API

```c
void persist_init(void);
bool persist_available(void);
int persist_set_u32(const char *key, u32 value);
int persist_get_u32(const char *key, u32 *value);
```

Keys: `lang`, `login_wp`, `icon_style`. Stored as text in `config.txt`.
No-ops when `persist_available()` is false (live ISO, no internal disk).

---

## Attach the volume

### QEMU

```bash
make ARCH=x86_64 out/persist.img
make ARCH=x86_64 run-persist
# equivalent:
qemu-system-x86_64 -cdrom out/cordos.iso \
  -drive file=out/persist.img,format=raw,if=ide \
  -boot order=d -vga std -serial stdio
```

ISO stays the boot medium (`-boot order=d`). The raw file is an IDE HDD.
ATA still probes **0x1F0 / 0x170**. For a closer match to a real PC, QEMU can
also expose AHCI (`-device ahci,id=ahci -drive ... if=none -device ide-hd,bus=ahci.0,...`).

`make run` is still ISO-only (initrd fallback).

### VirtualBox

ISO remains the CD boot disk. The writable volume is a **SATA AHCI** disk —
the same class of controller as the internal SSD/HDD on a real PC. `run-vbox.ps1`
creates `out/persist.vdi` if needed and attaches it to SATA port 0.

Manual attach:

```text
VBoxManage storagectl <VM> --name SATA --add sata --controller IntelAhci --portcount 2 --bootable off
VBoxManage convertfromraw out/persist.img persist.vdi --format VDI
VBoxManage storageattach <VM> --storagectl SATA --port 0 --device 0 \
  --type hdd --medium persist.vdi
```

Legacy IDE still works if a HDD is on 0x1F0/0x170. Optical (the ISO) is skipped.

Without a writable disk, boot is a live session (initrd only).
