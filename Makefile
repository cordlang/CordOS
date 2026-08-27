# CordOS — dual-arch build
# Default: x86_64. Legacy demo: make ARCH=i386

ARCH ?= x86_64

GRUB_FILE := grub-file
GRUB_MKRESCUE := grub-mkrescue

# Public headers live under src/include/<module>/; extra -I keeps
# `#include "draw.h"` working after the modular move.
INC := -Isrc/include \
	-Isrc/include/kernel -Isrc/include/boot -Isrc/include/arch \
	-Isrc/include/mm -Isrc/include/proc -Isrc/include/drivers \
	-Isrc/include/fs -Isrc/include/ui -Isrc/include/shell \
	-Isrc/include/net -Iout

PUB_HDRS := $(wildcard src/include/*.h) \
	$(wildcard src/include/*/*.h) \
	$(wildcard src/include/*/*/*.h)

PY := $(shell if [ -x /tmp/iconsvenv/bin/python ]; then echo /tmp/iconsvenv/bin/python; \
	elif [ -x /tmp/nv/bin/python ]; then echo /tmp/nv/bin/python; \
	else echo python3; fi)

.PHONY: all clean run run-vbox check check-host mbr disk run-bios run-persist userland

ifeq ($(ARCH),i386)

TARGET := i686-elf
CC := $(TARGET)-gcc
LD := $(TARGET)-ld
QEMU := qemu-system-i386

CFLAGS := -std=c11 -ffreestanding -fno-builtin -fno-stack-protector \
	-fno-pie -m32 -march=i686 -mno-mmx -mno-sse -mno-sse2 \
	-msoft-float -Wall -Wextra -Werror -O2 \
	$(INC)

KERNEL_OBJS := \
	out/boot.o \
	out/isr_asm.o \
	out/config.o \
	out/string.o \
	out/io.o \
	out/utf8.o \
	out/vga.o \
	out/panic.o \
	out/multiboot.o \
	out/gdt.o \
	out/idt.o \
	out/pic.o \
	out/isr.o \
	out/pit.o \
	out/time.o \
	out/keyboard.o \
	out/pmm.o \
	out/vmm.o \
	out/heap.o \
	out/page_fault.o \
	out/kernel.o

all: out/cordos32.iso

build:
	mkdir -p out iso/boot/grub

out/boot.o: src/boot/boot.s | build
	$(CC) -m32 -c $< -o $@

out/isr_asm.o: src/arch/i386/isr.s | build
	$(CC) -m32 -c $< -o $@

out/config.o: src/kernel/config.c $(PUB_HDRS) | build
	$(CC) $(CFLAGS) -c $< -o $@
out/string.o: src/kernel/string.c $(PUB_HDRS) | build
	$(CC) $(CFLAGS) -c $< -o $@
out/io.o: src/kernel/io.c $(PUB_HDRS) | build
	$(CC) $(CFLAGS) -c $< -o $@
out/utf8.o: src/kernel/utf8.c $(PUB_HDRS) | build
	$(CC) $(CFLAGS) -c $< -o $@
out/vga.o: src/drivers/vga.c $(PUB_HDRS) | build
	$(CC) $(CFLAGS) -c $< -o $@
out/panic.o: src/kernel/panic.c $(PUB_HDRS) | build
	$(CC) $(CFLAGS) -c $< -o $@
out/multiboot.o: src/boot/multiboot.c $(PUB_HDRS) | build
	$(CC) $(CFLAGS) -c $< -o $@
out/gdt.o: src/arch/i386/gdt.c $(PUB_HDRS) | build
	$(CC) $(CFLAGS) -c $< -o $@
out/idt.o: src/arch/i386/idt.c $(PUB_HDRS) | build
	$(CC) $(CFLAGS) -c $< -o $@
out/pic.o: src/arch/x86_64/pic.c $(PUB_HDRS) | build
	$(CC) $(CFLAGS) -c $< -o $@
out/isr.o: src/arch/i386/isr.c $(PUB_HDRS) | build
	$(CC) $(CFLAGS) -c $< -o $@
out/pit.o: src/arch/x86_64/pit.c $(PUB_HDRS) | build
	$(CC) $(CFLAGS) -c $< -o $@
