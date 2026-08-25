#ifndef NUEVOOS_PANIC_H
#define NUEVOOS_PANIC_H

void panic(const char *message) __attribute__((noreturn));
void halt_forever(void) __attribute__((noreturn));

#endif
