#ifndef CORDOS_PANIC_H
#define CORDOS_PANIC_H

void panic(const char *message) __attribute__((noreturn));
void halt_forever(void) __attribute__((noreturn));

#endif
