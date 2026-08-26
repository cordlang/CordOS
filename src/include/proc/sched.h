#ifndef CORDOS_SCHED_H
#define CORDOS_SCHED_H

#include "types.h"
#include "task.h"

extern volatile u32 scheduler_ticks_os;
extern volatile u32 sched_enabled_os;

void sched_init(void);
void sched_start(void);
void sched_clear_lock(void);
void schedule(void);
void scheduler_on_tick(void);

/* Used by task.c to enqueue / find next */
void sched_add_ready(struct task *task);
struct task *sched_pick_next(void);

#endif
