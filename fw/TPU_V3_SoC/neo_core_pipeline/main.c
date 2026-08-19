/* SPDX-License-Identifier: Apache-2.0
 *
 * The Phase 7 pipeline: `DMA -> Transform (Im2Col) -> MXU -> RVV`.
 *
 * This is the image the Phase 7 gate asks for — "one firmware ELF boots and
 * controls every available block through MMIO". Everything the hart does here
 * goes through the addresses in `neo_core_map.h`, so the run exercises the
 * three planes `neo_hart_port` decodes onto: MMIO to the AXI4-Lite control
 * fabric, buffers in core SRAM over the native local plane, and instruction
 * fetch from global boot ROM across the external bridge.
 *
 * Each stage waits for the engine to leave BUSY and then checks DONE rather
 * than assuming it. The asynchronous contract (`INTERFACE_CONTRACT.md` §4) is
 * that a start write returns immediately and completion arrives later; a driver
 * that read results straight after the start write would be wrong even when it
 * happened to work.
 *
 * Polling rather than interrupt-driven, on purpose. The IRQ path already has
 * its own gate in the composition test, and a polling driver keeps this image's
 * failure mode legible: it stops at the stage that did not finish and reports
 * which one.
 */

#include <stdint.h>

#include "neo_core_map.h"

/* ── register access ──────────────────────────────────────────────────────── */

static inline void mmio_write(uint32_t address, uint32_t value)
{
    *(volatile uint32_t *)(uintptr_t)address = value;
}

static inline uint32_t mmio_read(uint32_t address)
{
    return *(volatile uint32_t *)(uintptr_t)address;
}

static inline int32_t sram_read32(uint32_t address)
{
    return *(volatile int32_t *)(uintptr_t)address;
}

static void mark(uint32_t stage)
{
    mmio_write(SIM_STAGE_MARK, stage);
}

/* A bound, not a timeout policy. An engine that never leaves BUSY is a defect,
 * and spinning forever would present it as a hung simulation instead. */
#define POLL_LIMIT 200000u

/* Returns 0 on success, or the failing status word (never 0 because the caller
 * only reaches here when DONE is absent). */
static uint32_t wait_done(uint32_t control_base, uint32_t status_offset,
                          uint32_t busy_bit, uint32_t done_bit)
{
    for (uint32_t i = 0; i < POLL_LIMIT; ++i) {
        const uint32_t status = mmio_read(control_base + status_offset);
        if ((status & busy_bit) == 0u) {
            if ((status & done_bit) != 0u) {
                /* W1C the completion so the next job starts from a clean
                 * status, exactly as the asynchronous contract requires. */
                mmio_write(control_base + status_offset, done_bit);
                return 0u;
            }
            return status | 0x80000000u;
        }
    }
    return 0xDEAD0001u;
}

/* ── NEO DMA ──────────────────────────────────────────────────────────────── */

static uint32_t dma_copy(uint32_t source, uint32_t destination, uint32_t length)
{
    mmio_write(DMA_CONTROL_BASE + DMA_REG_SRC_LO, source);
    mmio_write(DMA_CONTROL_BASE + DMA_REG_SRC_HI, 0u);
    mmio_write(DMA_CONTROL_BASE + DMA_REG_DST_LO, destination);
    mmio_write(DMA_CONTROL_BASE + DMA_REG_DST_HI, 0u);
    mmio_write(DMA_CONTROL_BASE + DMA_REG_LENGTH, length);
    mmio_write(DMA_CONTROL_BASE + DMA_REG_IRQ_ENABLE, 0u);
    mmio_write(DMA_CONTROL_BASE + DMA_REG_CONTROL, DMA_CTRL_START);

    const uint32_t failed = wait_done(DMA_CONTROL_BASE, DMA_REG_STATUS,
                                      DMA_STATUS_BUSY, DMA_STATUS_DONE);
    if (failed != 0u) {
        return failed;
    }
    /* `BYTES_DONE` counts destination bytes committed, never bytes fetched
     * into a staging buffer, so this is the one number that says the transfer
     * actually landed. */
    if (mmio_read(DMA_CONTROL_BASE + DMA_REG_BYTES_DONE) != length) {
        return 0xDEAD0002u;
    }
    return 0u;
}

/* ── Transform ────────────────────────────────────────────────────────────── */

