/* SPDX-License-Identifier: Apache-2.0
 *
 * The firmware and host view of one standalone NEO-CORE benchmark run.
 *
 * Included by the guest C, by `crt0.S` (which is preprocessed) and by the host
 * runner, so there is exactly one definition of the protocol between them. Plan
 * §8 makes the generated firmware header Phase 10 work; until that generator
 * exists this is the second copy of the address map the plan warns about, and
 * the runner closes the gap the only way available — it asserts every constant
 * here against `tpu_v3/address_map.h` before it runs the image.
 *
 * Scope: D27. One core, chip index 0 and core index 0, `mhartid` 0, no chip
 * composition and no NoC.
 */

#ifndef TPU_V3_NEO_CORE_MICROBENCH_MAP_H
#define TPU_V3_NEO_CORE_MICROBENCH_MAP_H

/* ── identity ─────────────────────────────────────────────────────────────── */

#ifndef NEO_BENCH_CHIP_ID
#define NEO_BENCH_CHIP_ID 0u
#endif
#ifndef NEO_BENCH_CORE_ID
#define NEO_BENCH_CORE_ID 0u
#endif

#define BENCH_CHIP_ID        NEO_BENCH_CHIP_ID
#define BENCH_CORE_ID        NEO_BENCH_CORE_ID
#define BENCH_EXPECTED_MHARTID (BENCH_CHIP_ID * 2u + BENCH_CORE_ID)

#define GLOBAL_RAM_BASE      0x80000000u
#define BENCH_CHIP_BASE      (0xC0000000u + BENCH_CHIP_ID * 0x08000000u)
#define BENCH_CORE_BASE      (BENCH_CHIP_BASE + BENCH_CORE_ID * 0x02000000u)
#define BENCH_CORE_SRAM_BASE (BENCH_CORE_BASE + 0x00000000u)
#define BENCH_SA_CONTROL     (BENCH_CORE_BASE + 0x01010000u)
#define BENCH_DMA_CONTROL    (BENCH_CORE_BASE + 0x01020000u)

/* ── global RAM layout ────────────────────────────────────────────────────────
 *
 * The firmware's own `.data`, `.bss` and stack occupy the first 32 KiB, and the
 * host-staged buffers start after them. The split is enforced by the linker
 * script, not by arithmetic done here: `link.ld` carries an ASSERT that the
 * stack top stays below `BENCH_HOST_REGION_BASE`, so an image whose data grew
 * into the buffers fails to link instead of overwriting them at run time. The
 * runner asserts the two constants agree.
 */
#define BENCH_FIRMWARE_RAM_BYTES  0x8000u        /* 32 KiB */
#define BENCH_HOST_REGION_BASE    (GLOBAL_RAM_BASE + BENCH_FIRMWARE_RAM_BYTES)

#define BENCH_PARAMS_ADDR    (BENCH_HOST_REGION_BASE + 0x0000u)
#define BENCH_PARAMS_BYTES   0x1000u
#define BENCH_HOST_SRC_ADDR  (BENCH_HOST_REGION_BASE + 0x1000u)
#define BENCH_HOST_DST_ADDR  (BENCH_HOST_REGION_BASE + 0x11000u)
#define BENCH_HOST_BUF_BYTES 0x10000u            /* 64 KiB each */
#define BENCH_HOST_REGION_END (BENCH_HOST_DST_ADDR + BENCH_HOST_BUF_BYTES)

/* ── core SRAM layout ─────────────────────────────────────────────────────── */

#define BENCH_SRAM_SRC_ADDR  (BENCH_CORE_SRAM_BASE + 0x0000u)
#define BENCH_SRAM_DST_ADDR  (BENCH_CORE_SRAM_BASE + 0x10000u)
#define BENCH_SRAM_BUF_BYTES 0x10000u            /* 64 KiB each */

/* Sized for the largest *frozen* case of any benchmark, not for the one that
 * happens to be implemented.
 *
 * These were 16 KiB, chosen against MB1's 4 KiB largest case and a comment
 * claiming they left room for MB3 and MB4. They did not: the frozen MB4 case
 * `(M,N,K) = (64,64,256)` needs `64*256 + 256*64 = 32768` operand bytes, which
 * would have run the source buffer straight into the destination that begins
 * immediately after it — silently corrupting the operands of a case the plan
 * mandates.
 *
 * 64 KiB covers every frozen case with headroom, and the runner checks all of
 * them against these values at start-up rather than trusting this comment. The
 * check is what makes the sizing a property; the arithmetic above is only how
 * the number was chosen. */
#define BENCH_MAX_ELEMENTS   1024u

