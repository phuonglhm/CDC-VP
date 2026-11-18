/*
 * FX1 NPU N2: DFC acceptance through the neo_lite register file (82 words,
 * IEN, START, doorbell 0x17), strict neo_lite RTL revision 1 (HAS_HOST=neo,
 * the model default):
 *   A  RESIDUAL (ADD) and MAX_POOL accepted and run: ACCEPTED / RETIRED +2,
 *      neo CONTROL.DONE and ISTATUS set, outputs equal the C oracle
 *      (cross-checked with the drop's numpy reference on the host); every
 *      other byte of the window keeps its initial value
 *   B  GEMM_FUSED through the register file (the drop's encoder, table
 *      checked on the host) and AVG_POOL: refused by the strict hardware,
 *      RICH_STATUS UNSUPPORTED_OP, VP_ERROR.REFUSED, ACCEPTED unchanged,
 *      nothing written; a doorbell without START is refused as well
 *   C  the next doorbell after the refusals passes
 */
#include "npu_cases.h"
#include "npu_neo_gemm_words.h"
#include "npu_test.h"

#define WIN FX1_NPU_ARENA_SMALL_BASE

static fx1_npu_t npu;
static npu_cases_t cases;
static uint8_t expect[NPU_CASE_WINDOW] __attribute__((aligned(8)));

static uint32_t neo_reg(uint32_t off) { return fx1_read32(FX1_NPU_VP_BASE + FX1_NPU_RICH_OF_MODEL(FX1_NPU_M_NEO_BASE + off)); }

static void check_window(uint32_t code, const char* what)
{
    const uint32_t stray = npu_diff((const volatile uint8_t*)WIN, expect, NPU_CASE_WINDOW);
    fx1_log_hex("window bytes differing from the expected image", stray);
    fx1_check(stray == 0, code, what);
}

static void expect_refused(const fx1_npu_neo_t* n, uint32_t code, const char* what)
{
    const uint32_t acc0 = fx1_npu_accepted(&npu);
    const fx1_npu_status_t s = fx1_npu_neo_submit(&npu, n, NPU_MS(10));
    fx1_log(what);
    fx1_log_hex("  RICH_STATUS", npu.last.rich_status);
    fx1_log_hex("  VP_ERROR   ", npu.last.vp_error);
    fx1_check(s == FX1_NPU_E_REFUSED && FX1_NPU_RICH_STATUS_CODE(npu.last.rich_status) == FX1_NPU_RICH_ERR_UNSUPPORTED_OP &&
                  (npu.last.vp_error & FX1_NPU_VP_ERROR_REFUSED) && fx1_npu_accepted(&npu) == acc0,
              code, what);
    for (volatile int i = 0; i < 2000; ++i) {
    }
    check_window(code + 1, "nothing written for the refused doorbell");
}

