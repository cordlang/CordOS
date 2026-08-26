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
static u32 last_free_hint;
static u64 s_mb2_addr;
static u64 s_mb2_size;
static u64 s_kernel_start;
static u64 s_kernel_end;

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

    /*
     * Keep GRUB's handoff alive until FB/cmdline have read it. Do not free
     * here: fb_init() still walks the framebuffer tag after pmm_init().
     */
    s_mb2_addr = (u64)mb2_addr;
    s_mb2_size = mb2_header->total_size;
    s_kernel_start = kernel_start;
    s_kernel_end = kernel_end;
    pmm_mark_region_used(s_mb2_addr, s_mb2_size);

    last_free_hint = 0;
    while (last_free_hint < bitmap_frames && pmm_is_used(last_free_hint)) {
        last_free_hint++;
    }
}

void pmm_release_boot_info(void)
{
    if (s_mb2_size == 0) {
        return;
    }

    pmm_mark_region_free(s_mb2_addr, s_mb2_size);
    /* Low 1MB / kernel may overlap the tag blob; keep them reserved. */
    pmm_mark_region_used(0, 0x100000);
    if (s_kernel_end > s_kernel_start) {
        pmm_mark_region_used(s_kernel_start, s_kernel_end - s_kernel_start);
    }
    s_mb2_addr = 0;
    s_mb2_size = 0;
}

u32 pmm_alloc_frame(void)
{
    u32 start = last_free_hint;
    u32 frame;

    if (bitmap_frames == 0) {
        return (u32)-1;
    }
    if (start >= bitmap_frames) {
        start = 0;
    }

    frame = start;
    do {
        if (!pmm_is_used(frame)) {
            pmm_set_used(frame);
            last_free_hint = frame + 1u;
            if (last_free_hint >= bitmap_frames) {
                last_free_hint = 0;
            }
            return frame;
        }
        frame++;
        if (frame >= bitmap_frames) {
            frame = 0;
        }
    } while (frame != start);

    return (u32)-1;
}

u64 pmm_alloc_contiguous(u32 count)
{
    u32 start;
    u32 i;

    if (count == 0 || count > bitmap_frames) {
        return 0;
    }

    start = 0;
    while (start + count <= bitmap_frames) {
        u32 run = 0;

        while (run < count && !pmm_is_used(start + run)) {
            run++;
        }
        if (run == count) {
            for (i = 0; i < count; ++i) {
                pmm_set_used(start + i);
            }
            last_free_hint = start + count;
            if (last_free_hint >= bitmap_frames) {
                last_free_hint = 0;
            }
            return (u64)start * PAGE_SIZE;
        }
        /* First used frame in the window: skip past it. */
        start = start + run + 1u;
    }

    return 0;
}

void pmm_free_frame(u32 frame)
{
    pmm_set_free(frame);
    if (frame < last_free_hint) {
        last_free_hint = frame;
    }
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