/* ── the parameter block the host stages before the run ───────────────────────
 *
 * One command-line invocation runs one fresh case (§11), so these are read once
 * at start-up and never change. Passing them in memory rather than compiling
 * ten images keeps one ELF — and therefore one recorded ELF hash — across every
 * case of a benchmark, which is what makes the hash in a result row identify
 * the code that ran rather than the case it ran.
 */
#define BENCH_PARAM_MAGIC       0x31424D42u   /* 'B','M','B','1' */

#define BENCH_PARAM_OFF_MAGIC        0x00u
#define BENCH_PARAM_OFF_BENCHMARK    0x04u
#define BENCH_PARAM_OFF_CASE_INDEX   0x08u
#define BENCH_PARAM_OFF_IMPL         0x0Cu
#define BENCH_PARAM_OFF_MODE         0x10u
#define BENCH_PARAM_OFF_ELEMENTS     0x14u
/* Bytes staged at the source and produced at the destination. Two fields, not
 * one: MB1 writes as many bytes as it reads, but MB2 reduces 2N elements to a
 * single INT32, so a shared `bytes` would make one of its two DMA legs the
 * wrong length. */
#define BENCH_PARAM_OFF_IN_BYTES     0x18u
#define BENCH_PARAM_OFF_FAULT_FLAGS  0x1Cu
#define BENCH_PARAM_OFF_HOST_SRC     0x20u
#define BENCH_PARAM_OFF_HOST_DST     0x24u
#define BENCH_PARAM_OFF_SRAM_SRC     0x28u
#define BENCH_PARAM_OFF_SRAM_DST     0x2Cu
#define BENCH_PARAM_OFF_OUT_BYTES    0x30u
#define BENCH_PARAM_OFF_M            0x34u
#define BENCH_PARAM_OFF_N            0x38u
#define BENCH_PARAM_OFF_K            0x3Cu
#define BENCH_PARAM_WORDS            16u

#define BENCH_ID_RELU        1u
#define BENCH_ID_VECTOR_DOT  2u
#define BENCH_ID_GEMV_RVV    3u
#define BENCH_ID_GEMV_MXU    4u
#define BENCH_ID_GEMM        5u

#define BENCH_IMPL_SCALAR    0u
#define BENCH_IMPL_RVV       1u
#define BENCH_IMPL_MXU       2u

#define BENCH_MODE_KERNEL    0u
#define BENCH_MODE_E2E       1u

/* Fault injection for the G2 negative controls.
 *
 * Dead code unless the host sets a bit, and the host sets one only when it was
 * asked for a control run with `--inject`. Every result row records
 * `fault_flags`, so a control can never be mistaken for a measurement.
 */
#define BENCH_FAULT_SKIP_DMA_IN   (1u << 0)
#define BENCH_FAULT_SKIP_DMA_OUT  (1u << 1)
#define BENCH_FAULT_SKIP_END_MARK (1u << 2)
#define BENCH_FAULT_CORRUPT_ARITH (1u << 3)
/* Read the input a second time inside the measured interval.
 *
 * A model-side mutation of the local-plane byte counter, not of the check that
 * reads it: the kernel really does move the extra bytes, the golden is still
 * correct, and only the algorithm's declared traffic notices. This is what
 * `kernel_local_bytes` exists to catch. */
#define BENCH_FAULT_EXTRA_READ    (1u << 4)
/* Skip the kernel stage marker.
 *
 * Without it a neighbouring span simply widens and the interval still adds up,
 * so `interval_accounted_ns` alone cannot see it. The expected stage sequence
 * can. */
#define BENCH_FAULT_SKIP_STAGE    (1u << 5)

/* ── NEO DMA registers (the Phase 4 frozen map) ───────────────────────────── */

#define DMA_REG_CONTROL      0x008u
#define DMA_REG_STATUS       0x00Cu
#define DMA_REG_SRC_LO       0x010u
#define DMA_REG_SRC_HI       0x014u
#define DMA_REG_DST_LO       0x018u
#define DMA_REG_DST_HI       0x01Cu
#define DMA_REG_LENGTH       0x020u
#define DMA_REG_IRQ_ENABLE   0x024u
#define DMA_REG_BYTES_DONE   0x02Cu
#define DMA_REG_ID           0x000u

#define DMA_CTRL_START       (1u << 0)
#define DMA_STATUS_BUSY      (1u << 0)
#define DMA_STATUS_DONE      (1u << 1)

/* ── Sauria MXU registers (the Phase 5 frozen map) ───────────────────────── */

