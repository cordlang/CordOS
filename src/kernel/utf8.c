#include "utf8.h"

static int utf8_continuation(u8 value)
{
    return (value & 0xC0u) == 0x80u;
}

u32 utf8_decode_n(const char *text, size_t length, size_t *consumed)
{
    const u8 *bytes = (const u8 *)text;
    u8 first;
    u8 second;
    u8 third;
    u8 fourth;
    u32 codepoint;

    if (consumed != NULL) {
        *consumed = 0;
    }
    if (text == NULL || length == 0) {
        return 0;
    }

    first = bytes[0];
    if (first < 0x80u) {
        if (consumed != NULL) {
            *consumed = 1;
        }
        return first;
    }

    if (length >= 2 && first >= 0xC2u && first <= 0xDFu) {
        second = bytes[1];
        if (utf8_continuation(second)) {
            if (consumed != NULL) {
                *consumed = 2;
            }
            return ((u32)(first & 0x1Fu) << 6) | (u32)(second & 0x3Fu);
        }
    }

    if (length >= 3 && first >= 0xE0u && first <= 0xEFu) {
        second = bytes[1];
        third = bytes[2];
        if (utf8_continuation(third) &&
            ((first == 0xE0u && second >= 0xA0u && second <= 0xBFu) ||
             (first == 0xEDu && second >= 0x80u && second <= 0x9Fu) ||
             (first != 0xE0u && first != 0xEDu && utf8_continuation(second)))) {
            if (consumed != NULL) {
                *consumed = 3;
            }
            return ((u32)(first & 0x0Fu) << 12) |
                   ((u32)(second & 0x3Fu) << 6) |
                   (u32)(third & 0x3Fu);
        }
    }

    if (length >= 4 && first >= 0xF0u && first <= 0xF4u) {
        second = bytes[1];
        third = bytes[2];
        fourth = bytes[3];
        if (utf8_continuation(third) && utf8_continuation(fourth) &&
            ((first == 0xF0u && second >= 0x90u && second <= 0xBFu) ||
             (first == 0xF4u && second >= 0x80u && second <= 0x8Fu) ||
             (first != 0xF0u && first != 0xF4u && utf8_continuation(second)))) {
            if (consumed != NULL) {
                *consumed = 4;
            }
            codepoint = ((u32)(first & 0x07u) << 18) |
                        ((u32)(second & 0x3Fu) << 12) |
                        ((u32)(third & 0x3Fu) << 6) |
                        (u32)(fourth & 0x3Fu);
            return codepoint;
        }
    }

    if (consumed != NULL) {
        *consumed = 1;
    }
    return '?';
}

u32 utf8_decode(const char **text)
{
    const char *current;
    size_t available = 0;
    size_t used;

    if (text == NULL || *text == NULL || **text == '\0') {
        return 0;
    }

    current = *text;
    while (available < 4 && current[available] != '\0') {
        ++available;
    }

    used = 0;
    {
        u32 codepoint = utf8_decode_n(current, available, &used);
        if (used == 0) {
            used = 1;
        }
        *text = current + used;
        return codepoint;
    }
}

u32 utf8_encode(u32 codepoint, char out[4])
{
    if (out == NULL || codepoint > 0x10FFFFu ||
        (codepoint >= 0xD800u && codepoint <= 0xDFFFu)) {
        return 0;
    }

    if (codepoint <= 0x7Fu) {
        out[0] = (char)codepoint;
        return 1;
    }
    if (codepoint <= 0x7FFu) {
        out[0] = (char)(0xC0u | (codepoint >> 6));
        out[1] = (char)(0x80u | (codepoint & 0x3Fu));
        return 2;
    }
    if (codepoint <= 0xFFFFu) {
        out[0] = (char)(0xE0u | (codepoint >> 12));
        out[1] = (char)(0x80u | ((codepoint >> 6) & 0x3Fu));
        out[2] = (char)(0x80u | (codepoint & 0x3Fu));
        return 3;
    }

    out[0] = (char)(0xF0u | (codepoint >> 18));
    out[1] = (char)(0x80u | ((codepoint >> 12) & 0x3Fu));
    out[2] = (char)(0x80u | ((codepoint >> 6) & 0x3Fu));
    out[3] = (char)(0x80u | (codepoint & 0x3Fu));
    return 4;
}

