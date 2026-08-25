#ifndef NUEVOOS_INITRD_H
#define NUEVOOS_INITRD_H

#include "types.h"

/* Embedded NRD1 image in .rodata (see docs/nosfs.md). */
extern const u8 *initrd_blob;
extern const u32 initrd_blob_size;

#endif
