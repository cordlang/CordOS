#include "sched.h"
#include "task.h"
#include "gdt.h"
#include "isr.h"

static void tss_load_task_rsp0(struct task *task)
{
    u64 top;

    if (task != NULL && task->kstack_base != NULL) {
        top = ((u64)task->kstack_base + TASK_STACK_SIZE) & ~0xFULL;
        gdt_set_rsp0(top);
    } else {
        gdt_set_rsp0(gdt_idle_rsp0());
    }
}

volatile u32 scheduler_ticks_os = 0;
volatile u32 sched_enabled_os = 0;

static volatile u32 sched_lock_os = 0;
static u32 rr_index_os = 0;

extern struct task task_table_os[TASK_MAX_OS];

void sched_clear_lock(void)
{
    sched_lock_os = 0;
}

void sched_init(void)
{
    scheduler_ticks_os = 0;
    sched_enabled_os = 0;
    sched_lock_os = 0;
    rr_index_os = 0;
}

void sched_start(void)
{
    sched_enabled_os = 1;
}

void sched_add_ready(struct task *task)
{
    if (task == NULL) {
        return;
    }
    if (task->state != TASK_RUNNING) {
        task->state = TASK_READY;
    }
}

struct task *sched_pick_next(void)
{
    u32 i;
    u32 start;

    start = (rr_index_os + 1u) % TASK_MAX_OS;
    for (i = 0; i < TASK_MAX_OS; i++) {
        u32 idx = (start + i) % TASK_MAX_OS;
        struct task *candidate = &task_table_os[idx];

        if (candidate->state == TASK_READY) {
            rr_index_os = idx;
            return candidate;
        }
    }

    if (current_task_os != NULL &&
        (current_task_os->state == TASK_RUNNING ||
         current_task_os->state == TASK_READY)) {
        return current_task_os;
    }

    return NULL;
}

void schedule(void)
{
    struct task *prev;
    struct task *next;

    if (!sched_enabled_os || sched_lock_os) {
        return;
    }
    if (current_task_os == NULL) {
        return;
    }

    sched_lock_os = 1;

    next = sched_pick_next();
    if (next == NULL || next == current_task_os) {
        sched_lock_os = 0;
        return;
    }

    prev = current_task_os;
    if (prev->state == TASK_RUNNING) {
        prev->state = TASK_READY;
    }

    next->state = TASK_RUNNING;
    current_task_os = next;
    tss_load_task_rsp0(next);

    switch_context(&prev->kstack_top, next->kstack_top);

    /* Resumed task continues here — always re-enable IRQs (preempt from IRQ0
     * can leave IF=0 until a full iret; that froze the keyboard). */
    sched_lock_os = 0;
    interrupts_enable();
}

void scheduler_on_tick(void)
{
    ++scheduler_ticks_os;
    /*
     * Preempt from IRQ0 is disabled for now: switching away mid-interrupt
     * without iret left IF=0 and killed keyboard input. Cooperative yield OK.
     */
    (void)sched_enabled_os;
}
