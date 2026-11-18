/*
 * FX1 NPU N2: refusals, busy, driver timeout, recovery (review N1: N1-C, N1-I)
 * and soft reset, through the reference driver. After every failure the next
 * job runs and its output equals the C oracle; nothing reports PASS without
 * a checked output.
 *   A  refusals (a store that does not fault, read back by the driver):
 *      unsupported opcode, window size 0, a pointer outside the window, bad
 *      pooling geometry; each followed by a good job
 *   B  busy (review N2 R1/R2): while a GEMM-bank job is outstanding the
 *      driver refuses a rich push and set_window (NOT_IDLE, no access: the
 *      locked bank would answer the window stores with a store access fault);
 *      a push refused BUSY by the NPU below the driver does not fail the job
 *      (DONE decides) and is kept in npu->last; a GEMM-bank start or
 *      set_window while rich work is queued is refused by the driver
 *   C  driver timeout on a long rich job, then recovery: ABORT, QUIESCENT and
 *      ENGINE_BUSY = 0 within a timeout; the aborted job's output region
 *      keeps its canary, also long after (no late write); next job passes
 *   D  N1-I: ABORT of a running GEMM-bank job, and of a raw native START
 *      (test code, outside the driver): recovery marks the native
 *      configuration invalid; the next GEMM-bank job programs it again and
 *      matches the oracle
 *   E  soft reset: refused by the driver while busy; at idle every register
 *      is back at its reset value; the window is set again; a job passes
 *   F  failed recovery (review N2 R3): a recovery timeout shorter than the
 *      running job closes the context; an init while the aborted instruction
 *      still runs fails NOT_IDLE, one with a missing feature fails FEATURE,
 *      both keep the context closed and its diagnostics; once the engine went
 *      idle an init reopens it and a job passes
 */
#include "npu_cases.h"
#include "npu_oracle.h"
#include "npu_test.h"

#define WIN FX1_NPU_ARENA_SMALL_BASE
#define GEMM_A (FX1_NPU_ARENA_SMALL_BASE + 0x100000u)
#define GEMM_W (FX1_NPU_ARENA_SMALL_BASE + 0x110000u)
#define GEMM_C (FX1_NPU_ARENA_SMALL_BASE + 0x120000u)
#define BIG_IN (FX1_NPU_ARENA_SMALL_BASE + 0x200000u)
#define BIG_OUT (FX1_NPU_ARENA_SMALL_BASE + 0x280000u)
#define BIG_WIN_SIZE 0x00300000u

static fx1_npu_t npu;
static npu_cases_t cases;
static uint8_t expect[NPU_CASE_WINDOW] __attribute__((aligned(8)));
static int8_t ga[32 * 992], gw[992 * 32];
static int32_t gc[32 * 32];

/* A good job: D1 (MAX_POOL [16][12][12]) as a lane-A instruction. */
static void good_job(uint32_t code, const char* what)
{
    npu_cases_fill((uint8_t*)WIN);
    npu_cases_fill(expect);
    npu_cases_apply_dfc(&cases, expect, WIN); /* D1 and D2; only D1 runs */
    fx1_npu_instr_t i;
    fx1_check(fx1_npu_build_pool(&i, &cases.d1), code, "good job built");
    npu_expect(&npu, fx1_npu_set_window(&npu, WIN, NPU_CASE_WINDOW), FX1_NPU_OK, code + 1, "good job window");
    npu_expect(&npu, fx1_npu_rich_submit(&npu, &i, NPU_MS(10)), FX1_NPU_OK, code + 2, what);
    npu_expect(&npu, fx1_npu_rich_wait(&npu, NPU_MS(50)), FX1_NPU_OK, code + 3, what);
    const uint32_t off = i.out - WIN;
    const uint32_t bad = npu_diff((const volatile uint8_t*)WIN + off, expect + off, i.out_bytes);
    fx1_check(bad == 0, code + 4, what);
}

