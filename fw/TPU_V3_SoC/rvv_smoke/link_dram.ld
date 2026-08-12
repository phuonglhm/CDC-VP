/* SPDX-License-Identifier: Apache-2.0
 *
 * Linker script for the Spike differential corpus image.
 *
 * Identical in spirit to `link.ld`, with one difference that is forced rather
 * than chosen: the image is linked at `0x8000_0000` instead of `0`.
 *
 * Spike places its reset vector and boot ROM at `0x0000_1000` unconditionally
 * (`riscv/platform.h`, `DEFAULT_RSTVEC`) and registers it on the same bus as
 * main memory, where the closest-lower-base lookup wins. A flat image based at
 * 0 therefore has its `.text` shadowed by the boot ROM from `0x1000` onward,
 * and the ELF load itself fails against a read-only device. There is no Spike
 * option that moves the boot ROM.
 *
 * `0x8000_0000` also happens to be the right answer independently: it is
 * Spike's `DRAM_BASE`, and it is `GLOBAL_RAM_OR_HBM` in the TPU_V3 address map,
 * so this image is linked where Phase 10 firmware will actually run. The three
 * VP++-only images keep `link.ld` and base 0 — they are gates that already
 * pass, and rebasing them at Phase 2 closure would put three green results at
 * risk to buy nothing. Phase 5 should move them.
 *
 * The extra content over `link.ld`:
 *
 *   `tohost` / `fromhost`     the HTIF exit protocol. fesvr finds them by
 *                             symbol and stops when `tohost` becomes non-zero;
 *                             the VP++ harness watches a write to the same
 *                             address. One exit path, two simulators.
 *
 *   `begin_signature` /       the canonical signature block. fesvr dumps
 *   `end_signature`           exactly this range with `+signature=`, and the
 *                             harness reads the same range out of its probe
 *                             memory. No address constant on either side.
 */

OUTPUT_ARCH(riscv)
ENTRY(_start)

MEMORY
{
    /* Must match both the harness probe memory and Spike's `-m` argument.
     * Deliberately exactly 1 MiB rather than Spike's 2 GiB default: the
     * mid-vector fault probe works by running off the end of memory, so the
     * end has to be in the same place on both models. */
    RAM (rwxa) : ORIGIN = 0x80000000, LENGTH = 1M
}

SECTIONS
{
    . = ORIGIN(RAM);

    .text : ALIGN(4)
    {
        KEEP(*(.text.init))     /* _start must be first: it is the ELF entry */
        *(.text .text.*)
        *(.rodata .rodata.*)
    } > RAM

    .data : ALIGN(8)
    {
        PROVIDE(__global_pointer$ = . + 0x800);
        *(.data .data.*)
        *(.sdata .sdata.*)
    } > RAM

    /* HTIF. Loaded, not NOLOAD: fesvr reads `tohost` before the program has
     * necessarily written it, and it must read zero rather than whatever the
     * memory model happened to hold. */
    .htif : ALIGN(8)
    {
        tohost = .;
        LONG(0); LONG(0);
        fromhost = .;
        LONG(0); LONG(0);
    } > RAM

    .bss (NOLOAD) : ALIGN(8)
    {
        __bss_start = .;
        *(.bss .bss.*)
        *(.sbss .sbss.*)
        *(COMMON)
        __bss_end = .;
    } > RAM

    /* 64-byte aligned: one RVV register at VLEN=512 is 64 bytes, and a
     * misaligned buffer turns an alignment bug into what looks like an
     * arithmetic one. */
    .vdata (NOLOAD) : ALIGN(64)
    {
        __vdata_start = .;
        *(.vdata .vdata.*)
        __vdata_end = .;
    } > RAM

    /* The compared region. Loaded and zero-initialised, so a word the firmware
     * never reaches reads as 0 on both models instead of as stale memory —
     * which would make an early abort look like a divergence.
     *
     * `begin_signature` is 4-byte aligned to match `+signature-granularity=4`;
     * fesvr slices the range into fixed-size lines from `begin_signature`, so
     * a misaligned start would split every word across two lines. */
    .signature : ALIGN(4)
    {
        begin_signature = .;
        KEEP(*(.signature))
        end_signature = .;
    } > RAM

    . = ALIGN(16);
    __stack_bottom = .;
    . = . + 0x4000;                 /* 16 KiB */
    __stack_top = .;

    /* Everything above must leave room below the top of RAM: the mid-vector
     * fault probe deliberately addresses the last few words and then runs past
     * them. Checked at build time by the `verify` target. */
    __ram_end = ORIGIN(RAM) + LENGTH(RAM);

    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
