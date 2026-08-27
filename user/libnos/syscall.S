/* Userland syscall stubs (x86_64). Not linked into the kernel.
 * ABI: rax=num, rdi/rsi/rdx=args, int $0x80, return in rax. */

.global nos_exit
.global nos_write
.global nos_read
.global nos_yield
.global nos_getpid
.global nos_mmap
.global nos_open
.global nos_close
.global nos_spawn
.global nos_exec

nos_exit:
    mov $0, %rax
    int $0x80
    ret

nos_write:
    mov $1, %rax
    int $0x80
    ret

nos_read:
    mov $2, %rax
    int $0x80
    ret

nos_yield:
    mov $3, %rax
    int $0x80
    ret

nos_getpid:
    mov $4, %rax
    int $0x80
    ret

nos_mmap:
    mov $5, %rax
    int $0x80
    ret

nos_open:
    mov $6, %rax
    int $0x80
    ret

nos_close:
    mov $7, %rax
    int $0x80
    ret

nos_spawn:
    mov $8, %rax
    int $0x80
    ret

nos_exec:
    mov $9, %rax
    int $0x80
    ret
