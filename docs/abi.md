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
buffers and `open` paths are range-checked, then copied through a kernel bounce
buffer (`SYS_COPY_MAX` = 256 bytes per chunk; paths capped at `SYS_PATH_MAX` =
256 including NUL).

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

## Numbers 0–7

| # | Name | Prototype | Notes |
|---|------|-----------|--------|
| 0 | `exit` | `exit(code)` — `rdi` = code | If `task_current()` is non-NULL, `task_exit()` (TASK_DEAD + schedule). Else halt (`cli; hlt`). |
| 1 | `write` | `write(fd, buf, len)` | `fd=1` → VGA UTF-8 **and** serial. Other fds → `-1`. Bad pointer → `-1`. |
| 2 | `read` | `read(fd, buf, len)` | `fd=0` → keyboard UTF-8, blocking `hlt`. Else `-1`. Bad pointer → `-1`. |
| 3 | `yield` | `yield()` | `task_yield()` (weak no-op if F4 not linked) |
| 4 | `getpid` | `getpid()` | `task_current()->pid` or `0` |
| 5 | `mmap` | `mmap(hint, len, prot)` | Anonymous user pages, `PAGE_WRITE`. `hint=0` bump-allocates from `0x41000000`. `len` rounded to 4 KiB, max 16 MiB. Returns VA or `-1`. `prot` is accepted and currently ignored (always writable). |
| 6 | `open` | `open(path)` | Copies a NUL-terminated path, then `vfs_open`. `-1` if bad pointer, unterminated path, missing file, or vfs not mounted. |
| 7 | `close` | `close(fd)` | `vfs_close(fd)`. `-1` if not mounted / bad fd. |

Console fds `0` (keyboard) and `1` (VGA) are not the vfs table. `SYS_OPEN`
returns a raw vfs fd (`0`–`7`); `SYS_READ`/`SYS_WRITE` do not multiplex those
yet.

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
