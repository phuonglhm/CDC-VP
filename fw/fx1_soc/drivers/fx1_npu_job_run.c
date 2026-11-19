/*
 * FX1 NPU job executor (fx1_npu_job.h): runs a checked job image through the
 * reference driver. Single owner (the caller's task); never sleeps itself: the
 * hooks block (RTOS) between polls, so every wait is a poll with a deadline,
 * never a sleep until a DONE interrupt that may not come (review N2, N2-I).
 * The window is read and written with fx1_read32 / fx1_write32 (aligned
 * words), so the host test runs this file against the SystemC clone.
 */
#include "fx1_npu_job.h"

#include "fx1/fx1_memory_map.h"
#include "fx1_fw.h"

typedef struct {
    fx1_npu_t* npu;
    const fx1_npu_job_hooks* hooks;
    fx1_npu_job_result* r;
    uint64_t start, limit; /* job deadline: start + limit ticks */
} run_t;

static void clear_result(fx1_npu_job_result* r)
{
    volatile uint32_t* w = (volatile uint32_t*)r; /* no memset in a freestanding image */
    for (unsigned k = 0; k < sizeof *r / 4u; ++k) w[k] = 0;
}

static void snap(run_t* x, fx1_npu_status_t s)
{
    const fx1_npu_err_t* e = &x->npu->last;
    x->r->driver_status = (uint32_t)s;
    x->r->err[0] = e->status;
    x->r->err[1] = e->last_error;
    x->r->err[2] = e->irq_status;
    x->r->err[3] = e->rich_status;
    x->r->err[4] = e->vp_error;
    x->r->err[5] = e->vp_error_info;
    x->r->err[6] = e->vp_status;
}

static uint32_t job_status_of(fx1_npu_status_t s)
{
    switch (s) {
    case FX1_NPU_E_REFUSED: return FX1_NPU_JOB_REFUSED;
    case FX1_NPU_E_UNAVAILABLE: return FX1_NPU_JOB_UNAVAILABLE;
    case FX1_NPU_E_TIMEOUT: return FX1_NPU_JOB_TIMEOUT;
    default: return FX1_NPU_JOB_FAILED;
    }
}

static int expired(const run_t* x) { return x->npu->now() - x->start >= x->limit; }
static int aborted(const run_t* x) { return x->hooks && x->hooks->abort && x->hooks->abort(x->hooks->ctx); }
static void pause(const run_t* x)
{
    if (x->hooks && x->hooks->wait) x->hooks->wait(x->hooks->ctx);
}

/* Stops what runs (ABORT, engines idle): `status` (TIMEOUT / ABORTED) with the
 * state seen before the recovery, or UNAVAILABLE when the recovery failed. */
static uint32_t stop(run_t* x, uint32_t status)
{
    fx1_npu_read_errors(x->npu, &x->npu->last);
    snap(x, FX1_NPU_OK); /* not a driver failure: the executor stops the job */
    const fx1_npu_status_t s = fx1_npu_recover(x->npu, (uint64_t)x->npu->mtime_hz / 50u); /* 20 ms */
    if (s != FX1_NPU_OK) {
        snap(x, s);
        return FX1_NPU_JOB_UNAVAILABLE;
    }
    return status;
}

/* Polls the GEMM bank (gemm != 0) or the rich side until done: 0, or a job status. */
static uint32_t wait_done(run_t* x, int gemm)
{
    for (;;) {
        const fx1_npu_status_t s = gemm ? fx1_npu_gemm_poll(x->npu) : fx1_npu_rich_poll(x->npu);
        if (s == FX1_NPU_OK) return 0;
        if (s != FX1_NPU_PENDING) {
            snap(x, s);
            return job_status_of(s);
        }
        if (aborted(x)) return stop(x, FX1_NPU_JOB_ABORTED);
        if (expired(x)) return stop(x, FX1_NPU_JOB_TIMEOUT);
        pause(x);
    }
}

/* Before a push or start: 0, or the job's status when it was stopped or its
 * deadline passed (recovered: stop()). */
static uint32_t gate(run_t* x)
{
    if (aborted(x)) return stop(x, FX1_NPU_JOB_ABORTED);
    if (expired(x)) return stop(x, FX1_NPU_JOB_TIMEOUT);
    return 0;
}