int main(void)
{
    npu_expect(&npu,
               npu_test_init(&npu, FX1_NPU_FEATURE_RICH_HAS | FX1_NPU_FEATURE_DFC_WINDOW | FX1_NPU_FEATURE_QUEUE, 0),
               FX1_NPU_OK, 0x01, "probe: VP_ID, ABI 1.x, FEATURE with DFC_WINDOW");
    fx1_check(npu_cases_build(&cases, WIN), 0x02, "neo words encoded");
    npu_expect(&npu, fx1_npu_set_window(&npu, WIN, NPU_CASE_WINDOW), FX1_NPU_OK, 0x03, "RICH window");
    npu_cases_fill((uint8_t*)WIN);
    npu_cases_fill(expect);

    /* A. accepted set. */
    npu_cases_apply_dfc(&cases, expect, WIN);
    const uint32_t acc0 = fx1_npu_accepted(&npu), ret0 = fx1_npu_retired(&npu);
    npu_expect(&npu, fx1_npu_neo_submit(&npu, &cases.neo[NPU_D2], NPU_MS(10)), FX1_NPU_OK, 0x10, "A: RESIDUAL doorbell accepted");
    npu_expect(&npu, fx1_npu_neo_submit(&npu, &cases.neo[NPU_D1], NPU_MS(10)), FX1_NPU_OK, 0x11, "A: MAX_POOL doorbell accepted");
    npu_expect(&npu, fx1_npu_rich_wait(&npu, NPU_MS(50)), FX1_NPU_OK, 0x12, "A: both completed without error");
    fx1_check(fx1_npu_accepted(&npu) == acc0 + 2 && fx1_npu_retired(&npu) == ret0 + 2, 0x13, "A: ACCEPTED +2, RETIRED +2");
    fx1_check((neo_reg(FX1_NPU_NEO_CTRL_OFF) & FX1_NPU_NEO_CTRL_DONE) && (neo_reg(FX1_NPU_NEO_ISTATUS_OFF) & 1u), 0x14,
              "A: neo CONTROL.DONE and ISTATUS set");
    for (unsigned d = NPU_D1; d <= NPU_D2; ++d) {
        const fx1_npu_neo_t* n = &cases.neo[d];
        const uint32_t off = n->out - WIN, bad = npu_diff((const volatile uint8_t*)WIN + off, expect + off, n->out_bytes);
        fx1_log(cases.dfc_label[d]);
        fx1_log_hex("  output bytes checked", n->out_bytes);
        fx1_log_hex("  differ from the oracle", bad);
        fx1_check(n->out_bytes > 0 && bad == 0, 0x15 + d, "A: DFC output equals the oracle");
    }
    check_window(0x18, "A: no stray write");
    npu_expect(&npu, fx1_npu_clear_errors(&npu), FX1_NPU_OK, 0x1A, "A: clear at idle");
    fx1_check(!(neo_reg(FX1_NPU_NEO_CTRL_OFF) & FX1_NPU_NEO_CTRL_DONE) && !(neo_reg(FX1_NPU_NEO_ISTATUS_OFF) & 1u), 0x19,
              "A: DONE / ISTATUS W1C");
    fx1_log("npu: A DFC RESIDUAL + MAX_POOL through the register file");

    /* B. strict refusals. */
    static fx1_npu_neo_t g; /* static: no memset in a freestanding image */
    g.out = WIN + 0x0700u; /* R1's output region (npu_cases.c) */
    g.out_bytes = 8 * 8 * 8;
    for (unsigned k = 0; k < FX1_NPU_NEO_REG_COUNT; ++k) g.word[k] = npu_neo_gemm_words[k];
    for (unsigned k = 0; k < 4; ++k) g.word[npu_neo_gemm_addr_words[k]] += WIN;
    expect_refused(&g, 0x20, "B: GEMM_FUSED doorbell refused (A-H1)");
    expect_refused(&cases.neo[NPU_D3], 0x22, "B: AVG_POOL doorbell refused (A-H3)");
    /* Doorbell without START: refused by the model's port. */
    {
        const uint32_t acc1 = fx1_npu_accepted(&npu);
        fx1_write32(FX1_NPU_VP_BASE + FX1_NPU_RICH_OF_MODEL(FX1_NPU_M_PUSH_A), FX1_NPU_OP_NEO_DOORBELL);
        fx1_npu_err_t e;
        fx1_npu_read_errors(&npu, &e);
        fx1_check(fx1_npu_accepted(&npu) == acc1 && (e.vp_error & FX1_NPU_VP_ERROR_REFUSED), 0x24,
                  "B: doorbell without START refused");
        npu_expect(&npu, fx1_npu_clear_errors(&npu), FX1_NPU_OK, 0x25, "B: clear the latches of the raw doorbell (idle)");
    }
    fx1_log("npu: B strict refusals, nothing written");

    /* C. next doorbell. */
    npu_cases_fill((uint8_t*)WIN);
    npu_cases_fill(expect);
    npu_cases_apply_dfc(&cases, expect, WIN);
    npu_expect(&npu, fx1_npu_neo_submit(&npu, &cases.neo[NPU_D1], NPU_MS(10)), FX1_NPU_OK, 0x30, "C: MAX_POOL after refusals");
    npu_expect(&npu, fx1_npu_neo_submit(&npu, &cases.neo[NPU_D2], NPU_MS(10)), FX1_NPU_OK, 0x31, "C: RESIDUAL after refusals");
    npu_expect(&npu, fx1_npu_rich_wait(&npu, NPU_MS(50)), FX1_NPU_OK, 0x32, "C: completed");
    check_window(0x33, "C: outputs equal the oracle, no stray write");
    fx1_log("npu: C doorbells after the refusals pass");
    return 0;
}
