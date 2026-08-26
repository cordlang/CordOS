; CordOS — Stage1 BIOS MBR (512 bytes)
; Loads stage2 from LBA 1 into 0x7E00 and jumps to 0000:7E00
; (matches stage2 ORG 0x7E00).
; Assembled with NASM: nasm -f bin -o build/mbr.bin boot/mbr.s
;
; Disk layout (see docs/boot_protocol.md):
;   LBA 0     : this MBR
;   LBA 1..N  : stage2
;   LBA K..   : kernel ELF (cordos.bin)

BITS 16
ORG 0x7C00

STAGE2_SEGMENT equ 0x0000
STAGE2_OFFSET  equ 0x7E00
STAGE2_LBA     equ 1

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    ; DL = BIOS boot drive (preserve)
    mov [boot_drive], dl

    mov si, msg_loading
    call puts

    ; DAP: read stage2_sects sectors from LBA 1 -> 0x7E00
    mov ax, [stage2_sects]
    mov [dap + 2], ax
    mov word [dap + 4], STAGE2_OFFSET
    mov word [dap + 6], STAGE2_SEGMENT
    mov dword [dap + 8], STAGE2_LBA
    mov dword [dap + 12], 0

    mov si, dap
    mov dl, [boot_drive]
    mov ah, 0x42
    int 0x13
    jc disk_error

    mov dl, [boot_drive]
    jmp STAGE2_SEGMENT:STAGE2_OFFSET

disk_error:
    mov si, msg_disk
    call puts
.halt:
    cli
    hlt
    jmp .halt

puts:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0E
    mov bh, 0
    int 0x10
    jmp puts
.done:
    ret

msg_loading: db "CordOS MBR", 13, 10, 0
msg_disk:    db "MBR: disk err", 13, 10, 0

boot_drive:  db 0

align 4
dap:
    db 0x10
    db 0
    dw 0
    dw 0
    dw 0
    dq 0

; Default: 64 sectors (32 KiB stage2)
stage2_sects: dw 64

times 510 - ($ - $$) db 0
dw 0xAA55
