# Handoff F4 COMPLETE

## Files
- `src/include/task.h`, `src/include/sched.h`
- `src/task.c`, `src/sched.c`, `src/arch/x86_64/switch.s`
- hooks: `src/time.c` (`scheduler_on_tick` under `__x86_64__`), `isr64.c` EOI-before-handler
- wire: `phase4_init()` in `kmain64` after heap/memory test
- note: `agent-notes/PHASE4_F4.md`

## For F5 / INT
- **Strong** `task_yield()` / `task_current()` in `task.o` override F5 weak stubs → `SYS_YIELD` / `SYS_GETPID`
- `struct task.pid` is first field (compatible with F5’s minimal local struct)
- `KERNEL64_OBJS` already includes `task.o` `sched.o` `switch.o`

## Demo
Two kernel tasks print `taskA=` / `taskB=` counters (preempt + yield); idle continues into shell.
