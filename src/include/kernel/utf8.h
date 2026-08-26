#ifndef CORDOS_UTF8_H
#define CORDOS_UTF8_H

#include "types.h"

/* Decode one codepoint and advance text. Invalid input becomes '?'. */
u32 utf8_decode(const char **text);

/* Decode a bounded buffer. consumed is always at least one when length > 0. */
u32 utf8_decode_n(const char *text, size_t length, size_t *consumed);

/* Encode one Unicode scalar value. Returns the number of bytes written. */
u32 utf8_encode(u32 codepoint, char out[4]);

/* VGA text mode uses the firmware's CP437 glyph table. */
u8 utf8_to_cp437(u32 codepoint);

#endif
