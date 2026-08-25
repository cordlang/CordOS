/*
 * Optional userland smoke test — do NOT link into the kernel.
 *
 *   make ARCH=x86_64 userland    # → out/user_hello.elf
 */

#include "libnos/syscall.h"

void _start(void)
{
    const char msg[] = "hello from user\n";
    nos_write(1, msg, sizeof(msg) - 1);
    nos_exit(0);
}
