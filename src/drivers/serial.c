#include "serial.h"
#include "io.h"

/*
 * COM1 (0x3F8) early debug UART — 115200 8N1, polling TX.
 * Safe before heap/IRQ; no interrupt-driven RX for MVP.
 */

enum {
    SERIAL_DATA = 0,
    SERIAL_IER = 1,
    SERIAL_FCR = 2,
    SERIAL_LCR = 3,
    SERIAL_MCR = 4,
    SERIAL_LSR = 5,
    SERIAL_DLL = 0,
    SERIAL_DLH = 1,
    LSR_THR_EMPTY = 0x20
};

static u16 com1_port = SERIAL_COM1;
static bool serial_ready;

static int serial_tx_empty(void)
{
    return (inb(com1_port + SERIAL_LSR) & LSR_THR_EMPTY) != 0;
}

void serial_init(void)
{
    outb(com1_port + SERIAL_IER, 0x00); /* disable IRQs */
    outb(com1_port + SERIAL_LCR, 0x80); /* DLAB on */
    outb(com1_port + SERIAL_DLL, 0x01); /* 115200 baud divisor lo */
    outb(com1_port + SERIAL_DLH, 0x00); /* divisor hi */
    outb(com1_port + SERIAL_LCR, 0x03); /* 8N1, DLAB off */
    outb(com1_port + SERIAL_FCR, 0xC7); /* FIFO enable, clear, 14-byte */
    outb(com1_port + SERIAL_MCR, 0x0B); /* IRQs off, RTS/DSR set */

    /* Loopback self-check; still enable TX if QEMU skips loopback. */
    outb(com1_port + SERIAL_MCR, 0x1E);
    outb(com1_port + SERIAL_DATA, 0xAE);
    (void)inb(com1_port + SERIAL_DATA);

    outb(com1_port + SERIAL_MCR, 0x0F); /* normal RTS/DTR mode */
    serial_ready = true;
}

void serial_putc(char character)
{
    if (!serial_ready) {
        return;
    }

    if (character == '\n') {
        while (!serial_tx_empty()) {
        }
        outb(com1_port + SERIAL_DATA, '\r');
    }

    while (!serial_tx_empty()) {
    }
    outb(com1_port + SERIAL_DATA, (u8)character);
}

void serial_write(const char *text)
{
    if (text == NULL) {
        return;
    }

    while (*text != '\0') {
        serial_putc(*text);
        ++text;
    }
}

void serial_write_n(const char *data, size_t length)
{
    size_t i;

    if (data == NULL) {
        return;
    }

    for (i = 0; i < length; ++i) {
        serial_putc(data[i]);
    }
}

void serial_print_hex(u32 value)
{
    static const char digits[] = "0123456789ABCDEF";
    int shift;

    serial_write("0x");
    for (shift = 28; shift >= 0; shift -= 4) {
        serial_putc(digits[(value >> shift) & 0xFu]);
    }
}

void serial_print_u32(u32 value)
{
    char buf[11];
    int i = 0;
    u32 v = value;

    if (v == 0) {
        serial_putc('0');
        return;
    }

    while (v > 0 && i < (int)sizeof(buf)) {
        buf[i++] = (char)('0' + (v % 10u));
        v /= 10u;
    }

    while (i > 0) {
        serial_putc(buf[--i]);
    }
}
