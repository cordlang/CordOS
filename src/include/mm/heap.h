#ifndef CORDOS_HEAP_H
#define CORDOS_HEAP_H

#include "types.h"

extern volatile u32 heap_used_os;
extern volatile u32 heap_free_os;

void heap_init(void);
void *kmalloc(size_t size);
void kfree(void *ptr);
void heap_print_stats(void);

#endif
