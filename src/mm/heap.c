#include "heap.h"
#include "pmm.h"
#include "vmm.h"
#include "string.h"
#include "vga.h"
#include "panic.h"
#include "serial.h"
#ifdef __x86_64__
#include "spinlock.h"
#endif

#define HEAP_MAGIC 0x48454150u
#define HEAP_ALIGN 8u
#define HEAP_INITIAL_PAGES 32u
#define HEAP_GROW_PAGES    16u

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
static u32 heap_pages_os;
#ifdef __x86_64__
static spinlock_t heap_lock;
#define HEAP_LOCK()   spin_lock(&heap_lock)
#define HEAP_UNLOCK() spin_unlock(&heap_lock)
#else
#define HEAP_LOCK()   ((void)0)
#define HEAP_UNLOCK() ((void)0)
#endif

static size_t align_up(size_t value, size_t alignment)
{
    return (value + alignment - 1u) & ~(alignment - 1u);
}

static void heap_coalesce(void)
{
    struct heap_block *current = heap_head;

    while (current != NULL) {
        struct heap_block *nxt = current->next;
        u8 *end;

        if (!current->free || nxt == NULL || !nxt->free) {
            current = nxt;
            continue;
        }

        end = (u8 *)current + sizeof(struct heap_block) + current->size;
        if (end != (u8 *)nxt) {
            current = nxt;
            continue;
        }

        {
            u32 grow = (u32)sizeof(struct heap_block);

            if (nxt->size > 0xFFFFFFFFu - grow ||
                current->size > 0xFFFFFFFFu - (grow + nxt->size)) {
                current = nxt;
                continue;
            }
            current->size += grow + nxt->size;
        }
        current->next = nxt->next;
        heap_free_os += (u32)sizeof(struct heap_block);
    }
}

#ifdef __x86_64__
static int heap_append_region(u8 *base, size_t bytes)
{
    struct heap_block *block;
    struct heap_block *tail;

    if (base == NULL || bytes <= sizeof(struct heap_block) + HEAP_ALIGN) {
        return -1;
    }

    block = (struct heap_block *)base;
    block->magic = HEAP_MAGIC;
    block->size = (u32)(bytes - sizeof(struct heap_block));
    block->free = true;
    block->next = NULL;

    if (heap_head == NULL) {
        heap_head = block;
        heap_free_os = block->size;
        return 0;
    }

    tail = heap_head;
    while (tail->next != NULL) {
        tail = tail->next;
    }

    /* Grow the last free block if the new pages sit right after it. */
    if (tail->free) {
        u8 *end = (u8 *)tail + sizeof(struct heap_block) + tail->size;

        if (end == base) {
            if ((u32)bytes <= 0xFFFFFFFFu - tail->size &&
                (u32)bytes <= 0xFFFFFFFFu - heap_free_os) {
                tail->size += (u32)bytes;
                heap_free_os += (u32)bytes;
                return 0;
            }
        }
    }

    tail->next = block;
    heap_free_os += block->size;
    return 0;
}

static int heap_expand(size_t need)
{
    u32 pages;
    u64 phys;
    size_t bytes;

    pages = (u32)((need + sizeof(struct heap_block) + PAGE_SIZE - 1u) / PAGE_SIZE);
    if (pages < HEAP_GROW_PAGES) {
        pages = HEAP_GROW_PAGES;
    }

    phys = pmm_alloc_contiguous(pages);
    if (phys == 0) {
        return -1;
    }

    bytes = (size_t)pages * PAGE_SIZE;
    if (heap_append_region((u8 *)phys, bytes) < 0) {
        return -1;
    }
    heap_pages_os += pages;
    return 0;
}
#endif

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
    spinlock_init(&heap_lock);
#endif

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
    heap_pages_os = HEAP_INITIAL_PAGES;

    heap_used_os = 0;
    heap_free_os = heap_head->size;

    serial_write("Heap OK pages=");
    serial_print_u32(HEAP_INITIAL_PAGES);
    serial_write("\n");
}

static void *heap_try_alloc(size_t size)
{
    struct heap_block *block;

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

void *kmalloc(size_t size)
{
    void *p;

    if (size == 0) {
        return NULL;
    }

    HEAP_LOCK();
    size = align_up(size, HEAP_ALIGN);
    p = heap_try_alloc(size);
#ifdef __x86_64__
    if (p == NULL && heap_expand(size) == 0) {
        p = heap_try_alloc(size);
    }
#endif
    HEAP_UNLOCK();
    return p;
}

void kfree(void *ptr)
{
    struct heap_block *block;

    if (ptr == NULL) {
        return;
    }

    HEAP_LOCK();
    block = ((struct heap_block *)ptr) - 1;
    if (block->magic != HEAP_MAGIC) {
        HEAP_UNLOCK();
        panic("heap: magic corrupto en kfree");
    }

    if (block->free) {
        HEAP_UNLOCK();
        panic("heap: double free");
    }

    block->free = true;
    heap_used_os -= block->size;
    heap_free_os += block->size;
    heap_coalesce();
    HEAP_UNLOCK();
}

void heap_print_stats(void)
{
    serial_write("Heap used/free bytes: ");
    serial_print_u32(heap_used_os);
    serial_write(" / ");
    serial_print_u32(heap_free_os);
    serial_write("\n");
}
