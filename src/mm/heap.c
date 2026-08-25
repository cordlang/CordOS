#include "heap.h"
#include "pmm.h"
#include "vmm.h"
#include "string.h"
#include "vga.h"
#include "panic.h"
#include "serial.h"

#define HEAP_MAGIC 0x48454150u
#define HEAP_ALIGN 8u
#define HEAP_INITIAL_PAGES 256u

/*
 * Early heap strategy (x86_64):
 *   Physical == virtual in the identity-mapped low window (boot + vmm_init).
 *   kmalloc returns pointers into contiguous PMM pages; no high HHDM / 0xD0000000
 *   mapping required. Documented in agent-notes/PHASE3_D.md.
 *
 * i386 keeps the classic mapped heap at 0xD0000000.
 */
#ifndef __x86_64__
#define HEAP_VIRT_BASE 0xD0000000u
#endif

struct heap_block {
    u32 magic;
    u32 size;
    bool free;
    struct heap_block *next;
};

volatile u32 heap_used_os = 0;
volatile u32 heap_free_os = 0;

static struct heap_block *heap_head;

static size_t align_up(size_t value, size_t alignment)
{
    return (value + alignment - 1u) & ~(alignment - 1u);
}

static void heap_split(struct heap_block *block, size_t size)
{
    size_t total = sizeof(struct heap_block) + size;
    struct heap_block *rest;

    if (block->size < size + sizeof(struct heap_block) + HEAP_ALIGN) {
        return;
    }

    rest = (struct heap_block *)((u8 *)block + total);
    rest->magic = HEAP_MAGIC;
    rest->size = block->size - size - sizeof(struct heap_block);
    rest->free = true;
    rest->next = block->next;

    block->size = (u32)size;
    block->next = rest;
}

void heap_init(void)
{
    u8 *base;
    size_t bytes = HEAP_INITIAL_PAGES * PAGE_SIZE;

#ifdef __x86_64__
    u64 phys = pmm_alloc_contiguous(HEAP_INITIAL_PAGES);

    if (phys == 0) {
        panic("heap: sin frames contiguos (identity)");
    }

    base = (u8 *)phys;
#else
    u32 page;

    base = (u8 *)HEAP_VIRT_BASE;

    for (page = 0; page < HEAP_INITIAL_PAGES; ++page) {
        void *frame = pmm_alloc_page();
        if (frame == NULL) {
            panic("heap: sin frames");
        }
        vmm_map_page(
            HEAP_VIRT_BASE + page * PAGE_SIZE,
            (u32)frame,
            PAGE_PRESENT | PAGE_WRITE
        );
    }
#endif

    heap_head = (struct heap_block *)base;
    heap_head->magic = HEAP_MAGIC;
    heap_head->size = (u32)(bytes - sizeof(struct heap_block));
    heap_head->free = true;
    heap_head->next = NULL;

    heap_used_os = 0;
    heap_free_os = heap_head->size;

    serial_write("Heap OK pages=");
    serial_print_u32(HEAP_INITIAL_PAGES);
    serial_write("\n");
}

void *kmalloc(size_t size)
{
    struct heap_block *block;

    if (size == 0) {
        return NULL;
    }

    size = align_up(size, HEAP_ALIGN);

    for (block = heap_head; block != NULL; block = block->next) {
        if (block->magic != HEAP_MAGIC) {
            panic("heap: magic corrupto en kmalloc");
        }

        if (block->free && block->size >= size) {
            heap_split(block, size);
            block->free = false;
            heap_free_os -= block->size;
            heap_used_os += block->size;
            return (void *)(block + 1);
        }
    }

    return NULL;
}

void kfree(void *ptr)
{
    struct heap_block *block;
    struct heap_block *current;

    if (ptr == NULL) {
        return;
    }

    block = ((struct heap_block *)ptr) - 1;
    if (block->magic != HEAP_MAGIC) {
        panic("heap: magic corrupto en kfree");
    }

    if (block->free) {
        panic("heap: double free");
    }

    block->free = true;
    heap_used_os -= block->size;
    heap_free_os += block->size;

    /* Merge hacia adelante. */
    for (current = heap_head; current != NULL; current = current->next) {
        if (current->free && current->next != NULL && current->next->free) {
            current->size += sizeof(struct heap_block) + current->next->size;
            current->next = current->next->next;
        }
    }
}

void heap_print_stats(void)
{
    serial_write("Heap used/free bytes: ");
    serial_print_u32(heap_used_os);
    serial_write(" / ");
    serial_print_u32(heap_free_os);
    serial_write("\n");
}
