#ifndef NUEVOOS_SERIAL_H
#define NUEVOOS_SERIAL_H

#include "types.h"

#define SERIAL_COM1 0x3F8u

void serial_init(void);
void serial_putc(char character);
void serial_write(const char *text);
void serial_write_n(const char *data, size_t length);
void serial_print_hex(u32 value);
void serial_print_u32(u32 value);

#endif
