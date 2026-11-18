/*
 * FX1 NPU N2: lane-A rich instructions on HasNpuTop through the reference
 * driver (tests/npu_common/npu_cases.h chain R1..R5: conv 3x3 + ReLU with
 * per-channel requant, conv 1x1 residual, MAX_POOL, AVG_POOL, ADD; each output
 * feeds a later instruction, so the queue must stage every entry after the
 * previous copy-back).
 *   A  a long MAX_POOL first (about 0.3 ms of model time), so the five chain
 *      pushes really wait in the queue (VP_STATUS queued 5, credits checked);
 *      polled to completion: ACCEPTED and RETIRED +6, no error, the batch
 *      counters show the model's MAC work; every output byte equals the C
 *      oracle (cross-checked with the drop's numpy reference on the host) and
 *      every other byte of the case window keeps its initial value
 *   B  the same again from a fresh image, completion by one interrupt (PLIC
 *      source 4 on hart 0, DONE) for the whole batch
 */
#include "npu_cases.h"
#include "npu_test.h"

#define WIN FX1_NPU_ARENA_SMALL_BASE
#define CTX0 FX1_PLIC_CONTEXT_HART_M(0)
/* The long job: [48][48][48] MAX_POOL 5 pad 2 (longer than the chain pushes), outside the case window. */
#define BIG_IN (WIN + 0x100000u)
#define BIG_OUT (WIN + 0x140000u)
#define BIG_WIN_SIZE 0x00180000u

static fx1_npu_t npu;
static npu_cases_t cases;
static uint8_t expect[NPU_CASE_WINDOW] __attribute__((aligned(8)));
static volatile uint32_t irq_count, irq_bits;

static uint32_t handler(uint32_t mcause, uint32_t mepc, uint32_t mtval)
{
    (void)mtval;
    if (mcause != (FX1_MCAUSE_INTERRUPT | FX1_IRQ_M_EXT)) fx1_fail(0xF0);
    const uint32_t id = fx1_plic_claim(CTX0);
    if (id == FX1_IRQ_NPU) {
        irq_bits |= fx1_npu_irq_ack(&npu);
        ++irq_count;
    }
    if (id) fx1_plic_complete(CTX0, id);
    return mepc;
}

static fx1_npu_instr_t big;

static void prepare(void)
{
    npu_cases_fill((uint8_t*)WIN);
    npu_cases_fill(expect);
    npu_cases_apply_rich(&cases, expect, WIN);
    volatile uint32_t* o = (volatile uint32_t*)BIG_OUT;
    for (uint32_t k = 0; k < big.out_bytes / 4; ++k) o[k] = 0xC3C3C3C3u;
}

static void submit_chain(uint32_t code)
{
    npu_expect(&npu, fx1_npu_rich_submit(&npu, &big, NPU_MS(10)), FX1_NPU_OK, code, "long MAX_POOL first");
    for (unsigned r = 0; r < NPU_RICH_COUNT; ++r)
        npu_expect(&npu, fx1_npu_rich_submit(&npu, &cases.instr[r], NPU_MS(10)), FX1_NPU_OK, code + 1 + r, cases.label[r]);
    const uint32_t vs = fx1_npu_bank_read(&npu, FX1_NPU_VP_STATUS);
    fx1_log_hex("VP_STATUS after the pushes", vs);
    fx1_check(((vs & FX1_NPU_VP_STATUS_QUEUED_MASK) >> FX1_NPU_VP_STATUS_QUEUED_SHIFT) == NPU_RICH_COUNT &&
                  fx1_npu_bank_read(&npu, FX1_NPU_VP_QUEUE_FREE) == npu.queue_depth - NPU_RICH_COUNT,
              code + 7, "the chain waits in the queue behind the long job");
}
static void check_big(uint32_t code)
{
    const volatile uint32_t* o = (const volatile uint32_t*)BIG_OUT;
    uint32_t changed = 0;
    for (uint32_t k = 0; k < big.out_bytes / 4; ++k) changed += o[k] != 0xC3C3C3C3u;
    fx1_check(changed > big.out_bytes / 8, code, "the long job wrote its output");
}

