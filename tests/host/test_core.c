#include <stdio.h>
#include <stdint.h>

/* Kernel string/utf8 objects are linked in. Prototypes match src/include. */
extern unsigned long strlen(const char *text);
extern void *memcpy(void *dest, const void *src, unsigned long length);
extern void *memset(void *dest, int value, unsigned long length);
extern int memcmp(const void *a, const void *b, unsigned long length);
extern char *strcpy(char *dest, const char *src);
extern uint32_t utf8_decode(const char **text);
extern uint32_t utf8_decode_n(const char *text, unsigned long length,
                              unsigned long *consumed);
extern uint32_t utf8_encode(uint32_t codepoint, char out[4]);

static int fails;

static void expect(int cond, const char *name)
{
    if (!cond) {
        fprintf(stderr, "FAIL %s\n", name);
        fails++;
    }
}

int main(void)
{
    char enc[8];
    const char *p;
    unsigned char buf[64];
    unsigned char dst[64];
    uint32_t n;
    unsigned long used;

    expect(strlen(NULL) == 0, "strlen NULL");
    expect(strlen("") == 0, "strlen empty");
    expect(strlen("cordos") == 6, "strlen");

    memset(buf, 0, sizeof(buf));
    memcpy(buf, "hello", 6);
    expect(memcmp(buf, "hello", 6) == 0, "memcpy");

    memcpy(buf + 4, "ABCDEFGH", 8);
    expect(memcmp(buf + 4, "ABCDEFGH", 8) == 0, "memcpy align4");

    memset(dst, 0xA5, 16);
    expect(dst[0] == 0xA5u && dst[15] == 0xA5u, "memset");

    memset(dst, 0, sizeof(dst));
    strcpy((char *)dst, "ok");
    expect(dst[0] == (unsigned char)'o' && dst[1] == (unsigned char)'k' &&
               dst[2] == 0,
           "strcpy");
    strcpy((char *)dst, NULL);
    expect(dst[0] == 0, "strcpy NULL src");

    memset(enc, 0, sizeof(enc));
    n = utf8_encode(0x00E1u, enc);
    expect(n == 2u, "utf8 encode len");
    p = enc;
    expect(utf8_decode(&p) == 0x00E1u, "utf8 decode");

    used = 0;
    expect(utf8_decode_n("A", 1, &used) == (uint32_t)'A' && used == 1,
           "utf8 ascii");
    expect(utf8_encode(0xD800u, enc) == 0, "utf8 surrogate rejected");
    expect(utf8_encode(0x110000u, enc) == 0, "utf8 too high rejected");

    if (fails != 0) {
        fprintf(stderr, "%d host tests failed\n", fails);
        return 1;
    }
    puts("host tests ok");
    return 0;
}
