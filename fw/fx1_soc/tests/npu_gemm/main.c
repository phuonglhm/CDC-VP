/*
 * FX1 NPU N2: GEMM bank (INT8 x INT8 -> INT32) through the reference driver,
 * on hart 0, with an independent C oracle (npu_oracle.c):
 *   A  polling: K = 64, every int32 of C[32][32] checked
 *   B  interrupt: PLIC source 4 to hart 0 (priority, enable, threshold),
 *      MEIE; one claim, the handler acks the NPU (IRQ_STATUS W1C) and
 *      completes; the line is low afterwards
 *   C  mask / unmask: source 4 disabled in the PLIC, the job completes, the
 *      level stays pending without an interrupt; enabled again: one interrupt
 *   D  error interrupt: a source buffer outside RAM, IRQ_STATUS ERROR,
 *      LAST_ERROR INVALID_ADDRESS, the next job passes
 */
#include "npu_oracle.h"
#include "npu_test.h"

#define BUF_A (FX1_NPU_ARENA_SMALL_BASE + 0x00000u)
#define BUF_W (FX1_NPU_ARENA_SMALL_BASE + 0x10000u)
#define BUF_C (FX1_NPU_ARENA_SMALL_BASE + 0x20000u)
#define CTX0 FX1_PLIC_CONTEXT_HART_M(0)

static fx1_npu_t npu;
static int32_t expect[32 * 32];
static volatile uint32_t irq_count, irq_bits, irq_other;

static uint32_t handler(uint32_t mcause, uint32_t mepc, uint32_t mtval)
{
    (void)mtval;
    if (mcause != (FX1_MCAUSE_INTERRUPT | FX1_IRQ_M_EXT)) fx1_fail(0xF0);
    const uint32_t id = fx1_plic_claim(CTX0);
    if (id == FX1_IRQ_NPU) {
        irq_bits |= fx1_npu_irq_ack(&npu);
        ++irq_count;
    } else if (id) {
        ++irq_other;
    }
    if (id) fx1_plic_complete(CTX0, id);
    return mepc;
}

static uint32_t seed = 12345u;
static int8_t rnd8(void)
{
    seed = seed * 1103515245u + 12345u;
    return (int8_t)(seed >> 16);
}

static void make_job(fx1_npu_gemm_t* g, uint32_t k)
{
    volatile int8_t* a = (volatile int8_t*)BUF_A;
    volatile int8_t* w = (volatile int8_t*)BUF_W;
    static int8_t ha[32 * 992], hw[992 * 32];
    for (uint32_t i = 0; i < 32 * k; ++i) a[i] = ha[i] = rnd8();
    for (uint32_t i = 0; i < k * 32; ++i) w[i] = hw[i] = rnd8();
    volatile int32_t* c = (volatile int32_t*)BUF_C;
    for (uint32_t i = 0; i < 32 * 32; ++i) c[i] = 0x5A5A5A5A;
    npu_oracle_gemm_bank(ha, hw, k, expect);
    *g = (fx1_npu_gemm_t){.a = BUF_A, .w = BUF_W, .c = BUF_C, .k = k};
}

static uint32_t check_c(void)
{
    const volatile int32_t* c = (const volatile int32_t*)BUF_C;
    uint32_t bad = 0;
    for (uint32_t i = 0; i < 32 * 32; ++i) bad += c[i] != expect[i];
    return bad;
}

static int wait_irq(uint32_t count)
{
    const uint64_t start = fx1_mtime();
    while (irq_count < count)
        if (fx1_mtime() - start > NPU_MS(20)) return 0;
    return 1;
}

