// SPDX-License-Identifier: Apache-2.0
//
// Host test: the firmware's reference driver (drivers/fx1_npu.c, compiled as C)
// against the real clone (components/FX1_Components/npu, SystemC), through
// the bench of the clone's own tests (npu_bench.h, direct topology). The
// driver's MMIO accessors are TLM transactions of the bench CPU; the test can
// count them and release a held DDR write at any access, so the NPU completes
// at a chosen point inside a driver call (review N2 R1-R3). Outputs are
// checked against the C oracle of the firmware tests (tests/npu_common).
//
//   test_npu_driver_host <case>
//     race_rich       a batch completes at every access of a refused push's
//                     handling: the refusal is reported, the batch passes,
//                     its output equals the oracle, the latches end clear
//     race_rich_irq   the same with interrupts: the handler acks, the batch
//                     ends without a DONE interrupt (N2-I), the poll decides
//     race_rich_fail  the batch's copy-back fails at every such access: the
//                     wait reports the job error (never a false success)
//     race_rich_fail_irq
//     gemm_gate       while a GEMM-bank job runs every rich/config call
//                     returns NOT_IDLE without one access; the job passes
//                     (polling and interrupt); a refused push below the
//                     driver does not make the job fail
//     window          set_window NOT_IDLE during GEMM, rich work, and an
//                     unconsumed batch; no bus error, cache = hardware
//     reinit          failed recovery: init NOT_IDLE while the engine runs,
//                     ABI/FEATURE/ABSENT failures keep the context closed and
//                     its diagnostics; garbage context fails closed; init after
//                     idle reopens, jobs pass
//     poll            PENDING / ARG / TIMEOUT semantics, credit timeout
#define SC_INCLUDE_DYNAMIC_PROCESSES
#include "npu_bench.h"

extern "C" {
#include "fx1_npu.h"
#include "npu_oracle.h"
}

namespace {

// ---- MMIO shim -----------------------------------------------------------------
Bench* g_bench = nullptr;
sc_process_handle g_owner;  // the test body: only its accesses are counted
unsigned g_reads = 0, g_writes = 0, g_bus_errors = 0;
int g_release_at = -1;      // release the held write at this counted access
unsigned g_counted = 0;

void count_access()
{
    if (sc_get_current_process_handle() != g_owner) return;
    if (g_release_at >= 0 && g_counted++ == static_cast<unsigned>(g_release_at)) {
        g_release_at = -1;
        g_bench->mem->write_release.notify(SC_ZERO_TIME);  // runs at the next scheduling point
    }
}

} // namespace

extern "C" uint32_t fx1_read32(uintptr_t a)
{
    tlm::tlm_response_status s;
    const std::uint32_t v = g_bench->cpu0->rd(static_cast<std::uint32_t>(a), &s);
    if (sc_get_current_process_handle() == g_owner) ++g_reads;
    if (s != tlm::TLM_OK_RESPONSE) {
        ++g_bus_errors;
        std::printf("  bus error: read 0x%lx\n", static_cast<unsigned long>(a));
    }
    count_access();
    return v;
}
extern "C" void fx1_write32(uintptr_t a, uint32_t v)
{
    const auto s = g_bench->cpu0->wr(static_cast<std::uint32_t>(a), v);
    if (sc_get_current_process_handle() == g_owner) ++g_writes;
    if (s != tlm::TLM_OK_RESPONSE) {
        ++g_bus_errors;
        std::printf("  bus error: write 0x%lx\n", static_cast<unsigned long>(a));
    }
    count_access();
}

