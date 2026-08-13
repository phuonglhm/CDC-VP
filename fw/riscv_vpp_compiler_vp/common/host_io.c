/* SPDX-License-Identifier: Apache-2.0
 *
 * The freestanding host-I/O shim. See `host_io.h`.
 */

#include "host_io.h"

void hio_putchar(char c)
{
    hio_write32(COMPILER_VP_CONSOLE_DATA, (uint32_t)(unsigned char)c);
}

void hio_puts(const char *s)
{
    while (*s != '\0') {
        hio_putchar(*s++);
    }
}

void hio_putline(const char *s)
{
    hio_puts(s);
    hio_putchar('\n');
}

void hio_put_u32(uint32_t value)
{
    /* Ten digits is the widest a uint32_t can be. Built back-to-front into a
     * local buffer rather than by repeated division from the top, which would
     * need a table of powers and a leading-zero rule. */
    char digits[10];
    int count = 0;

    if (value == 0) {
        hio_putchar('0');
        return;
    }
    while (value != 0 && count < (int)sizeof(digits)) {
        digits[count++] = (char)('0' + (value % 10u));
        value /= 10u;
    }
    while (count > 0) {
        hio_putchar(digits[--count]);
    }
}

void hio_put_hex32(uint32_t value)
{
    static const char table[] = "0123456789abcdef";
    int shift;

    hio_puts("0x");
    for (shift = 28; shift >= 0; shift -= 4) {
        hio_putchar(table[(value >> shift) & 0xfu]);
    }
}

void hio_flush(void)
{
    hio_write32(COMPILER_VP_CONSOLE_FLUSH, 1);
}

void hio_put_kv_u32(const char *name, uint32_t value)
{
    hio_puts(name);
    hio_putchar('=');
    hio_put_u32(value);
    hio_putchar('\n');
}

/* ── the two libcalls a freestanding build can still emit ────────────────────
 *
 * `-ffreestanding -fno-builtin` stops GCC treating calls to these as builtins,
 * but it does not stop it *generating* them: a structure assignment or an array
 * initialiser can still lower to `memcpy`/`memset` with no call written
 * anywhere in the source. With `-nostdlib` there is nothing to link against and
 * the failure is an undefined reference at the very end of a build, which is
 * both late and confusing. Two obvious implementations cost less than the
 * explanation of why they are missing.
 */

void *memset(void *dest, int value, unsigned int count);
void *memcpy(void *dest, const void *src, unsigned int count);

void *memset(void *dest, int value, unsigned int count)
{
    unsigned char *p = (unsigned char *)dest;
    while (count-- != 0u) {
        *p++ = (unsigned char)value;
    }
    return dest;
}

void *memcpy(void *dest, const void *src, unsigned int count)
{
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    while (count-- != 0u) {
        *d++ = *s++;
    }
    return dest;
}
