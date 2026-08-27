#include "string.h"

size_t strlen(const char *text)
{
    size_t length = 0;

    if (text == NULL) {
        return 0;
    }
    while (text[length] != '\0') {
        ++length;
    }

    return length;
}

void *memcpy(void *dest, const void *src, size_t length)
{
    u8 *d = (u8 *)dest;
    const u8 *s = (const u8 *)src;

    if (length == 0 || d == s) {
        return dest;
    }

#ifdef __x86_64__
    if ((((size_t)d ^ (size_t)s) & 7u) == 0) {
        /* Identical 8-byte phase. If both are 4-mod-8, one word then qwords. */
        if (((size_t)d & 7u) == 4u && length >= 4u) {
            *(u32 *)d = *(const u32 *)s;
            d += 4;
            s += 4;
            length -= 4u;
        }
        if (((size_t)d & 7u) == 0) {
            u64 *dw = (u64 *)d;
            const u64 *sw = (const u64 *)s;

            while (length >= sizeof(u64)) {
                *dw++ = *sw++;
                length -= sizeof(u64);
            }
            d = (u8 *)dw;
            s = (const u8 *)sw;
        }
    } else if ((((size_t)d | (size_t)s) & 3u) == 0) {
        u32 *dw = (u32 *)d;
        const u32 *sw = (const u32 *)s;

        while (length >= sizeof(u32)) {
            *dw++ = *sw++;
            length -= sizeof(u32);
        }
        d = (u8 *)dw;
        s = (const u8 *)sw;
    }
#else
    if ((((size_t)d | (size_t)s) & 3u) == 0) {
        u32 *dw = (u32 *)d;
        const u32 *sw = (const u32 *)s;

        while (length >= sizeof(u32)) {
            *dw++ = *sw++;
            length -= sizeof(u32);
        }
        d = (u8 *)dw;
        s = (const u8 *)sw;
    }
#endif

    while (length > 0u) {
        *d++ = *s++;
        --length;
    }

    return dest;
}

void *memset(void *dest, int value, size_t length)
{
    u8 *d = (u8 *)dest;
    u8 v = (u8)value;

    if (((size_t)d & 3u) == 0 && length >= 4u) {
        if (((size_t)d & 7u) != 0) {
            *(u32 *)d = (u32)v * 0x01010101u;
            d += 4;
            length -= 4u;
        }
#ifdef __x86_64__
        {
            u64 rep = (u64)v * 0x0101010101010101ull;
            u64 *dw = (u64 *)d;

            while (length >= sizeof(u64)) {
                *dw++ = rep;
                length -= sizeof(u64);
            }
            d = (u8 *)dw;
        }
#else
        {
            u32 rep = (u32)v * 0x01010101u;
            u32 *dw = (u32 *)d;

            while (length >= sizeof(u32)) {
                *dw++ = rep;
                length -= sizeof(u32);
            }
            d = (u8 *)dw;
        }
#endif
    }

    while (length > 0u) {
        *d++ = v;
        --length;
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

    if (dest == NULL) {
        return dest;
    }
    if (src == NULL) {
        dest[0] = '\0';
        return dest;
    }

    while (src[index] != '\0') {
        dest[index] = src[index];
        ++index;
    }

    dest[index] = '\0';
    return dest;
}
