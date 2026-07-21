/* SPDX-License-Identifier: Apache-2.0
 *
 * Minimal freestanding string routines. The firmware links with -nostdlib;
 * these cover the calls GCC may emit implicitly plus the kernel's needs.
 */

#include <stddef.h>
#include <stdint.h>

/*
 * Newlib's libm calls this hook when a range/domain error must set errno.
 * TFLM Softmax pulls expf(), but this freestanding image deliberately does
 * not link the rest of newlib. A single slot is sufficient because only the
 * console task invokes TFLM and no firmware decision depends on errno.
 */
int *__errno(void)
{
    static int value;

    return &value;
}

void *memset(void *dst, int value, size_t n)
{
    unsigned char *d = (unsigned char *)dst;

    while (n-- != 0u) {
        *d++ = (unsigned char)value;
    }
    return dst;
}

void *memcpy(void *dst, const void *src, size_t n)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;

    while (n-- != 0u) {
        *d++ = *s++;
    }
    return dst;
}

void *memmove(void *dst, const void *src, size_t n)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;

    if (d < s) {
        while (n-- != 0u) {
            *d++ = *s++;
        }
    } else if (d > s) {
        d += n;
        s += n;
        while (n-- != 0u) {
            *--d = *--s;
        }
    }
    return dst;
}

int memcmp(const void *a, const void *b, size_t n)
{
    const unsigned char *pa = (const unsigned char *)a;
    const unsigned char *pb = (const unsigned char *)b;

    while (n-- != 0u) {
        if (*pa != *pb) {
            return (int)*pa - (int)*pb;
        }
        ++pa;
        ++pb;
    }
    return 0;
}

size_t strlen(const char *s)
{
    const char *p = s;

    while (*p != '\0') {
        ++p;
    }
    return (size_t)(p - s);
}

int strcmp(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b) {
        ++a;
        ++b;
    }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n)
{
    while (n != 0u && *a != '\0' && *a == *b) {
        ++a;
        ++b;
        --n;
    }
    if (n == 0u) {
        return 0;
    }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

__attribute__((noreturn)) void abort(void)
{
    /* Reaching abort means an internal library invariant failed. Avoid
     * pulling hosted libc termination into the freestanding RTOS image. */
    for (;;) {
        __asm__ volatile("wfi");
    }
}