static void expect_refusal(const fx1_npu_instr_t* i, uint32_t rich_code, uint32_t vp_bit, uint32_t code, const char* what)
{
    const uint32_t acc0 = fx1_npu_accepted(&npu);
    const fx1_npu_status_t s = fx1_npu_rich_submit(&npu, i, NPU_MS(10));
    fx1_log(what);
    fx1_log_hex("  RICH_STATUS", npu.last.rich_status);
    fx1_log_hex("  VP_ERROR   ", npu.last.vp_error);
    fx1_check(s == FX1_NPU_E_REFUSED && FX1_NPU_RICH_STATUS_CODE(npu.last.rich_status) == rich_code &&
                  (npu.last.vp_error & vp_bit) && fx1_npu_accepted(&npu) == acc0,
              code, what);
}

static void gemm_job(uint32_t k, uint32_t code, const char* what)
{
    static uint32_t s = 777u;
    volatile int8_t* a = (volatile int8_t*)GEMM_A;
    volatile int8_t* w = (volatile int8_t*)GEMM_W;
    for (uint32_t i = 0; i < 32 * k; ++i) {
        s = s * 1103515245u + 12345u;
        a[i] = ga[i] = (int8_t)(s >> 16);
    }
    for (uint32_t i = 0; i < k * 32; ++i) {
        s = s * 1103515245u + 12345u;
        w[i] = gw[i] = (int8_t)(s >> 16);
    }
    npu_oracle_gemm_bank(ga, gw, k, gc);
    const fx1_npu_gemm_t g = {.a = GEMM_A, .w = GEMM_W, .c = GEMM_C, .k = k};
    npu_expect(&npu, fx1_npu_gemm_start(&npu, &g), FX1_NPU_OK, code, what);
    npu_expect(&npu, fx1_npu_gemm_wait(&npu, NPU_MS(50)), FX1_NPU_OK, code + 1, what);
    const volatile int32_t* c = (const volatile int32_t*)GEMM_C;
    uint32_t bad = 0;
    for (uint32_t i = 0; i < 32 * 32; ++i) bad += c[i] != gc[i];
    fx1_check(bad == 0 && npu.native_config_valid, code + 2, what);
}

/* A long MAX_POOL: [32][32][32], k 5, pad 2 (about 0.1 ms of model time). */
static void big_pool(fx1_npu_instr_t* i)
{
    const fx1_npu_pool_t p = {.in = BIG_IN, .out = BIG_OUT, .c = 32, .h = 32, .w = 32, .k = 5, .pad = 2, .stride = 1};
    fx1_check(fx1_npu_build_pool(i, &p), 0x09, "long job built");
    volatile uint32_t* o = (volatile uint32_t*)BIG_OUT;
    for (uint32_t k = 0; k < i->out_bytes / 4; ++k) o[k] = 0xC3C3C3C3u; /* canary */
}
/* Words of the output region that lost the canary. */
static uint32_t canary_lost(const fx1_npu_instr_t* i)
{
    const volatile uint32_t* o = (const volatile uint32_t*)BIG_OUT;
    uint32_t n = 0;
    for (uint32_t k = 0; k < i->out_bytes / 4; ++k) n += o[k] != 0xC3C3C3C3u;
    return n;
}

/* A K = 992 GEMM-bank job on whatever the buffers hold (no oracle: only its
 * duration matters), to completion. */
static void gemm_long(uint32_t code, const char* what)
{
    const fx1_npu_gemm_t g = {.a = GEMM_A, .w = GEMM_W, .c = GEMM_C, .k = 992};
    npu_expect(&npu, fx1_npu_gemm_start(&npu, &g), FX1_NPU_OK, code, what);
    npu_expect(&npu, fx1_npu_gemm_wait(&npu, NPU_MS(50)), FX1_NPU_OK, code + 1, what);
}

