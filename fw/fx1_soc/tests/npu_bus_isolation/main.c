/*
 * FX1 NPU N1 review (N1-B, N1-E): a push that waits for a queue slot holds only
 * the NPU port of SYSBUS_1 (plan FX1_NPU_V47, review N1).
 *
 * Hart 0 keeps the NPU busy with one long MAX_POOL (entry 0) and fills the
 * 16-entry queue with short ones, then:
 *   A. N1-E: with VP_PUSH_WAIT_NS = 2 us, one more push is a store access
 *      fault (mcause 7, mtval = RICH_PUSH_A) and latches PUSH_TIMEOUT; the
 *      push is not accepted;
 *   B. N1-B: with VP_PUSH_WAIT_NS = 20 ms, one more push stalls until entry 0
 *      ends and frees a slot. Meanwhile hart 1 reads CLINT mtime, a PLIC
 *      priority and the UART flag register in a loop; complete loop
 *      iterations that started and ended inside the stall prove that the
 *      stalled store did not hold the SYSBUS_0 / PERIBUS_0 path;
 *   C. clean-up per N1-C: ABORT, then wait for QUIESCENT and ENGINE_BUSY = 0
 *      with a timeout (QUIESCENT alone is not recovery), check ABORTED.
 * Registers: components/FX1_Components/npu/include/fx1_npu/fx1_npu_regs.h
 * (plain accesses on purpose: the timed-out push must be a bare store).
 */
#include "fx1_fw.h"
#include "fx1_npu/fx1_npu_regs.h"

#define NPU(off) (FX1_NPU_VP_BASE + (off))
#define BANK(local) NPU(FX1_NPU_BANK(local))
#define RICH(model) NPU(FX1_NPU_RICH_OF_MODEL(model))
#define NPU_CTRL BANK(FX1_NPU_CTRL)
#define NPU_STATUS BANK(FX1_NPU_STATUS)
#define NPU_IRQ_ENABLE BANK(FX1_NPU_IRQ_ENABLE)
#define NPU_IRQ_STATUS BANK(FX1_NPU_IRQ_STATUS)
#define NPU_WIN_BASE BANK(FX1_NPU_RICH_WINDOW_BASE)
#define NPU_WIN_SIZE BANK(FX1_NPU_RICH_WINDOW_SIZE)
#define NPU_VP_ID BANK(FX1_NPU_VP_ID)
#define NPU_VP_QUEUE_FREE BANK(FX1_NPU_VP_QUEUE_FREE)
#define NPU_VP_CMD BANK(FX1_NPU_VP_CMD)
#define NPU_VP_STATUS BANK(FX1_NPU_VP_STATUS)
#define NPU_VP_ERROR BANK(FX1_NPU_VP_ERROR)
#define NPU_VP_PUSH_WAIT_NS BANK(FX1_NPU_VP_PUSH_WAIT_NS)
#define NPU_VP_ACCEPTED BANK(FX1_NPU_VP_ACCEPTED)
#define NPU_VP_ABORT_DROPPED BANK(FX1_NPU_VP_ABORT_DROPPED)
#define NPU_RICH_PUSH_A RICH(FX1_NPU_M_PUSH_A)
#define NPU_RICH_STATUS RICH(FX1_NPU_M_STATUS)

#define VP_STATUS_QUIESCENT FX1_NPU_VP_STATUS_QUIESCENT
#define VP_STATUS_ENGINE_BUSY FX1_NPU_VP_STATUS_ENGINE_BUSY
#define VP_ERROR_PUSH_TIMEOUT FX1_NPU_VP_ERROR_PUSH_TIMEOUT
#define VP_ERROR_ABORTED FX1_NPU_VP_ERROR_ABORTED
#define OPCODE_ELEM_WISE FX1_NPU_OP_ELEM_WISE
#define ELEM_MAX_POOL FX1_NPU_ELEM_MAX_POOL

#define WIN FX1_NPU_ARENA_SMALL_BASE
#define WIN_SIZE 0x00200000u
#define MTIME_LO (FX1_CLINT_BASE + FX1_CLINT_MTIME_OFF)
#define TICK_NS (1000000000u / FX1_MTIME_HZ)

