#ifndef CORDOS_STRING_H
#define CORDOS_STRING_H

#include "types.h"

size_t strlen(const char *text);
void *memcpy(void *dest, const void *src, size_t length);
void *memset(void *dest, int value, size_t length);
int memcmp(const void *a, const void *b, size_t length);
char *strcpy(char *dest, const char *src);

#endif