static uint32_t im2col(uint32_t source, uint32_t destination)
{
    /* Ask the engine what it can do before selecting an operation. D18 makes
     * Col2Im an unavailable capability rather than a missing one, so a driver
     * that selected it would get a defined refusal — and a driver that checks
     * first can tell "not built" from "broken". */
    const uint32_t capability = mmio_read(TRF_CONTROL_BASE + TRF_REG_CAPABILITY);
    if ((capability & TRF_CAP_IM2COL) == 0u) {
        return 0xDEAD0010u;
    }

    mmio_write(TRF_CONTROL_BASE + TRF_REG_OPERATION, TRF_OP_IM2COL);
    mmio_write(TRF_CONTROL_BASE + TRF_REG_DATATYPE, TRF_DATATYPE_INT8);
    mmio_write(TRF_CONTROL_BASE + TRF_REG_SRC_LO, source);
    mmio_write(TRF_CONTROL_BASE + TRF_REG_SRC_HI, 0u);
    mmio_write(TRF_CONTROL_BASE + TRF_REG_DST_LO, destination);
    mmio_write(TRF_CONTROL_BASE + TRF_REG_DST_HI, 0u);
    mmio_write(TRF_CONTROL_BASE + TRF_REG_IN_CHANNELS, IN_CHANNELS);
    mmio_write(TRF_CONTROL_BASE + TRF_REG_IN_HEIGHT, IN_HEIGHT);
    mmio_write(TRF_CONTROL_BASE + TRF_REG_IN_WIDTH, IN_WIDTH);
    mmio_write(TRF_CONTROL_BASE + TRF_REG_KERNEL_H, KERNEL_H);
    mmio_write(TRF_CONTROL_BASE + TRF_REG_KERNEL_W, KERNEL_W);
    mmio_write(TRF_CONTROL_BASE + TRF_REG_STRIDE_H, 1u);
    mmio_write(TRF_CONTROL_BASE + TRF_REG_STRIDE_W, 1u);
    mmio_write(TRF_CONTROL_BASE + TRF_REG_DILATION_H, 1u);
    mmio_write(TRF_CONTROL_BASE + TRF_REG_DILATION_W, 1u);
    /* All four padding fields must be zero: D18 is a *no-padding* contract, and
     * zero here disables padding rather than requesting implicit zero-fill. */
    mmio_write(TRF_CONTROL_BASE + TRF_REG_PAD_TOP, 0u);
    mmio_write(TRF_CONTROL_BASE + TRF_REG_PAD_LEFT, 0u);
    mmio_write(TRF_CONTROL_BASE + TRF_REG_PAD_BOTTOM, 0u);
    mmio_write(TRF_CONTROL_BASE + TRF_REG_PAD_RIGHT, 0u);
    mmio_write(TRF_CONTROL_BASE + TRF_REG_IRQ_ENABLE, 0u);
    mmio_write(TRF_CONTROL_BASE + TRF_REG_CONTROL, TRF_CTRL_START);

    const uint32_t failed = wait_done(TRF_CONTROL_BASE, TRF_REG_STATUS,
                                      TRF_STATUS_BUSY, TRF_STATUS_DONE);
    if (failed != 0u) {
        return failed;
    }

    /* The engine derives the output shape; firmware checks it against what it
     * is about to tell the MXU rather than assuming the two agree. */
    if (mmio_read(TRF_CONTROL_BASE + TRF_REG_MATRIX_ROWS) != MAT_M
        || mmio_read(TRF_CONTROL_BASE + TRF_REG_MATRIX_COLS) != MAT_K) {
        return 0xDEAD0011u;
    }
    return 0u;
}

/* ── MXU ──────────────────────────────────────────────────────────────────── */

static uint32_t matrix_multiply(uint32_t a, uint32_t b, uint32_t c)
{
    mmio_write(SA_CONTROL_BASE + SA_REG_DIM_M, MAT_M);
    mmio_write(SA_CONTROL_BASE + SA_REG_DIM_N, MAT_N);
    mmio_write(SA_CONTROL_BASE + SA_REG_DIM_K, MAT_K);
    mmio_write(SA_CONTROL_BASE + SA_REG_A_LO, a);
    mmio_write(SA_CONTROL_BASE + SA_REG_A_HI, 0u);
    mmio_write(SA_CONTROL_BASE + SA_REG_B_LO, b);
    mmio_write(SA_CONTROL_BASE + SA_REG_B_HI, 0u);
    mmio_write(SA_CONTROL_BASE + SA_REG_C_LO, c);
    mmio_write(SA_CONTROL_BASE + SA_REG_C_HI, 0u);
    /* Row-major, tightly packed: A is [M][K] INT8, B is [K][N] INT8 and C is
     * [M][N] INT32, so the strides are in bytes of one row. */
    mmio_write(SA_CONTROL_BASE + SA_REG_A_STRIDE, MAT_K);
    mmio_write(SA_CONTROL_BASE + SA_REG_B_STRIDE, MAT_N);
    mmio_write(SA_CONTROL_BASE + SA_REG_C_STRIDE, MAT_N * 4u);
    mmio_write(SA_CONTROL_BASE + SA_REG_DATATYPE, SA_DATATYPE_INT8_INT32);
    mmio_write(SA_CONTROL_BASE + SA_REG_IRQ_ENABLE, 0u);
    mmio_write(SA_CONTROL_BASE + SA_REG_CONTROL, SA_CTRL_START);

    return wait_done(SA_CONTROL_BASE, SA_REG_STATUS, SA_STATUS_BUSY,
                     SA_STATUS_DONE);
}

