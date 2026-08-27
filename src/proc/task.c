#include "task.h"
#include "sched.h"
#include "heap.h"
#include "string.h"
#include "serial.h"
#include "panic.h"
#include "isr.h"
#include "user.h"
#include "vfs.h"

struct task task_table_os[TASK_MAX_OS];
struct task *current_task_os = NULL;
volatile u32 tasks_ready_os = 0;

static u32 next_pid_os = 1;
static void (*task_entries_os[TASK_MAX_OS])(void);

static void task_bootstrap(void);
static void demo_task_a(void);
static void demo_task_b(void);

static void task_bootstrap(void)
{
    void (*entry)(void);
    u32 i;

    /* New task starts inside schedule()'s lock; clear before running. */
    sched_clear_lock();
    interrupts_enable();

    entry = NULL;
    for (i = 0; i < TASK_MAX_OS; i++) {
        if (&task_table_os[i] == current_task_os) {
            entry = task_entries_os[i];
            break;
        }
    }

    if (entry != NULL) {
        entry();
    }
    task_exit();
}

static void setup_stack(struct task *task)
{
    u8 *base;
    u64 *sp;

    base = (u8 *)kmalloc(TASK_STACK_SIZE);
    if (base == NULL) {
        panic("task: sin stack");
    }
    memset(base, 0, TASK_STACK_SIZE);

    task->kstack_base = base;

    /*
     * switch_context frame (high → low):
     *   task_exit, bootstrap, rbp, rbx, r12, r13, r14, r15
     * 8 qwords from 16-byte-aligned top → RSP ≡ 0; after 6 pops + ret
     * into bootstrap, RSP ≡ 8 (SysV).
     */
    sp = (u64 *)(((u64)base + TASK_STACK_SIZE) & ~0xFULL);
    *--sp = (u64)task_exit;
    *--sp = (u64)task_bootstrap;
    *--sp = 0; /* rbp */
    *--sp = 0; /* rbx */
    *--sp = 0; /* r12 */
    *--sp = 0; /* r13 */
    *--sp = 0; /* r14 */
    *--sp = 0; /* r15 */

    task->kstack_top = sp;
}

void task_init(void)
{
    u32 i;

    sched_init();

    for (i = 0; i < TASK_MAX_OS; i++) {
        task_table_os[i].pid = 0;
        task_table_os[i].state = TASK_DEAD;
        task_table_os[i].kstack_top = NULL;
        task_table_os[i].kstack_base = NULL;
        task_table_os[i].name = NULL;
        task_table_os[i].user_rip = 0;
        task_table_os[i].user_rsp = 0;
        task_entries_os[i] = NULL;
    }

    /* Slot 0: bootstrap / idle = currently executing kmain stack. */
    task_table_os[0].pid = 0;
    task_table_os[0].state = TASK_RUNNING;
    task_table_os[0].kstack_top = NULL;
    task_table_os[0].kstack_base = NULL;
    task_table_os[0].name = "idle";
    task_table_os[0].user_rip = 0;
    task_table_os[0].user_rsp = 0;
    current_task_os = &task_table_os[0];
    tasks_ready_os = 1;
    next_pid_os = 1;
}

u32 task_create_user(void (*entry)(void), const char *name, u64 rip, u64 rsp)
{
    u32 i;
    struct task *task;

    if (entry == NULL) {
        return 0;
    }

    task = NULL;
    for (i = 1; i < TASK_MAX_OS; i++) {
        if (task_table_os[i].state == TASK_DEAD) {
            task = &task_table_os[i];
            break;
        }
    }
    if (task == NULL) {
        return 0;
    }

    /* Reusing a DEAD slot whose stack was not reaped yet. */
    if (task->kstack_base != NULL) {
        kfree(task->kstack_base);
        task->kstack_base = NULL;
        task->kstack_top = NULL;
    }
    task->user_rip = 0;
    task->user_rsp = 0;

    {
        u32 tries;
        u32 pid = 0;

        for (tries = 0; tries < TASK_MAX_OS + 2u; ++tries) {
            u32 cand = next_pid_os++;
            u32 k;

            if (next_pid_os == 0) {
                next_pid_os = 1;
            }
            if (cand == 0) {
                continue;
            }
            for (k = 0; k < TASK_MAX_OS; ++k) {
                if (task_table_os[k].state != TASK_DEAD &&
                    task_table_os[k].pid == cand) {
                    cand = 0;
                    break;
                }
            }
            if (cand != 0) {
                pid = cand;
                break;
            }
        }
        if (pid == 0) {
            return 0;
        }
        task->pid = pid;
    }
    task->name = name ? name : "?";
    task->user_rip = rip;
    task->user_rsp = rsp;
    task_entries_os[i] = entry;
    setup_stack(task);
    task->state = TASK_READY;
    tasks_ready_os++;
    sched_add_ready(task);

    return task->pid;
}

u32 task_create(void (*entry)(void), const char *name)
{
    return task_create_user(entry, name, 0, 0);
}

/* Strong exports: override __attribute__((weak)) stubs in syscall.c (F5). */
struct task *task_current(void)
{
    return current_task_os;
}

void task_yield(void)
{
    schedule();
}

void task_exit(void)
{
    u32 pid;

    interrupts_disable();
    if (current_task_os != NULL) {
        pid = current_task_os->pid;
        current_task_os->state = TASK_DEAD;
        if (tasks_ready_os > 0) {
            tasks_ready_os--;
        }
        if (pid != 0) {
            vfs_close_task(pid);
        }
    }
    schedule();
    panic("task_exit: no runnable task");
}

static void demo_task_a(void)
{
    for (;;) {
        __asm__ volatile ("hlt");
        task_yield();
    }
}

static void demo_task_b(void)
{
    for (;;) {
        __asm__ volatile ("hlt");
        task_yield();
    }
}

/* Second READY task so IRQ0 RR actually switches (idle alone is a no-op). */
static void preempt_companion(void)
{
    for (;;) {
        __asm__ volatile ("hlt");
        task_yield();
    }
}

void phase4_init(void)
{
    u32 t0;
    u32 pid;

    task_init();
    sched_start();
    serial_write("scheduler: idle OK\n");

    /* One-shot ring-3 iret; SYS_EXIT kills the smoke task and returns here. */
    user_smoke();

    pid = task_create(preempt_companion, "tick");
    if (pid == 0) {
        serial_write("phase14: companion failed\n");
    } else {
        serial_write("scheduler: preempt ON\n");
        t0 = scheduler_ticks_os;
        while ((scheduler_ticks_os - t0) < 10u) {
            __asm__ volatile ("hlt");
        }
        serial_write("phase14: ticks=");
        serial_print_u32(scheduler_ticks_os - t0);
        serial_write("\n");
    }

    (void)demo_task_a;
    (void)demo_task_b;
}
