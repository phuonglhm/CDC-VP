/*
 * FX1 NPU N4: two harts, one NPU, the ownership lock (FX1_NPU_LOCK_BASE,
 * fx1_npu_claim / fx1_npu_init_shared / fx1_npu_release; LR/SC on the word).
 *
 * ROUNDS rounds. In each, both harts meet, wait for the same mtime instant
 * and claim at once:
 *   - exactly one claim succeeds, the other gets FX1_NPU_E_BUSY;
 *   - the holder opens its context and runs a GEMM-bank job (K=16, its own
 *     buffers and data) that must equal the C oracle;
 *   - while the holder holds the NPU, the other hart: init_shared ->
 *     NOT_OWNER, a call on its closed context -> UNAVAILABLE, a release of
 *     the lock -> NOT_OWNER; none of it reaches the NPU (ACCEPTED, the
 *     holder's GEMM count and STATUS do not move, the holder's job passes);
 *   - the holder releases (its context closes); the other hart claims (OK),
 *     opens its context, runs its own job (oracle), releases.
 * At the end: every round had one winner (which hart wins a simultaneous
 * claim is up to the simulator's scheduling: logged, not required), all
 * 2 x ROUNDS jobs matched (each hart ran one per round), the lock is free.
 */
#include "fx1_fw.h"
#include "fx1_npu.h"
#include "npu_oracle.h"

#define ROUNDS 4u
#define K 16u
#define LOCK ((volatile uint32_t*)FX1_NPU_LOCK_BASE)
#define BUF(h) (FX1_NPU_ARENA_SMALL_BASE + 0x100000u + (uint32_t)(h) * 0x20000u)
#define BA(h) BUF(h)
#define BW(h) (BUF(h) + 0x1000u)
#define BC(h) (BUF(h) + 0x2000u)
#define MS(n) ((uint64_t)FX1_MTIME_HZ / 1000u * (n))

static fx1_npu_t ctx[2];
static int8_t ta[2][32 * K], tw[2][K * 32];
static int32_t tc[2][32 * 32];

static volatile uint32_t arrived[2], go_round, claim_ok[2], held_by;
static volatile uint64_t go_time;
static volatile uint32_t holder_ready, other_checked, released, second_done, jobs_ok, wins[2];

static void wait_for(volatile uint32_t* v, uint32_t want, uint32_t code, const char* what)
{
    const uint64_t t = fx1_mtime();
    while (*v != want) fx1_check(fx1_mtime() - t < MS(200), code, what);
    fx1_acquire();
}

/* One GEMM-bank job of hart h, checked against the oracle. */
static void job(uint32_t h, uint32_t round, uint32_t code)
{
    uint32_t s = 0x9E3779B9u * (h + 1) + round;
    volatile int8_t* a = (volatile int8_t*)BA(h);
    volatile int8_t* w = (volatile int8_t*)BW(h);
    for (uint32_t i = 0; i < 32 * K; ++i) {
        s = s * 1103515245u + 12345u;
        a[i] = ta[h][i] = (int8_t)(s >> 16);
        s = s * 1103515245u + 12345u;
        w[i] = tw[h][i] = (int8_t)(s >> 16);
    }
    npu_oracle_gemm_bank(ta[h], tw[h], K, tc[h]);
    fx1_npu_t* n = &ctx[h];
    fx1_check(fx1_npu_init_shared(n, FX1_NPU_VP_BASE, FX1_NPU_FEATURE_NATIVE_GEMM, 0, fx1_mtime, FX1_MTIME_HZ, LOCK, h) ==
                  FX1_NPU_OK,
              code, "holder opens its context");
    const fx1_npu_gemm_t g = {.a = BA(h), .w = BW(h), .c = BC(h), .k = K};
    fx1_check(fx1_npu_gemm_start(n, &g) == FX1_NPU_OK, code + 1, "holder starts its GEMM-bank job");
    fx1_check(fx1_npu_gemm_wait(n, MS(20)) == FX1_NPU_OK, code + 2, "holder's job completes");
    const volatile int32_t* c = (const volatile int32_t*)BC(h);
    uint32_t bad = 0;
    for (uint32_t i = 0; i < 32 * 32; ++i) bad += c[i] != tc[h][i];
    fx1_check(bad == 0, code + 3, "holder's job equals the oracle");
    ++jobs_ok;
}

