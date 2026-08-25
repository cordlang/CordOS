/* int 0x80 entry — same interrupt_frame layout as isr64.s (r15 at top of
 * frame on stack). CPU already pushed SS/RSP/RFLAGS/CS/RIP; we add err=0,
 * int_no=0x80, then GPRs. Result left in frame->rax for iretq restore. */

.global syscall_entry
.extern syscall_interrupt

syscall_entry:
    pushq $0
    pushq $0x80

    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rbp
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    movq %rsp, %rdi
    call syscall_interrupt

    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rbp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax
    addq $16, %rsp
    iretq
