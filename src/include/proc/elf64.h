#ifndef CORDOS_ELF64_H
#define CORDOS_ELF64_H

#include "types.h"

#define ELF_LOAD_MAX (4u * 1024u * 1024u)

/* Static ET_EXEC linked at USER_IMAGE_BASE (user/hello.ld). */
int elf64_load(const void *blob, u32 size, u64 *entry_out, u64 *stack_out);

#endif