out/time.o: src/drivers/time.c $(PUB_HDRS) | build
	$(CC) $(CFLAGS) -c $< -o $@
out/keyboard.o: src/drivers/keyboard.c $(PUB_HDRS) | build
	$(CC) $(CFLAGS) -c $< -o $@
out/pmm.o: src/arch/i386/pmm.c $(PUB_HDRS) | build
	$(CC) $(CFLAGS) -c $< -o $@
out/vmm.o: src/arch/i386/vmm.c $(PUB_HDRS) | build
	$(CC) $(CFLAGS) -c $< -o $@
out/heap.o: src/mm/heap.c $(PUB_HDRS) | build
	$(CC) $(CFLAGS) -c $< -o $@
out/page_fault.o: src/mm/page_fault.c $(PUB_HDRS) | build
	$(CC) $(CFLAGS) -c $< -o $@
out/kernel.o: src/arch/i386/kernel.c $(PUB_HDRS) | build
	$(CC) $(CFLAGS) -c $< -o $@

out/cordos32.bin: $(KERNEL_OBJS) linker.ld
	$(LD) -T linker.ld -m elf_i386 --build-id=none -o $@ $(KERNEL_OBJS)

check: out/cordos32.bin
	$(GRUB_FILE) --is-x86-multiboot $<

out/cordos32.iso: out/cordos32.bin grub.cfg | build
	mkdir -p iso/boot/grub
	cp out/cordos32.bin iso/boot/cordos32.bin
	cp grub.cfg iso/boot/grub/grub.cfg
	$(GRUB_MKRESCUE) -o $@ iso

run: out/cordos32.iso
	$(QEMU) -cdrom $<

clean:
	rm -rf out iso/boot

else ifeq ($(ARCH),x86_64)

CROSS64 ?= $(HOME)/opt/cross64/bin
ifeq ($(wildcard $(CROSS64)/x86_64-elf-gcc),)
  ifneq ($(shell command -v x86_64-elf-gcc 2>/dev/null),)
    CC64 := x86_64-elf-gcc
    LD64 := x86_64-elf-ld
  else
    CC64 := gcc
    LD64 := ld
  endif
else
  CC64 := $(CROSS64)/x86_64-elf-gcc
  LD64 := $(CROSS64)/x86_64-elf-ld
endif

QEMU := qemu-system-x86_64

NASM ?= $(shell command -v nasm 2>/dev/null)
ifeq ($(NASM),)
  ifneq ($(wildcard $(CURDIR)/tools/nasm),)
    NASM := $(CURDIR)/tools/nasm
  else
    NASM := nasm
  endif
endif

CFLAGS64 := -std=c11 -ffreestanding -fno-builtin -fno-stack-protector \
	-fno-pie -m64 -mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
	-Wall -Wextra -Werror -O2 $(INC)

KERNEL64_OBJS := \
	out/boot64.o \
	out/isr64_asm.o \
	out/config64.o \
	out/string64.o \
	out/io64.o \
	out/utf864.o \
	out/vga64.o \
	out/panic64.o \
	out/multiboot2.o \
	out/gdt64.o \
	out/idt64.o \
	out/pic64.o \
	out/isr64.o \
	out/pit64.o \
	out/time64.o \
	out/rtc.o \
	out/keyboard64.o \
	out/mouse.o \
	out/pmm64.o \
	out/vmm64.o \
	out/heap64.o \
	out/kselftest.o \
	out/page_fault64.o \
	out/task.o \
	out/sched.o \
	out/user.o \
	out/elf64.o \
	out/user_hello_blob.o \
	out/switch.o \
	out/syscall.o \
	out/syscall_entry.o \
	out/vfs.o \
	out/nosfs.o \
	out/nosfs_disk.o \
	out/initrd.o \
	out/persist.o \
	out/userdb.o \
	out/shell.o \
	out/i18n.o \
	out/i18n_data.o \
	out/lang_select.o \
	out/onboarding.o \
	out/session.o \
	out/login.o \
	out/desktop.o \
	out/desktop_win.o \
	out/desktop_chrome.o \
	out/desktop_spot.o \
	out/desktop_input.o \
	out/draw.o \
	out/compositor.o \
	out/widget.o \
	out/metrics.o \
	out/icons.o \
	out/cursor_data.o \
	out/font.o \
	out/wallpaper.o \
	out/brand.o \
	out/brand_draw.o \
	out/fb_console.o \
	out/animation.o \
	out/serial.o \
	out/pci.o \
	out/virtio_net.o \
	out/ehci.o \
	out/wlan.o \
	out/wlan_host.o \
	out/rtl8187.o \
	out/e1000.o \
	out/pcnet.o \
	out/nic.o \
	out/net.o \
	out/ipc.o \
	out/spinlock.o \
	out/smp.o \
	out/fb.o \
	out/gamma_lut.o \
	out/power.o \
	out/ata.o \
	out/blk.o \
	out/ahci.o \
	out/kernel64.o

