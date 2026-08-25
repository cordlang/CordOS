#ifndef NUEVOOS_TASK_H
#define NUEVOOS_TASK_H

#include "types.h"

enum task_state {
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_DEAD
};

struct task {
    u32 pid;
    enum task_state state;
    u64 *kstack_top;
    void *kstack_base;
    const char *name;
};

#define TASK_MAX_OS 16u
#define TASK_STACK_SIZE 8192u

extern struct task task_table_os[TASK_MAX_OS];
extern struct task *current_task_os;
extern volatile u32 tasks_ready_os;

void task_init(void);
u32 task_create(void (*entry)(void), const char *name);

/* Strong defs in task.c — override F5 weak stubs for SYS_YIELD / SYS_GETPID. */
struct task *task_current(void);
void task_yield(void);
void task_exit(void);

/* Saved/restored by arch/x86_64/switch.s */
void switch_context(u64 **old_sp, u64 *new_sp);

/* Fase 4 wire-up from kmain64 */
void phase4_init(void);

#endif