static void verify(uint32_t code, const char* part)
{
    const volatile uint8_t* w = (const volatile uint8_t*)WIN;
    for (unsigned r = 0; r < NPU_RICH_COUNT; ++r) {
        const fx1_npu_instr_t* in = &cases.instr[r];
        const uint32_t off = in->out - WIN, bad = npu_diff(w + off, expect + off, in->out_bytes);
        fx1_log(cases.label[r]);
        fx1_log_hex("  output bytes checked", in->out_bytes);
        fx1_log_hex("  differ from the oracle", bad);
        fx1_check(in->out_bytes > 0 && bad == 0, code + r, part);
    }
    const uint32_t stray = npu_diff(w, expect, NPU_CASE_WINDOW);
    fx1_log_hex("window bytes differing from the expected image (of 256 KiB)", stray);
    fx1_check(stray == 0, code + 8, "whole window equals the expected image (no stray write)");
}

int main(void)
{
    fx1_set_trap_handler(handler);
    npu_expect(&npu, npu_test_init(&npu, FX1_NPU_FEATURE_RICH_HAS | FX1_NPU_FEATURE_QUEUE | FX1_NPU_FEATURE_ABORT, 1),
               FX1_NPU_OK, 0x01, "probe: VP_ID, ABI 1.x, FEATURE");
    fx1_check(npu_cases_build(&cases, WIN), 0x02, "instructions built");
    const fx1_npu_pool_t bp = {.in = BIG_IN, .out = BIG_OUT, .c = 48, .h = 48, .w = 48, .k = 5, .pad = 2, .stride = 1};
    fx1_check(fx1_npu_build_pool(&big, &bp), 0x04, "long job built");
    npu_expect(&npu, fx1_npu_set_window(&npu, WIN, BIG_WIN_SIZE), FX1_NPU_OK, 0x03, "RICH window");

    /* A. polled. */
    prepare();
    const uint32_t acc0 = fx1_npu_accepted(&npu), ret0 = fx1_npu_retired(&npu);
    submit_chain(0x10);
    npu_expect(&npu, fx1_npu_rich_wait(&npu, NPU_MS(50)), FX1_NPU_OK, 0x18, "A: chain completed without error");
    fx1_check(fx1_npu_accepted(&npu) == acc0 + 6 && fx1_npu_retired(&npu) == ret0 + 6, 0x19, "A: ACCEPTED +6, RETIRED +6");
    check_big(0x1B);
    const uint32_t exec = fx1_npu_bank_read(&npu, FX1_NPU_PERF_EXEC_CYCLES), mac = fx1_npu_bank_read(&npu, FX1_NPU_PERF_MAC_OPS);
    fx1_log_hex("A: PERF_EXEC_CYCLES", exec);
    fx1_log_hex("A: PERF_MAC_OPS", mac);
    fx1_check(exec > 0 && mac > 0, 0x1A, "A: model counters show the work (HasNpuTop path)");
    verify(0x20, "A: output equals the oracle");
    fx1_log("npu: A rich chain R1..R5 polled");

    /* B. interrupt. */
    irq_count = irq_bits = 0;
    prepare();
    fx1_log_hex("B: stale PLIC claims dropped", npu_plic_route(CTX0));
    fx1_irq_enable(FX1_MIE_MEIE);
    fx1_global_irq_enable();
    submit_chain(0x30);
    const uint64_t t = fx1_mtime();
    while (!irq_count) fx1_check(fx1_mtime() - t < NPU_MS(50), 0x38, "B: DONE interrupt");
    fx1_check((irq_bits & FX1_NPU_IRQ_DONE) && !(irq_bits & FX1_NPU_IRQ_ERROR), 0x39, "B: DONE, no ERROR");
    npu_expect(&npu, fx1_npu_rich_wait(&npu, NPU_MS(1)), FX1_NPU_OK, 0x3A, "B: rich side idle, no error");
    fx1_check(fx1_npu_retired(&npu) == ret0 + 12, 0x3B, "B: RETIRED +6 again");
    check_big(0x3C);
    verify(0x40, "B: output equals the oracle");
    fx1_global_irq_disable();
    fx1_check(irq_count == 1, 0x48, "B: one interrupt for the batch");
    fx1_log("npu: B rich chain by interrupt");
    return 0;
}
