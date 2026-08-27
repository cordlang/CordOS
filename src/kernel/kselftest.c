#include "phase9.h"
#include "heap.h"
#include "panic.h"
#include "serial.h"
#include "string.h"
#include "utf8.h"

extern const unsigned char user_hello_elf_start[];

static void fail(const char *why)
{
    serial_write("kselftest FAIL: ");
    serial_write(why);
    serial_write("\n");
    panic(why);
}

void kselftest_run(void)
{
    void *a;
    void *b;
    char enc[8];
    const char *p;
    u8 buf[32];
    u8 dst[32];
    u32 n;

    serial_write("kselftest: begin\n");

    if (strlen(NULL) != 0) {
        fail("strlen NULL");
    }
    if (strlen("ab") != 2) {
        fail("strlen");
    }

    memset(buf, 0, sizeof(buf));
    memcpy(buf, "hello", 6);
    if (memcmp(buf, "hello", 6) != 0) {
        fail("memcpy");
    }

    memset(dst, 0xA5, 8);
    if (dst[0] != 0xA5u || dst[7] != 0xA5u) {
        fail("memset");
    }

    memset(dst, 0, sizeof(dst));
    strcpy((char *)dst, "x");
    if (dst[0] != (u8)'x' || dst[1] != 0) {
        fail("strcpy");
    }

    memset(enc, 0, sizeof(enc));
    n = utf8_encode(0x00E1u, enc);
    if (n != 2u) {
        fail("utf8 encode");
    }
    p = enc;
    if (utf8_decode(&p) != 0x00E1u) {
        fail("utf8 decode");
    }

    a = kmalloc(64);
    b = kmalloc(128);
    if (a == NULL || b == NULL) {
        fail("kmalloc");
    }
    memset(a, 0x11, 64);
    memset(b, 0x22, 128);
    kfree(a);
    kfree(b);
    a = kmalloc(64);
    if (a == NULL) {
        fail("kmalloc reuse");
    }
    kfree(a);

    if (kmalloc((size_t)-1) != NULL) {
        fail("kmalloc overflow");
    }

    if (user_hello_elf_start[0] != 0x7Fu || user_hello_elf_start[1] != 'E' ||
        user_hello_elf_start[2] != 'L' || user_hello_elf_start[3] != 'F') {
        fail("embedded user ELF");
    }

    serial_write("kselftest: ok\n");
}