/* ---- hart 1: peripheral loop ---------------------------------------------- */
#define MAX_SAMPLES 1024u
static volatile uint32_t phase; /* 0 wait, 1 sample, 2 stop */
static volatile uint32_t hart1_ready, n_samples;
static volatile uint32_t sample_start[MAX_SAMPLES], sample_end[MAX_SAMPLES];

void secondary_main(uint32_t hart)
{
    (void)hart;
    hart1_ready = 1;
    while (phase != 2) {
        if (phase != 1) continue;
        const uint32_t t = fx1_read32(MTIME_LO);                                  /* CLINT     */
        (void)fx1_read32(FX1_PLIC_BASE + FX1_PLIC_PRIORITY_OFF + 4u * FX1_IRQ_NPU); /* PLIC */
        (void)fx1_read32(FX1_UART_BASE + 0x18u);                                  /* UART FR   */
        const uint32_t e = fx1_read32(MTIME_LO);                                  /* CLINT     */
        const uint32_t n = n_samples;
        if (n < MAX_SAMPLES) {
            sample_start[n] = t;
            sample_end[n] = e;
        }
        n_samples = n + 1;
    }
    for (;;) fx1_wfi();
}

/* ---- hart 0 ---------------------------------------------------------------- */
static volatile uint32_t got_cause, got_tval, traps;
static volatile uint32_t resume_at;

static uint32_t handler(uint32_t mcause, uint32_t mepc, uint32_t mtval)
{
    (void)mepc;
    got_cause = mcause;
    got_tval = mtval;
    ++traps;
    return resume_at;
}

static void rich_write(uint32_t model, uint32_t value) { fx1_write32(RICH(model), value); }

/* MAX_POOL c x h x w, k x k, pad p, stride 1 (geometry registers as the
 * component tests program them); returns without pushing. */
static void pool_setup(uint32_t in_off, uint32_t out_off, uint32_t c, uint32_t h, uint32_t w, uint32_t k, uint32_t p)
{
    rich_write(FX1_NPU_M_A_ADDR, WIN + in_off);
    rich_write(FX1_NPU_M_OUT_ADDR, WIN + out_off);
    rich_write(FX1_NPU_M_MODE, ELEM_MAX_POOL);
    rich_write(FX1_NPU_M_STRIDE, 1u);
    rich_write(FX1_NPU_M_IN_C, c);
    rich_write(FX1_NPU_M_IN_H, h);
    rich_write(FX1_NPU_M_IN_W, w);
    rich_write(FX1_NPU_M_POOL_K, k);
    rich_write(FX1_NPU_M_POOL_P, p);
}
static void small_setup(unsigned i) { pool_setup(0x00100000u + 0x2000u * i, 0x00101000u + 0x2000u * i, 4, 8, 8, 3, 1); }
static void push_small(unsigned i)
{
    small_setup(i);
    fx1_write32(NPU_RICH_PUSH_A, OPCODE_ELEM_WISE);
}
static void clear_errors(void)
{
    fx1_write32(NPU_VP_CMD, FX1_NPU_VP_CMD_CLEAR_ERROR);
    fx1_write32(NPU_RICH_STATUS, 0);
    fx1_write32(NPU_STATUS, FX1_NPU_STATUS_DONE | FX1_NPU_STATUS_ERROR);
    fx1_write32(NPU_IRQ_STATUS, FX1_NPU_IRQ_DONE | FX1_NPU_IRQ_ERROR);
}

/* Entry 0 running, 16 queued: QUEUE_FREE = 0. */
static void fill_queue(void)
{
    pool_setup(0x00000000u, 0x00080000u, 64, 64, 64, 5, 2); /* 256 KiB in, 256 KiB out */
    fx1_write32(NPU_RICH_PUSH_A, OPCODE_ELEM_WISE);
    const uint64_t start = fx1_mtime();
    while (fx1_read32(NPU_VP_QUEUE_FREE) != 16u || !(fx1_read32(NPU_VP_STATUS) & VP_STATUS_ENGINE_BUSY))
        fx1_check(fx1_mtime() - start < FX1_MTIME_HZ / 1000, 0x10, "entry 0 started within 1 ms");
    for (unsigned i = 0; i < 16; ++i) push_small(i);
    fx1_check(fx1_read32(NPU_VP_QUEUE_FREE) == 0, 0x11, "queue full while entry 0 runs (QUEUE_FREE 0)");
}

