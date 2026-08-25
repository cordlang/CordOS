/*
 * CordOS — Multiboot2 entry + long mode transition (x86_64).
 * Starts in 32-bit protected mode (GRUB), identity-maps low memory,
 * enters long mode, then calls kmain64(void *mb2_info) with SysV AMD64
 * (RDI = Multiboot2 info pointer).
 */

.set MULTIBOOT2_MAGIC,        0xE85250D6
.set MULTIBOOT2_ARCHITECTURE, 0
.set MULTIBOOT2_HEADER_LENGTH, multiboot_header_end - multiboot_header_start
.set MULTIBOOT2_CHECKSUM,     -(MULTIBOOT2_MAGIC + MULTIBOOT2_ARCHITECTURE + MULTIBOOT2_HEADER_LENGTH)

.set MULTIBOOT2_BOOTLOADER_MAGIC, 0x36d76289

.set PAGE_PRESENT, 0x001
.set PAGE_WRITABLE, 0x002
.set PAGE_LARGE,   0x080   /* PS: 2 MiB page */

.section .multiboot
.align 8
multiboot_header_start:
    .long MULTIBOOT2_MAGIC
    .long MULTIBOOT2_ARCHITECTURE
    .long MULTIBOOT2_HEADER_LENGTH
    .long MULTIBOOT2_CHECKSUM
    /* Framebuffer request: prefer 1920x1080x32 (bootloader may pick another). */
    .align 8
    .word 5
    .word 0
    .long 20
    .long 1920
    .long 1080
    .long 32
    /* End tag */
    .align 8
    .word 0
    .word 0
    .long 8
multiboot_header_end:

.section .bss
.align 4096
pml4_table:
    .skip 4096
pdpt_table:
    .skip 4096
pd_table:
    .skip 4096

.align 16
stack64_bottom:
    .skip 16384
stack64_top:

.section .data
.align 4
mb2_info_phys:
    .long 0

.align 16
gdt64:
    .quad 0x0000000000000000          /* null */
    .quad 0x00AF9A000000FFFF          /* 0x08: 64-bit code */
    .quad 0x00CF92000000FFFF          /* 0x10: data */
gdt64_end:

.align 4
gdt64_ptr:
    .word gdt64_end - gdt64 - 1
    .long gdt64

.section .text
.code32
.global _start64
.type _start64, @function
_start64:
    cli

    /* EAX = Multiboot2 magic, EBX = info structure (physical). */
    cmpl $MULTIBOOT2_BOOTLOADER_MAGIC, %eax
    jne .halt32

    movl %ebx, mb2_info_phys

    /* PML4[0] -> PDPT */
    movl $pdpt_table, %eax
    orl $(PAGE_PRESENT | PAGE_WRITABLE), %eax
    movl %eax, pml4_table
    movl $0, pml4_table + 4

    /* PDPT[0] -> PD */
    movl $pd_table, %eax
    orl $(PAGE_PRESENT | PAGE_WRITABLE), %eax
    movl %eax, pdpt_table
    movl $0, pdpt_table + 4

    /*
     * Identity-map the first 1 GiB with 512 × 2 MiB pages.
     * Covers kernel at 1 MiB, VGA at 0xB8000, and early low memory.
     */
    movl $pd_table, %edi
    xorl %eax, %eax
    movl $512, %ecx
1:
    movl %eax, %edx
    orl $(PAGE_PRESENT | PAGE_WRITABLE | PAGE_LARGE), %edx
    movl %edx, (%edi)
    movl $0, 4(%edi)
    addl $0x200000, %eax
    addl $8, %edi
    loop 1b

    lgdt gdt64_ptr

    /* CR4.PAE = 1 */
    movl %cr4, %eax
    orl $(1 << 5), %eax
    movl %eax, %cr4

    /* CR3 = PML4 */
    movl $pml4_table, %eax
    movl %eax, %cr3

    /* EFER.LME = 1 */
    movl $0xC0000080, %ecx
    rdmsr
    orl $(1 << 8), %eax
    wrmsr

    /* CR0.PG = 1 (enter compatibility / long mode) */
    movl %cr0, %eax
    orl $(1 << 31), %eax
    movl %eax, %cr0

    ljmp $0x08, $long_mode_entry

.halt32:
    cli
    hlt
    jmp .halt32

.code64
.align 16
long_mode_entry:
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw %ax, %fs
    movw %ax, %gs

    movq $stack64_top, %rsp
    xorq %rbp, %rbp

    /* SysV AMD64: first arg in RDI */
    movl mb2_info_phys, %edi
    call kmain64

.halt64:
    cli
    hlt
    jmp .halt64

.size _start64, . - _start64

.section .note.GNU-stack,"",@progbits
