/*
 * FX1 NPU reference bare-metal driver: MMIO part (fx1_npu.h).
 *
 * Completion and error latches (review N2 R1). STATUS.DONE/ERROR, IRQ_STATUS,
 * LAST_ERROR, RICH_STATUS.ERROR and VP_ERROR are shared by the GEMM bank, the
 * rich queue and the push refusals, and the NPU completes work asynchronously
 * to the owner. So:
 *   - the driver clears latches only when the NPU is quiet: no job it accepted
 *     is still waiting to be consumed by a wait/poll, and every engine is idle
 *     (no completion can then land between its reads and its writes);
 *   - a failure that is not consumed by the wait of its own job (a refused
 *     push while other work runs) is reported at once (npu->last) and its
 *     latches stay until the next quiet point;
 *   - the verdict of a wait never comes from the shared error bits: a GEMM-bank
 *     job passed when STATUS.DONE is set (an error clears it); the rich side
 *     passed when RETIRED advanced by the number of instructions the driver
 *     had accepted (the NPU retires an instruction only after its copy-back).
 */
#include "fx1_npu.h"

#include "fx1_fw.h"

static uint32_t rd(const fx1_npu_t* npu, uint32_t off) { return fx1_read32(npu->base + off); }
static void wr(const fx1_npu_t* npu, uint32_t off, uint32_t v) { fx1_write32(npu->base + off, v); }
static uint32_t bank(const fx1_npu_t* npu, uint32_t local) { return rd(npu, FX1_NPU_BANK(local)); }
static void set_bank(const fx1_npu_t* npu, uint32_t local, uint32_t v) { wr(npu, FX1_NPU_BANK(local), v); }
static uint32_t rich(const fx1_npu_t* npu, uint32_t model) { return rd(npu, FX1_NPU_RICH_OF_MODEL(model)); }
static void set_rich(const fx1_npu_t* npu, uint32_t model, uint32_t v) { wr(npu, FX1_NPU_RICH_OF_MODEL(model), v); }

static int expired(const fx1_npu_t* npu, uint64_t start, uint64_t ticks) { return npu->now() - start >= ticks; }

uint32_t fx1_npu_bank_read(const fx1_npu_t* npu, uint32_t local) { return bank(npu, local); }
uint32_t fx1_npu_native_read(const fx1_npu_t* npu, uint32_t offset) { return rd(npu, offset); }
static int owned(const fx1_npu_t* npu);
void fx1_npu_native_write(const fx1_npu_t* npu, uint32_t offset, uint32_t value)
{
    if (owned(npu)) wr(npu, offset, value);
}
uint32_t fx1_npu_retired(const fx1_npu_t* npu) { return rich(npu, FX1_NPU_M_RETIRED); }
uint32_t fx1_npu_accepted(const fx1_npu_t* npu) { return bank(npu, FX1_NPU_VP_ACCEPTED); }

void fx1_npu_read_errors(const fx1_npu_t* npu, fx1_npu_err_t* e)
{
    e->status = bank(npu, FX1_NPU_STATUS);
    e->last_error = bank(npu, FX1_NPU_LAST_ERROR);
    e->irq_status = bank(npu, FX1_NPU_IRQ_STATUS);
    e->rich_status = rich(npu, FX1_NPU_M_STATUS);
    e->vp_error = bank(npu, FX1_NPU_VP_ERROR);
    e->vp_error_info = bank(npu, FX1_NPU_VP_ERROR_INFO);
    e->vp_status = bank(npu, FX1_NPU_VP_STATUS);
}

static int rich_busy(const fx1_npu_t* npu) { return (rich(npu, FX1_NPU_M_STATUS) & FX1_NPU_RICH_STATUS_BUSY) != 0; }

/* Every engine idle: nothing queued or running on the rich side, no GEMM-bank
 * job or raw native run, no DMA (QUIESCENT). Reads only npu->base. */
