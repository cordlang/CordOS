#ifndef NUEVOOS_PERSIST_H
#define NUEVOOS_PERSIST_H

#include "types.h"

/* Called from phase6_init after the disk (or initrd) root is chosen. */
void persist_init(void);

/* True when a writable NOSF volume is mounted. */
bool persist_available(void);

/* Store / load a u32 key. Known keys: lang, login_wp, desk_wp, icon_style. */
int persist_set_u32(const char *key, u32 value);
int persist_get_u32(const char *key, u32 *value);

#endif
