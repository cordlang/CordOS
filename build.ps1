param(
    [ValidateSet("i386", "x86_64")]
    [string]$Arch = "i386"
)

$ErrorActionPreference = "Stop"

# Dual-arch helper. x86_64 freestanding build is intended for WSL/Linux Make
# with x86_64-elf-gcc (see README). This script keeps the i386 Windows path.

if ($Arch -eq "x86_64") {
    Write-Host "ARCH=x86_64: use WSL/Linux, e.g.:"
    Write-Host "  wsl -d Ubuntu -- bash -lc 'cd /mnt/d/os && make ARCH=x86_64'"
    Write-Host "Requires x86_64-elf-gcc/ld (Agent A: ~/opt/cross64) and grub-mkrescue."
    exit 1
}

$cc = "i686-elf-gcc"
$ld = "i686-elf-ld"
$grubFile = "grub-file"
$grubMkrescue = "grub-mkrescue"

$cflags = @(
    "-std=c11", "-ffreestanding", "-fno-builtin", "-fno-stack-protector",
    "-fno-pie", "-m32", "-march=i686", "-mno-mmx", "-mno-sse", "-mno-sse2",
    "-msoft-float", "-Wall", "-Wextra", "-Werror", "-O2",
    "-Isrc/include"
)

New-Item -ItemType Directory -Force -Path "build", "iso\boot\grub" | Out-Null

& $cc -m32 -c "src\boot.s" -o "build\boot.o"
& $cc -m32 -c "src\isr.s" -o "build\isr_asm.o"

$sources = @(
    "config", "string", "io", "utf8", "vga", "panic", "multiboot",
    "gdt", "idt", "pic", "isr", "pit", "time", "keyboard",
    "pmm", "vmm", "heap", "page_fault", "kernel"
)
foreach ($name in $sources) {
    & $cc @cflags -c "src\$name.c" -o "build\$name.o"
}

$objs = @(
    "build\boot.o",
    "build\isr_asm.o",
    "build\config.o",
    "build\string.o",
    "build\io.o",
    "build\utf8.o",
    "build\vga.o",
    "build\panic.o",
    "build\multiboot.o",
    "build\gdt.o",
    "build\idt.o",
    "build\pic.o",
    "build\isr.o",
    "build\pit.o",
    "build\time.o",
    "build\keyboard.o",
    "build\pmm.o",
    "build\vmm.o",
    "build\heap.o",
    "build\page_fault.o",
    "build\kernel.o"
)

& $ld -T "linker.ld" -m elf_i386 --build-id=none -o "build\cordos32.bin" @objs

& $grubFile --is-x86-multiboot "build\cordos32.bin"
Copy-Item -Force "build\cordos32.bin" "iso\boot\cordos32.bin"
Copy-Item -Force "grub.cfg" "iso\boot\grub\grub.cfg"
& $grubMkrescue -o "build\cordos32.iso" "iso"

Write-Host "ISO creada: build\cordos32.iso (ARCH=i386)"
