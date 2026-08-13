/* SPDX-License-Identifier: Apache-2.0
 *
 * `scalar_hello` — the scalar half of the Phase 4.5 handoff demonstration.
 *
 * Prints the four lines the plan requires and exits zero:
 *
 *     Hello from RISC-V VP++ RV32GCV
 *     XLEN=32
 *     hart_id=0
 *     SCALAR HELLO: PASS
 *
 * ── why this is more than four `puts` calls ─────────────────────────────────
 *
 * A program that printed those lines from string literals would pass the
 * demonstration on a simulator that had nothing behind it. Every value below is
 * read from somewhere and cross-checked against somewhere else before it is
 * printed, so the banner is a *result*:
 *
 *   * `XLEN` compares the toolchain's `__riscv_xlen` — what the compiler
 *     generated code for — against the XLEN the platform reports. A mismatch
 *     means the image and the model disagree about the machine, which is the
 *     exact failure a compiler handoff exists to catch.
 *   * `hart_id` compares the `mhartid` CSR — what the hart says — against the
 *     hart id the platform was configured with. `--hart-id 3` therefore has to
 *     reach the ISS, not just the banner.
 *   * `misa` is checked for every letter in the frozen `rv32gcv_zvl512b`
 *     string, so a build that silently fell back to a smaller ISA fails here
 *     rather than in a later example.
 *
 * ── and why it contains no vector instruction ───────────────────────────────
 *
 * Phase 4.5 requires this example to be real scalar-path evidence: compiled
 * with auto-vectorization off, and *verified* by disassembly to contain none.
 * `misa.V` is still checked — reading a CSR is not executing a vector
 * instruction, and the point of the check is that the machine offers V, not
 * that this image uses it. The `Makefile` enforces the absence.
 */

#include <stdint.h>

#include "host_io.h"

/* Check ids, reported as the exit status so a failure names itself. */
enum {
    CHECK_PASS = 0,
    CHECK_IDENTITY = 1,
    CHECK_ABI_VERSION = 2,
    CHECK_XLEN = 3,
    CHECK_HART_COUNT = 4,
    CHECK_HART_ID = 5,
    CHECK_MISA_BASE = 6,
    CHECK_MISA_VECTOR = 7,
    CHECK_RAM_WINDOW = 8,
    CHECK_ARITHMETIC = 9
};

static void fail(uint32_t check, const char *what, uint32_t got,
                 uint32_t expected)
{
    hio_puts("SCALAR HELLO: FAIL (");
    hio_puts(what);
    hio_puts(" is ");
    hio_put_hex32(got);
    hio_puts(", expected ");
    hio_put_hex32(expected);
    hio_putline(")");
    hio_exit(check);
}

/* A trivial scalar computation with a known answer.
 *
 * Its purpose is not arithmetic coverage — Phase 2 has that — but to make the
 * image execute a loop with real loads and stores, so the platform's TLM
 * counters see scalar data traffic and not only the console writes. `volatile`
 * keeps the optimiser from folding the whole thing to a constant, which at -O2
 * it otherwise will. */
static volatile uint32_t work[64];

static uint32_t scalar_workload(void)
{
    uint32_t sum = 0;
    unsigned i;

    for (i = 0; i < 64u; ++i) {
        work[i] = i * 3u + 1u;
    }
    for (i = 0; i < 64u; ++i) {
        sum += work[i];
    }
    return sum;
}

int main(void)
{
    const uint32_t identity = hio_read32(COMPILER_VP_ID_IDENTITY);
    const uint32_t abi = hio_read32(COMPILER_VP_ID_ABI_VERSION);
    const uint32_t host_xlen = hio_read32(COMPILER_VP_ID_XLEN);
    const uint32_t hart_count = hio_read32(COMPILER_VP_ID_HART_COUNT);
    const uint32_t host_hart_id = hio_read32(COMPILER_VP_ID_HART_ID);
    const uint32_t ram_base = hio_read32(COMPILER_VP_ID_RAM_BASE);
    const uint32_t ram_size = hio_read32(COMPILER_VP_ID_RAM_SIZE);
    const uint32_t hart_id = hio_csr_mhartid();
    const uint32_t misa = hio_csr_misa();
    uint32_t sum;

    /* Talking to the right platform at all. Everything after this is only
     * meaningful once the identity register has answered. */
    if (identity != (uint32_t)COMPILER_VP_IDENTITY_VALUE) {
        fail(CHECK_IDENTITY, "host identity", identity,
             (uint32_t)COMPILER_VP_IDENTITY_VALUE);
    }
    if (abi != (uint32_t)COMPILER_VP_ABI_VERSION) {
        fail(CHECK_ABI_VERSION, "host-I/O ABI version", abi,
             (uint32_t)COMPILER_VP_ABI_VERSION);
    }

    if (host_xlen != (uint32_t)__riscv_xlen) {
        fail(CHECK_XLEN, "XLEN", host_xlen, (uint32_t)__riscv_xlen);
    }
    if (hart_count != 1u) {
        fail(CHECK_HART_COUNT, "hart count", hart_count, 1u);
    }
    if (host_hart_id != hart_id) {
        fail(CHECK_HART_ID, "mhartid", hart_id, host_hart_id);
    }

    /* rv32imafdc: the letters the frozen `rv32gcv_zvl512b` string expands to,
     * minus V, which is checked separately so the two failures are told
     * apart. */
    {
        const uint32_t base = HIO_MISA_BIT('i') | HIO_MISA_BIT('m')
                              | HIO_MISA_BIT('a') | HIO_MISA_BIT('f')
                              | HIO_MISA_BIT('d') | HIO_MISA_BIT('c');
        if ((misa & base) != base) {
            fail(CHECK_MISA_BASE, "misa base extensions", misa & base, base);
        }
        if ((misa & HIO_MISA_BIT('v')) == 0u) {
            fail(CHECK_MISA_VECTOR, "misa.V", misa, HIO_MISA_BIT('v'));
        }
    }

    /* This code is running from the window the platform says it mapped. A
     * program linked for a different base would already have faulted, so the
     * value of the check is the diagnostic when the *sizes* disagree. */
    {
        const uint32_t here = (uint32_t)(uintptr_t)&main;
        if (here < ram_base || (here - ram_base) >= ram_size) {
            fail(CHECK_RAM_WINDOW, "text address", here, ram_base);
        }
    }

    sum = scalar_workload();
    /* sum of 3i+1 for i in [0,64) = 3*(63*64/2) + 64 = 6048 + 64 = 6112 */
    if (sum != 6112u) {
        fail(CHECK_ARITHMETIC, "scalar workload", sum, 6112u);
    }

    hio_putline("Hello from RISC-V VP++ RV32GCV");
    hio_put_kv_u32("XLEN", host_xlen);
    hio_put_kv_u32("hart_id", hart_id);
    hio_putline("SCALAR HELLO: PASS");

    return CHECK_PASS;
}
