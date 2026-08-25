.section .multiboot
.align 4
.long 0x1BADB002
.long 0
.long -(0x1BADB002)

.section .bss
.align 16
stack_bottom:
.skip 16384
stack_top:

.section .text
.global _start
_start:
    cli
    movl $stack_top, %esp
    xorl %ebp, %ebp

    # Multiboot entrega EAX=magic y EBX=puntero a la informacion.
    pushl %ebx
    pushl %eax
    call kmain

.halt:
    cli
    hlt
    jmp .halt