/* Until the queue has a credit: 0, or a job status. */
static uint32_t wait_credit(run_t* x)
{
    while (fx1_npu_bank_read(x->npu, FX1_NPU_VP_QUEUE_FREE) == 0) {
        if (aborted(x)) return stop(x, FX1_NPU_JOB_ABORTED);
        if (expired(x)) return stop(x, FX1_NPU_JOB_TIMEOUT);
        pause(x);
    }
    return 0;
}

static void add_perf(run_t* x)
{
    const uint32_t lo = fx1_npu_bank_read(x->npu, FX1_NPU_PERF_MAC_OPS);
    const uint32_t hi = fx1_npu_bank_read(x->npu, FX1_NPU_PERF_MAC_OPS_HI);
    const uint64_t mac = ((uint64_t)x->r->mac_ops_hi << 32 | x->r->mac_ops_lo) + ((uint64_t)hi << 32 | lo);
    x->r->mac_ops_lo = (uint32_t)mac;
    x->r->mac_ops_hi = (uint32_t)(mac >> 32);
    x->r->exec_cycles += fx1_npu_bank_read(x->npu, FX1_NPU_PERF_EXEC_CYCLES);
}

/* Tensors start 4-byte aligned; the bytes after a tensor's last byte up to the
 * next word belong to no tensor and keep their value. */
static void fill(uint32_t addr, uint32_t bytes, uint8_t v)
{
    const uint32_t word = 0x01010101u * v;
    uint32_t k = 0;
    for (; k + 4 <= bytes; k += 4) fx1_write32(addr + k, word);
    if (k < bytes) {
        const uint32_t keep = 0xFFFFFFFFu << (8u * (bytes - k));
        fx1_write32(addr + k, (fx1_read32(addr + k) & keep) | (word & ~keep));
    }
}

static uint32_t chunk[64];
static uint32_t crc_window(uint32_t addr, uint32_t bytes)
{
    uint32_t crc = 0;
    while (bytes) {
        const uint32_t n = bytes < sizeof chunk ? bytes : (uint32_t)sizeof chunk;
        for (uint32_t k = 0; k < (n + 3u) / 4u; ++k) chunk[k] = fx1_read32(addr + 4u * k);
        crc = fx1_npu_job_crc32(crc, chunk, n); /* little-endian: the bytes in memory order */
        addr += n;
        bytes -= n;
    }
    return crc;
}