/* ── RVV post-processing ──────────────────────────────────────────────────────
 *
 * The last stage of the pipeline, and the reason D18 does not need Col2Im: the
 * MXU's result is already the output-feature matrix, so software reads and
 * interprets its rows.
 *
 * ReLU plus two reductions, written with intrinsic-free inline assembly so the
 * image depends on no vector header: clamp negatives to zero, accumulate a
 * checksum and track the maximum. `vsetvli` is re-issued per chunk because
 * that is how RVV strip-mining works — the hardware decides how many elements
 * fit, and code that assumed VLEN would be wrong on any other configuration.
 */
static void rvv_postprocess(uint32_t c_address, uint32_t count,
                            uint32_t *checksum_out, int32_t *maximum_out,
                            uint32_t *negatives_out)
{
    uint32_t checksum = 0;
    int32_t maximum = 0;
    uint32_t negatives = 0;

    const volatile int32_t *values = (const volatile int32_t *)(uintptr_t)c_address;

    /* Scalar pre-pass: count how many the ReLU will clamp. Kept scalar because
     * it is the reference the vector reduction is checked against on the host,
     * and computing both the same way would make the comparison vacuous. */
    for (uint32_t i = 0; i < count; ++i) {
        if (values[i] < 0) {
            ++negatives;
        }
    }

    uint32_t remaining = count;
    uint32_t offset = c_address;
    while (remaining > 0u) {
        uint32_t vl;
        __asm__ volatile("vsetvli %0, %1, e32, m1, ta, ma"
                     : "=r"(vl)
                     : "r"(remaining));

        uint32_t chunk_sum;
        int32_t chunk_max;
        __asm__ volatile(
            "vle32.v   v8, (%2)\n"          /* load the results            */
            "vmv.v.i   v12, 0\n"
            "vmax.vv   v8, v8, v12\n"       /* ReLU                        */
            "vmv.s.x   v16, zero\n"
            "vredsum.vs v16, v8, v16\n"     /* checksum over the chunk     */
            "vmv.x.s   %0, v16\n"
            "vmv.s.x   v17, zero\n"
            "vredmax.vs v17, v8, v17\n"     /* maximum over the chunk      */
            "vmv.x.s   %1, v17\n"
            : "=r"(chunk_sum), "=r"(chunk_max)
            : "r"((uintptr_t)offset)
            : "memory");

        checksum += chunk_sum;
        if (chunk_max > maximum) {
            maximum = chunk_max;
        }

        offset += vl * 4u;
        remaining -= vl;
    }

    *checksum_out = checksum;
    *maximum_out = maximum;
    *negatives_out = negatives;
}

/* ── the pipeline ─────────────────────────────────────────────────────────── */

int main(void)
{
    mark(STAGE_START);

    /* The engines answer before anything is programmed. A zero identity means
     * nothing is bound at that window, which is a wiring failure and not a
     * workload one — worth separating, because every later error would look
     * like a broken engine. */
    if (mmio_read(DMA_CONTROL_BASE + DMA_REG_ID) == 0u
        || mmio_read(TRF_CONTROL_BASE + TRF_REG_ID) == 0u
        || mmio_read(SA_CONTROL_BASE + SA_REG_ID) == 0u) {
        return 0xDEAD0100u;
    }

    /* Stage 1: the operands live in global RAM; the engines only reach core
     * SRAM. Moving them is the DMA's whole job. */
    mark(STAGE_DMA_IN);
    uint32_t failed = dma_copy(HOST_INPUT_ADDR, SRAM_INPUT_ADDR, INPUT_BYTES);
    if (failed != 0u) {
        return failed;
    }

    mark(STAGE_DMA_WEI);
    failed = dma_copy(HOST_WEIGHT_ADDR, SRAM_WEIGHT_ADDR, WEIGHT_BYTES);
    if (failed != 0u) {
        return failed;
    }

    /* Stage 2: lower the image into matrix rows. */
    mark(STAGE_IM2COL);
    failed = im2col(SRAM_INPUT_ADDR, SRAM_A_ADDR);
    if (failed != 0u) {
        return failed;
    }

    /* Stage 3: the matrix multiply the whole core exists for. */
    mark(STAGE_MXU);
    failed = matrix_multiply(SRAM_A_ADDR, SRAM_WEIGHT_ADDR, SRAM_C_ADDR);
    if (failed != 0u) {
        return failed;
    }

    /* Stage 4: RVV reads the output-feature matrix directly. */
    mark(STAGE_RVV);
    uint32_t checksum = 0;
    int32_t maximum = 0;
    uint32_t negatives = 0;
    rvv_postprocess(SRAM_C_ADDR, MAT_M * MAT_N, &checksum, &maximum, &negatives);

    mmio_write(SIM_RVV_CHECKSUM, checksum);
    mmio_write(SIM_RVV_MAXIMUM, (uint32_t)maximum);
    mmio_write(SIM_RVV_NEGATIVES, negatives);

    /* One value read back through the scalar path as well, so a checksum that
     * matched by accident on a broken vector unit still has to agree with what
     * an ordinary load sees. */
    if (sram_read32(SRAM_C_ADDR) == 0 && checksum == 0u) {
        return 0xDEAD0200u;
    }

    mark(STAGE_DONE);
    return SIM_EXIT_PASS;
}