u8 utf8_to_cp437(u32 codepoint)
{
    if (codepoint <= 0x7Fu) {
        return (u8)codepoint;
    }

    switch (codepoint) {
    case 0x00A1u: return 0xADu; /* inverted exclamation */
    case 0x00BFu: return 0xA8u; /* inverted question */
    case 0x00ABu: return 0xAEu;
    case 0x00BBu: return 0xAFu;
    case 0x00B0u: return 0xF8u;
    case 0x00B1u: return 0xF1u;
    case 0x00B5u: return 0xE6u;
    case 0x00B7u: return 0xFAu;
    case 0x00C0u: return (u8)'A';
    case 0x00C1u: return (u8)'A';
    case 0x00C2u: return (u8)'A';
    case 0x00C3u: return (u8)'A';
    case 0x00C4u: return 0x8Eu;
    case 0x00C5u: return 0x8Fu;
    case 0x00C6u: return 0x92u;
    case 0x00C7u: return 0x80u;
    case 0x00C8u: return (u8)'E';
    case 0x00C9u: return 0x90u;
    case 0x00CAu: return (u8)'E';
    case 0x00CBu: return (u8)'E';
    case 0x00CCu: return (u8)'I';
    case 0x00CDu: return (u8)'I';
    case 0x00CEu: return (u8)'I';
    case 0x00CFu: return (u8)'I';
    case 0x00D1u: return 0xA5u;
    case 0x00D2u: return (u8)'O';
    case 0x00D3u: return (u8)'O';
    case 0x00D4u: return (u8)'O';
    case 0x00D5u: return (u8)'O';
    case 0x00D6u: return 0x99u;
    case 0x00D7u: return (u8)'x';
    case 0x00D9u: return (u8)'U';
    case 0x00DAu: return (u8)'U';
    case 0x00DBu: return (u8)'U';
    case 0x00DCu: return 0x9Au;
    case 0x00DFu: return 0xE1u;
    case 0x00E0u: return 0x85u;
    case 0x00E1u: return 0xA0u;
    case 0x00E2u: return 0x83u;
    case 0x00E3u: return (u8)'a';
    case 0x00E4u: return 0x84u;
    case 0x00E5u: return 0x86u;
    case 0x00E6u: return 0x91u;
    case 0x00E7u: return 0x87u;
    case 0x00E8u: return 0x8Au;
    case 0x00E9u: return 0x82u;
    case 0x00EAu: return 0x88u;
    case 0x00EBu: return 0x89u;
    case 0x00ECu: return 0x8Du;
    case 0x00EDu: return 0xA1u;
    case 0x00EEu: return 0x8Cu;
    case 0x00EFu: return 0x8Bu;
    case 0x00F1u: return 0xA4u;
    case 0x00F2u: return 0x95u;
    case 0x00F3u: return 0xA2u;
    case 0x00F4u: return 0x93u;
    case 0x00F8u: return (u8)'o';
    case 0x00F6u: return 0x94u;
    case 0x00F7u: return 0xF6u;
    case 0x00F9u: return 0x97u;
    case 0x00FAu: return 0xA3u;
    case 0x00FBu: return 0x96u;
    case 0x00FCu: return 0x81u;
    case 0x00FDu: return (u8)'y';
    case 0x00FFu: return 0x98u;
    case 0x2013u: return (u8)'-';
    case 0x2014u: return (u8)'-';
    case 0x2018u: return (u8)'\'';
    case 0x2019u: return (u8)'\'';
    case 0x201Cu: return (u8)'"';
    case 0x201Du: return (u8)'"';
    case 0x2026u: return (u8)'.';
    default: return (u8)'?';
    }
}
