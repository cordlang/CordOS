#ifndef NUEVOOS_LANG_SELECT_H
#define NUEVOOS_LANG_SELECT_H

#include "types.h"

/* Apply lang=es|en from Multiboot2 cmdline if present. Returns true if set. */
bool lang_try_cmdline(void *mb2_addr);

/* Interactive picker (blocks). Call after keyboard works. */
void lang_select_run(void);

#endif
