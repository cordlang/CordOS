#ifndef CORDOS_USER_H
#define CORDOS_USER_H

#include "types.h"

/*
 * Preferred ring-3 smoke addresses. Must stay below F5 USER_IDENTITY_END
 * (1 GiB) so SYS_WRITE copy_from_user accepts the trampoline buffer.
 * 256 MiB sits above a typical 128 MiB identity map and below that cap.
 */
#define USER_TEXT_BASE  0x0000000010000000ULL
#define USER_STACK_BASE 0x0000000010001000ULL
#define USER_STACK_TOP  (USER_STACK_BASE + 0x1000ULL)

void user_enter(u64 rip, u64 rsp);
void user_smoke(void);

#endif
