# CordOS FS — initrd (NRD1) + on-disk NosFS (NOSF)

Phase 6 has two images:

| Image | Magic | Media | Access |
|-------|-------|--------|--------|
| Initrd | `NRD1` | Kernel `.rodata` | Read-only |
| Disk volume | `NOSF` | ATA LBA 2048..6143 | Read/write |

`phase6_init()` probes ATA. If a HDD is present and large enough, it mounts
(or formats) the NOSF volume as `/` and keeps initrd as a **read-only overlay**
(disk names shadow initrd). If there is no disk (ISO-only VM), `/` is the
initrd exactly as before.

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

`phase6_init()` (splash stage 2): ATA probe → NOSF mount or initrd → `persist_init()`.

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
No-ops when `persist_available()` is false (ISO-only).

---

## Attach the volume

### QEMU

```bash
make ARCH=x86_64 out/persist.img
make ARCH=x86_64 run-persist
# equivalent:
qemu-system-x86_64 -cdrom out/nuevoos64.iso \
  -drive file=out/persist.img,format=raw,if=ide \
  -boot order=d -vga std -serial stdio
```

ISO stays the boot medium (`-boot order=d`). The raw file is an IDE HDD.
ATA probes **0x1F0 master/slave** then **0x170 master/slave** so either
primary-master (typical QEMU) or a slave/secondary disk works.

`make run` is still ISO-only (initrd fallback).

### VirtualBox

ISO remains the CD boot disk. Attach a second **IDE** disk:

1. `make ARCH=x86_64 out/persist.img` (or `scripts/mkpersist.sh`).
2. Convert if you want VDI:

   ```text
   VBoxManage convertfromraw out/persist.img persist.vdi --format VDI
   ```

3. Attach as IDE (not SATA/NVMe — this driver is ATA PIO):

   ```text
   VBoxManage storageattach <VM> --storagectl "IDE" --port 1 --device 0 \
     --type hdd --medium persist.vdi
   ```

   Typical layout: CD = primary master (ATAPI, skipped), HDD = primary slave
   or secondary master. Both are probed.

Without a HDD, boot is unchanged (initrd).