static void spin_us(uint32_t us)
{
    const uint64_t t = fx1_mtime(), n = (uint64_t)FX1_MTIME_HZ / 1000000u * us;
    while (fx1_mtime() - t < n) {
    }
}

int main(void)
{
    npu_expect(&npu, npu_test_init(&npu, FX1_NPU_FEATURE_NATIVE_GEMM | FX1_NPU_FEATURE_RICH_HAS | FX1_NPU_FEATURE_ABORT, 0),
               FX1_NPU_OK, 0x01, "probe");
    fx1_check(npu_cases_build(&cases, WIN), 0x02, "cases built");
    good_job(0x03, "first job");

    /* A. refusals. */
    {
        fx1_npu_instr_t i;
        const uint32_t a[] = {FX1_NPU_M_OUT_ADDR};
        const uint32_t v[] = {WIN + 0x1000u};
        fx1_npu_build_raw(&i, FX1_NPU_OP_FUSED_ATTN, a, v, 1);
        expect_refusal(&i, FX1_NPU_RICH_ERR_VP_UNSUPPORTED, FX1_NPU_VP_ERROR_UNSUPPORTED, 0x10, "A: FUSED_ATTN refused UNSUPPORTED");
        good_job(0x11, "A: job after UNSUPPORTED");

        fx1_npu_instr_t p;
        fx1_npu_build_pool(&p, &cases.d1);
        fx1_npu_native_write(&npu, FX1_NPU_BANK(FX1_NPU_RICH_WINDOW_SIZE), 0); /* below the driver */
        p.out_bytes = 0; /* skip the driver's own window check: the NPU decides */
        expect_refusal(&p, FX1_NPU_RICH_ERR_VP_BAD_ADDR, FX1_NPU_VP_ERROR_BAD_ADDR, 0x18, "A: window size 0 refused BAD_ADDR");
        good_job(0x19, "A: job after the window refusal");

        fx1_npu_pool_t bad_in = cases.d1;
        bad_in.in = WIN - 0x1000u; /* outside the window */
        fx1_npu_build_pool(&p, &bad_in);
        expect_refusal(&p, FX1_NPU_RICH_ERR_VP_BAD_ADDR, FX1_NPU_VP_ERROR_BAD_ADDR, 0x20, "A: input outside the window refused");
        good_job(0x21, "A: job after the address refusal");

        const uint32_t ga_[] = {FX1_NPU_M_A_ADDR, FX1_NPU_M_OUT_ADDR, FX1_NPU_M_MODE, FX1_NPU_M_STRIDE, FX1_NPU_M_IN_C,
                                FX1_NPU_M_IN_H, FX1_NPU_M_IN_W, FX1_NPU_M_POOL_K, FX1_NPU_M_POOL_P};
        const uint32_t gv[] = {WIN + 0x1000u, WIN + 0x1400u, FX1_NPU_ELEM_MAX_POOL, 1, 8, 10, 10, 20, 0};
        fx1_npu_build_raw(&p, FX1_NPU_OP_ELEM_WISE, ga_, gv, 9);
        expect_refusal(&p, FX1_NPU_RICH_ERR_BAD_GEOM, FX1_NPU_VP_ERROR_REFUSED, 0x28, "A: pooling window larger than the input: BAD_GEOM");
        good_job(0x29, "A: job after BAD_GEOM");
        fx1_log("npu: A refusals, each followed by a passing job");
    }

    /* B. busy. */
    {
        const fx1_npu_gemm_t g = {.a = GEMM_A, .w = GEMM_W, .c = GEMM_C, .k = 992};
        npu_expect(&npu, fx1_npu_gemm_start(&npu, &g), FX1_NPU_OK, 0x30, "B: long GEMM-bank job started");
        fx1_npu_instr_t i;
        fx1_npu_build_pool(&i, &cases.d1);
        const uint32_t acc0 = fx1_npu_accepted(&npu), ws = fx1_npu_bank_read(&npu, FX1_NPU_RICH_WINDOW_SIZE);
        npu_expect(&npu, fx1_npu_rich_submit(&npu, &i, NPU_MS(10)), FX1_NPU_E_NOT_IDLE, 0x31,
                   "B: rich push while the GEMM-bank job is outstanding: NOT_IDLE");
        npu_expect(&npu, fx1_npu_set_window(&npu, WIN, BIG_WIN_SIZE), FX1_NPU_E_NOT_IDLE, 0x3D,
                   "B: set_window during the GEMM-bank job: NOT_IDLE (no store access fault)");
        fx1_check(fx1_npu_accepted(&npu) == acc0 && fx1_npu_bank_read(&npu, FX1_NPU_RICH_WINDOW_SIZE) == ws && npu.win_size == ws,
                  0x3E, "B: nothing accepted, window unchanged (hardware and cache)");
        /* Below the driver: a push the NPU refuses BUSY while the job runs. */
        fx1_write32(FX1_NPU_VP_BASE + FX1_NPU_RICH_OF_MODEL(FX1_NPU_M_PUSH_A), FX1_NPU_OP_ELEM_WISE);
        fx1_check(fx1_npu_bank_read(&npu, FX1_NPU_VP_ERROR) & FX1_NPU_VP_ERROR_BUSY, 0x3F, "B: raw push refused BUSY by the NPU");
        npu_expect(&npu, fx1_npu_gemm_wait(&npu, NPU_MS(50)), FX1_NPU_OK, 0x32, "B: the GEMM-bank job completed (DONE decides)");
        fx1_check((npu.last.vp_error & FX1_NPU_VP_ERROR_BUSY) && fx1_npu_bank_read(&npu, FX1_NPU_VP_ERROR) == 0, 0x2F,
                  "B: the stray refusal kept in npu->last, latches cleared at idle");
        fx1_npu_instr_t big;
        big_pool(&big);
        npu_expect(&npu, fx1_npu_set_window(&npu, WIN, BIG_WIN_SIZE), FX1_NPU_OK, 0x33, "B: large window");
        npu_expect(&npu, fx1_npu_rich_submit(&npu, &big, NPU_MS(10)), FX1_NPU_OK, 0x34, "B: long rich job");
        npu_expect(&npu, fx1_npu_gemm_start(&npu, &g), FX1_NPU_E_NOT_IDLE, 0x35, "B: GEMM-bank start refused by the driver (rich busy)");
        npu_expect(&npu, fx1_npu_set_window(&npu, WIN, NPU_CASE_WINDOW), FX1_NPU_E_NOT_IDLE, 0x3C,
                   "B: set_window refused by the driver (rich busy)");
        npu_expect(&npu, fx1_npu_rich_wait(&npu, NPU_MS(50)), FX1_NPU_OK, 0x36, "B: the long rich job completed");
        fx1_check(canary_lost(&big) > 0, 0x37, "B: it wrote its output");
        gemm_job(64, 0x38, "B: GEMM-bank job afterwards");
        fx1_log("npu: B busy refusals");
    }

    /* C. driver timeout, recovery, no late write. */
    {
        fx1_npu_instr_t big;
        big_pool(&big);
        npu_expect(&npu, fx1_npu_set_window(&npu, WIN, BIG_WIN_SIZE), FX1_NPU_OK, 0x40, "C: large window");
        npu_expect(&npu, fx1_npu_rich_submit(&npu, &big, NPU_MS(10)), FX1_NPU_OK, 0x41, "C: long job");
        npu_expect(&npu, fx1_npu_rich_wait(&npu, NPU_MS(0) + FX1_MTIME_HZ / 50000u), FX1_NPU_E_TIMEOUT, 0x42,
                   "C: driver wait of 20 us times out");
        const uint64_t t0 = fx1_mtime();
        npu_expect(&npu, fx1_npu_recover(&npu, NPU_MS(20)), FX1_NPU_OK, 0x43, "C: recovery");
        fx1_log_hex("C: recovery took (us)", (uint32_t)((fx1_mtime() - t0) * 1000000u / FX1_MTIME_HZ));
        fx1_check(npu.last.vp_error & FX1_NPU_VP_ERROR_ABORTED, 0x44, "C: VP_ERROR.ABORTED seen by the recovery");
        fx1_check(canary_lost(&big) == 0, 0x45, "C: the aborted job wrote nothing");
        spin_us(2000);
        fx1_check(canary_lost(&big) == 0, 0x46, "C: still nothing 2 ms later (no late write)");
        good_job(0x47, "C: job after the recovery");
        fx1_log("npu: C timeout, recovery, no late write");
    }

    /* D. N1-I. */
    {
        const fx1_npu_gemm_t g = {.a = GEMM_A, .w = GEMM_W, .c = GEMM_C, .k = 992};
        npu_expect(&npu, fx1_npu_gemm_start(&npu, &g), FX1_NPU_OK, 0x50, "D: long GEMM-bank job");
        fx1_check(fx1_npu_bank_read(&npu, FX1_NPU_VP_STATUS) & FX1_NPU_VP_STATUS_NATIVE_BUSY, 0x51, "D: NATIVE_BUSY");
        npu_expect(&npu, fx1_npu_recover(&npu, NPU_MS(20)), FX1_NPU_OK, 0x52, "D: recovery of the GEMM-bank job");
        fx1_check(npu.last.last_error == FX1_NPU_ERR_RESET_ABORTED && !npu.native_config_valid, 0x53,
                  "D: RESET_ABORTED, native configuration invalid");
        gemm_job(48, 0x54, "D: GEMM-bank job after the abort matches");
        gemm_long(0x57, "D: K = 992 GEMM-bank job (the configuration the raw run re-runs)");
        /* Raw native START: re-runs the K = 992 configuration the GEMM bank left. */
        fx1_npu_native_write(&npu, FX1_NPU_NATIVE_CONTROL, 1);
        fx1_check(fx1_npu_bank_read(&npu, FX1_NPU_VP_STATUS) & FX1_NPU_VP_STATUS_NATIVE_BUSY, 0x58, "D: raw run active");
        npu_expect(&npu, fx1_npu_recover(&npu, NPU_MS(20)), FX1_NPU_OK, 0x59, "D: recovery of the raw run");
        fx1_check(!npu.native_config_valid, 0x5A, "D: native configuration invalid after the raw ABORT");
        gemm_job(64, 0x5B, "D: GEMM-bank job reprograms the core and matches");
        fx1_log("npu: D N1-I recovery of GEMM-bank and raw native runs");
    }

    /* E. soft reset. */
    {
        fx1_npu_instr_t big;
        big_pool(&big);
        npu_expect(&npu, fx1_npu_set_window(&npu, WIN, BIG_WIN_SIZE), FX1_NPU_OK, 0x60, "E: large window");
        npu_expect(&npu, fx1_npu_rich_submit(&npu, &big, NPU_MS(10)), FX1_NPU_OK, 0x61, "E: long job");
        npu_expect(&npu, fx1_npu_soft_reset(&npu, NPU_MS(5)), FX1_NPU_E_NOT_IDLE, 0x62, "E: soft reset refused while busy");
        npu_expect(&npu, fx1_npu_recover(&npu, NPU_MS(20)), FX1_NPU_OK, 0x63, "E: ABORT first");
        npu_expect(&npu, fx1_npu_soft_reset(&npu, NPU_MS(5)), FX1_NPU_OK, 0x64, "E: soft reset at idle");
        fx1_check(fx1_npu_accepted(&npu) == 0 && fx1_npu_retired(&npu) == 0 &&
                      fx1_npu_bank_read(&npu, FX1_NPU_RICH_WINDOW_SIZE) == 0 &&
                      fx1_npu_bank_read(&npu, FX1_NPU_VP_ERROR) == 0 &&
                      fx1_npu_bank_read(&npu, FX1_NPU_VP_PUSH_WAIT_NS) == 100000u,
                  0x65, "E: registers back at reset values");
        good_job(0x66, "E: job after the soft reset");
        gemm_job(64, 0x6C, "E: GEMM-bank job after the soft reset");
        fx1_log("npu: E soft reset");
    }

    /* F. failed recovery. */
    {
        fx1_npu_instr_t big;
        big_pool(&big);
        npu_expect(&npu, fx1_npu_set_window(&npu, WIN, BIG_WIN_SIZE), FX1_NPU_OK, 0x70, "F: large window");
        npu_expect(&npu, fx1_npu_rich_submit(&npu, &big, NPU_MS(10)), FX1_NPU_OK, 0x71, "F: long job");
        spin_us(20); /* staging done (32 KiB), the model runs (about 0.1 ms) */
        npu_expect(&npu, fx1_npu_recover(&npu, FX1_MTIME_HZ / 1000000u), FX1_NPU_E_UNAVAILABLE, 0x72,
                   "F: recovery with a 1 us limit: engine still busy, unavailable");
        fx1_check((npu.last.vp_status & FX1_NPU_VP_STATUS_QUIESCENT) && (npu.last.vp_status & FX1_NPU_VP_STATUS_ENGINE_BUSY), 0x73,
                  "F: QUIESCENT but ENGINE_BUSY (QUIESCENT is not recovery)");
        fx1_npu_instr_t i;
        fx1_npu_build_pool(&i, &cases.d1);
        npu_expect(&npu, fx1_npu_rich_submit(&npu, &i, NPU_MS(10)), FX1_NPU_E_UNAVAILABLE, 0x74, "F: new work refused");
        const fx1_npu_err_t saved = npu.last;
        npu_expect(&npu, npu_test_init(&npu, FX1_NPU_FEATURE_NATIVE_GEMM | FX1_NPU_FEATURE_RICH_HAS | FX1_NPU_FEATURE_ABORT, 0),
                   FX1_NPU_E_NOT_IDLE, 0x75, "F: init while the aborted instruction runs: NOT_IDLE");
        fx1_log_hex("F: VP_STATUS at that init", fx1_npu_bank_read(&npu, FX1_NPU_VP_STATUS));
        npu_expect(&npu, npu_test_init(&npu, FX1_NPU_FEATURE_RICH_HAS | 0x80000000u, 0), FX1_NPU_E_FEATURE, 0x76,
                   "F: init with a missing feature: FEATURE");
        fx1_check(npu.unavailable && npu.last.vp_status == saved.vp_status && npu.last.vp_error == saved.vp_error &&
                      npu.last.rich_status == saved.rich_status,
                  0x77, "F: still closed, the recovery's diagnostics kept");
        npu_expect(&npu, fx1_npu_rich_submit(&npu, &i, NPU_MS(10)), FX1_NPU_E_UNAVAILABLE, 0x78, "F: still no new work");
        const uint64_t t = fx1_mtime();
        while (fx1_npu_bank_read(&npu, FX1_NPU_VP_STATUS) & (FX1_NPU_VP_STATUS_ENGINE_BUSY | FX1_NPU_VP_STATUS_ABORT_ACTIVE))
            fx1_check(fx1_mtime() - t < NPU_MS(20), 0x79, "F: the engine goes idle");
        npu_expect(&npu, npu_test_init(&npu, FX1_NPU_FEATURE_NATIVE_GEMM | FX1_NPU_FEATURE_RICH_HAS | FX1_NPU_FEATURE_ABORT, 0),
                   FX1_NPU_OK, 0x7A, "F: init once the engine is idle: reopened");
        fx1_check(!npu.unavailable, 0x7B, "F: context open");
        good_job(0x7C, "F: job after the new init");
        fx1_log("npu: F failed recovery marks the device unavailable");
    }
    return 0;
}