static int hw_idle(const fx1_npu_t* npu)
{
    const uint32_t busy = FX1_NPU_VP_STATUS_ENGINE_BUSY | FX1_NPU_VP_STATUS_ABORT_ACTIVE | FX1_NPU_VP_STATUS_COPYBACK_ACTIVE |
                          FX1_NPU_VP_STATUS_NATIVE_BUSY | FX1_NPU_VP_STATUS_QUEUED_MASK;
    if ((bank(npu, FX1_NPU_VP_STATUS) & (FX1_NPU_VP_STATUS_QUIESCENT | busy)) != FX1_NPU_VP_STATUS_QUIESCENT) return 0;
    if (bank(npu, FX1_NPU_STATUS) & FX1_NPU_STATUS_BUSY) return 0;
    return !rich_busy(npu);
}
/* This context's hart may access the NPU (no lock, or it holds the lock). */
static int owned(const fx1_npu_t* npu) { return !npu->lock || *npu->lock == npu->hart + 1u; }
#define OWNER_OR_RETURN(npu)                                                                                         \
    do {                                                                                                             \
        if (!owned(npu)) return FX1_NPU_E_NOT_OWNER;                                                                 \
    } while (0)

/* Quiet: idle, and no accepted job is waiting for its wait/poll. */
static int quiet(const fx1_npu_t* npu) { return !npu->gemm_active && npu->rich_inflight == 0 && hw_idle(npu); }

static void clear_latches(const fx1_npu_t* npu)
{
    set_bank(npu, FX1_NPU_VP_CMD, FX1_NPU_VP_CMD_CLEAR_ERROR);
    set_rich(npu, FX1_NPU_M_STATUS, 0);
    set_bank(npu, FX1_NPU_STATUS, FX1_NPU_STATUS_DONE | FX1_NPU_STATUS_ERROR);
    set_bank(npu, FX1_NPU_IRQ_STATUS, FX1_NPU_IRQ_DONE | FX1_NPU_IRQ_ERROR);
    if (npu->feature & FX1_NPU_FEATURE_DFC_WINDOW) {
        set_rich(npu, FX1_NPU_M_NEO_BASE + FX1_NPU_NEO_CTRL_OFF, FX1_NPU_NEO_CTRL_DONE);
        set_rich(npu, FX1_NPU_M_NEO_BASE + FX1_NPU_NEO_ISTATUS_OFF, 1u);
    }
}

static int any_error(const fx1_npu_err_t* e)
{
    return (e->status & FX1_NPU_STATUS_ERROR) || (e->irq_status & FX1_NPU_IRQ_ERROR) ||
           (e->rich_status & FX1_NPU_RICH_STATUS_ERROR) || e->vp_error;
}

/* At a quiet point only: clears the latches when an error is latched. An error
 * no call reported (an access below the driver) is kept in npu->last first.
 * Without an error nothing is written (neo DONE/ISTATUS stay readable). */
static void settle(fx1_npu_t* npu)
{
    fx1_npu_err_t e;
    fx1_npu_read_errors(npu, &e);
    const int err = any_error(&e);
    if (!npu->latched && err) npu->last = e;
    if (npu->latched || err) clear_latches(npu);
    npu->latched = 0;
}

/* Reports a failure: npu->last; the latches are cleared when the NPU is quiet,
 * else kept (other work may still complete) until the next quiet point. */
static fx1_npu_status_t report(fx1_npu_t* npu, fx1_npu_status_t s)
{
    fx1_npu_read_errors(npu, &npu->last);
    npu->latched = 1;
    if (quiet(npu)) settle(npu);
    return s;
}
/* A timeout: the state in npu->last, nothing cleared (the work still runs). */
static fx1_npu_status_t snapshot(fx1_npu_t* npu, fx1_npu_status_t s)
{
    fx1_npu_read_errors(npu, &npu->last);
    npu->latched = 1;
    return s;
}

