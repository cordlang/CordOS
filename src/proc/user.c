#include "user.h"
#include "elf64.h"
#include "gdt.h"
#include "heap.h"
#include "isr.h"
#include "pmm.h"
#include "serial.h"
#include "string.h"
#include "task.h"
#include "vfs.h"
#include "vmm.h"

extern const u8 user_hello_elf_start[];
extern const u8 user_hello_elf_end[];

#define USER_MSG_OFF 0x200u
#define USER_CPL_OFF 0x800u

static const char user_hello[] = "r3\n";

static u64 user_text_va;
static u64 user_stack_va;
static u32 user_smoke_pid;

static int path_eq(const char *a, const char *b)
{
    if (a == NULL || b == NULL) {
        return 0;
    }
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static int path_is_blob(const char *path)
{
    if (path == NULL || path[0] == '\0') {
        return 1;
    }
    if (path[0] == '/') {
        path++;
    }
    return path_eq(path, "hello");
}

static const char *path_task_name(const char *path)
{
    if (path_is_blob(path)) {
        return "hello";
    }
    return "elf";
}

static int user_window_busy(u32 except_pid)
{
    u32 i;

    for (i = 1; i < TASK_MAX_OS; i++) {
        if (task_table_os[i].state == TASK_DEAD) {
            continue;
        }
        if (task_table_os[i].pid == except_pid) {
            continue;
        }
        if (task_table_os[i].user_rip != 0) {
            return 1;
        }
    }
    return 0;
}

static int user_load_blob(const void *blob, u32 size, u64 *entry, u64 *stack)
{
    if (blob == NULL || size == 0 || entry == NULL || stack == NULL) {
        return -1;
    }
    return elf64_load(blob, size, entry, stack);
}

static int user_load_vfs(const char *path, u64 *entry, u64 *stack)
{
    int fd;
    u32 size;
    u8 *buf;
    ssize_t got;
    ssize_t n;
    int rc;

    fd = vfs_open(path);
    if (fd < 0) {
        return -1;
    }
    if (vfs_size(fd, &size) < 0 || size == 0 || size > ELF_LOAD_MAX) {
        vfs_close(fd);
        return -1;
    }
    buf = (u8 *)kmalloc(size);
    if (buf == NULL) {
        vfs_close(fd);
        return -1;
    }
    got = 0;
    while ((u32)got < size) {
        n = vfs_read(fd, buf + got, size - (u32)got);
        if (n < 0) {
            kfree(buf);
            vfs_close(fd);
            return -1;
        }
        if (n == 0) {
            break;
        }
        got += n;
    }
    vfs_close(fd);
    rc = user_load_blob(buf, (u32)got, entry, stack);
    kfree(buf);
    return rc;
}

static int user_load_path(const char *path, u64 *entry, u64 *stack)
{
    if (path_is_blob(path)) {
        u32 elf_size;

        elf_size = (u32)(user_hello_elf_end - user_hello_elf_start);
        return user_load_blob(user_hello_elf_start, elf_size, entry, stack);
    }
    return user_load_vfs(path, entry, stack);
}

static void user_spawn_task(void)
{
    struct task *t = task_current();

    if (t == NULL || t->user_rip == 0) {
        task_exit();
        return;
    }
    user_enter(t->user_rip, t->user_rsp);
    task_exit();
}

u32 user_spawn_elf(const void *blob, u32 size, const char *name)
{
    u64 entry;
    u64 stack;

    if (user_window_busy(0)) {
        serial_write("spawn: user window busy\n");
        return 0;
    }
    if (user_load_blob(blob, size, &entry, &stack) != 0) {
        return 0;
    }
    return task_create_user(user_spawn_task, name != NULL ? name : "elf",
                            entry, stack);
}

u32 user_spawn_path(const char *path)
{
    u64 entry;
    u64 stack;

    if (user_window_busy(0)) {
        serial_write("spawn: user window busy\n");
        return 0;
    }
    if (user_load_path(path, &entry, &stack) != 0) {
        return 0;
    }
    return task_create_user(user_spawn_task, path_task_name(path),
                            entry, stack);
}

int user_exec_path(const char *path, u64 *entry_out, u64 *stack_out)
{
    struct task *t = task_current();
    u32 except = (t != NULL) ? t->pid : 0;
    u64 entry;
    u64 stack;

    if (user_window_busy(except)) {
        serial_write("exec: user window busy\n");
        return -1;
    }
    if (user_load_path(path, &entry, &stack) != 0) {
        return -1;
    }
    if (t != NULL) {
        t->user_rip = entry;
        t->user_rsp = stack;
    }
    if (entry_out != NULL) {
        *entry_out = entry;
    }
    if (stack_out != NULL) {
        *stack_out = stack;
    }
    return 0;
}

static u8 *emit8(u8 *p, u8 b)
{
    *p++ = b;
    return p;
}

static u8 *emit32(u8 *p, u32 v)
{
    p[0] = (u8)(v);
    p[1] = (u8)(v >> 8);
    p[2] = (u8)(v >> 16);
    p[3] = (u8)(v >> 24);
    return p + 4;
}

static u8 *emit64(u8 *p, u64 v)
{
    u32 i;

    for (i = 0; i < 8; i++) {
        p[i] = (u8)(v >> (8 * i));
    }
    return p + 8;
}

/*
 * Build a tiny CPL3 snippet:
 *   store CS at text+USER_CPL_OFF
 *   int 0x80 SYS_WRITE(1, msg, len)
 *   int 0x80 SYS_EXIT(0)
 */
static void user_build_trampoline(u8 *page, u64 text_va)
{
    u8 *p = page;
    u64 cpl_addr = text_va + USER_CPL_OFF;
    u64 msg_addr = text_va + USER_MSG_OFF;
    u32 hello_len = (u32)(sizeof(user_hello) - 1u);

    p = emit8(p, 0x31); /* xor %eax,%eax */
    p = emit8(p, 0xC0);
    p = emit8(p, 0x66); /* mov %cs,%ax */
    p = emit8(p, 0x8C);
    p = emit8(p, 0xC8);
    p = emit8(p, 0x48); /* movabs $cpl_addr,%rbx */
    p = emit8(p, 0xBB);
    p = emit64(p, cpl_addr);
    p = emit8(p, 0x48); /* mov %rax,(%rbx) */
    p = emit8(p, 0x89);
    p = emit8(p, 0x03);

    p = emit8(p, 0xB8); /* mov $1,%eax   SYS_WRITE */
    p = emit32(p, 1);
    p = emit8(p, 0xBF); /* mov $1,%edi   fd=1 */
    p = emit32(p, 1);
    p = emit8(p, 0x48); /* movabs $msg,%rsi */
    p = emit8(p, 0xBE);
    p = emit64(p, msg_addr);
    p = emit8(p, 0xBA); /* mov $len,%edx */
    p = emit32(p, hello_len);
    p = emit8(p, 0xCD); /* int $0x80 */
    p = emit8(p, 0x80);

    p = emit8(p, 0x31); /* xor %edi,%edi */
    p = emit8(p, 0xFF);
    p = emit8(p, 0x31); /* xor %eax,%eax  SYS_EXIT */
    p = emit8(p, 0xC0);
    p = emit8(p, 0xCD); /* int $0x80 */
    p = emit8(p, 0x80);
    p = emit8(p, 0xF4); /* hlt */
    p = emit8(p, 0xEB); /* jmp .-1 */
    p = emit8(p, 0xFD);

    memcpy(page + USER_MSG_OFF, user_hello, hello_len);
}

static u64 user_pick_va(u64 preferred, u64 phys)
{
    if (vmm_get_physical(preferred) == 0) {
        return preferred;
    }
    return phys;
}

static bool user_map_pages(void)
{
    void *text_phys;
    void *stack_phys;
    u32 flags = PAGE_PRESENT | PAGE_WRITE | PAGE_USER;

    text_phys = pmm_alloc_page();
    stack_phys = pmm_alloc_page();
    if (text_phys == NULL || stack_phys == NULL) {
        return false;
    }

    user_text_va = user_pick_va(USER_TEXT_BASE, (u64)text_phys);
    user_stack_va = user_pick_va(USER_STACK_BASE, (u64)stack_phys);

    vmm_map_page(user_text_va, (u64)text_phys, flags);
    vmm_map_page(user_stack_va, (u64)stack_phys, flags);

    memset((void *)user_text_va, 0, PAGE_SIZE);
    memset((void *)user_stack_va, 0, PAGE_SIZE);
    user_build_trampoline((u8 *)user_text_va, user_text_va);
    return true;
}

__attribute__((noinline)) void user_enter(u64 rip, u64 rsp)
{
    u64 rflags = 0x202; /* reserved bit 1 | IF */
    u64 user_cs = GDT_USER_CODE;
    u64 user_ss = GDT_USER_DATA;
    struct task *t = task_current();

    if (t != NULL && t->kstack_base != NULL) {
        gdt_set_rsp0(((u64)t->kstack_base + TASK_STACK_SIZE) & ~0xFULL);
    } else {
        gdt_set_rsp0(gdt_idle_rsp0());
    }

    interrupts_disable();

    __asm__ volatile (
        "pushq %[ss]\n\t"
        "pushq %[usrsp]\n\t"
        "pushq %[rflags]\n\t"
        "pushq %[cs]\n\t"
        "pushq %[urip]\n\t"
        "iretq\n\t"
        :
        : [ss] "r"(user_ss),
          [usrsp] "r"(rsp),
          [rflags] "r"(rflags),
          [cs] "r"(user_cs),
          [urip] "r"(rip)
        : "memory"
    );

    __builtin_unreachable();
}

static void user_smoke_task(void)
{
    user_enter(user_text_va, user_stack_va + PAGE_SIZE);
    task_exit();
}

static bool user_pid_dead(u32 pid)
{
    u32 i;

    for (i = 1; i < TASK_MAX_OS; i++) {
        if (task_table_os[i].pid == pid &&
            task_table_os[i].state != TASK_DEAD) {
            return false;
        }
    }
    return true;
}

void user_smoke(void)
{
    u16 cs;

    user_smoke_pid = user_spawn_path(NULL);
    if (user_smoke_pid != 0) {
        serial_write("phase13: entering elf user\n");
        while (!user_pid_dead(user_smoke_pid)) {
            task_yield();
        }
        serial_write("phase13: elf exited\n");
        return;
    }

    serial_write("phase13: elf load failed, trampoline\n");
    if (!user_map_pages()) {
        serial_write("phase4: user map failed\n");
        return;
    }

    serial_write("phase4: entering user\n");
    user_smoke_pid = task_create(user_smoke_task, "user3");
    if (user_smoke_pid == 0) {
        serial_write("phase4: user task create failed\n");
        return;
    }

    while (!user_pid_dead(user_smoke_pid)) {
        task_yield();
    }

    cs = *(volatile u16 *)(user_text_va + USER_CPL_OFF);
    if ((cs & 3u) == 3u) {
        serial_write("phase4: user cpl=3\n");
    } else {
        serial_write("phase4: user cpl missing cs=0x");
        serial_print_hex((u32)cs);
        serial_write("\n");
    }
}
