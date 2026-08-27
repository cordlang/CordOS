# CordOS — System call ABI (x86_64)

## Entry

| Item | Value |
|------|--------|
| Mechanism | Software interrupt `int 0x80` |
| IDT vector | `0x80` |
| Gate type | Interrupt gate (type field `0xE`) |
| DPL | **3** — ring-3 `int $0x80` is allowed (`idt_set_gate` flags `0xEE`) |
| Kernel CS | `GDT_KERNEL_CODE` (`0x08`) |

`syscall_init()` / `phase5_init()` install the gate after `isr_install()`. Kernel
code may still call `syscall_dispatch()` directly (no `int`).

Long-mode interrupt frames always include `SS`/`RSP`; `syscall_entry` matches
the `interrupt_frame` layout used by `isr64.s` and returns with `iretq`.

## Registers

| Register | Role |
|----------|------|
| `rax` | Syscall number (in); return value (out) |
| `rdi` | Arg 0 |
| `rsi` | Arg 1 |
| `rdx` | Arg 2 |
| `rcx`, `r8`–`r11` | Unused by this ABI (clobbered by the interrupt path) |

No stack arguments.

## Error model

Negative returns are errors. MVP uses **`-1`** for failure / unsupported /
bad pointer / unknown number. There is no errno object.

`len == 0` on `write`/`read` with a valid buffer returns `0`.

## User pointer validation

Handlers never operate on a user pointer in place. `write` / `read` / `mmap`
buffers and `open` / `spawn` / `exec` paths are range-checked, then copied
through a kernel bounce buffer (`SYS_COPY_MAX` = 256 bytes per chunk; VFS
`write` copies the whole buffer up to `SYS_VFS_IO_MAX` = 64 KiB; paths capped
at `SYS_PATH_MAX` = 256 including NUL).

A pointer range `[addr, addr+len)` is accepted only if **all** of:

1. `addr != NULL` (`0` is rejected; `mmap` hint `0` means “kernel picks the VA”)
2. Bit 63 is clear (not a kernel canonical high address; FB lives at
   `0xFFFF800000100000`)
3. No wrap; `len` ≤ 16 MiB
4. **Every page is `PAGE_USER`** at the leaf (and user bit on the walk).
   Identity RAM without `U/S` is **not** a valid user buffer.

User images live at `0x40000000` (see `user/hello.ld`). `mmap` anonymous
mappings start at `0x41000000`. Kernel `.rodata` is no longer a valid
`syscall_dispatch` buffer.

## Numbers 0–9

| # | Name | Prototype | Notes |
|---|------|-----------|--------|
| 0 | `exit` | `exit(code)` — `rdi` = code | If `task_current()` is non-NULL, `task_exit()` (TASK_DEAD + schedule). Else halt (`cli; hlt`). |
| 1 | `write` | `write(fd, buf, len)` | `fd=1` → VGA UTF-8 **and** serial. `fd>=2` → `vfs_write` (whole-file replace at pos 0, max 64 KiB). Else `-1`. Bad pointer → `-1`. |
| 2 | `read` | `read(fd, buf, len)` | `fd=0` → keyboard UTF-8, blocking `hlt`. `fd>=2` → `vfs_read` (short read / EOF OK). Else `-1`. Bad pointer → `-1`. |
| 3 | `yield` | `yield()` | `task_yield()` (weak no-op if F4 not linked) |
| 4 | `getpid` | `getpid()` | `task_current()->pid` or `0` |
| 5 | `mmap` | `mmap(hint, len, prot)` | Anonymous user pages. `PROT_WRITE` → `PAGE_WRITE`; without it the leaf is read-only. `PROT_WRITE|PROT_EXEC` is `-1` (W^X). `hint=0` bump-allocates from `0x41000000`. `len` rounded to 4 KiB, max 16 MiB. Returns VA or `-1`. NX (EFER.NXE) still open. |
| 6 | `open` | `open(path)` | Copies a NUL-terminated path, then `vfs_open`. Returns a vfs fd **≥ 2**. `-1` if bad pointer, unterminated path, missing file, or vfs not mounted. |
| 7 | `close` | `close(fd)` | `vfs_close(fd)` for `fd>=2`. `-1` for console fds 0/1, not mounted, or bad fd. |
| 8 | `spawn` | `spawn(path)` | Load ELF64 `ET_EXEC` x86_64 via `elf64_load`, create a task, `iretq` into it. `path=NULL` / empty / `"hello"` (optional leading `/`) uses the embedded `user_hello.elf` blob. Else the file is read from VFS. Returns **pid** or `-1`. Shared user window (`0x40000000`): a second live ring-3 image is `-1` (`spawn: user window busy`). |
| 9 | `exec` | `exec(path)` | Same loader as `spawn`, but replaces the **current** task: `int 0x80` returns into the new `RIP`/`RSP` (CS/SS = user). Kernel `syscall_dispatch` (no interrupt frame) returns `-1`. Path convention matches `spawn`. |

Console fds `0` (keyboard) and `1` (VGA/serial) are not the vfs table.
`SYS_OPEN` allocates from fd `2` so those numbers never collide. A CPL3 page
fault still kills only that task (`page_fault.c`); there is no per-process CR3
yet.

## Limits (this is the ABI as implemented)

Canonical open work: [`ROADMAP.md`](ROADMAP.md). Do not read this table as
“userland / mmap done”.

- `spawn`/`exec` exist (8/9). One live ring-3 image at `0x40000000`; no per-process CR3. Dock Terminal is still ring 0.
- `mmap` leaf `prot` is honored (RO vs W; W+X is `-1`). NX (EFER.NXE) and ELF `PT_LOAD` flags are still open (Fase 17).
- Login password hashing is not part of this ABI. It is FNV-1a + pepper
  (`src/fs/userdb.c`), not a KDF — before “usuarios reales”.

## Kernel C API

```c
i64 syscall_dispatch(u64 num, u64 a0, u64 a1, u64 a2);
void syscall_init(void);   /* idt_set_gate(0x80, syscall_entry, GDT_KERNEL_CODE, 0xEE) */
void phase5_init(void);    /* → syscall_init() */
```

`syscall_entry` (asm) builds an `interrupt_frame`-compatible stack and calls
`syscall_interrupt()`, which reads `rax`/`rdi`/`rsi`/`rdx` and writes the
result back into `frame->rax` before `iretq`.

## Userland (`user/libnos`)

Thin wrappers set `rax` + args and execute `int $0x80`. **Not** linked into the
kernel.

```text
make ARCH=x86_64 userland     # → out/user_hello.elf (host-side freestanding blob)
```

Sources: `user/libnos/syscall.S`, `user/libnos/syscall.h`, `user/test_write.c`.

## Init order

After `isr_install()` (and preferably after `keyboard_init()` for `read`):

```
gdt_init → idt_init → isr_install → … → phase5_init() → …
```

`kmain64` already calls `phase5_init()` immediately after `init_interrupts()`.
Objects: `out/syscall.o`, `out/syscall_entry.o`.
