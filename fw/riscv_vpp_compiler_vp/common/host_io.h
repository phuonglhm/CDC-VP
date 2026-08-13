/* SPDX-License-Identifier: Apache-2.0
 *
 * The freestanding host-I/O shim: `putchar`/`puts` and friends for images that
 * link no libc.
 *
 * Phase 4.5's non-goals are explicit that the first handoff requires no
 * libc/newlib, and that `Hello World` uses "a documented freestanding MMIO
 * putchar/puts shim". This is it. Every function here is a store or a load to
 * the window described in `compiler_vp/host_io_map.h`; there is no buffering on
 * the target side, no format-string engine and no heap.
 *
 * Names are prefixed `hio_` rather than being called `putchar`/`printf`. A
 * freestanding `putchar` that quietly is not C's `putchar` — no return value
 * contract, no `stdout`, no `EOF` — is the kind of near-miss that gets someone
 * to `#include <stdio.h>` next to it and link two different consoles.
 */

#ifndef CDC_VP_COMPILER_VP_FW_HOST_IO_H
#define CDC_VP_COMPILER_VP_FW_HOST_IO_H

#include <stdint.h>

#include "compiler_vp/host_io_map.h"

/* ── raw register access ─────────────────────────────────────────────────── */

static inline void hio_write32(uint32_t address, uint32_t value)
{
    *(volatile uint32_t *)address = value;
}

static inline uint32_t hio_read32(uint32_t address)
{
    return *(volatile uint32_t *)address;
}

/* ── console ─────────────────────────────────────────────────────────────── */

void hio_putchar(char c);
void hio_puts(const char *s);      /* no newline appended */
void hio_putline(const char *s);   /* newline appended */
void hio_put_u32(uint32_t value);  /* decimal, no padding */
void hio_put_hex32(uint32_t value);/* "0x" + 8 lower-case hex digits */
void hio_flush(void);

/* `name=value` followed by a newline. Every banner line the two examples are
 * required to print has this shape, and writing it once keeps the separator
 * from drifting between them. */
void hio_put_kv_u32(const char *name, uint32_t value);

/* ── exit ────────────────────────────────────────────────────────────────── */

/* Defined in crt0.S; declared here so C code can end a run early.
 * `status` 0 is a pass. Does not return. */
void hio_exit_raw(uint32_t kind, uint32_t status, uint32_t mcause,
                  uint32_t mepc) __attribute__((noreturn));

static inline void hio_exit(uint32_t status) __attribute__((noreturn));
static inline void hio_exit(uint32_t status)
{
    hio_exit_raw(COMPILER_VP_EXIT_KIND_NORMAL, status, 0, 0);
}

/* ── measurement window ──────────────────────────────────────────────────── */

/* Open a measurement window. `id` must be non-zero. */
static inline void hio_mark_begin(uint32_t id)
{
    hio_write32(COMPILER_VP_TRACE_MARK, id);
}

/* State the minimum number of RAM *data* accesses the open window must have
 * caused. The platform fails the run if the window closes below it. */
static inline void hio_expect_accesses(uint32_t count)
{
    hio_write32(COMPILER_VP_EXPECT_ACCESSES, count);
}

static inline void hio_mark_end(void)
{
    hio_write32(COMPILER_VP_TRACE_MARK, 0);
}

/* ── the hart's own view ─────────────────────────────────────────────────────
 *
 * Read from CSRs, not from the host-I/O identity block. The two are compared
 * against each other by the examples; reading one of them twice would compare
 * nothing.
 */

static inline uint32_t hio_csr_mhartid(void)
{
    uint32_t value;
    __asm__ volatile("csrr %0, mhartid" : "=r"(value));
    return value;
}

static inline uint32_t hio_csr_misa(void)
{
    uint32_t value;
    __asm__ volatile("csrr %0, misa" : "=r"(value));
    return value;
}

static inline uint32_t hio_csr_vlenb(void)
{
    uint32_t value;
    __asm__ volatile("csrr %0, vlenb" : "=r"(value));
    return value;
}

/* misa bit for extension letter `x` (lower case). */
#define HIO_MISA_BIT(x) (1u << ((x) - 'a'))

#endif /* CDC_VP_COMPILER_VP_FW_HOST_IO_H */
