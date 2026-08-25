#include "string.h"

size_t strlen(const char *text)
{
    size_t length = 0;

    while (text[length] != '\0') {
        ++length;
    }

    return length;
}

void *memcpy(void *dest, const void *src, size_t length)
{
    u8 *d = (u8 *)dest;
    const u8 *s = (const u8 *)src;
    size_t index;

#ifdef __x86_64__
    if ((((u64)d | (u64)s) & 7u) == 0) {
        u64 *dw = (u64 *)d;
        const u64 *sw = (const u64 *)s;

        while (length >= sizeof(u64)) {
            *dw++ = *sw++;
            length -= sizeof(u64);
        }
        d = (u8 *)dw;
        s = (const u8 *)sw;
    }
#endif

    for (index = 0; index < length; ++index) {
        d[index] = s[index];
    }

    return dest;
}

void *memset(void *dest, int value, size_t length)
{
    u8 *d = (u8 *)dest;
    size_t index;

    for (index = 0; index < length; ++index) {
        d[index] = (u8)value;
    }

    return dest;
}

int memcmp(const void *a, const void *b, size_t length)
{
    const u8 *left = (const u8 *)a;
    const u8 *right = (const u8 *)b;
    size_t index;

    for (index = 0; index < length; ++index) {
        if (left[index] != right[index]) {
            return (int)left[index] - (int)right[index];
        }
    }

    return 0;
}

char *strcpy(char *dest, const char *src)
{
    size_t index = 0;

    while (src[index] != '\0') {
        dest[index] = src[index];
        ++index;
    }

    dest[index] = '\0';
    return dest;
}