all: dist/cordos.iso

# Syscall numbers 0–7 (docs/abi.md):
#   SYS_EXIT=0 SYS_WRITE=1 SYS_READ=2 SYS_YIELD=3
#   SYS_GETPID=4 SYS_MMAP=5 SYS_OPEN=6 SYS_CLOSE=7
build:
	mkdir -p out out/isoroot/boot/grub out/user

# --- asm ---
out/boot64.o: src/boot/boot64.s | build
	$(CC64) -c $< -o $@
out/isr64_asm.o: src/arch/x86_64/isr64.s | build
	$(CC64) -c $< -o $@
out/switch.o: src/arch/x86_64/switch.s | build
	$(CC64) -c $< -o $@
out/syscall_entry.o: src/arch/x86_64/syscall_entry.s | build
	$(CC64) -c $< -o $@

# --- C: boot / arch / kernel / mm / proc ---
out/gdt64.o: src/arch/x86_64/gdt64.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/idt64.o: src/arch/x86_64/idt64.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/isr64.o: src/arch/x86_64/isr64.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/pic64.o: src/arch/x86_64/pic.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/pit64.o: src/arch/x86_64/pit.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/kernel64.o: src/kernel/kernel64.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/config64.o: src/kernel/config.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/string64.o: src/kernel/string.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/io64.o: src/kernel/io.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/utf864.o: src/kernel/utf8.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/panic64.o: src/kernel/panic.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/multiboot2.o: src/boot/multiboot2.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/pmm64.o: src/mm/pmm64.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/vmm64.o: src/mm/vmm64.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/heap64.o: src/mm/heap.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/kselftest.o: src/kernel/kselftest.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/page_fault64.o: src/mm/page_fault.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/task.o: src/proc/task.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/sched.o: src/proc/sched.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/user.o: src/proc/user.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/elf64.o: src/proc/elf64.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/syscall.o: src/proc/syscall.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/ipc.o: src/proc/ipc.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/spinlock.o: src/proc/spinlock.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/smp.o: src/proc/smp.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@

# --- drivers ---
out/vga64.o: src/drivers/vga64.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/time64.o: src/drivers/time.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/rtc.o: src/drivers/rtc.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/keyboard64.o: src/drivers/keyboard.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/mouse.o: src/drivers/mouse.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/serial.o: src/drivers/serial.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/pci.o: src/drivers/pci.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/virtio_net.o: src/drivers/virtio_net.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/ehci.o: src/drivers/usb/ehci.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/wlan.o: src/drivers/wifi/wlan.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/wlan_host.o: src/drivers/wifi/wlan_host.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/rtl8187.o: src/drivers/wifi/rtl8187.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/e1000.o: src/drivers/e1000.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/pcnet.o: src/drivers/pcnet.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/nic.o: src/net/nic.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/net.o: src/net/net.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/fb.o: src/drivers/fb.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/gamma_lut.o: src/drivers/gamma_lut.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/power.o: src/drivers/power.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/ata.o: src/drivers/ata.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/blk.o: src/drivers/blk.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/ahci.o: src/drivers/ahci.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@

# --- fs / shell ---
out/vfs.o: src/fs/vfs.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/nosfs.o: src/fs/nosfs.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/nosfs_disk.o: src/fs/nosfs_disk.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/persist.o: src/fs/persist.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/userdb.o: src/fs/userdb.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/initrd.o: src/fs/initrd.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/shell.o: src/shell/shell.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@