fx1_npu_status_t fx1_npu_clear_errors(fx1_npu_t* npu)
{
    if (npu->unavailable) return FX1_NPU_E_UNAVAILABLE;
    OWNER_OR_RETURN(npu);
    if (!quiet(npu)) return FX1_NPU_E_NOT_IDLE;
    clear_latches(npu);
    npu->latched = 0;
    return FX1_NPU_OK;
}

fx1_npu_status_t fx1_npu_claim(volatile uint32_t* lock, uint32_t hart)
{
    if (!lock) return FX1_NPU_E_ARG;
    uint32_t expected = 0;
    if (__atomic_compare_exchange_n(lock, &expected, hart + 1u, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) return FX1_NPU_OK;
    return expected == hart + 1u ? FX1_NPU_OK : FX1_NPU_E_BUSY;
}

fx1_npu_status_t fx1_npu_release(fx1_npu_t* npu, uintptr_t base, volatile uint32_t* lock, uint32_t hart)
{
    if (!lock) return FX1_NPU_E_ARG;
    if (*lock != hart + 1u) return FX1_NPU_E_NOT_OWNER;
    /* The handover needs the device itself idle (review N4 R1): a context that
     * is closed (a failed init or recovery) proves nothing about the DMA of the
     * work it started. So the engines are read at `base` in every case (a probe
     * context: only its base is used); an open context on this lock must also
     * have nothing outstanding. */
    fx1_npu_t t;
    t.base = base;
    if (!hw_idle(&t)) return FX1_NPU_E_NOT_IDLE;
    if (npu && !npu->unavailable && npu->lock == lock) {
        if (!quiet(npu)) return FX1_NPU_E_NOT_IDLE;
        npu->unavailable = 1;
    }
    __atomic_store_n(lock, 0u, __ATOMIC_RELEASE);
    return FX1_NPU_OK;
}

fx1_npu_status_t fx1_npu_init(fx1_npu_t* npu, uintptr_t base, uint32_t required, int irq, uint64_t (*now)(void),
                              uint32_t hz)
{
    return fx1_npu_init_shared(npu, base, required, irq, now, hz, 0, 0);
}

fx1_npu_status_t fx1_npu_init_shared(fx1_npu_t* npu, uintptr_t base, uint32_t required, int irq, uint64_t (*now)(void),
                                     uint32_t hz, volatile uint32_t* lock, uint32_t hart)
{
    if (!npu) return FX1_NPU_E_ARG;
    /* Closed until this init succeeds (review N2 R3). Nothing else of *npu is
     * read or written on a failure: it may never have been initialised, or
     * hold the diagnostics of the failure that made it unavailable. */
    npu->unavailable = 1;
    if (!now || !hz) return FX1_NPU_E_ARG;
    /* The ownership check uses the arguments: *npu may never have been set. */
    if (lock && *lock != hart + 1u) return FX1_NPU_E_NOT_OWNER;
    fx1_npu_t t; /* probe context: only t.base is read */
    t.base = base;
    if (bank(&t, FX1_NPU_VP_ID) != FX1_NPU_VP_ID_VALUE) return FX1_NPU_E_ABSENT;
    const uint32_t abi = bank(&t, FX1_NPU_VP_ABI);
    if (FX1_NPU_VP_ABI_MAJOR(abi) != FX1_NPU_VP_ABI_MAJOR(FX1_NPU_VP_ABI_VALUE)) return FX1_NPU_E_ABI;
    const uint32_t feature = bank(&t, FX1_NPU_VP_FEATURE);
    if ((feature & required) != required) return FX1_NPU_E_FEATURE;
    /* An engine still running (an aborted instruction after a failed
     * recovery, work of an earlier image) must finish first. */
    if (!hw_idle(&t)) return FX1_NPU_E_NOT_IDLE;
    npu->base = base;
    npu->now = now;
    npu->mtime_hz = hz;
    npu->lock = lock;
    npu->hart = hart;
    npu->abi = abi;
    npu->feature = feature;
    npu->model_hash = bank(npu, FX1_NPU_VP_MODEL_HASH);
    npu->queue_depth = bank(npu, FX1_NPU_VP_QUEUE_DEPTH);
    npu->window_max = bank(npu, FX1_NPU_VP_WINDOW_MAX);
    npu->win_base = bank(npu, FX1_NPU_RICH_WINDOW_BASE);
    npu->win_size = bank(npu, FX1_NPU_RICH_WINDOW_SIZE);
    npu->native_config_valid = 0;
    npu->gemm_active = 0;
    npu->rich_inflight = 0;
    npu->retired_mark = rich(npu, FX1_NPU_M_RETIRED);
    npu->last = (fx1_npu_err_t){0};
    set_bank(npu, FX1_NPU_IRQ_ENABLE, irq ? (FX1_NPU_IRQ_DONE | FX1_NPU_IRQ_ERROR) : 0u);
    set_bank(npu, FX1_NPU_CTRL, FX1_NPU_CTRL_ENABLE | (irq ? FX1_NPU_CTRL_IRQ_EN : 0u));
    clear_latches(npu); /* idle, nothing accepted: no completion to lose */
    npu->latched = 0;
    npu->unavailable = 0;
    return FX1_NPU_OK;
}

uint32_t fx1_npu_irq_ack(fx1_npu_t* npu)
{
    if (!owned(npu)) return 0;
    const uint32_t v = bank(npu, FX1_NPU_IRQ_STATUS);
    if (v) set_bank(npu, FX1_NPU_IRQ_STATUS, v);
    return v;
}

void fx1_npu_set_push_wait_ns(fx1_npu_t* npu, uint32_t ns)
{
    if (owned(npu)) set_bank(npu, FX1_NPU_VP_PUSH_WAIT_NS, ns);
}

static uint32_t ctrl_value(const fx1_npu_t* npu)
{
    return FX1_NPU_CTRL_ENABLE | (bank(npu, FX1_NPU_IRQ_ENABLE) ? FX1_NPU_CTRL_IRQ_EN : 0u);
}

/* ---- GEMM bank ----------------------------------------------------------------- */
fx1_npu_status_t fx1_npu_gemm_start(fx1_npu_t* npu, const fx1_npu_gemm_t* g)
{
    if (npu->unavailable) return FX1_NPU_E_UNAVAILABLE;
    OWNER_OR_RETURN(npu);
    if (!(npu->feature & FX1_NPU_FEATURE_NATIVE_GEMM)) return FX1_NPU_E_FEATURE;
    if (!g || !g->k) return FX1_NPU_E_ARG;
    /* The bank is locked (store access fault) while any work runs, and a start
     * would drop the latches of rich work not consumed yet: quiet first. */
    if (!quiet(npu)) return FX1_NPU_E_NOT_IDLE;
    settle(npu);
    const uint32_t ab = FX1_NPU_GEMM_ROWS * g->k, cb = FX1_NPU_GEMM_ROWS * FX1_NPU_GEMM_COLS * 4u;
    set_bank(npu, FX1_NPU_SRC_ADDR, g->a);
    set_bank(npu, FX1_NPU_SRC_SIZE_BYTES, ab);
    set_bank(npu, FX1_NPU_WEIGHTS_ADDR, g->w);
    set_bank(npu, FX1_NPU_WEIGHTS_SIZE_BYTES, g->k * FX1_NPU_GEMM_COLS);
    set_bank(npu, FX1_NPU_DST_ADDR, g->c);
    set_bank(npu, FX1_NPU_DST_SIZE_BYTES, cb);
    set_bank(npu, FX1_NPU_WIDTH, FX1_NPU_GEMM_COLS);
    set_bank(npu, FX1_NPU_HEIGHT, FX1_NPU_GEMM_ROWS);
    set_bank(npu, FX1_NPU_K_DIMENSION, g->k);
    set_bank(npu, FX1_NPU_FORMAT, FX1_NPU_FORMAT_INT8_INT8_INT32);
    set_bank(npu, FX1_NPU_OP_MODE, FX1_NPU_OP_GEMM);
    npu->gemm_active = 1;
    set_bank(npu, FX1_NPU_CTRL, ctrl_value(npu) | FX1_NPU_CTRL_START); /* STATUS = BUSY at once */
    return FX1_NPU_OK;
}

fx1_npu_status_t fx1_npu_gemm_poll(fx1_npu_t* npu)
{
    if (npu->unavailable) return FX1_NPU_E_UNAVAILABLE;
    OWNER_OR_RETURN(npu);
    if (!npu->gemm_active) return FX1_NPU_E_ARG;
    const uint32_t s = bank(npu, FX1_NPU_STATUS);
    if (s & FX1_NPU_STATUS_BUSY) return FX1_NPU_PENDING;
    npu->gemm_active = 0;
    /* An error ends the job with ERROR and without DONE; a latched ERROR next
     * to DONE is not the job's. Neither: the completion is lost (fail closed). */
    if (!(s & FX1_NPU_STATUS_DONE)) return report(npu, FX1_NPU_E_JOB);
    set_bank(npu, FX1_NPU_STATUS, FX1_NPU_STATUS_DONE); /* also IRQ_STATUS.DONE */
    npu->native_config_valid = 1;                        /* the GEMM bank programmed the core */
    if (quiet(npu)) settle(npu);
    return FX1_NPU_OK;
}

fx1_npu_status_t fx1_npu_gemm_wait(fx1_npu_t* npu, uint64_t timeout_ticks)
{
    const uint64_t start = npu->now();
    for (;;) {
        const fx1_npu_status_t s = fx1_npu_gemm_poll(npu);
        if (s != FX1_NPU_PENDING) return s;
        if (expired(npu, start, timeout_ticks)) return snapshot(npu, FX1_NPU_E_TIMEOUT); /* still running */
    }
}

/* ---- rich side ----------------------------------------------------------------------- */
fx1_npu_status_t fx1_npu_set_window(fx1_npu_t* npu, uint32_t base, uint32_t size)
{
    if (npu->unavailable) return FX1_NPU_E_UNAVAILABLE;
    OWNER_OR_RETURN(npu);
    if (!size || size > npu->window_max) return FX1_NPU_E_ARG;
    /* The configuration registers are locked while any work runs (review N2 R2). */
    if (!quiet(npu)) return FX1_NPU_E_NOT_IDLE;
    set_bank(npu, FX1_NPU_RICH_WINDOW_BASE, base);
    set_bank(npu, FX1_NPU_RICH_WINDOW_SIZE, size);
    npu->win_base = bank(npu, FX1_NPU_RICH_WINDOW_BASE);
    npu->win_size = bank(npu, FX1_NPU_RICH_WINDOW_SIZE);
    return npu->win_base == base && npu->win_size == size ? FX1_NPU_OK : FX1_NPU_E_REFUSED;
}

static fx1_npu_status_t wait_credit(fx1_npu_t* npu, uint64_t timeout_ticks)
{
    const uint64_t start = npu->now();
    while (bank(npu, FX1_NPU_VP_QUEUE_FREE) == 0)
        if (expired(npu, start, timeout_ticks)) return FX1_NPU_E_TIMEOUT; /* nothing written */
    return FX1_NPU_OK;
}

static fx1_npu_status_t after_push(fx1_npu_t* npu, uint32_t accepted0)
{
    if (bank(npu, FX1_NPU_VP_ACCEPTED) == accepted0 + 1u) {
        ++npu->rich_inflight;
        return FX1_NPU_OK;
    }
    return report(npu, FX1_NPU_E_REFUSED); /* the store did not fault: the refusal is latched */
}

static int in_window(const fx1_npu_t* npu, uint32_t a, uint32_t n)
{
    return n && a >= npu->win_base && a - npu->win_base <= npu->win_size && n <= npu->win_size - (a - npu->win_base);
}

fx1_npu_status_t fx1_npu_rich_submit(fx1_npu_t* npu, const fx1_npu_instr_t* i, uint64_t timeout_ticks)
{
    if (npu->unavailable) return FX1_NPU_E_UNAVAILABLE;
    OWNER_OR_RETURN(npu);
    if (!i || i->n > FX1_NPU_INSTR_MAX_WRITES) return FX1_NPU_E_ARG;
    if (i->out_bytes && !in_window(npu, i->out, i->out_bytes)) return FX1_NPU_E_ARG;
    /* The NPU refuses a push while a GEMM-bank job runs, and an accepted push
     * would drop its DONE: the GEMM-bank job is consumed first. */
    if (npu->gemm_active) return FX1_NPU_E_NOT_IDLE;
    const fx1_npu_status_t c = wait_credit(npu, timeout_ticks);
    if (c != FX1_NPU_OK) return c;
    const uint32_t accepted0 = bank(npu, FX1_NPU_VP_ACCEPTED);
    for (uint32_t k = 0; k < i->n; ++k) set_rich(npu, i->addr[k], i->val[k]);
    set_rich(npu, FX1_NPU_M_PUSH_A, i->push);
    return after_push(npu, accepted0);
}

fx1_npu_status_t fx1_npu_neo_submit(fx1_npu_t* npu, const fx1_npu_neo_t* n, uint64_t timeout_ticks)
{
    if (npu->unavailable) return FX1_NPU_E_UNAVAILABLE;
    OWNER_OR_RETURN(npu);
    if (!(npu->feature & FX1_NPU_FEATURE_DFC_WINDOW)) return FX1_NPU_E_FEATURE;
    if (!n || (n->out_bytes && !in_window(npu, n->out, n->out_bytes))) return FX1_NPU_E_ARG;
    if (npu->gemm_active) return FX1_NPU_E_NOT_IDLE;
    const fx1_npu_status_t c = wait_credit(npu, timeout_ticks);
    if (c != FX1_NPU_OK) return c;
    const uint32_t accepted0 = bank(npu, FX1_NPU_VP_ACCEPTED);
    const uint32_t neo = FX1_NPU_M_NEO_BASE;
    set_rich(npu, FX1_NPU_M_NEO_MARKER, 0);
    for (unsigned k = 0; k < FX1_NPU_NEO_REG_COUNT; ++k) set_rich(npu, neo + FX1_NPU_NEO_REG_OFF + 4u * k, n->word[k]);
    set_rich(npu, neo + FX1_NPU_NEO_IEN_OFF, 1u);
    set_rich(npu, neo + FX1_NPU_NEO_CTRL_OFF, FX1_NPU_NEO_CTRL_START);
    set_rich(npu, FX1_NPU_M_PUSH_A, FX1_NPU_OP_NEO_DOORBELL);
    return after_push(npu, accepted0);
}

fx1_npu_status_t fx1_npu_rich_poll(fx1_npu_t* npu)
{
    if (npu->unavailable) return FX1_NPU_E_UNAVAILABLE;
    OWNER_OR_RETURN(npu);
    if (rich_busy(npu)) return FX1_NPU_PENDING;
    /* Idle: RETIRED is final (it advances before the entry leaves the engine). */
    const uint32_t r = rich(npu, FX1_NPU_M_RETIRED), done = r - npu->retired_mark, want = npu->rich_inflight;
    npu->retired_mark = r;
    npu->rich_inflight = 0;
    if (done != want) return report(npu, FX1_NPU_E_JOB);
    if (want == 0) return FX1_NPU_OK; /* nothing to consume (DONE may be a GEMM-bank job's) */
    set_bank(npu, FX1_NPU_STATUS, FX1_NPU_STATUS_DONE); /* also IRQ_STATUS.DONE */
    if (quiet(npu)) settle(npu);
    return FX1_NPU_OK;
}

fx1_npu_status_t fx1_npu_rich_wait(fx1_npu_t* npu, uint64_t timeout_ticks)
{
    const uint64_t start = npu->now();
    for (;;) {
        const fx1_npu_status_t s = fx1_npu_rich_poll(npu);
        if (s != FX1_NPU_PENDING) return s;
        if (expired(npu, start, timeout_ticks)) return snapshot(npu, FX1_NPU_E_TIMEOUT); /* still running */
    }
}

/* ---- recovery ------------------------------------------------------------------------ */
fx1_npu_status_t fx1_npu_recover(fx1_npu_t* npu, uint64_t timeout_ticks)
{
    if (npu->unavailable) return FX1_NPU_E_UNAVAILABLE;
    OWNER_OR_RETURN(npu);
    if (!(npu->feature & FX1_NPU_FEATURE_ABORT)) return FX1_NPU_E_FEATURE;
    set_bank(npu, FX1_NPU_VP_CMD, FX1_NPU_VP_CMD_ABORT);
    npu->native_config_valid = 0; /* the core may have been soft reset */
    const uint64_t start = npu->now();
    while (!hw_idle(npu)) {
        if (expired(npu, start, timeout_ticks)) {
            npu->unavailable = 1; /* until an init finds the engines idle */
            return snapshot(npu, FX1_NPU_E_UNAVAILABLE);
        }
    }
    /* Every job the driver had accepted is over (lost); the NPU is quiet. */
    npu->gemm_active = 0;
    npu->rich_inflight = 0;
    npu->retired_mark = rich(npu, FX1_NPU_M_RETIRED);
    fx1_npu_read_errors(npu, &npu->last);
    npu->latched = 1;
    settle(npu);
    return FX1_NPU_OK;
}

fx1_npu_status_t fx1_npu_soft_reset(fx1_npu_t* npu, uint64_t timeout_ticks)
{
    if (npu->unavailable) return FX1_NPU_E_UNAVAILABLE;
    OWNER_OR_RETURN(npu);
    if (!quiet(npu)) return FX1_NPU_E_NOT_IDLE;
    const uint32_t irq = bank(npu, FX1_NPU_IRQ_ENABLE);
    set_bank(npu, FX1_NPU_CTRL, FX1_NPU_CTRL_ENABLE | FX1_NPU_CTRL_SOFT_RESET);
    const uint64_t start = npu->now();
    /* Done when the bank is back at its reset value (CTRL 0, window 0). */
    while (bank(npu, FX1_NPU_CTRL) != 0 || bank(npu, FX1_NPU_RICH_WINDOW_SIZE) != 0)
        if (expired(npu, start, timeout_ticks)) return report(npu, FX1_NPU_E_TIMEOUT);
    /* The rich engine leaves reset a few clocks later: wait until a read of the
     * bank shows it quiescent with nothing accepted. */
    while (bank(npu, FX1_NPU_VP_STATUS) != FX1_NPU_VP_STATUS_QUIESCENT || bank(npu, FX1_NPU_VP_ACCEPTED) != 0)
        if (expired(npu, start, timeout_ticks)) return report(npu, FX1_NPU_E_TIMEOUT);
    npu->native_config_valid = 0;
    npu->win_base = npu->win_size = 0;
    npu->retired_mark = rich(npu, FX1_NPU_M_RETIRED);
    npu->latched = 0;
    set_bank(npu, FX1_NPU_IRQ_ENABLE, irq);
    set_bank(npu, FX1_NPU_CTRL, FX1_NPU_CTRL_ENABLE | (irq ? FX1_NPU_CTRL_IRQ_EN : 0u));
    return FX1_NPU_OK;
}