/* The hart that lost the claim, while the holder holds the NPU. */
static void not_holder(uint32_t me, uint32_t code)
{
    fx1_npu_t* n = &ctx[me];
    const uint32_t acc = fx1_npu_bank_read(n, FX1_NPU_VP_ACCEPTED);
    fx1_check(fx1_npu_claim(LOCK, me) == FX1_NPU_E_BUSY, code, "claim while held: BUSY");
    fx1_check(fx1_npu_init_shared(n, FX1_NPU_VP_BASE, 0, 0, fx1_mtime, FX1_MTIME_HZ, LOCK, me) == FX1_NPU_E_NOT_OWNER,
              code + 1, "init_shared without the lock: NOT_OWNER");
    const fx1_npu_gemm_t g = {.a = BA(me), .w = BW(me), .c = BC(me), .k = K};
    fx1_check(fx1_npu_gemm_start(n, &g) == FX1_NPU_E_UNAVAILABLE, code + 2, "start on the closed context: refused");
    fx1_check(fx1_npu_release(n, FX1_NPU_VP_BASE, LOCK, me) == FX1_NPU_E_NOT_OWNER, code + 3, "release of the other hart's lock: NOT_OWNER");
    fx1_check(fx1_npu_bank_read(n, FX1_NPU_VP_ACCEPTED) == acc && *LOCK == held_by + 1u, code + 4,
              "nothing reached the NPU, the lock still the holder's");
}

static void round_body(uint32_t me, uint32_t r)
{
    const uint32_t other = me ^ 1u;
    /* meet, then claim at the same instant */
    arrived[me] = r + 1;
    if (me == 0) {
        wait_for(&arrived[1], r + 1, 0x10, "hart 1 at the start line");
        holder_ready = other_checked = released = second_done = 0;
        go_time = fx1_mtime() + MS(1);
        fx1_release();
        go_round = r + 1;
    } else {
        wait_for(&go_round, r + 1, 0x11, "hart 0 opens the round");
    }
    while (fx1_mtime() < go_time) {
    }
    const fx1_npu_status_t c = fx1_npu_claim(LOCK, me);
    claim_ok[me] = c == FX1_NPU_OK;
    fx1_check(c == FX1_NPU_OK || c == FX1_NPU_E_BUSY, 0x12, "claim: OK or BUSY");
    if (c == FX1_NPU_OK) {
        held_by = me;
        ++wins[me];
        fx1_release();
        holder_ready = r + 1;
        wait_for(&other_checked, r + 1, 0x13, "the other hart checked its calls");
        job(me, r, 0x20 + 0x10 * me);
        fx1_check(fx1_npu_release(&ctx[me], FX1_NPU_VP_BASE, LOCK, me) == FX1_NPU_OK && ctx[me].unavailable, 0x14,
                  "holder releases, its context closes");
        fx1_release();
        released = r + 1;
        wait_for(&second_done, r + 1, 0x15, "the other hart's turn");
    } else {
        wait_for(&holder_ready, r + 1, 0x16, "the holder holds the NPU");
        not_holder(me, 0x40 + 0x10 * me);
        fx1_release();
        other_checked = r + 1;
        wait_for(&released, r + 1, 0x17, "the holder released");
        fx1_check(fx1_npu_claim(LOCK, me) == FX1_NPU_OK, 0x18, "claim after the release: OK");
        held_by = me;
        job(me, r + 100, 0x60 + 0x10 * me);
        fx1_check(fx1_npu_release(&ctx[me], FX1_NPU_VP_BASE, LOCK, me) == FX1_NPU_OK, 0x19, "second holder releases");
        fx1_release();
        second_done = r + 1;
    }
    (void)other;
}

static volatile uint32_t hart1_done;

void secondary_main(uint32_t hart)
{
    for (uint32_t r = 0; r < ROUNDS; ++r) round_body(hart, r);
    fx1_release();
    hart1_done = 1;
    for (;;) fx1_wfi();
}

int main(void)
{
    *LOCK = 0;
    fx1_release();
    fx1_release_secondaries();
    for (uint32_t r = 0; r < ROUNDS; ++r) {
        round_body(0, r);
        fx1_check(claim_ok[0] + claim_ok[1] == 1, 0x30, "exactly one claim succeeded at the same instant");
        fx1_log_hex("npu: claim won by hart", claim_ok[0] ? 0u : 1u); /* then the other hart took its turn */
    }
    wait_for(&hart1_done, 1, 0x31, "hart 1 finished");
    fx1_log_hex("npu: rounds won by hart 0", wins[0]);
    fx1_log_hex("npu: rounds won by hart 1", wins[1]);
    fx1_log_hex("npu: jobs equal to the oracle", jobs_ok);
    fx1_check(wins[0] + wins[1] == ROUNDS && jobs_ok == 2 * ROUNDS && *LOCK == 0, 0x32,
              "every round one winner, every job matched, the lock free");
    fx1_log("npu: two harts, one owner at a time");
    return 0;
}
