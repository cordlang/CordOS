; CordOS — Stage2 BIOS loader (real → 32-bit protected mode)
; Loaded by MBR at 0x7E00. Loads ELF64 kernel from disk, builds a minimal
; Multiboot2 info (mmap from E820), jumps to e_entry with Multiboot2 magic.
;
; Experimental: make run-bios. Default remains make run (GRUB).
; See docs/boot_protocol.md.
;
; nasm -f bin -o build/stage2.bin boot/stage2.s

BITS 16
ORG 0x7E00

KERNEL_LOAD_SEG  equ 0x1000          ; phys 0x10000 — temp ELF buffer
KERNEL_LOAD_OFF  equ 0x0000
KERNEL_BUF_PHYS  equ 0x10000
; Keep scratch below stage2 (0x7E00..0xFE00) and MBR (0x7C00)
E820_BUF_PHYS    equ 0x1000
MB2_INFO_PHYS    equ 0x2000
MAX_KERNEL_SECTS equ 512             ; 256 KiB max into low buffer
STACK32_TOP      equ 0x7C00

MULTIBOOT2_BOOTLOADER_MAGIC equ 0x36d76289

; ---------------------------------------------------------------------------
start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    mov [boot_drive], dl
    sti

    mov si, msg_stage2
    call puts16

    call enable_a20
    call collect_e820
    jc .e820_fail

    call load_kernel_bios
    jc .disk_fail

    cli
    lgdt [gdt_desc]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp dword 0x08:pm_entry

.e820_fail:
    mov si, msg_e820
    call puts16
    jmp halt16
.disk_fail:
    mov si, msg_disk
    call puts16
halt16:
    cli
    hlt
    jmp halt16

puts16:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0E
    mov bh, 0
    int 0x10
    jmp puts16
.done:
    ret

enable_a20:
    in al, 0x92
    or al, 2
    and al, 0xFE
    out 0x92, al
    ret

; INT 15h E820 → 24-byte records at E820_BUF_PHYS; count in e820_count
collect_e820:
    mov di, E820_BUF_PHYS
    xor ebx, ebx
    xor bp, bp
    mov edx, 0x534D4150
.loop:
    mov eax, 0xE820
    mov ecx, 24
    int 0x15
    jc .fail
    cmp eax, 0x534D4150
    jne .fail
    add di, 24
    inc bp
    test ebx, ebx
    jnz .loop
    mov [e820_count], bp
    clc
    ret
.fail:
    stc
    ret

; Read kernel_sects from kernel_lba → 0x10000 via INT 13h AH=42h
load_kernel_bios:
    mov ax, [kernel_sects]
    test ax, ax
    jz .fail
    cmp ax, MAX_KERNEL_SECTS
    ja .fail

    mov word [dap + 2], ax
    mov word [dap + 4], KERNEL_LOAD_OFF
    mov word [dap + 6], KERNEL_LOAD_SEG
    mov eax, [kernel_lba]
    mov [dap + 8], eax
    mov dword [dap + 12], 0

    mov si, dap
    mov dl, [boot_drive]
    mov ah, 0x42
    int 0x13
    jc .fail
    clc
    ret
.fail:
    stc
    ret

; ===========================================================================
BITS 32
pm_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, STACK32_TOP

    call build_mb2_info
    call load_elf64

    mov ebx, MB2_INFO_PHYS
    mov eax, MULTIBOOT2_BOOTLOADER_MAGIC
    jmp dword [elf_entry]

; Minimal Multiboot2: header + meminfo + mmap + end
build_mb2_info:
    mov edi, MB2_INFO_PHYS
    add edi, 8                      ; leave room for header

    ; basic meminfo (type 4, size 16)
    mov dword [edi], 4
    mov dword [edi + 4], 16
    mov dword [edi + 8], 640        ; mem_lower KiB
    push edi
    call e820_upper_kib
    pop edi
    mov [edi + 12], eax
    add edi, 16

    ; mmap tag (type 6)
    mov ebx, edi
    mov dword [edi], 6
    mov dword [edi + 8], 24         ; entry_size
    mov dword [edi + 12], 0
    add edi, 16

    movzx ecx, word [e820_count]
    mov esi, E820_BUF_PHYS
.map_loop:
    test ecx, ecx
    jz .map_done
    mov eax, [esi]
    mov [edi], eax
    mov eax, [esi + 4]
    mov [edi + 4], eax
    mov eax, [esi + 8]
    mov [edi + 8], eax
    mov eax, [esi + 12]
    mov [edi + 12], eax
    mov eax, [esi + 16]
    mov [edi + 16], eax
    mov dword [edi + 20], 0
    add edi, 24
    add esi, 24
    dec ecx
    jmp .map_loop
