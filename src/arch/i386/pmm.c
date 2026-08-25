#include "pmm.h"
#include "string.h"
#include "vga.h"
#include "panic.h"

#define PMM_MAX_FRAMES (1024u * 256u) /* hasta 1 GiB */
#define MULTIBOOT_MEMORY_AVAILABLE 1u

extern u32 _kernel_start;
extern u32 _kernel_end;

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

static void pmm_mark_region_used(u32 base, u32 length)
{
    u32 start = base / PAGE_SIZE;
    u32 end = (base + length + PAGE_SIZE - 1u) / PAGE_SIZE;
    u32 frame;

    if (length == 0) {
        return;
    }

    for (frame = start; frame < end && frame < bitmap_frames; ++frame) {
        pmm_set_used(frame);
    }
}

static void pmm_mark_region_free(u32 base, u32 length)
{
    u32 start = (base + PAGE_SIZE - 1u) / PAGE_SIZE;
    u32 end = (base + length) / PAGE_SIZE;
    u32 frame;

    if (length < PAGE_SIZE || end <= start) {
        return;
    }

    for (frame = start; frame < end && frame < bitmap_frames; ++frame) {
        pmm_set_free(frame);
    }
}

void pmm_init(const struct multiboot_info *info)
{
    u32 max_addr = 0;
    u32 kernel_start = (u32)&_kernel_start;
    u32 kernel_end = (u32)&_kernel_end;
    u32 offset = 0;

    if (!(info->flags & MULTIBOOT_INFO_MMAP)) {
        panic("Multiboot sin mmap; no se puede iniciar PMM");
    }

    while (offset < info->mmap_length) {
        const struct multiboot_mmap_entry *entry =
            (const struct multiboot_mmap_entry *)(info->mmap_addr + offset);
        u32 end;

        if (entry->base_addr_high == 0 && entry->length_high == 0) {
            end = entry->base_addr_low + entry->length_low;
            if (end > max_addr) {
                max_addr = end;
            }
        }

        offset += entry->size + sizeof(entry->size);
    }

    if (max_addr == 0) {
        panic("mmap vacio");
    }

    bitmap_frames = max_addr / PAGE_SIZE;
    if (bitmap_frames > PMM_MAX_FRAMES) {
        bitmap_frames = PMM_MAX_FRAMES;
    }

    total_frames_os = bitmap_frames;
    used_frames_os = 0;
    free_frames_os = 0;
    memset(frame_bitmap, 0xFF, (bitmap_frames + 7u) / 8u);
    used_frames_os = bitmap_frames;
    free_frames_os = 0;

    offset = 0;
    while (offset < info->mmap_length) {
        const struct multiboot_mmap_entry *entry =
            (const struct multiboot_mmap_entry *)(info->mmap_addr + offset);

        if (entry->type == MULTIBOOT_MEMORY_AVAILABLE &&
            entry->base_addr_high == 0 &&
            entry->length_high == 0) {
            pmm_mark_region_free(entry->base_addr_low, entry->length_low);
        }

        offset += entry->size + sizeof(entry->size);
    }

    /* Reservar BIOS/IVT/VGA y el propio kernel. */
    pmm_mark_region_used(0, 0x100000);
    pmm_mark_region_used(kernel_start, kernel_end - kernel_start);
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

    return (void *)(frame * PAGE_SIZE);
}

void pmm_free_page(void *page)
{
    if (page == NULL) {
        return;
    }

    pmm_free_frame((u32)page / PAGE_SIZE);
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
