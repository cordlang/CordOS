.section .rodata
.balign 16
.global user_hello_elf_start
.global user_hello_elf_end
user_hello_elf_start:
	.incbin "out/user_hello.elf"
user_hello_elf_end:
