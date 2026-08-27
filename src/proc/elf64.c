#include "elf64.h"
#include "pmm.h"
#include "serial.h"
#include "string.h"
#include "user.h"
#include "vmm.h"

#define ELF_MAGIC0      0x7Fu
#define ELF_CLASS64     2u
#define ELF_DATA2LSB    1u
#define ELF_EV_CURRENT  1u
#define ELF_ET_EXEC     2u
#define ELF_EM_X86_64   62u
#define ELF_PT_LOAD     1u
#define ELF_PH_MAX      16u
#define ELF_MEM_MAX     ELF_LOAD_MAX
#define ELF_STACK_PAGES 4u
#define ELF_TRACK_MAX   20u

struct elf64_ehdr {
    u8 ident[16];
    u16 type;
    u16 machine;
    u32 version;
    u64 entry;
    u64 phoff;
    u64 shoff;
    u32 flags;
    u16 ehsize;
    u16 phentsize;
    u16 phnum;
    u16 shentsize;
    u16 shnum;
    u16 shstrndx;
} __attribute__((packed));

struct elf64_phdr {
    u32 type;
    u32 flags;
    u64 offset;
    u64 vaddr;
    u64 paddr;
    u64 filesz;
    u64 memsz;
    u64 align;
} __attribute__((packed));

static u64 page_down(u64 addr)
{
    return addr & ~((u64)PAGE_SIZE - 1ull);
}

static u64 page_span(u64 addr, u64 len)
{
    u64 start;
    u64 end;

    if (len == 0) {
        return 0;
    }
    start = page_down(addr);
    end = (addr + len + PAGE_SIZE - 1ull) & ~((u64)PAGE_SIZE - 1ull);
    if (end < start) {
        return 0;
    }
    return end - start;
}

static u64 s_map_va[ELF_TRACK_MAX];
static u64 s_map_len[ELF_TRACK_MAX];
static u32 s_map_n;

static void elf64_unload(void)
{
    u32 i;

    for (i = 0; i < s_map_n; i++) {
        vmm_unmap_user_anon(s_map_va[i], s_map_len[i]);
    }
    s_map_n = 0;
}

static int elf_track(u64 va, u64 len)
{
    if (s_map_n >= ELF_TRACK_MAX) {
        return -1;
    }
    s_map_va[s_map_n] = va;
    s_map_len[s_map_n] = len;
    s_map_n++;
    return 0;
}

int elf64_load(const void *blob, u32 size, u64 *entry_out, u64 *stack_out)
{
    const u8 *raw;
    const struct elf64_ehdr *eh;
    u32 i;
    u64 stack_va;
    u64 stack_len;

    if (blob == NULL || entry_out == NULL || stack_out == NULL) {
        return -1;
    }
    if (size < sizeof(struct elf64_ehdr)) {
        serial_write("elf: too small\n");
        return -1;
    }

    raw = (const u8 *)blob;
    eh = (const struct elf64_ehdr *)raw;

    if (eh->ident[0] != ELF_MAGIC0 || eh->ident[1] != 'E' ||
        eh->ident[2] != 'L' || eh->ident[3] != 'F') {
        serial_write("elf: bad magic\n");
        return -1;
    }
    if (eh->ident[4] != ELF_CLASS64 || eh->ident[5] != ELF_DATA2LSB ||
        eh->ident[6] != ELF_EV_CURRENT) {
        serial_write("elf: not elf64 le\n");
        return -1;
    }
    if (eh->type != ELF_ET_EXEC || eh->machine != ELF_EM_X86_64) {
        serial_write("elf: not x86_64 exec\n");
        return -1;
    }
    if (eh->phentsize != sizeof(struct elf64_phdr) || eh->phnum == 0 ||
        eh->phnum > ELF_PH_MAX) {
        serial_write("elf: bad phdrs\n");
        return -1;
    }
    if (eh->phoff > size ||
        eh->phoff + (u64)eh->phnum * sizeof(struct elf64_phdr) > size) {
        serial_write("elf: phoff oob\n");
        return -1;
    }

    for (i = 0; i < eh->phnum; i++) {
        const struct elf64_phdr *ph;

        ph = (const struct elf64_phdr *)(raw + eh->phoff +
                                         (u64)i * sizeof(struct elf64_phdr));
        if (ph->type != ELF_PT_LOAD || ph->memsz == 0) {
            continue;
        }
        if (ph->filesz > ph->memsz || ph->memsz > ELF_MEM_MAX) {
            serial_write("elf: segment too big\n");
            return -1;
        }
        if (ph->vaddr < USER_IMAGE_BASE ||
            ph->vaddr + ph->memsz < ph->vaddr ||
            ph->vaddr + ph->memsz > USER_IMAGE_MAX) {
            serial_write("elf: vaddr not in user image window\n");
            return -1;
        }
        if (ph->offset > size || ph->filesz > size - ph->offset) {
            serial_write("elf: file oob\n");
            return -1;
        }
        if (page_span(ph->vaddr, ph->memsz) == 0) {
            return -1;
        }
    }
    if (eh->entry < USER_IMAGE_BASE || eh->entry >= USER_IMAGE_MAX) {
        serial_write("elf: bad entry\n");
        return -1;
    }

    /* Shared user window: drop the previous image so spawn/exec can remap. */
    elf64_unload();

    for (i = 0; i < eh->phnum; i++) {
        const struct elf64_phdr *ph;
        u64 map_va;
        u64 map_len;
        u32 extra;

        ph = (const struct elf64_phdr *)(raw + eh->phoff +
                                         (u64)i * sizeof(struct elf64_phdr));
        if (ph->type != ELF_PT_LOAD) {
            continue;
        }
        if (ph->memsz == 0) {
            continue;
        }

        map_va = page_down(ph->vaddr);
        map_len = page_span(ph->vaddr, ph->memsz);
        extra = PAGE_WRITE;
        (void)ph->flags;
        if (vmm_map_user_anon(map_va, map_len, extra) != 0) {
            serial_write("elf: map failed (RAM >= 1 GiB?)\n");
            elf64_unload();
            return -1;
        }
        if (elf_track(map_va, map_len) != 0) {
            elf64_unload();
            return -1;
        }
        memcpy((void *)ph->vaddr, raw + ph->offset, (size_t)ph->filesz);
    }

    stack_len = (u64)ELF_STACK_PAGES * PAGE_SIZE;
    stack_va = USER_STACK_TOP_ELF - stack_len;
    if (vmm_map_user_anon(stack_va, stack_len, PAGE_WRITE) != 0) {
        serial_write("elf: stack map failed\n");
        elf64_unload();
        return -1;
    }
    if (elf_track(stack_va, stack_len) != 0) {
        elf64_unload();
        return -1;
    }

    *entry_out = eh->entry;
    *stack_out = USER_STACK_TOP_ELF;
    return 0;
}
