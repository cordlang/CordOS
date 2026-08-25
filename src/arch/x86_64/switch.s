/* Kernel context switch (x86_64).
 *
 * void switch_context(u64 **old_sp, u64 *new_sp);
 *   rdi = &old->kstack_top
 *   rsi = new->kstack_top
 *
 * Saves callee-saved GPRs + return RIP on the current stack, stores RSP
 * into *old_sp, loads RSP from new_sp, restores GPRs and RETs into the
 * new task (either prior switch site or task entry).
 */

.global switch_context
switch_context:
    pushq %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    movq %rsp, (%rdi)
    movq %rsi, %rsp

    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp
    ret
