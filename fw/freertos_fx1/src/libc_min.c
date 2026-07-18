/* SPDX-License-Identifier: Apache-2.0
 *
 * Minimal freestanding string routines. The firmware links with -nostdlib;
 * these cover the calls GCC may emit implicitly plus the kernel's needs.
 */

#include <stddef.h>
#include <stdint.h>

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