.map_done:
    mov eax, edi
    sub eax, ebx
    mov [ebx + 4], eax              ; tag size
    add eax, 7
    and eax, ~7
    lea edi, [ebx + eax]

    ; end tag
    mov dword [edi], 0
    mov dword [edi + 4], 8
    add edi, 8

    mov eax, edi
    sub eax, MB2_INFO_PHYS
    mov [MB2_INFO_PHYS], eax
    mov dword [MB2_INFO_PHYS + 4], 0
    ret

; Available bytes above 1 MiB / 1024 → EAX
e820_upper_kib:
    xor edx, edx
    movzx ecx, word [e820_count]
    mov esi, E820_BUF_PHYS
.u_loop:
    test ecx, ecx
    jz .u_done
    cmp dword [esi + 16], 1
    jne .u_next
    cmp dword [esi + 4], 0          ; base high
    jne .u_next
    mov eax, [esi]                  ; base low
    cmp eax, 0x100000
    jae .u_whole
    mov ebx, [esi + 8]
    add eax, ebx
    jc .u_next
    cmp eax, 0x100000
    jbe .u_next
    sub eax, 0x100000
    add edx, eax
    jmp .u_next
.u_whole:
    add edx, [esi + 8]
.u_next:
    add esi, 24
    dec ecx
    jmp .u_loop
.u_done:
    mov eax, edx
    shr eax, 10
    ret

; Parse ELF64 at KERNEL_BUF_PHYS; load PT_LOAD; set elf_entry
load_elf64:
    mov esi, KERNEL_BUF_PHYS
    cmp dword [esi], 0x464C457F
    jne elf_fail
    cmp byte [esi + 4], 2
    jne elf_fail
    cmp word [esi + 18], 0x3E
    jne elf_fail

    mov eax, [esi + 24]             ; e_entry
    mov [elf_entry], eax

    movzx ecx, word [esi + 56]      ; e_phnum
    mov edx, [esi + 32]             ; e_phoff
    add edx, KERNEL_BUF_PHYS
    xor ebp, ebp

.elf_ph:
    cmp ebp, ecx
    jge .elf_done

    movzx eax, word [KERNEL_BUF_PHYS + 54]  ; e_phentsize
    imul eax, ebp
    add eax, edx
    mov ebx, eax                    ; phdr *

    cmp dword [ebx], 1              ; PT_LOAD
    jne .elf_next

    push ecx
    push edx
    push ebp

    mov eax, [ebx + 8]              ; p_offset
    mov esi, KERNEL_BUF_PHYS
    add esi, eax
    mov edi, [ebx + 16]             ; p_vaddr (identity-mapped low)
    mov ecx, [ebx + 32]             ; p_filesz
    call memcpy32

    mov eax, [ebx + 32]
    mov ecx, [ebx + 40]             ; p_memsz
    cmp ecx, eax
    jbe .elf_nobss
    sub ecx, eax
    add edi, eax
    xor eax, eax
    call memset32
.elf_nobss:
    pop ebp
    pop edx
    pop ecx

.elf_next:
    inc ebp
    jmp .elf_ph

.elf_done:
    ret

elf_fail:
    mov word [0xB8000], 0x4F45      ; red 'E'
    jmp $

; EDI=dst ESI=src ECX=len
memcpy32:
    test ecx, ecx
    jz .done
.loop:
    mov al, [esi]
    mov [edi], al
    inc esi
    inc edi
    dec ecx
    jnz .loop
.done:
    ret

; EDI=dst ECX=len AL=val
memset32:
    test ecx, ecx
    jz .done
.loop:
    mov [edi], al
    inc edi
    dec ecx
    jnz .loop
.done:
    ret

; ===========================================================================
; Data (16-bit addresses; same linear map in PM)
BITS 16

msg_stage2: db "CordOS stage2", 13, 10, 0
msg_e820:   db "stage2: E820 fail", 13, 10, 0
msg_disk:   db "stage2: disk fail", 13, 10, 0

boot_drive: db 0
e820_count: dw 0

align 4
elf_entry:  dd 0

align 4
dap:
    db 0x10
    db 0
    dw 0
    dw 0
    dw 0
    dq 0

; Patched by scripts/mkdisk.sh — magic 'NOSP' (0x4F534E50) then two dwords
align 4
patch_magic:    dd 0x4F534E50
kernel_lba:     dd 65
kernel_sects:   dd 128

align 8
gdt:
    dq 0x0000000000000000
    dq 0x00CF9A000000FFFF       ; 0x08: 32-bit code
    dq 0x00CF92000000FFFF       ; 0x10: data
gdt_end:

gdt_desc:
    dw gdt_end - gdt - 1
    dd gdt

; Pad to 64 sectors (32 KiB) — matches MBR default stage2_sects
times (64 * 512) - ($ - $$) db 0
