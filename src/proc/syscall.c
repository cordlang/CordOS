#include "syscall.h"
#include "idt.h"
#include "gdt.h"
#include "vga.h"
#include "keyboard.h"
#include "utf8.h"
#include "task.h"
#include "vfs.h"
#include "string.h"

extern void syscall_entry(void);

#define SYS_COPY_MAX 256u
#define SYS_PATH_MAX 256u

/* Soft deps on F4 — strong symbols override when task.o is linked. */
__attribute__((weak)) void task_yield(void)
{
}

__attribute__((weak)) struct task *task_current(void)
{
    return NULL;
}

__attribute__((weak)) void task_exit(void)
{
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

static char sys_read_pending[4];
static u32 sys_read_pending_length;
static u32 sys_read_pending_position;

static int user_range_ok(u64 addr, u64 len)
{
    if (addr == 0) {
        return 0;
    }
    /* Kernel canonical high-half (and non-canonical sign-extended). */
    if ((addr & (1ull << 63)) != 0) {
        return 0;
    }
    if (addr >= USER_IDENTITY_END) {
        return 0;
    }
    if (len > USER_IDENTITY_END - addr) {
        return 0;
    }
    return 1;
}

static int copy_from_user(void *dst, u64 src, u64 len)
{
    if (len == 0) {
        return 0;
    }
    if (!user_range_ok(src, len)) {
        return -1;
    }
    memcpy(dst, (const void *)src, (size_t)len);
    return 0;
}

static int copy_to_user(u64 dst, const void *src, u64 len)
{
    if (len == 0) {
        return 0;
    }
    if (!user_range_ok(dst, len)) {
        return -1;
    }
    memcpy((void *)dst, src, (size_t)len);
    return 0;
}

static int copy_user_path(char *dst, u64 src, u64 max)
{
    u64 i;

    if (max == 0) {
        return -1;
    }

    for (i = 0; i < max; i++) {
        if (!user_range_ok(src + i, 1)) {
            return -1;
        }
        dst[i] = ((const char *)src)[i];
        if (dst[i] == '\0') {
            return 0;
        }
    }
    return -1;
}

static i64 sys_exit(u64 code)
{
    struct task *t = task_current();

    (void)code;
    if (t != NULL) {
        task_exit();
        return 0;
    }

    vga_print("\n[sys_exit] halt\n");
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

static i64 sys_write(u64 fd, u64 buf_u, u64 len)
{
    char kbuf[SYS_COPY_MAX];
    u64 done;

    if (fd != 1) {
        /* vfs_write is Phase 6; extra fds are not writable here. */
        return -1;
    }
    if (!user_range_ok(buf_u, len)) {
        return -1;
    }

    done = 0;
    while (done < len) {
        u64 chunk = len - done;

        if (chunk > SYS_COPY_MAX) {
            chunk = SYS_COPY_MAX;
        }
        if (copy_from_user(kbuf, buf_u + done, chunk) < 0) {
            return -1;
        }
        vga_write_utf8(kbuf, (size_t)chunk);
        done += chunk;
    }

    return (i64)len;
}

static char sys_read_one(void)
{
    if (sys_read_pending_position >= sys_read_pending_length) {
        u32 codepoint;

        while (!keyboard_has_char()) {
            __asm__ volatile ("hlt");
        }

        codepoint = keyboard_get_codepoint();
        sys_read_pending_length = utf8_encode(codepoint, sys_read_pending);
        if (sys_read_pending_length == 0) {
            sys_read_pending[0] = '?';
            sys_read_pending_length = 1;
        }
        sys_read_pending_position = 0;
    }

    return sys_read_pending[sys_read_pending_position++];
}

static i64 sys_read(u64 fd, u64 buf_u, u64 len)
{
    char kbuf[SYS_COPY_MAX];
    u64 done;

    if (fd != 0) {
        return -1;
    }
    if (!user_range_ok(buf_u, len)) {
        return -1;
    }

    done = 0;
    while (done < len) {
        u64 chunk = len - done;
        u64 i;

        if (chunk > SYS_COPY_MAX) {
            chunk = SYS_COPY_MAX;
        }
        for (i = 0; i < chunk; i++) {
            kbuf[i] = sys_read_one();
        }
        if (copy_to_user(buf_u + done, kbuf, chunk) < 0) {
            return -1;
        }
        done += chunk;
    }

    return (i64)done;
}

static i64 sys_yield(void)
{
    task_yield();
    return 0;
}

static i64 sys_getpid(void)
{
    struct task *t = task_current();
    return t != NULL ? (i64)t->pid : 0;
}

static i64 sys_mmap(u64 hint, u64 len, u64 prot)
{
    (void)prot;

    if (len == 0) {
        return -1;
    }
    if (hint != 0 && !user_range_ok(hint, len)) {
        return -1;
    }
    if (hint == 0 && len > USER_IDENTITY_END) {
        return -1;
    }

    /* Stub — Phase 4 maps user images; no allocator here. */
    return -1;
}

static i64 sys_open(u64 path_u)
{
    char kpath[SYS_PATH_MAX];

    if (copy_user_path(kpath, path_u, SYS_PATH_MAX) < 0) {
        return -1;
    }

    /* vfs_open returns -1 if not mounted / missing path. */
    return (i64)vfs_open(kpath);
}

static i64 sys_close(u64 fd)
{
    return (i64)vfs_close((int)fd);
}

i64 syscall_dispatch(u64 num, u64 a0, u64 a1, u64 a2)
{
    switch (num) {
    case SYS_EXIT:
        return sys_exit(a0);
    case SYS_WRITE:
        return sys_write(a0, a1, a2);
    case SYS_READ:
        return sys_read(a0, a1, a2);
    case SYS_YIELD:
        return sys_yield();
    case SYS_GETPID:
        return sys_getpid();
    case SYS_MMAP:
        return sys_mmap(a0, a1, a2);
    case SYS_OPEN:
        return sys_open(a0);
    case SYS_CLOSE:
        return sys_close(a0);
    default:
        return -1;
    }
}

void syscall_interrupt(struct interrupt_frame *frame)
{
    i64 result;

    result = syscall_dispatch(frame->rax, frame->rdi, frame->rsi, frame->rdx);
    frame->rax = (u64)result;
}

void syscall_init(void)
{
    /* 0xEE = P=1 DPL=3 interrupt gate. Ring-3 int $0x80 is allowed. */
    idt_set_gate(0x80, (u64)syscall_entry, GDT_KERNEL_CODE, 0xEE);
}

void phase5_init(void)
{
    syscall_init();
}
