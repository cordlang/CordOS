#ifndef CORDOS_TYPES_H
#define CORDOS_TYPES_H

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

typedef signed char i8;
typedef short i16;
typedef int i32;
typedef long long i64;

#ifdef __x86_64__
typedef u64 size_t;
typedef i64 ssize_t;
#else
typedef u32 size_t;
typedef i32 ssize_t;
#endif

typedef u8 bool;

#define true 1
#define false 0
#define NULL ((void *)0)

#endif
