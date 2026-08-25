#ifndef NUEVOOS_VGA_H
#define NUEVOOS_VGA_H

#include "types.h"

void vga_clear(void);
void vga_clear_row(u8 row);
void vga_putc(char character);
void vga_put_codepoint(u32 codepoint);
void vga_print(const char *text);
void vga_write_utf8(const char *text, size_t length);
void vga_print_hex(u32 value);
void vga_print_u32(u32 value);
void vga_write_at(u8 row, u8 column, const char *text);
void vga_write_u32_at(u8 row, u8 column, u32 value, u8 width);
void vga_status(const char *text);

#endif