int main(void)
{
    fx1_set_trap_handler(handler);
    fx1_check(fx1_read32(NPU_VP_ID) == FX1_NPU_VP_ID_VALUE, 0x01, "NPU present (VP_ID FXNP)");
    fx1_write32(NPU_IRQ_ENABLE, 0);
    fx1_write32(NPU_CTRL, FX1_NPU_CTRL_ENABLE);
    fx1_write32(NPU_WIN_BASE, WIN);
    fx1_write32(NPU_WIN_SIZE, WIN_SIZE);
    fx1_release_secondaries();
    const uint64_t up = fx1_mtime();
    while (!hart1_ready) fx1_check(fx1_mtime() - up < FX1_MTIME_HZ / 100, 0x02, "hart 1 up within 10 ms");

    /* Baseline: the same store sequence answered at once with a TLM error
     * (store to read-only VP_ID): the trap path's own cost. */
    uint32_t t_before = traps;
    const uint32_t fa = fx1_read32(MTIME_LO);
    __asm__ volatile("la t0, 1f\n sw t0, 0(%0)\n sw %2, 0(%1)\n1:\n"
                     :
                     : "r"(&resume_at), "r"(NPU_VP_ID), "r"(1u)
                     : "t0", "memory");
    const uint32_t fb = fx1_read32(MTIME_LO);
    fx1_check(traps == t_before + 1 && got_cause == 7u, 0x03, "baseline store fault");
    const uint32_t fault_ns = (fb - fa) * TICK_NS;
    fx1_log_hex("npu: immediate store fault, mtime delta (ns)", fault_ns);

    /* A. N1-E: push timeout on a real hart = store access fault. */
    fill_queue();
    fx1_write32(NPU_VP_PUSH_WAIT_NS, 2000);
    small_setup(16); /* parameters; the push below is the store under test */
    const uint32_t before = traps, accepted0 = fx1_read32(NPU_VP_ACCEPTED);
    const uint32_t ta = fx1_read32(MTIME_LO);
    __asm__ volatile("la t0, 1f\n sw t0, 0(%0)\n sw %2, 0(%1)\n1:\n"
                     :
                     : "r"(&resume_at), "r"(NPU_RICH_PUSH_A), "r"(OPCODE_ELEM_WISE)
                     : "t0", "memory");
    const uint32_t tb = fx1_read32(MTIME_LO);
    fx1_check(traps == before + 1 && got_cause == 7u && got_tval == NPU_RICH_PUSH_A, 0x20,
              "push into the full queue after 2 us: store access fault, mtval = RICH_PUSH_A");
    fx1_check((fx1_read32(NPU_VP_ERROR) & VP_ERROR_PUSH_TIMEOUT) && fx1_read32(NPU_VP_ACCEPTED) == accepted0, 0x21,
              "PUSH_TIMEOUT latched, the push was not accepted");
    /* Minus the trap path (baseline); the hart runs with a 1 us quantum. */
    const uint32_t wait_ns = (tb - ta) * TICK_NS - fault_ns;
    fx1_log_hex("npu: timed-out push, mtime delta (ns)", (tb - ta) * TICK_NS);
    fx1_log_hex("npu: timed-out push minus the trap path (ns)", wait_ns);
    fx1_check((int32_t)wait_ns >= 1000 && wait_ns <= 4000u, 0x22, "the store waited the 2 us limit (+-1 us quantum)");
    clear_errors();
    fx1_log("npu: N1-E push timeout = store access fault on hart 0");

    /* B. N1-B: a long stall on hart 0, hart 1 on CLINT/PLIC/UART meanwhile. */
    fx1_check(fx1_read32(NPU_VP_QUEUE_FREE) == 0 && (fx1_read32(NPU_VP_STATUS) & VP_STATUS_ENGINE_BUSY), 0x30,
              "still full, entry 0 still running");
    fx1_write32(NPU_VP_PUSH_WAIT_NS, 20000000u);
    small_setup(17); /* the timed-out push cleared the one-shot extension registers */
    const uint32_t accepted1 = fx1_read32(NPU_VP_ACCEPTED);
    n_samples = 0;
    fx1_mb();
    phase = 1;
    fx1_mb();
    while (n_samples == 0) {
    }
    const uint32_t t0 = fx1_read32(MTIME_LO);
    fx1_write32(NPU_RICH_PUSH_A, OPCODE_ELEM_WISE); /* stalls until entry 0 frees a slot */
    const uint32_t t1 = fx1_read32(MTIME_LO);
    phase = 2;
    fx1_mb();
    fx1_check(fx1_read32(NPU_VP_ACCEPTED) == accepted1 + 1 && !(fx1_read32(NPU_VP_ERROR) & VP_ERROR_PUSH_TIMEOUT), 0x31,
              "the stalled push was accepted when a slot freed (no timeout)");
    const uint32_t stall_ns = (t1 - t0) * TICK_NS;
    fx1_check(stall_ns >= 5000u, 0x32, "the push stalled at least 5 us");
    const uint32_t n = n_samples < MAX_SAMPLES ? n_samples : MAX_SAMPLES;
    uint32_t inside = 0;
    for (uint32_t i = 0; i < n; ++i)
        inside += (int32_t)(sample_start[i] - t0) > 0 && (int32_t)(t1 - sample_end[i]) > 0;
    fx1_log_hex("npu: push stall (ns)", stall_ns);
    fx1_log_hex("npu: hart 1 CLINT+PLIC+UART rounds inside the stall", inside);
    fx1_check(inside >= 8u, 0x33, "hart 1 completed CLINT, PLIC and UART accesses while hart 0's push was stalled");
    fx1_log("npu: N1-B stalled push does not hold CLINT/PLIC/UART");

    /* C. ABORT per N1-C, with work to abort: a long entry 0 on the model and 16
     * queued. QUIESCENT comes first (no DMA will follow); ENGINE_BUSY stays 1
     * until the model finished entry 0, so the driver waits for both, with a
     * timeout. */
    const uint64_t drain = fx1_mtime();
    while (fx1_read32(NPU_RICH_STATUS) & FX1_NPU_RICH_STATUS_BUSY)
        fx1_check(fx1_mtime() - drain < FX1_MTIME_HZ / 50, 0x34, "queue drained within 20 ms");
    clear_errors();
    fill_queue();
    fx1_write32(NPU_VP_CMD, FX1_NPU_VP_CMD_ABORT);
    const uint32_t vs0 = fx1_read32(NPU_VP_STATUS);
    fx1_log_hex("npu: VP_STATUS right after ABORT", vs0);
    fx1_check((vs0 & VP_STATUS_ENGINE_BUSY) && fx1_read32(NPU_VP_QUEUE_FREE) == 16u && fx1_read32(NPU_VP_ABORT_DROPPED) == 16u, 0x43,
              "ABORT dropped 16 queued entries; the model still runs entry 0 (ENGINE_BUSY)");
    const uint64_t ab = fx1_mtime();
    uint32_t vs;
    while (((vs = fx1_read32(NPU_VP_STATUS)) & (VP_STATUS_QUIESCENT | VP_STATUS_ENGINE_BUSY)) != VP_STATUS_QUIESCENT)
        fx1_check(fx1_mtime() - ab < FX1_MTIME_HZ / 50, 0x40, "QUIESCENT and ENGINE_BUSY 0 within 20 ms after ABORT");
    const uint64_t idle_at = fx1_mtime();
    fx1_check((fx1_read32(NPU_VP_ERROR) & VP_ERROR_ABORTED) && (fx1_read32(NPU_STATUS) & FX1_NPU_STATUS_ERROR), 0x41,
              "VP_ERROR.ABORTED, STATUS ERROR");
    clear_errors();
    fx1_check(fx1_read32(NPU_VP_ERROR) == 0 && fx1_read32(NPU_VP_QUEUE_FREE) == 16, 0x42, "errors cleared, queue empty");
    fx1_log_hex("npu: ENGINE_BUSY cleared after ABORT (ns)", (uint32_t)(idle_at - ab) * TICK_NS);
    fx1_log("npu: ABORT, quiescent and engine idle");
    return 0;
}
