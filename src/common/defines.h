#pragma once

#define true 1
#define false 0
#define TRUE true
#define FALSE false
#ifndef NULL
#define NULL 0
#endif
#define auto __auto_type

typedef char bool;
typedef signed char i8;
typedef unsigned char u8;
typedef signed short i16;
typedef unsigned short u16;
typedef signed int i32;
typedef unsigned int u32;
typedef signed long long i64;
typedef unsigned long long u64;

typedef i64 isize;
typedef u64 usize;

/* Efficient min and max operations */
#define MIN(_a, _b)                                                            \
    ({                                                                         \
        typeof(_a) __a = (_a);                                                 \
        typeof(_b) __b = (_b);                                                 \
        __a <= __b ? __a : __b;                                                \
    })

#define MAX(_a, _b)                                                            \
    ({                                                                         \
        typeof(_a) __a = (_a);                                                 \
        typeof(_b) __b = (_b);                                                 \
        __a >= __b ? __a : __b;                                                \
    })

#define BIT(i) (1ull << (i))

#define NO_BSS __attribute__((section(".data")))
#define NO_RETURN __attribute__((noreturn))
#define INLINE inline __attribute__((unused))
#define ALWAYS_INLINE inline __attribute__((unused, always_inline))
#define NO_INLINE __attribute__((noinline))
#define NO_IPA __attribute__((noipa))

// NOTE: no_return will disable traps.
// NO_RETURN NO_INLINE void no_return();

/* Return the offset of `member` inside struct `type`. */
#define offset_of(type, member) ((usize)(&((type*)NULL)->member))

// 返回结构体成员member 所在的type类型结构体的指针
#define container_of(mptr, type, member)                                       \
    ({                                                                         \
        const typeof(((type*)NULL)->member)* _mptr = (mptr);                   \
        (type*)((u8*)_mptr - offset_of(type, member));                         \
    })

/* Return the largest c that c is a multiple of b and c <= a. */
static INLINE u64 round_down(u64 a, u64 b) { return a - a % b; }

/* Return the smallest c that c is a multiple of b and c >= a. */
static INLINE u64 round_up(u64 a, u64 b) { return round_down(a + b - 1, b); }

void _panic(const char*, int);
NO_INLINE NO_RETURN void _panic(const char*, int);

#define PANIC() _panic(__FILE__, __LINE__)

#define ASSERT(expr)                                                           \
    ({                                                                         \
        if (!(expr))                                                           \
            PANIC();                                                           \
    })