#define SA_REG_ID            0x000u
#define SA_REG_CONTROL       0x008u
#define SA_REG_STATUS        0x00Cu
#define SA_REG_DIM_M         0x010u
#define SA_REG_DIM_N         0x014u
#define SA_REG_DIM_K         0x018u
#define SA_REG_A_LO          0x020u
#define SA_REG_A_HI          0x024u
#define SA_REG_B_LO          0x028u
#define SA_REG_B_HI          0x02Cu
#define SA_REG_C_LO          0x030u
#define SA_REG_C_HI          0x034u
#define SA_REG_A_STRIDE      0x038u
#define SA_REG_B_STRIDE      0x03Cu
#define SA_REG_C_STRIDE      0x040u
#define SA_REG_DATATYPE      0x044u
#define SA_REG_IRQ_ENABLE    0x048u
#define SA_REG_ERROR_CAUSE   0x04Cu
#define SA_REG_GEOMETRY      0x080u
#define SA_REG_CAPABILITY    0x084u

#define SA_CTRL_START        (1u << 0)
#define SA_STATUS_BUSY       (1u << 0)
#define SA_STATUS_DONE       (1u << 1)
#define SA_STATUS_ERROR      (1u << 2)
#define SA_DATATYPE_INT8_INT32 2u
#define SA_CAP_INT8_INT32    (1u << SA_DATATYPE_INT8_INT32)

#define BENCH_MXU_ROWS       64u
#define BENCH_MXU_COLUMNS    64u
#define BENCH_MATRIX_MAX_K   256u

/* ── simulator-only host I/O ──────────────────────────────────────────────────
 *
 * The same unmapped low-address window Phase 2, Phase 4.5 and the Phase 7
 * pipeline image use. Not a TPU_V3 architectural peripheral and it must not
 * leak into the SoC firmware ABI.
 *
 * The two measurement markers are ordinary stores to this window, so they cross
 * the hart's external path like any other. That places the marker store itself
 * inside the interval it opens or closes: a documented boundary skew of one
 * store each side, not a correction anyone should apply silently.
 */
#define SIM_BASE             0x000F0000u
#define SIM_EXIT_KIND        (SIM_BASE + 0u)
#define SIM_EXIT_STATUS      (SIM_BASE + 4u)
#define SIM_EXIT_MCAUSE      (SIM_BASE + 8u)
#define SIM_EXIT_MEPC        (SIM_BASE + 12u)
#define SIM_STAGE_MARK       (SIM_BASE + 16u)
#define SIM_MHARTID          (SIM_BASE + 20u)
#define SIM_MEASURE_BEGIN    (SIM_BASE + 24u)
#define SIM_MEASURE_END      (SIM_BASE + 28u)
#define SIM_GUEST_CHECKSUM   (SIM_BASE + 32u)
#define SIM_GUEST_ELEMENTS   (SIM_BASE + 36u)
#define SIM_VLENB            (SIM_BASE + 40u)
#define SIM_VLMAX            (SIM_BASE + 44u)
#define SIM_VL_FIRST         (SIM_BASE + 48u)
#define SIM_VL_LAST          (SIM_BASE + 52u)
#define SIM_VECTOR_ITERS     (SIM_BASE + 56u)
#define SIM_PARAM_ECHO       (SIM_BASE + 60u)
#define SIM_WORDS            16u

#define SIM_EXIT_KIND_NORMAL 0u
#define SIM_EXIT_KIND_TRAPPED 1u
#define SIM_EXIT_PASS        0u

/* Stage markers, so a failure names the stage that stopped. */
#define BENCH_STAGE_START     1u
#define BENCH_STAGE_PARAMS    2u
#define BENCH_STAGE_DMA_IN    3u
#define BENCH_STAGE_KERNEL    4u
#define BENCH_STAGE_DMA_OUT   5u
#define BENCH_STAGE_VERIFY    6u
#define BENCH_STAGE_DONE      7u

/* Guest-side failure codes. Distinct from zero so `status != 0` is a failure
 * whatever produced it. */
#define BENCH_ERR_MAGIC       0xBAD00001u
#define BENCH_ERR_ELEMENTS    0xBAD00002u
#define BENCH_ERR_BENCHMARK   0xBAD00003u
#define BENCH_ERR_IMPL        0xBAD00004u
#define BENCH_ERR_MODE        0xBAD00005u
#define BENCH_ERR_DMA_ID      0xBAD00010u
#define BENCH_ERR_DMA_STATUS  0xBAD00011u
#define BENCH_ERR_DMA_BYTES   0xBAD00012u
#define BENCH_ERR_OUT_BYTES   0xBAD00006u
#define BENCH_ERR_SHAPE       0xBAD00007u
#define BENCH_ERR_MXU_ID      0xBAD00020u
#define BENCH_ERR_MXU_STATUS  0xBAD00021u
#define BENCH_ERR_MXU_CAP     0xBAD00022u

#define TRAP_ACTION_ABORT     0u
#define TRAP_ACTION_RESUME    1u
#define TRAP_ACTION_SKIP      2u
#define TRAP_ACTION_RESUME_AT 3u

#endif /* TPU_V3_NEO_CORE_MICROBENCH_MAP_H */
