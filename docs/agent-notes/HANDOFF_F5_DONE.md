# Handoff F5 COMPLETE

## Files
- `docs/abi.md`
- `src/syscall.c` + `src/include/syscall.h`
- `src/arch/x86_64/syscall_entry.s`
- `user/libnos/*`, `user/test_write.c` (not linked into kernel)

## Wire-up (INT)
1. KERNEL64_OBJS: `build/syscall.o` `build/syscall_entry.o` (from syscall_entry.s)
2. `phase5_init()` after `isr_install()` (registers int 0x80)
3. F4 should provide strong `task_yield` / `task_current` (F5 uses weak refs)

## Behavior
- write(1)→VGA, read(0)→keyboard, yield/getpid weak to tasks, mmap stub -1
- Gate DPL=0 for now