I18N_JSON := $(wildcard assets/i18n/*.json)

out/i18n_gen.h out/i18n_data.c: tools/gen_i18n.py $(I18N_JSON) | build
	$(PY) tools/gen_i18n.py

out/i18n.o: src/shell/i18n.c out/i18n_gen.h $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/i18n_data.o: out/i18n_data.c out/i18n_gen.h $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@

$(KERNEL64_OBJS): out/i18n_gen.h

# --- ui ---
out/lang_select.o: src/ui/session/lang_select.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/onboarding.o: src/ui/session/onboarding.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/session.o: src/ui/session/session.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/login.o: src/ui/login/login.c src/include/ui/brand.h $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/desktop.o: src/ui/desktop/desktop.c src/ui/desktop/desktop_priv.h $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/desktop_win.o: src/ui/desktop/desktop_win.c src/ui/desktop/desktop_priv.h $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/desktop_chrome.o: src/ui/desktop/desktop_chrome.c src/ui/desktop/desktop_priv.h $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/desktop_spot.o: src/ui/desktop/desktop_spot.c src/ui/desktop/desktop_priv.h $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/desktop_input.o: src/ui/desktop/desktop_input.c src/ui/desktop/desktop_priv.h $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/draw.o: src/ui/gfx/draw.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/compositor.o: src/ui/gfx/compositor.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/metrics.o: src/ui/gfx/metrics.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/widget.o: src/ui/widgets/widget.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/cursor_data.o: src/ui/gfx/cursor_data.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/font.o: src/ui/gfx/font.c out/wallpaper.rgb $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/fb_console.o: src/ui/gfx/fb_console.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/animation.o: src/ui/gfx/animation.c $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@
out/brand_draw.o: src/ui/gfx/brand_draw.c src/include/ui/brand.h $(PUB_HDRS) | build
	$(CC64) $(CFLAGS64) -c $< -o $@

# --- generated assets ---
ICON_SHEETS := assets/icon-library/Files.svg assets/icon-library/Settings.svg \
	assets/icon-library/Essetional.svg assets/icon-library/Grid.svg \
	assets/icon-library/Programing.svg assets/icon-library/Arrow.svg

out/icon48.rgba out/icon24.rgba: tools/gen_icons.py $(ICON_SHEETS) | build
	$(PY) tools/gen_icons.py

out/icons.o: src/ui/gfx/icons.s out/icon48.rgba out/icon24.rgba | build
	$(CC64) -c src/ui/gfx/icons.s -o $@

out/wallpaper.o: src/ui/gfx/wallpaper.s out/wallpaper.rgb \
		out/login_wallpaper.rgb out/login_wallpaper_alt.rgb | build
	$(CC64) -c src/ui/gfx/wallpaper.s -o $@

out/brand_logo.rgba out/brand_login.rgba out/brand_name.rgba \
out/splash.png src/include/ui/brand.h: \
		tools/gen_brand.py assets/logo/cordos_logo_white.svg \
		assets/logo/cordos_name_white.svg | build
	$(PY) tools/gen_brand.py

out/brand.o: src/ui/gfx/brand.s out/brand_logo.rgba out/brand_login.rgba \
		out/brand_name.rgba | build
	$(CC64) -c src/ui/gfx/brand.s -o $@

out/wallpaper.rgb: assets/backgrounds/default.png | build
	$(PY) -c "from PIL import Image; im=Image.open('assets/backgrounds/default.png').convert('RGB'); im=im.resize((1920,1080), Image.Resampling.LANCZOS) if im.size!=(1920,1080) else im; open('out/wallpaper.rgb','wb').write(im.tobytes('raw','RGB'))"

out/login_wallpaper.rgb: assets/backgrounds/login.jpg | build
	$(PY) -c "from PIL import Image; im=Image.open('assets/backgrounds/login.jpg').convert('RGB'); im=im.resize((1920,1080), Image.Resampling.LANCZOS) if im.size!=(1920,1080) else im; open('out/login_wallpaper.rgb','wb').write(im.tobytes('raw','RGB'))"

out/login_wallpaper_alt.rgb: assets/backgrounds/login_abstract.jpg | build
	$(PY) -c "from PIL import Image; im=Image.open('assets/backgrounds/login_abstract.jpg').convert('RGB'); im=im.resize((1920,1080), Image.Resampling.LANCZOS) if im.size!=(1920,1080) else im; open('out/login_wallpaper_alt.rgb','wb').write(im.tobytes('raw','RGB'))"

out/cordos.bin: $(KERNEL64_OBJS) linker64.ld
	$(LD64) -T linker64.ld -m elf_x86_64 --build-id=none -o $@ $(KERNEL64_OBJS)

# Host-side freestanding user blob. Not linked into the kernel.
USER_CFLAGS64 := -std=c11 -ffreestanding -nostdlib -fno-builtin \
	-fno-stack-protector -fno-pie -m64 -mno-red-zone \
	-mno-mmx -mno-sse -mno-sse2 -Wall -Wextra -O2 -Iuser

out/user/syscall.o: user/libnos/syscall.S | build
	$(CC64) $(USER_CFLAGS64) -c $< -o $@

out/user/test_write.o: user/test_write.c user/libnos/syscall.h | build
	$(CC64) $(USER_CFLAGS64) -c $< -o $@

out/user_hello.elf: out/user/test_write.o out/user/syscall.o user/hello.ld
	$(LD64) -nostdlib -m elf_x86_64 -T user/hello.ld --build-id=none -o $@ \
		out/user/test_write.o out/user/syscall.o

out/user_hello_blob.o: src/proc/user_hello_blob.s out/user_hello.elf | build
	$(CC64) -c src/proc/user_hello_blob.s -o $@

userland: out/user_hello.elf

check-host: | build
	gcc -std=c11 -Wall -Wextra -Werror -O2 -fno-builtin \
		-I src/include -I src/include/kernel \
		tests/host/test_core.c src/kernel/string.c src/kernel/utf8.c \
		-o out/test_core
	out/test_core

check: out/cordos.bin check-host
	$(GRUB_FILE) --is-x86-multiboot2 $<

GRUBCFG ?= grub64.cfg

out/cordos.iso: out/cordos.bin $(GRUBCFG) out/splash.png | build
	mkdir -p out/isoroot/boot/grub
	cp out/cordos.bin out/isoroot/boot/cordos.bin
	cp out/splash.png out/isoroot/boot/splash.png
	cp $(GRUBCFG) out/isoroot/boot/grub/grub.cfg
	$(GRUB_MKRESCUE) -o $@ out/isoroot -- -volid CORDOS

dist/cordos.iso: out/cordos.iso | build
	mkdir -p dist
	cp out/cordos.iso dist/cordos.iso

run: dist/cordos.iso
	$(QEMU) -cdrom $< -vga std

# 16 MiB zeroed raw disk. NOSF is formatted at first mount (LBA 2048).
out/persist.img: | build
	dd if=/dev/zero of=$@ bs=1048576 count=16 status=none

run-persist: dist/cordos.iso out/persist.img
	$(QEMU) -cdrom dist/cordos.iso -drive file=out/persist.img,format=raw,if=ide -boot order=d -vga std -serial stdio

run-vbox: dist/cordos.iso
	@if command -v VBoxManage >/dev/null 2>&1; then \
		bash scripts/run-vbox.sh; \
	else \
		echo "ISO lista: dist/cordos.iso"; \
		echo "En Windows (VirtualBox): .\\run-vbox.ps1"; \
	fi

out/mbr.bin: boot/mbr.s | build
	$(NASM) -f bin -o $@ $<

out/stage2.bin: boot/stage2.s | build
	$(NASM) -f bin -o $@ $<

mbr: out/mbr.bin out/stage2.bin
	@echo "mbr: $^ (NASM=$(NASM))"

out/disk.img: out/mbr.bin out/stage2.bin out/cordos.bin scripts/mkdisk.sh
	chmod +x scripts/mkdisk.sh
	scripts/mkdisk.sh out/cordos.bin out/disk.img

disk: out/disk.img

run-bios: out/disk.img
	@echo "run-bios: experimental BIOS MBR path (see docs/boot_protocol.md)"
	$(QEMU) -drive file=$<,format=raw,if=ide -boot order=c

clean:
	rm -rf out iso/boot

else

$(error Unsupported ARCH=$(ARCH). Use x86_64 or i386)

endif