int main(void)
{
    fx1_set_trap_handler(handler);
    npu_expect(&npu, npu_test_init(&npu, FX1_NPU_FEATURE_NATIVE_GEMM | FX1_NPU_FEATURE_ABORT, 1), FX1_NPU_OK, 0x01,
               "probe: VP_ID, ABI 1.x, FEATURE");
    fx1_log_hex("npu: ABI", npu.abi);
    fx1_log_hex("npu: feature", npu.feature);
    fx1_npu_gemm_t g;

    /* A. polling, PLIC source disabled. */
    make_job(&g, 32);
    npu_expect(&npu, fx1_npu_gemm_start(&npu, &g), FX1_NPU_OK, 0x10, "A: start");
    npu_expect(&npu, fx1_npu_gemm_wait(&npu, NPU_MS(20)), FX1_NPU_OK, 0x11, "A: done");
    uint32_t bad = check_c();
    fx1_log_hex("A: INT32 outputs differing from the oracle (of 1024)", bad);
    fx1_check(bad == 0 && npu.native_config_valid, 0x12, "A: GEMM K=32 polled, 1024 INT32 match");
    fx1_check(fx1_npu_bank_read(&npu, FX1_NPU_CYCLE_COUNT) > 0, 0x13, "A: CYCLE_COUNT > 0");
    fx1_check(irq_count == 0, 0x14, "A: no interrupt taken");
    fx1_npu_irq_ack(&npu); /* the level of A was never wanted */
    fx1_log("npu: A GEMM polling");

    /* B. interrupt to hart 0. */
    fx1_log_hex("B: stale PLIC claims dropped", npu_plic_route(CTX0));
    fx1_irq_enable(FX1_MIE_MEIE);
    fx1_global_irq_enable();
    make_job(&g, 48);
    npu_expect(&npu, fx1_npu_gemm_start(&npu, &g), FX1_NPU_OK, 0x20, "B: start");
    fx1_check(wait_irq(1), 0x21, "B: NPU interrupt on hart 0");
    fx1_check(irq_count == 1 && (irq_bits & FX1_NPU_IRQ_DONE) && irq_other == 0, 0x22, "B: one claim of source 4, DONE");
    npu_expect(&npu, fx1_npu_gemm_poll(&npu), FX1_NPU_OK, 0x23, "B: STATUS DONE");
    bad = check_c();
    fx1_check(bad == 0, 0x24, "B: GEMM K=48 by interrupt, 1024 INT32 match");
    fx1_check(!(fx1_plic_pending() & (1u << FX1_IRQ_NPU)), 0x25, "B: line low after the ack");
    fx1_log("npu: B GEMM interrupt, claim/complete");

    /* C. masked in the PLIC, then unmasked. */
    fx1_plic_enable(CTX0, FX1_IRQ_NPU, 0);
    irq_bits = 0;
    make_job(&g, 64);
    npu_expect(&npu, fx1_npu_gemm_start(&npu, &g), FX1_NPU_OK, 0x30, "C: start");
    const uint64_t t = fx1_mtime();
    while (!(fx1_npu_bank_read(&npu, FX1_NPU_STATUS) & FX1_NPU_STATUS_DONE))
        fx1_check(fx1_mtime() - t < NPU_MS(20), 0x31, "C: done while masked");
    for (volatile int i = 0; i < 200; ++i) {
    }
    fx1_check(irq_count == 1 && (fx1_plic_pending() & (1u << FX1_IRQ_NPU)), 0x32,
              "C: masked: no interrupt, source 4 pending");
    fx1_plic_enable(CTX0, FX1_IRQ_NPU, 1);
    fx1_check(wait_irq(2) && (irq_bits & FX1_NPU_IRQ_DONE), 0x33, "C: unmasked: the pending interrupt is taken");
    npu_expect(&npu, fx1_npu_gemm_poll(&npu), FX1_NPU_OK, 0x34, "C: STATUS DONE");
    fx1_check(check_c() == 0 && irq_count == 2, 0x35, "C: K=64 matches, exactly two interrupts so far");
    fx1_log("npu: C PLIC mask / unmask");

    /* D. error interrupt. */
    irq_bits = 0;
    g.a = FX1_DDR_BASE + FX1_DDR_SIZE - 0x100u; /* 32 x K bytes cross the end of RAM */
    npu_expect(&npu, fx1_npu_gemm_start(&npu, &g), FX1_NPU_OK, 0x40, "D: start");
    fx1_check(wait_irq(3) && (irq_bits & FX1_NPU_IRQ_ERROR), 0x41, "D: ERROR interrupt");
    npu_expect(&npu, fx1_npu_gemm_poll(&npu), FX1_NPU_E_JOB, 0x42, "D: job error reported");
    fx1_check(npu.last.last_error == FX1_NPU_ERR_INVALID_ADDRESS, 0x43, "D: LAST_ERROR INVALID_ADDRESS");
    make_job(&g, 16);
    npu_expect(&npu, fx1_npu_gemm_start(&npu, &g), FX1_NPU_OK, 0x44, "D: next start");
    fx1_check(wait_irq(4), 0x45, "D: next job interrupt");
    npu_expect(&npu, fx1_npu_gemm_poll(&npu), FX1_NPU_OK, 0x46, "D: next job done");
    fx1_check(check_c() == 0, 0x47, "D: next job K=16 matches");
    fx1_log("npu: D error interrupt, next job passes");
    fx1_global_irq_disable();
    return 0;
}