static void run(run_t* xp, const void* image)
{
    run_t x = *xp;
    fx1_npu_t* npu = x.npu;
    const fx1_npu_job_hooks* hooks = x.hooks;
    fx1_npu_job_result* r = x.r;
    const fx1_npu_job_header* h = (const fx1_npu_job_header*)image;
    const uint32_t bad = fx1_npu_job_check(image, h->total_bytes);
    if (bad) {
        r->status = FX1_NPU_JOB_BAD_JOB;
        r->detail = bad;
        return;
    }
    r->job_crc = h->job_crc;
    const uint32_t need = fx1_npu_job_features(image);
    if ((npu->feature & need) != need) {
        r->status = FX1_NPU_JOB_UNSUPPORTED;
        r->detail = need & ~npu->feature; /* the missing FEATURE bits */
        return;
    }
    if ((h->model_hash && h->model_hash != npu->model_hash) || h->window_size > npu->window_max) {
        r->status = FX1_NPU_JOB_UNSUPPORTED;
        r->detail = h->window_size > npu->window_max ? 0x40000000u : 0x80000000u; /* window / model hash */
        return;
    }
    const uint32_t base = fx1_npu_job_window_base(h);
    const fx1_npu_job_tensor* t = fx1_npu_job_tensors(image);
    const fx1_npu_job_op* op = fx1_npu_job_ops(image);
    fx1_npu_status_t s = fx1_npu_set_window(npu, base, h->window_size);
    if (s != FX1_NPU_OK) {
        snap(&x, s);
        r->status = job_status_of(s);
        return;
    }
    for (uint32_t i = 0; i < h->n_tensors; ++i) {
        if (t[i].role == FX1_NPU_JOB_INPUT || t[i].role == FX1_NPU_JOB_CONST) {
            const uint32_t c = crc_window(base + t[i].offset, t[i].bytes);
            if (c != t[i].crc) {
                r->status = FX1_NPU_JOB_INPUT_CRC;
                r->detail = i;
                r->out_crc[i] = c;
                return;
            }
        } else {
            fill(base + t[i].offset, t[i].bytes, FX1_NPU_JOB_CANARY);
        }
    }

    const uint32_t acc0 = fx1_npu_accepted(npu), ret0 = fx1_npu_retired(npu);
    x.limit = (uint64_t)npu->mtime_hz / 1000u * h->timeout_ms;
    x.start = npu->now();
    static fx1_npu_instr_t in; /* one owner: static keeps them off the task stack */
    static fx1_npu_neo_t neo;
    fx1_npu_gemm_t g;
    uint32_t st = 0;
    /* One operation at a time: each is pushed (or started) once the previous
     * one completed. The model counters cover one hardware batch and reset at
     * the next push into an idle queue, so this is what makes them exact per
     * operation, and a failure names its operation (the queue would run later
     * instructions after a failing one). The queue itself is covered by the
     * N2 firmware tests. */
    for (uint32_t i = 0; i < h->n_ops; ++i) {
        if (hooks && hooks->progress) hooks->progress(hooks->ctx, i); /* may yield */
        fx1_npu_job_build(image, i, base, &g, &in, &neo);
        const uint32_t kind = op[i].kind;
        const int gemm = kind == FX1_NPU_JOB_GEMM_BANK;
        if (!gemm && (st = wait_credit(&x)) != 0) { /* may yield */
            r->op_index = i;
            break;
        }
        /* After the last point that may yield, right before the push / start:
         * nothing more starts once stopped or past the deadline (review N3 R2;
         * the NPU is quiet here, the previous op completed). */
        if ((st = gate(&x)) != 0) {
            r->op_index = i;
            break;
        }
        if (gemm) {
            s = fx1_npu_gemm_start(npu, &g);
        } else if (kind == FX1_NPU_JOB_NEO_MAX_POOL || kind == FX1_NPU_JOB_NEO_RESIDUAL) {
            s = fx1_npu_neo_submit(npu, &neo, (uint64_t)npu->mtime_hz / 1000u);
        } else {
            s = fx1_npu_rich_submit(npu, &in, (uint64_t)npu->mtime_hz / 1000u);
        }
        if (s != FX1_NPU_OK) {
            snap(&x, s);
            st = job_status_of(s);
        } else {
            if (!gemm) {
                if (kind >= FX1_NPU_JOB_NEO_MAX_POOL) ++r->neo_instr;
                else ++r->rich_instr;
            }
            if (hooks && hooks->issued) hooks->issued(hooks->ctx, i);
            st = wait_done(&x, gemm);
        }
        if (st) {
            r->op_index = i;
            break;
        }
        if (gemm) {
            r->gemm_cycles += fx1_npu_bank_read(npu, FX1_NPU_CYCLE_COUNT);
            ++r->gemm_jobs;
        } else {
            add_perf(&x);
        }
        ++r->ops_done;
    }
    r->ticks = (uint32_t)(npu->now() - x.start);
    r->accepted_delta = fx1_npu_accepted(npu) - acc0;
    r->retired_delta = fx1_npu_retired(npu) - ret0;
    r->status = st;
    if (!st) {
        for (uint32_t i = 0; i < h->n_tensors; ++i) {
            if (t[i].role != FX1_NPU_JOB_OUTPUT) continue;
            const uint32_t c = crc_window(base + t[i].offset, t[i].bytes);
            r->out_crc[i] = c;
            ++r->n_outputs;
            if (c == t[i].crc) ++r->outputs_ok;
            else if (r->status == FX1_NPU_JOB_OK) {
                r->status = FX1_NPU_JOB_MISMATCH;
                r->detail = i;
            }
        }
    }
}

void fx1_npu_job_run(fx1_npu_t* npu, const void* image, const fx1_npu_job_hooks* hooks, fx1_npu_job_result* r)
{
    clear_result(r);
    r->version = FX1_NPU_JOB_VERSION;
    r->op_index = 0xFFFFFFFFu;
    r->model_hash = npu->model_hash;
    run_t x = {npu, hooks, r, 0, 0};
    run(&x, image);
    r->magic = FX1_NPU_JOB_RESULT_MAGIC;
}
