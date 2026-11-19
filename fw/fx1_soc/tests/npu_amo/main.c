/*
 * FX1 NPU N4: CPU atomics against the NPU's DMA writes. NPU_DMA goes through
 * a WriteGuard (master 0x104): a write waits while an AMO or SC bracket is
 * open on its 64-byte granule, and on completion cancels the LR reservations
 * it overlaps (as the Ethernet DMA, tests/eth_amo).
 *
 * One image per cell, chosen at build time:
 *   NPU_AMO_SC  0: amoadd.w loop; 1: lr.w / addi / sc.w loop (one asm block)
 *
 * Fixture: a GEMM-bank job (K=32) whose C output starts 32 bytes into a
 * 4 KiB page, so its last copy-back transaction (the DMA splits at 4 KiB)
 * writes the first half of a granule whose second half holds the atomic word
 * and canaries. TRIALS times: the job runs while the hart loops on the word
 * until STATUS.DONE (interrupts masked: nothing but an NPU write can cancel a
 * reservation). Per trial: the output equals the oracle (no NPU write lost or
 * torn), the word holds exactly the successful atomics (none lost, no NPU byte
 * on it), the canaries are intact.
 *
 * The data checks alone do not prove an overlap. The ctest registration
 * requires "device writes held" > 0 for the AMO image (an NPU write waited
 * for an AMO bracket); the SC image requires SC failures (0x24 otherwise:
 * NPU writes between LR and SC). Negative controls with --npu-unguarded-dma:
 * the SC cell sees no failure (0x24), the AMO cell holds nothing.
 */
#include "fx1_fw.h"
#include "fx1_npu.h"
#include "npu_oracle.h"

#ifndef NPU_AMO_SC
#define NPU_AMO_SC 0
#endif

#define TRIALS 24u
#define K 32u
#define PAGE (FX1_NPU_ARENA_SMALL_BASE + 0x200000u)
#define GA (PAGE + 0x10000u)
#define GW (PAGE + 0x11000u)
#define GC (PAGE + 32u)                         /* 4096 bytes: [PAGE+32, PAGE+4128) */
#define LAST_GRANULE (PAGE + 4096u)             /* NPU bytes [+0, +32), free [+32, +64) */
#define WORD (LAST_GRANULE + 32u)
#define CANARY(i) (LAST_GRANULE + 36u + 4u * (i)) /* 7 words */
#define CANARY_VALUE 0x5AA5C33Cu

static fx1_npu_t npu;
static int8_t ta[32 * K], tw[K * 32];
static int32_t tc[32 * 32];

static inline uint32_t amoadd(uint32_t addr, uint32_t v)
{
    uint32_t old;
    __asm__ volatile("amoadd.w %0, %2, (%1)" : "=r"(old) : "r"(addr), "r"(v) : "memory");
    return old;
}
/* lr / a register-only delay of `spin` iterations / addi / sc as one block,
 * so the reservation is open most of the time: 0 stored, nonzero failed. */
static inline uint32_t lr_add_sc(uint32_t addr, uint32_t spin)
{
    uint32_t v, failed, n = spin;
    __asm__ volatile("lr.w %0, (%3)\n"
                     "1: addi %2, %2, -1\n"
                     "bnez %2, 1b\n"
                     "addi %0, %0, 1\n"
                     "sc.w %1, %0, (%3)\n"
                     : "=&r"(v), "=&r"(failed), "+r"(n)
                     : "r"(addr)
                     : "memory");
    return failed;
}
#define AMO_PER_POLL 32u /* back-to-back AMOs between two STATUS polls */
#define SC_SPIN 64u

int main(void)
{
    fx1_check(fx1_npu_init(&npu, FX1_NPU_VP_BASE, FX1_NPU_FEATURE_NATIVE_GEMM, 0, fx1_mtime, FX1_MTIME_HZ) == FX1_NPU_OK,
              0x01, "probe");
    uint32_t s = 0x2468ACE1u;
    volatile int8_t* a = (volatile int8_t*)GA;
    volatile int8_t* w = (volatile int8_t*)GW;
    for (uint32_t i = 0; i < 32 * K; ++i) {
        s = s * 1103515245u + 12345u;
        a[i] = ta[i] = (int8_t)(s >> 16);
        s = s * 1103515245u + 12345u;
        w[i] = tw[i] = (int8_t)(s >> 16);
    }
    npu_oracle_gemm_bank(ta, tw, K, tc);
    const fx1_npu_gemm_t g = {.a = GA, .w = GW, .c = GC, .k = K};
    uint32_t atomics = 0, sc_failed = 0;
    for (uint32_t t = 0; t < TRIALS; ++t) {
        volatile int32_t* c = (volatile int32_t*)GC;
        for (uint32_t i = 0; i < 32 * 32; ++i) c[i] = (int32_t)0xDEADBEEF;
        *(volatile uint32_t*)WORD = 0;
        for (uint32_t i = 0; i < 7; ++i) *(volatile uint32_t*)CANARY(i) = CANARY_VALUE ^ i;
        fx1_check(fx1_npu_gemm_start(&npu, &g) == FX1_NPU_OK, 0x10, "GEMM-bank job starts");
        uint32_t ok = 0;
        /* Atomics until the job's DONE; a few more ensure the last write is covered. */
        for (uint32_t tail = 8 + t; tail;) {
#if NPU_AMO_SC
            if (lr_add_sc(WORD, SC_SPIN) == 0) ++ok;
            else ++sc_failed;
#else
            for (uint32_t k = 0; k < AMO_PER_POLL; ++k) amoadd(WORD, 1u);
            ok += AMO_PER_POLL;
#endif
            if (fx1_npu_bank_read(&npu, FX1_NPU_STATUS) & FX1_NPU_STATUS_DONE) --tail;
        }
        fx1_check(fx1_npu_gemm_wait(&npu, (uint64_t)FX1_MTIME_HZ / 100u) == FX1_NPU_OK, 0x11, "job completed");
        uint32_t bad = 0;
        for (uint32_t i = 0; i < 32 * 32; ++i) bad += c[i] != tc[i];
        fx1_check(bad == 0, 0x20, "every NPU output word equals the oracle (no NPU write lost)");
        fx1_check(*(volatile uint32_t*)WORD == ok, 0x21, "the word holds every successful atomic (none lost)");
        for (uint32_t i = 0; i < 7; ++i)
            fx1_check(*(volatile uint32_t*)CANARY(i) == (CANARY_VALUE ^ i), 0x22, "canaries of the granule intact");
        atomics += ok;
    }
    fx1_log_hex("npu atomic: successful atomics", atomics);
#if NPU_AMO_SC
    fx1_log_hex("npu atomic: SC failed (NPU write between LR and SC)", sc_failed);
    fx1_check(sc_failed > 0, 0x24, "NPU writes cancelled reservations");
    fx1_log("npu atomic: LR/SC against NPU writes: no NPU DMA write lost, every atomic kept");
#else
    (void)sc_failed;
    fx1_log("npu atomic: AMO against NPU writes: no NPU DMA write lost, every atomic kept");
#endif
    return 0;
}
