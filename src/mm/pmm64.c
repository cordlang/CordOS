#include "pmm.h"
#include "multiboot2.h"
#include "string.h"
#include "vga.h"
#include "panic.h"

#define PMM_MAX_FRAMES (1024u * 256u) /* hasta 1 GiB (cubre identity-map de boot) */

extern char _kernel_start[];
extern char _kernel_end[];

volatile u32 total_frames_os = 0;
volatile u32 used_frames_os = 0;
volatile u32 free_frames_os = 0;

static u8 frame_bitmap[(PMM_MAX_FRAMES + 7u) / 8u];
static u32 bitmap_frames;

static void pmm_set_used(u32 frame)
{
    if (frame >= bitmap_frames) {
        return;
    }

    if (!(frame_bitmap[frame / 8u] & (1u << (frame % 8u)))) {
        frame_bitmap[frame / 8u] |= (u8)(1u << (frame % 8u));
        ++used_frames_os;
        --free_frames_os;
    }
}

static void pmm_set_free(u32 frame)
{
    if (frame >= bitmap_frames) {
        return;
    }

    if (frame_bitmap[frame / 8u] & (1u << (frame % 8u))) {
        frame_bitmap[frame / 8u] &= (u8)~(1u << (frame % 8u));
        --used_frames_os;
        ++free_frames_os;
    }
}

static bool pmm_is_used(u32 frame)
{
    if (frame >= bitmap_frames) {
        return true;
    }

    return (frame_bitmap[frame / 8u] & (1u << (frame % 8u))) != 0;
}

static void pmm_mark_region_used(u64 base, u64 length)
{
    u64 start;
    u64 end;
    u64 frame;

    if (length == 0) {
        return;
    }

    start = base / PAGE_SIZE;
    end = (base + length + PAGE_SIZE - 1u) / PAGE_SIZE;

    for (frame = start; frame < end && frame < bitmap_frames; ++frame) {
        pmm_set_used((u32)frame);
    }
}

static void pmm_mark_region_free(u64 base, u64 length)
{
    u64 start;
    u64 end;
    u64 frame;

    if (length < PAGE_SIZE) {
        return;
    }

    start = (base + PAGE_SIZE - 1u) / PAGE_SIZE;
    end = (base + length) / PAGE_SIZE;
    if (end <= start) {
        return;
    }

    for (frame = start; frame < end && frame < bitmap_frames; ++frame) {
        pmm_set_free((u32)frame);
    }
}

void pmm_init(void *mb2_addr)
{
    const struct multiboot2_info_header *mb2_header;
    const struct multiboot2_tag *tag;
    const struct multiboot2_tag_mmap *mmap;
    const u8 *entries;
    const u8 *entries_end;
    u64 max_addr = 0;
    u64 kernel_start = (u64)_kernel_start;
    u64 kernel_end = (u64)_kernel_end;

    if (mb2_addr == NULL) {
        panic("PMM: Multiboot2 nulo");
    }

    mb2_header = (const struct multiboot2_info_header *)mb2_addr;
    if (mb2_header->total_size < sizeof(*mb2_header) + sizeof(struct multiboot2_tag)) {
        panic("PMM: cabecera Multiboot2 invalida");
    }

    tag = multiboot2_find_tag(mb2_addr, MULTIBOOT2_TAG_TYPE_MMAP);
    if (tag == NULL) {
        panic("Multiboot2 sin mmap; no se puede iniciar PMM");
    }

    mmap = (const struct multiboot2_tag_mmap *)tag;
    if (mmap->size < sizeof(*mmap) ||
        mmap->size > mb2_header->total_size ||
        mmap->entry_size < sizeof(struct multiboot2_mmap_entry)) {
        panic("Multiboot2 mmap entry_size invalido");
    }

    entries = (const u8 *)(mmap + 1);
    entries_end = (const u8 *)tag + mmap->size;

    while (entries + mmap->entry_size <= entries_end) {
        const struct multiboot2_mmap_entry *entry =
            (const struct multiboot2_mmap_entry *)entries;
        u64 end = entry->addr + entry->len;

        if (end > max_addr) {
            max_addr = end;
        }

        entries += mmap->entry_size;
    }

    if (max_addr == 0) {
        panic("mmap2 vacio");
    }

    /* Cap at 1 GiB: boot64 identity-maps that window with 2 MiB pages. */
    if (max_addr > (u64)PMM_MAX_FRAMES * PAGE_SIZE) {
        max_addr = (u64)PMM_MAX_FRAMES * PAGE_SIZE;
    }

    bitmap_frames = (u32)(max_addr / PAGE_SIZE);
    if (bitmap_frames > PMM_MAX_FRAMES) {
        bitmap_frames = PMM_MAX_FRAMES;
    }

    total_frames_os = bitmap_frames;
    memset(frame_bitmap, 0xFF, (bitmap_frames + 7u) / 8u);
    used_frames_os = bitmap_frames;
    free_frames_os = 0;

    entries = (const u8 *)(mmap + 1);
    while (entries + mmap->entry_size <= entries_end) {
        const struct multiboot2_mmap_entry *entry =
            (const struct multiboot2_mmap_entry *)entries;

        if (entry->type == MULTIBOOT2_MEMORY_AVAILABLE) {
            pmm_mark_region_free(entry->addr, entry->len);
        }

        entries += mmap->entry_size;
    }

    pmm_mark_region_used(0, 0x100000);
    pmm_mark_region_used(kernel_start, kernel_end - kernel_start);

    /* VMM allocates page-table frames from this pool. Keep GRUB's handoff
     * structure alive until every post-PMM initializer has consumed it. */
    pmm_mark_region_used((u64)mb2_addr, mb2_header->total_size);
}

u32 pmm_alloc_frame(void)
{
    u32 frame;

    for (frame = 0; frame < bitmap_frames; ++frame) {
        if (!pmm_is_used(frame)) {
            pmm_set_used(frame);
            return frame;
        }
    }

    return (u32)-1;
}

u64 pmm_alloc_contiguous(u32 count)
{
    u32 start;
    u32 i;

    if (count == 0) {
        return 0;
    }

    for (start = 0; start + count <= bitmap_frames; ++start) {
        bool ok = true;

        for (i = 0; i < count; ++i) {
            if (pmm_is_used(start + i)) {
                ok = false;
                break;
            }
        }

        if (ok) {
            for (i = 0; i < count; ++i) {
                pmm_set_used(start + i);
            }
            return (u64)start * PAGE_SIZE;
        }
    }

    return 0;
}

void pmm_free_frame(u32 frame)
{
    pmm_set_free(frame);
}

void *pmm_alloc_page(void)
{
    u32 frame = pmm_alloc_frame();

    if (frame == (u32)-1) {
        return NULL;
    }

    return (void *)((u64)frame * PAGE_SIZE);
}

void pmm_free_page(void *page)
{
    if (page == NULL) {
        return;
    }

    pmm_free_frame((u32)((u64)page / PAGE_SIZE));
}

void pmm_print_stats(void)
{
    vga_print("PMM frames total/used/free: ");
    vga_print_u32(total_frames_os);
    vga_print(" / ");
    vga_print_u32(used_frames_os);
    vga_print(" / ");
    vga_print_u32(free_frames_os);
    vga_print("\n");
}
