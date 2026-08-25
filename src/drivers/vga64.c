#include "vga.h"
#include "fb_console.h"
#include "utf8.h"

enum {
    VGA_WIDTH = 80,
    VGA_HEIGHT = 25,
    VGA_STATUS_ROW = 24,
    VGA_COLOR = 0x0F,
    VGA_STATUS_COLOR = 0x1F
};

static volatile u16 *const vga = (volatile u16 *)(u64)0xB8000;
static u8 cursor_row;
static u8 cursor_column;

static void vga_put_raw(u8 row, u8 column, char character, u8 color)
{
    u32 index = (u32)row * VGA_WIDTH + (u32)column;
    vga[index] = ((u16)color << 8) | (u8)character;
    if (fb_console_is_enabled()) {
        fb_console_put_cell(row, column, character);
    }
}

static void vga_scroll_up(void)
{
    u32 row;
    u32 col;

    for (row = 1; row < VGA_STATUS_ROW; ++row) {
        for (col = 0; col < VGA_WIDTH; ++col) {
            vga[(row - 1) * VGA_WIDTH + col] = vga[row * VGA_WIDTH + col];
        }
    }

    for (col = 0; col < VGA_WIDTH; ++col) {
        vga_put_raw((u8)(VGA_STATUS_ROW - 1), (u8)col, ' ', VGA_COLOR);
    }

    cursor_row = (u8)(VGA_STATUS_ROW - 1);
    cursor_column = 0;
}

void vga_clear(void)
{
    u32 index;

    for (index = 0; index < VGA_WIDTH * VGA_HEIGHT; ++index) {
        vga[index] = ((u16)VGA_COLOR << 8) | ' ';
    }

    cursor_row = 0;
    cursor_column = 0;

    if (fb_console_is_enabled()) {
        u8 r;
        u8 c;
        for (r = 0; r < VGA_HEIGHT; ++r) {
            for (c = 0; c < VGA_WIDTH; ++c) {
                fb_console_put_cell(r, c, ' ');
            }
        }
    }
}

void vga_clear_row(u8 row)
{
    u8 col;

    for (col = 0; col < VGA_WIDTH; ++col) {
        vga_put_raw(row, col, ' ',
                    row == VGA_STATUS_ROW ? VGA_STATUS_COLOR : VGA_COLOR);
    }
}

void vga_putc(char character)
{
    if (character == '\r') {
        cursor_column = 0;
        return;
    }

    if (character == '\b') {
        if (cursor_column > 0) {
            --cursor_column;
            vga_put_raw(cursor_row, cursor_column, ' ', VGA_COLOR);
        } else if (cursor_row > 0) {
            --cursor_row;
            cursor_column = VGA_WIDTH - 1;
            vga_put_raw(cursor_row, cursor_column, ' ', VGA_COLOR);
        }
        return;
    }

    if (character == '\n') {
        cursor_column = 0;
        ++cursor_row;
    } else {
        vga_put_raw(cursor_row, cursor_column, character, VGA_COLOR);
        ++cursor_column;

        if (cursor_column >= VGA_WIDTH) {
            cursor_column = 0;
            ++cursor_row;
        }
    }

    if (cursor_row >= VGA_STATUS_ROW) {
        vga_scroll_up();
    }
}

void vga_put_codepoint(u32 codepoint)
{
    if (codepoint == '\t') {
        vga_putc(' ');
        vga_putc(' ');
        vga_putc(' ');
        vga_putc(' ');
        return;
    }

    if (codepoint == '\n' || codepoint == '\r' || codepoint == '\b') {
        vga_putc((char)codepoint);
        return;
    }

    vga_putc((char)utf8_to_cp437(codepoint));
}

void vga_print(const char *text)
{
    while (text != NULL && *text != '\0') {
        vga_put_codepoint(utf8_decode(&text));
    }
}

void vga_write_utf8(const char *text, size_t length)
{
    size_t offset = 0;

    if (text == NULL) {
        return;
    }

    while (offset < length) {
        size_t consumed = 0;
        u32 codepoint = utf8_decode_n(text + offset, length - offset, &consumed);

        if (consumed == 0) {
            break;
        }
        vga_put_codepoint(codepoint);
        offset += consumed;
    }
}

void vga_print_hex(u32 value)
{
    static const char digits[] = "0123456789ABCDEF";
    int shift;

    vga_print("0x");
    for (shift = 28; shift >= 0; shift -= 4) {
        vga_putc(digits[(value >> shift) & 0x0F]);
    }
}

void vga_print_u32(u32 value)
{
    char buffer[10];
    u32 index = 0;
    u32 temp = value;

    if (value == 0) {
        vga_putc('0');
        return;
    }

    while (temp > 0) {
        buffer[index++] = (char)('0' + (temp % 10));
        temp /= 10;
    }

    while (index > 0) {
        --index;
        vga_putc(buffer[index]);
    }
}

void vga_write_at(u8 row, u8 column, const char *text)
{
    u8 col = column;
    u8 color = (row == VGA_STATUS_ROW) ? VGA_STATUS_COLOR : VGA_COLOR;

    while (text != NULL && *text != '\0' && col < VGA_WIDTH) {
        u32 codepoint = utf8_decode(&text);
        vga_put_raw(row, col, (char)utf8_to_cp437(codepoint), color);
        ++col;
    }
}

void vga_write_u32_at(u8 row, u8 column, u32 value, u8 width)
{
    char buffer[11];
    u8 index = 0;
    u8 pos;
    u32 temp = value;
    u8 color = (row == VGA_STATUS_ROW) ? VGA_STATUS_COLOR : VGA_COLOR;

    do {
        buffer[index++] = (char)('0' + (temp % 10));
        temp /= 10;
    } while (temp > 0 && index < sizeof(buffer));

    while (index < width && index < sizeof(buffer)) {
        buffer[index++] = '0';
    }

    pos = column;
    while (index > 0 && pos < VGA_WIDTH) {
        --index;
        vga_put_raw(row, pos, buffer[index], color);
        ++pos;
    }
}

void vga_status(const char *text)
{
    u8 col;

    vga_clear_row(VGA_STATUS_ROW);
    vga_write_at(VGA_STATUS_ROW, 0, text);
    for (col = 0; col < VGA_WIDTH; ++col) {
        /* ensure blue bar background already set by clear_row */
        (void)col;
    }
}