namespace {

uint64_t now_ns() { return static_cast<uint64_t>(sc_time_stamp() / sc_time(1, SC_NS)); }
constexpr uint32_t kHz = 1000000000u;  // time source: ns
constexpr uint64_t kMs = 1000000u;

constexpr uint32_t kWin = static_cast<uint32_t>(kLowWin);
constexpr uint32_t kWinSize = 0x00200000u;
constexpr uint32_t kGemmA = kWin + 0x100000u, kGemmW = kWin + 0x110000u, kGemmC = kWin + 0x120000u;

unsigned accesses() { return g_reads + g_writes; }

// Interrupt handler stand-in with the PLIC level gateway's behaviour: taken
// while the level is high, fx1_npu_irq_ack, then taken again if the level is
// still high once the ack propagated (a bit latched during the handler).
struct Isr {
    fx1_npu_t* npu = nullptr;
    unsigned count = 0;
    uint32_t bits = 0;
    void attach(Bench& b, fx1_npu_t* n)
    {
        npu = n;
        sc_spawn([this, &b] {
            for (;;) {
                while (!b.irq.read()) wait(b.irq.value_changed_event());
                ++count;
                bits |= fx1_npu_irq_ack(npu);
                for (int d = 0; d < 3; ++d) wait(SC_ZERO_TIME);  // the level follows the W1C
            }
        });
    }
};

std::mt19937 g_rng(20261118u);
void fill(uint32_t addr, std::vector<int8_t>& v, uint32_t n)
{
    v.resize(n);
    for (auto& x : v) x = static_cast<int8_t>(static_cast<int>(g_rng() % 256) - 128);
    g_bench->mem->poke(addr, v.data(), n);
}

// A MAX_POOL job of the firmware builder, with its oracle output.
struct PoolJob {
    fx1_npu_pool_t p{};
    fx1_npu_instr_t i{};
    std::vector<int8_t> in, expect;
    void make(uint32_t in_addr, uint32_t out_addr, uint32_t c, uint32_t h, uint32_t w, uint32_t k, uint32_t pad)
    {
        p = fx1_npu_pool_t{in_addr, out_addr, c, h, w, k, pad, 1, 0, 0, 0};
        check(fx1_npu_build_pool(&i, &p) != 0, "pool job built");
        fill(in_addr, in, c * h * w);
        expect.assign(i.out_bytes, 0);
        npu_oracle_pool(&p, in.data(), expect.data());
        std::vector<uint8_t> canary(i.out_bytes, 0x5A);
        g_bench->mem->poke(out_addr, canary.data(), canary.size());
    }
    bool matches() const
    {
        std::vector<int8_t> got(i.out_bytes);
        g_bench->mem->peek(p.out, got.data(), got.size());
        return got == expect;
    }
};

struct GemmJob {
    fx1_npu_gemm_t g{kGemmA, kGemmW, kGemmC, 0};
    std::vector<int8_t> a, w;
    std::vector<int32_t> expect;
    void make(uint32_t k)
    {
        g.k = k;
        fill(kGemmA, a, 32 * k);
        fill(kGemmW, w, k * 32);
        expect.assign(32 * 32, 0);
        npu_oracle_gemm_bank(a.data(), w.data(), k, expect.data());
        std::vector<uint8_t> canary(32 * 32 * 4, 0x5A);
        g_bench->mem->poke(kGemmC, canary.data(), canary.size());
    }
    bool matches() const
    {
        std::vector<int32_t> got(32 * 32);
        g_bench->mem->peek(kGemmC, got.data(), got.size() * 4);
        return got == expect;
    }
};

bool wait_held(Bench& b)
{
    const sc_time end = sc_time_stamp() + sc_time(20, SC_MS);
    while (!b.mem->write_held && sc_time_stamp() < end) wait(50, SC_NS);
    return b.mem->write_held;
}
void release_if_held(Bench& b)
{
    g_release_at = -1;
    if (b.mem->write_held) b.mem->write_release.notify(SC_ZERO_TIME);
    wait(SC_ZERO_TIME);
}

bool latches_clear(Bench& b)
{
    return (b.cpu0->bank(STATUS) & (STATUS_ERROR | STATUS_DONE)) == 0 && b.cpu0->bank(IRQ_STATUS) == 0 &&
           b.cpu0->bank(VP_ERROR) == 0 && (b.cpu0->rich_rd(MODEL_STATUS) & RICH_STATUS_ERROR) == 0 &&
           b.cpu0->bank(LAST_ERROR) == 0;
}

fx1_npu_status_t npu_open(fx1_npu_t* npu, int irq, uint32_t required = FX1_NPU_FEATURE_NATIVE_GEMM | FX1_NPU_FEATURE_RICH_HAS |
                                                                       FX1_NPU_FEATURE_ABORT)
{
    return fx1_npu_init(npu, 0, required, irq, now_ns, kHz);
}

std::string st(fx1_npu_status_t s) { return fx1_npu_status_name(s); }

// ---- R1: a completion inside a refused push's handling -----------------------------
// The batch: one MAX_POOL whose single output write (4 KiB) is held; the
// refused push (FUSED_ATTN, UNSUPPORTED) runs with the release at access k.
void race_rich(Bench& b, bool irq, bool fail)
{
    fx1_npu_t npu{};
    Isr isr;
    check(npu_open(&npu, irq) == FX1_NPU_OK, "init");
    if (irq) isr.attach(b, &npu);
    check(fx1_npu_set_window(&npu, kWin, kWinSize) == FX1_NPU_OK, "window");
    const uint32_t ra[] = {FX1_NPU_M_OUT_ADDR};
    const uint32_t rv[] = {kWin + 0x40000u};
    fx1_npu_instr_t refused;
    fx1_npu_build_raw(&refused, FX1_NPU_OP_FUSED_ATTN, ra, rv, 1);
    unsigned covered = 0, refusal_accesses = 0, bad = 0, done_irqs = 0;
    for (int k = 0; k <= 48; ++k) {
        PoolJob job;
        job.make(kWin, kWin + 0x10000u, 4, 32, 32, 3, 1);  // 4 KiB output: one write
        const uint32_t acc0 = fx1_npu_accepted(&npu), ret0 = fx1_npu_retired(&npu);
        isr.count = 0;
        isr.bits = 0;
        b.mem->hold_write = 1;
        b.mem->writes_seen = 0;
        b.mem->fail_held = fail;
        bool ok = fx1_npu_rich_submit(&npu, &job.i, kMs) == FX1_NPU_OK && wait_held(b);
        const unsigned a0 = accesses();
        g_counted = 0;
        g_release_at = k;
        const fx1_npu_status_t r = fx1_npu_rich_submit(&npu, &refused, kMs);
        const bool released_inside = g_release_at < 0;
        refusal_accesses = accesses() - a0;
        release_if_held(b);
        covered += released_inside;
        // VP_ERROR bits accumulate; RICH_STATUS.CODE is the latest error's (the
        // failing copy-back may land after the refusal, inside the snapshot).
        const uint32_t code = FX1_NPU_RICH_STATUS_CODE(npu.last.rich_status);
        ok &= r == FX1_NPU_E_REFUSED && (npu.last.vp_error & FX1_NPU_VP_ERROR_UNSUPPORTED) &&
              (code == FX1_NPU_RICH_ERR_VP_UNSUPPORTED || (fail && code == FX1_NPU_RICH_ERR_VP_DMA));
        const fx1_npu_err_t refusal_last = npu.last;
        const fx1_npu_status_t w = fx1_npu_rich_wait(&npu, 20 * kMs);
        if (fail) {
            ok &= w == FX1_NPU_E_JOB && (npu.last.vp_error & FX1_NPU_VP_ERROR_DMA) && fx1_npu_retired(&npu) == ret0;
        } else {
            ok &= w == FX1_NPU_OK && job.matches() && fx1_npu_retired(&npu) == ret0 + 1;
        }
        ok &= fx1_npu_accepted(&npu) == acc0 + 1 && latches_clear(b) && npu.rich_inflight == 0 && !npu.latched;
        if (irq) {
            wait(100, SC_NS);
            // The refusal raises ERROR at once. A batch that ends after the
            // refusal ends without DONE (N2-I): always so when the release
            // came after the call; before it, DONE only for a batch that
            // passed and ended before the push was refused.
            done_irqs += (isr.bits & FX1_NPU_IRQ_DONE) != 0;
            ok &= isr.count >= 1 && (isr.bits & FX1_NPU_IRQ_ERROR) && !b.irq.read();
            if (!released_inside || fail) ok &= !(isr.bits & FX1_NPU_IRQ_DONE);
        }
        if (!ok) {
            ++bad;
            std::printf("  release at access %d: refusal %s (code 0x%x vp 0x%x), wait %s (last vp 0x%x), retired +%u, "
                        "irq %u bits 0x%x, STATUS 0x%x VP_ERROR 0x%x\n",
                        k, st(r).c_str(), FX1_NPU_RICH_STATUS_CODE(refusal_last.rich_status), refusal_last.vp_error,
                        st(w).c_str(), npu.last.vp_error, fx1_npu_retired(&npu) - ret0, isr.count, isr.bits,
                        b.cpu0->bank(STATUS), b.cpu0->bank(VP_ERROR));
        }
    }
    std::printf("  refused push handling: %u accesses; completion released inside it at %u points\n", refusal_accesses,
                covered);
    if (irq) std::printf("  DONE interrupts: %u of 49 (batches that ended before the refusal)\n", done_irqs);
    check(covered >= refusal_accesses && covered > 10, "every access of the refusal handling was a completion point");
    check(bad == 0, std::string(fail ? "failing" : "passing") + " batch completing inside a refusal: " +
                        (fail ? "the wait reports JOB (no false success)" : "refusal reported, batch passes, oracle matches") +
                        ", latches clear afterwards (" + std::to_string(bad) + " bad of 49)");
    // A good job afterwards.
    PoolJob job;
    job.make(kWin, kWin + 0x10000u, 4, 32, 32, 3, 1);
    check(fx1_npu_rich_submit(&npu, &job.i, kMs) == FX1_NPU_OK && fx1_npu_rich_wait(&npu, 20 * kMs) == FX1_NPU_OK &&
              job.matches(),
          "next job passes");
    check(g_bus_errors == 0, "no bus error");
}

// ---- GEMM-bank job outstanding ----------------------------------------------------
void gemm_gate(Bench& b, bool irq)
{
    fx1_npu_t npu{};
    Isr isr;
    check(npu_open(&npu, irq) == FX1_NPU_OK, std::string("init, interrupts ") + (irq ? "on" : "off"));
    if (irq) isr.attach(b, &npu);
    check(fx1_npu_set_window(&npu, kWin, kWinSize) == FX1_NPU_OK, "window");
    PoolJob pool;
    pool.make(kWin, kWin + 0x10000u, 4, 16, 16, 3, 1);
    GemmJob g;
    g.make(992);
    b.mem->hold_write = 1;  // the job's output (4 KiB, one write)
    b.mem->writes_seen = 0;
    check(fx1_npu_gemm_start(&npu, &g.g) == FX1_NPU_OK && wait_held(b), "GEMM K=992 runs, output write held");
    const unsigned a0 = accesses();
    const fx1_npu_status_t s1 = fx1_npu_rich_submit(&npu, &pool.i, kMs);
    const fx1_npu_status_t s2 = fx1_npu_set_window(&npu, kWin, 0x4000u);
    const fx1_npu_status_t s3 = fx1_npu_gemm_start(&npu, &g.g);
    const fx1_npu_status_t s4 = fx1_npu_soft_reset(&npu, kMs);
    const fx1_npu_status_t s5 = fx1_npu_clear_errors(&npu);
    check(s1 == FX1_NPU_E_NOT_IDLE && s2 == FX1_NPU_E_NOT_IDLE && s3 == FX1_NPU_E_NOT_IDLE && s4 == FX1_NPU_E_NOT_IDLE &&
              s5 == FX1_NPU_E_NOT_IDLE,
          "rich submit, set_window, gemm_start, soft_reset, clear_errors: NOT_IDLE (" + st(s1) + " " + st(s2) + " " +
              st(s3) + " " + st(s4) + " " + st(s5) + ")");
    check(accesses() == a0, "... without a single access (" + std::to_string(accesses() - a0) + ")");
    check(fx1_npu_gemm_poll(&npu) == FX1_NPU_PENDING, "poll: PENDING");
    release_if_held(b);
    const fx1_npu_status_t w = fx1_npu_gemm_wait(&npu, 20 * kMs);
    check(w == FX1_NPU_OK && g.matches(), "GEMM job completes, 1024 INT32 equal the oracle (" + st(w) + ")");
    if (irq) {
        wait(100, SC_NS);
        check(isr.count == 1 && isr.bits == FX1_NPU_IRQ_DONE && !b.irq.read(), "one DONE interrupt, level low after the ack");
    }
    check(latches_clear(b), "latches clear");
    // The reviewer's sequence below the driver: a push refused BUSY by the NPU
    // (test code) while the job runs; the job's verdict is DONE, the stray
    // error is kept in npu->last and cleared at the quiet point.
    g.make(992);
    b.mem->hold_write = 1;
    b.mem->writes_seen = 0;
    isr.count = 0;
    isr.bits = 0;
    check(fx1_npu_gemm_start(&npu, &g.g) == FX1_NPU_OK && wait_held(b), "GEMM again, output held");
    g_bench->cpu0->rich(MODEL_PUSH_A, RICH_OPCODE_ELEM_WISE);  // below the driver
    check(b.cpu0->bank(VP_ERROR) & FX1_NPU_VP_ERROR_BUSY, "raw push refused BUSY by the NPU");
    release_if_held(b);
    check(fx1_npu_gemm_wait(&npu, 20 * kMs) == FX1_NPU_OK && g.matches(), "GEMM job still passes (DONE decides)");
    check((npu.last.vp_error & FX1_NPU_VP_ERROR_BUSY) && latches_clear(b), "stray error kept in npu->last, latches clear");
    check(fx1_npu_rich_submit(&npu, &pool.i, kMs) == FX1_NPU_OK && fx1_npu_rich_wait(&npu, 20 * kMs) == FX1_NPU_OK,
          "rich work after the GEMM job");
    check(g_bus_errors == 0, "no bus error");
}

// ---- R2: window while busy ---------------------------------------------------------
void window(Bench& b)
{
    fx1_npu_t npu{};
    check(npu_open(&npu, 0) == FX1_NPU_OK, "init");
    check(fx1_npu_set_window(&npu, kWin, 0x2000u) == FX1_NPU_OK && npu.win_size == 0x2000u, "window 8 KiB");
    auto unchanged = [&](const char* what) {
        check(npu.win_base == kWin && npu.win_size == 0x2000u && b.cpu0->bank(RICH_WINDOW_SIZE) == 0x2000u &&
                  b.cpu0->bank(RICH_WINDOW_BASE) == kWin,
              std::string(what) + ": cache and hardware keep 8 KiB");
    };
    GemmJob g;
    g.make(992);
    check(fx1_npu_gemm_start(&npu, &g.g) == FX1_NPU_OK, "GEMM K=992 starts");
    check(fx1_npu_set_window(&npu, kWin, 0x4000u) == FX1_NPU_E_NOT_IDLE, "set_window during GEMM: NOT_IDLE");
    unchanged("during GEMM");
    check(fx1_npu_gemm_wait(&npu, 20 * kMs) == FX1_NPU_OK && g.matches(), "GEMM passes");
    // Rich work running, then done but not consumed.
    check(fx1_npu_set_window(&npu, kWin, kWinSize) == FX1_NPU_OK, "window 2 MiB at idle");
    PoolJob p;
    p.make(kWin, kWin + 0x40000u, 32, 32, 32, 5, 2);
    check(fx1_npu_rich_submit(&npu, &p.i, kMs) == FX1_NPU_OK, "long rich job");
    check(fx1_npu_set_window(&npu, kWin, 0x2000u) == FX1_NPU_E_NOT_IDLE && npu.win_size == kWinSize &&
              b.cpu0->bank(RICH_WINDOW_SIZE) == kWinSize,
          "set_window during rich work: NOT_IDLE, unchanged");
    check(wait_rich_idle(*b.cpu0), "rich side idle (seen below the driver)");
    check(fx1_npu_set_window(&npu, kWin, 0x2000u) == FX1_NPU_E_NOT_IDLE, "batch not consumed yet: NOT_IDLE");
    check(fx1_npu_rich_wait(&npu, kMs) == FX1_NPU_OK && p.matches(), "batch consumed, output equals the oracle");
    check(fx1_npu_set_window(&npu, kWin, 0x2000u) == FX1_NPU_OK && b.cpu0->bank(RICH_WINDOW_SIZE) == 0x2000u,
          "set_window after the wait");
    unchanged("after the wait");
    check(g_bus_errors == 0, "no bus error (no store access fault): " + std::to_string(g_bus_errors));
}

// ---- R3: init after a failed recovery ---------------------------------------------
void reinit(Bench& b)
{
    fx1_npu_t npu{};
    check(npu_open(&npu, 0) == FX1_NPU_OK, "init");
    check(fx1_npu_set_window(&npu, kWin, kWinSize) == FX1_NPU_OK, "window");
    PoolJob p;
    p.make(kWin, kWin + 0x40000u, 64, 32, 32, 5, 2);
    check(fx1_npu_rich_submit(&npu, &p.i, kMs) == FX1_NPU_OK, "long rich job");
    wait(2, SC_US);
    check(fx1_npu_recover(&npu, 1) == FX1_NPU_E_UNAVAILABLE && npu.unavailable, "recovery with a 1 ns limit: UNAVAILABLE");
    const fx1_npu_err_t saved = npu.last;
    const auto same_last = [&] { return std::memcmp(&saved, &npu.last, sizeof saved) == 0; };
    check(b.cpu0->bank(VP_STATUS) & VP_STATUS_ENGINE_BUSY, "the aborted instruction still runs (ENGINE_BUSY)");

    fx1_npu_status_t s = npu_open(&npu, 0);
    check(s == FX1_NPU_E_NOT_IDLE && npu.unavailable && same_last(),
          "init while the engine runs: NOT_IDLE, still unavailable, diagnostics kept (" + st(s) + ", VP_STATUS " +
              hex(b.cpu0->bank(VP_STATUS)) + ")");
    s = npu_open(&npu, 0, FX1_NPU_FEATURE_RICH_HAS | 0x80000000u);
    check(s == FX1_NPU_E_FEATURE && npu.unavailable && same_last(), "init with a missing feature: FEATURE, closed, kept");
    s = fx1_npu_init(&npu, 4, 0, 0, now_ns, kHz);  // VP_ID reads VP_ABI there
    check(s == FX1_NPU_E_ABSENT && npu.unavailable && same_last(), "init at a wrong base: ABSENT, closed, kept");
    s = fx1_npu_init(&npu, 0, 0, 0, nullptr, kHz);
    check(s == FX1_NPU_E_ARG && npu.unavailable && same_last(), "init without a time source: ARG, closed, kept");

    const unsigned a0 = accesses();
    fx1_npu_instr_t i = p.i;
    fx1_npu_neo_t n{};
    const fx1_npu_gemm_t gg{kGemmA, kGemmW, kGemmC, 16};
    const fx1_npu_status_t r[] = {fx1_npu_gemm_start(&npu, &gg), fx1_npu_rich_submit(&npu, &i, kMs),
                                  fx1_npu_neo_submit(&npu, &n, kMs),      fx1_npu_set_window(&npu, kWin, 0x2000u),
                                  fx1_npu_rich_poll(&npu),                fx1_npu_gemm_poll(&npu),
                                  fx1_npu_recover(&npu, kMs),             fx1_npu_soft_reset(&npu, kMs),
                                  fx1_npu_clear_errors(&npu)};
    bool all = true;
    for (auto x : r) all &= x == FX1_NPU_E_UNAVAILABLE;
    check(all && accesses() == a0, "closed context: every call UNAVAILABLE without an access");

    // A context that was never initialised (garbage), opened while busy: fails closed.
    fx1_npu_t junk;
    std::memset(&junk, 0xA5, sizeof junk);
    check(npu_open(&junk, 0) == FX1_NPU_E_NOT_IDLE && junk.unavailable == 1, "garbage context, busy NPU: NOT_IDLE, closed");
    const unsigned a1 = accesses();
    check(fx1_npu_rich_submit(&junk, &i, kMs) == FX1_NPU_E_UNAVAILABLE && accesses() == a1,
          "garbage context refuses work without an access");

    const sc_time t0 = sc_time_stamp();
    while ((b.cpu0->bank(VP_STATUS) & (VP_STATUS_ENGINE_BUSY | VP_STATUS_ABORT_ACTIVE)) && sc_time_stamp() - t0 < sc_time(20, SC_MS))
        wait(1, SC_US);
    std::printf("  engine idle %.1f us after the failed recovery\n", (sc_time_stamp() - t0).to_seconds() * 1e6);
    s = npu_open(&npu, 0);
    check(s == FX1_NPU_OK && !npu.unavailable && npu.last.vp_error == 0 && latches_clear(b),
          "init after the engine went idle: OK, reopened, latches clear (" + st(s) + ")");
    check(fx1_npu_set_window(&npu, kWin, kWinSize) == FX1_NPU_OK, "window");
    p.make(kWin, kWin + 0x40000u, 8, 16, 16, 3, 1);
    check(fx1_npu_rich_submit(&npu, &p.i, kMs) == FX1_NPU_OK && fx1_npu_rich_wait(&npu, 20 * kMs) == FX1_NPU_OK && p.matches(),
          "rich job after the reopen equals the oracle");
    GemmJob g;
    g.make(64);
    check(fx1_npu_gemm_start(&npu, &g.g) == FX1_NPU_OK && fx1_npu_gemm_wait(&npu, 20 * kMs) == FX1_NPU_OK && g.matches(),
          "GEMM-bank job after the reopen equals the oracle");
    check(npu_open(&junk, 0) == FX1_NPU_OK && junk.unavailable == 0, "garbage context opens once the NPU is idle");
    check(g_bus_errors == 0, "no bus error");
}

// ---- poll / timeout semantics -------------------------------------------------------
void poll(Bench& b)
{
    fx1_npu_t npu{};
    check(npu_open(&npu, 0) == FX1_NPU_OK, "init");
    check(fx1_npu_gemm_poll(&npu) == FX1_NPU_E_ARG, "gemm_poll without a job: ARG");
    const unsigned pw = g_writes;
    check(fx1_npu_rich_poll(&npu) == FX1_NPU_OK && g_writes == pw, "rich_poll with nothing accepted: OK, nothing written");
    GemmJob g;
    g.make(992);
    check(fx1_npu_gemm_start(&npu, &g.g) == FX1_NPU_OK, "GEMM K=992 starts");
    check(fx1_npu_gemm_poll(&npu) == FX1_NPU_PENDING && npu.last.status == 0 && !npu.latched,
          "gemm_poll while running: PENDING, nothing recorded");
    check(fx1_npu_gemm_wait(&npu, 100) == FX1_NPU_E_TIMEOUT && npu.gemm_active && (npu.last.status & FX1_NPU_STATUS_BUSY),
          "gemm_wait of 100 ns: TIMEOUT, job still outstanding, state recorded");
    check(fx1_npu_gemm_wait(&npu, 20 * kMs) == FX1_NPU_OK && g.matches() && !npu.gemm_active, "waited again: OK, oracle");

    // Credit timeout: one job executing (output held) and a full queue.
    check(fx1_npu_set_window(&npu, kWin, kWinSize) == FX1_NPU_OK, "window");
    PoolJob p;
    p.make(kWin, kWin + 0x10000u, 4, 32, 32, 3, 1);
    b.mem->hold_write = 1;
    b.mem->writes_seen = 0;
    bool ok = fx1_npu_rich_submit(&npu, &p.i, kMs) == FX1_NPU_OK && wait_held(b);
    for (unsigned k = 0; k < npu.queue_depth; ++k) ok &= fx1_npu_rich_submit(&npu, &p.i, kMs) == FX1_NPU_OK;
    check(ok && npu.rich_inflight == npu.queue_depth + 1, "one job executing, " + std::to_string(npu.queue_depth) + " queued");
    const uint32_t acc = fx1_npu_accepted(&npu);
    const unsigned w0 = g_writes;
    const fx1_npu_status_t s = fx1_npu_rich_submit(&npu, &p.i, 2000);
    check(s == FX1_NPU_E_TIMEOUT && g_writes == w0 && fx1_npu_accepted(&npu) == acc,
          "no credit in 2 us: TIMEOUT, nothing written, nothing accepted (" + st(s) + ")");
    check(fx1_npu_rich_poll(&npu) == FX1_NPU_PENDING, "earlier work still runs: PENDING");
    release_if_held(b);
    check(fx1_npu_rich_wait(&npu, 50 * kMs) == FX1_NPU_OK && p.matches() && npu.rich_inflight == 0,
          "the batch of 17 completes: OK, output equals the oracle");
    check(latches_clear(b) && g_bus_errors == 0, "latches clear, no bus error");
}

} // namespace

int sc_main(int argc, char* argv[])
{
    const std::string c = argc > 1 ? argv[1] : "";
    const std::map<std::string, std::function<void(Bench&)>> cases = {
        {"race_rich", [](Bench& b) { race_rich(b, false, false); }},
        {"race_rich_irq", [](Bench& b) { race_rich(b, true, false); }},
        {"race_rich_fail", [](Bench& b) { race_rich(b, false, true); }},
        {"race_rich_fail_irq", [](Bench& b) { race_rich(b, true, true); }},
        {"gemm_gate", [](Bench& b) { gemm_gate(b, false); }},
        {"gemm_gate_irq", [](Bench& b) { gemm_gate(b, true); }},
        {"window", window},
        {"reinit", reinit},
        {"poll", poll},
    };
    const auto it = cases.find(c);
    if (it == cases.end()) {
        std::printf("usage: test_npu_driver_host <case>\n");
        return 2;
    }
    fx1::npu::NpuOptions o;
    return run_bench("direct", o, [&](Bench& b) {
        g_bench = &b;
        g_owner = sc_get_current_process_handle();
        it->second(b);
    });
}
