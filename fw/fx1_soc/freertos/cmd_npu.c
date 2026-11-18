/*
 * `npu` commands (plan FX1_NPU_V47 N3) on the reference driver
 * (drivers/fx1_npu.h) and the job format (drivers/fx1_npu_job.h).
 *
 * Ownership: the NPU task (priority 1, below the CLI task, so the console
 * answers while it checks a job) is the only caller of the driver (it is not
 * reentrant). Commands post one request (run, selftest, reset) and the task
 * runs it; `npu run` returns at once, the others wait for it with a limit.
 * `npu status` and `npu info` read the task's published state and NPU
 * registers that have no read side effect, so they answer while a job runs.
 * Only the CLI task prints (the NPU task records, the command prints).
 *
 * Interrupt (PLIC source 4): the handler acknowledges IRQ_STATUS and notifies
 * the task; nothing else. The task polls the driver with a deadline and
 * blocks at most one tick between polls (woken early by the interrupt): a
 * batch with an error, or a refused push, ends without a DONE interrupt
 * (review N2, N2-I), so nothing ever sleeps until DONE. A claim that finds
 * IRQ_STATUS 0 (the PLIC latched a request the driver cleared since, N2-B) is
 * counted as stale.
 *
 * Presence: probed by the task (fault-tolerant read of VP_ID). Without an NPU
 * (fx1_soc --no-npu, or a platform built without it) every command says so
 * and nothing counts as a failure.
 *
 *   npu info                 identity, features, arenas, job contract
 *   npu status               task state, the running / last job, NPU registers
 *   npu selftest             GEMM bank K=32, the rich chain R1..R5 and the DFC
 *                            cases D1/D2 against the C oracle (small arena from
 *                            +3 MiB); a check
 *   npu run                  the job the host loaded at FX1_NPU_JOB_BASE (copied,
 *                            checked, run; result block at FX1_NPU_RESULT_BASE);
 *                            returns at once
 *   npu wait [MS]            until the job ended (MS: limit, then the job keeps
 *                            running), then its summary; a check (OK passes)
 *   npu reset                idle: recovery + soft reset (or a reopen after
 *                            UNAVAILABLE); while a job runs: the job is stopped
 *                            first (ABORTED, recovered); a check
 */
#include <string.h>

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include "fx1_fw.h"
#include "fx1_npu.h"
#include "fx1_npu_job.h"
#include "npu_cases.h"
#include "npu_oracle.h"
#include "rtos_app.h"
#include "rtos_bsp.h"

enum { ST_PROBING = 0, ST_ABSENT, ST_IDLE, ST_RUNNING, ST_CLOSED };
enum { REQ_NONE = 0, REQ_RUN, REQ_SELFTEST, REQ_RESET };
static const char* const state_names[] = {"probing", "absent", "idle", "running", "closed (UNAVAILABLE)"};

static fx1_npu_t npu;
static TaskHandle_t npu_task;
static SemaphoreHandle_t req_done;
static volatile int state, req, abort_req, reset_pending;
static volatile uint32_t cur_op, irq_count, stale_count;
static volatile uint64_t run_t0;
static fx1_npu_status_t open_status;

static fx1_npu_job_result last;
static volatile int have_last;
static char last_name[32];
static uint32_t last_ops;
static uint32_t job_copy[FX1_NPU_JOB_SIZE / 4]; /* the image, copied before it is checked */

static struct {
    int ran, gemm, rich, dfc, dfc_skipped;
    uint32_t gemm_bad, rich_bad, dfc_bad;
    fx1_npu_status_t first_err;
} st_res;
static struct {
    int ran, stopped_job;
    fx1_npu_status_t recover, soft, open;
    uint32_t vp_status;
} rs_res;

/* ---- interrupt ------------------------------------------------------------------- */
static BaseType_t npu_isr(void)
{
    BaseType_t woken = pdFALSE;
    if (fx1_npu_irq_ack(&npu)) ++irq_count;
    else ++stale_count;
    if (npu_task) vTaskNotifyGiveFromISR(npu_task, &woken);
    return woken;
}

/* ---- the NPU task ------------------------------------------------------------------ */
static void hook_wait(void* c)
{
    (void)c;
    ulTaskNotifyTake(pdTRUE, 1); /* one tick, or the interrupt */
}
static int hook_abort(void* c)
{
    (void)c;
    return abort_req;
}
static void hook_progress(void* c, uint32_t i)
{
    (void)c;
    cur_op = i;
}
static const fx1_npu_job_hooks hooks = {0, hook_wait, hook_abort, hook_progress};

static uint64_t ms_ticks(uint32_t ms) { return (uint64_t)FX1_MTIME_HZ / 1000u * ms; }

static void publish(const fx1_npu_job_result* r)
{
    volatile uint32_t* out = (volatile uint32_t*)FX1_NPU_RESULT_BASE;
    const uint32_t* w = (const uint32_t*)r;
    for (unsigned k = 1; k < sizeof *r / 4u; ++k) out[k] = w[k];
    fx1_release(); /* the block before its magic */
    out[0] = w[0];
}

static void do_run(void)
{
    volatile fx1_npu_job_result* out = (volatile fx1_npu_job_result*)FX1_NPU_RESULT_BASE;
    out->magic = 0;
    const volatile uint32_t* src = (const volatile uint32_t*)FX1_NPU_JOB_BASE;
    uint32_t len = src[3]; /* total_bytes */
    if (len < sizeof(fx1_npu_job_header) || len > FX1_NPU_JOB_SIZE) len = sizeof(fx1_npu_job_header);
    for (uint32_t k = 0; k < len / 4u; ++k) job_copy[k] = src[k];
    const fx1_npu_job_header* h = (const fx1_npu_job_header*)job_copy;
    memcpy(last_name, h->name, sizeof last_name);
    last_name[sizeof last_name - 1] = 0;
    last_ops = h->n_ops <= FX1_NPU_JOB_MAX_OPS ? h->n_ops : 0;
    const uint32_t i0 = irq_count, s0 = stale_count;
    cur_op = 0;
    run_t0 = fx1_mtime();
    fx1_npu_job_run(&npu, job_copy, &hooks, &last);
    last.irqs = irq_count - i0;
    last.stale_irqs = stale_count - s0;
    publish(&last);
    have_last = 1;
}

/* Selftest: the GEMM bank, the rich chain, the DFC cases, each against the C oracle. */
#define ST_BASE (FX1_NPU_ARENA_SMALL_BASE + 0x00300000u)
#define ST_WIN NPU_CASE_WINDOW
#define ST_GA (ST_BASE + 0x10000u)
#define ST_GW (ST_BASE + 0x11000u)
#define ST_GC (ST_BASE + 0x12000u)
static npu_cases_t cases;
static uint8_t st_expect[NPU_CASE_WINDOW] __attribute__((aligned(8)));
static int8_t st_a[32 * 32], st_w[32 * 32];
static int32_t st_c[32 * 32];

static uint32_t diff(const volatile uint8_t* a, const uint8_t* b, uint32_t n)
{
    uint32_t bad = 0;
    for (uint32_t i = 0; i < n; ++i) bad += a[i] != b[i];
    return bad;
}

static fx1_npu_status_t wait_job(int gemm)
{
    const uint64_t t0 = fx1_mtime();
    for (;;) {
        const fx1_npu_status_t s = gemm ? fx1_npu_gemm_poll(&npu) : fx1_npu_rich_poll(&npu);
        if (s != FX1_NPU_PENDING) return s;
        if (fx1_mtime() - t0 > ms_ticks(50)) {
            (void)fx1_npu_recover(&npu, ms_ticks(20));
            return FX1_NPU_E_TIMEOUT;
        }
        hook_wait(0);
    }
}

static void note(fx1_npu_status_t s)
{
    if (s != FX1_NPU_OK && st_res.first_err == FX1_NPU_OK) st_res.first_err = s;
}

static void do_selftest(void)
{
    st_res.ran = 1;
    st_res.gemm = st_res.rich = st_res.dfc = st_res.dfc_skipped = 0;
    st_res.gemm_bad = st_res.rich_bad = st_res.dfc_bad = 0;
    st_res.first_err = FX1_NPU_OK;
    /* GEMM bank K=32. */
    uint32_t s = 0x1234567u;
    volatile int8_t* a = (volatile int8_t*)ST_GA;
    volatile int8_t* w = (volatile int8_t*)ST_GW;
    for (unsigned i = 0; i < 32 * 32; ++i) {
        s = s * 1103515245u + 12345u;
        a[i] = st_a[i] = (int8_t)(s >> 16);
        s = s * 1103515245u + 12345u;
        w[i] = st_w[i] = (int8_t)(s >> 16);
    }
    npu_oracle_gemm_bank(st_a, st_w, 32, st_c);
    const fx1_npu_gemm_t g = {.a = ST_GA, .w = ST_GW, .c = ST_GC, .k = 32};
    fx1_npu_status_t r = fx1_npu_gemm_start(&npu, &g);
    if (r == FX1_NPU_OK) r = wait_job(1);
    note(r);
    if (r == FX1_NPU_OK) {
        const volatile int32_t* c = (const volatile int32_t*)ST_GC;
        for (unsigned i = 0; i < 32 * 32; ++i) st_res.gemm_bad += c[i] != st_c[i];
        st_res.gemm = st_res.gemm_bad == 0;
    }
    /* Rich chain R1..R5 (lane A, HasNpuTop). */
    npu_cases_build(&cases, ST_BASE);
    npu_cases_fill((uint8_t*)ST_BASE);
    npu_cases_fill(st_expect);
    npu_cases_apply_rich(&cases, st_expect, ST_BASE);
    r = fx1_npu_set_window(&npu, ST_BASE, ST_WIN);
    for (unsigned k = 0; r == FX1_NPU_OK && k < NPU_RICH_COUNT; ++k) r = fx1_npu_rich_submit(&npu, &cases.instr[k], ms_ticks(10));
    if (r == FX1_NPU_OK) r = wait_job(0);
    else (void)wait_job(0); /* the accepted part, so the NPU is quiet again */
    note(r);
    if (r == FX1_NPU_OK) {
        st_res.rich_bad = diff((const volatile uint8_t*)ST_BASE, st_expect, ST_WIN);
        st_res.rich = st_res.rich_bad == 0;
    }
    /* DFC D1 (MAX_POOL) + D2 (RESIDUAL) through the neo_lite register file. */
    if (!(npu.feature & FX1_NPU_FEATURE_DFC_WINDOW)) {
        st_res.dfc_skipped = 1;
        return;
    }
    npu_cases_fill((uint8_t*)ST_BASE);
    npu_cases_fill(st_expect);
    npu_cases_apply_dfc(&cases, st_expect, ST_BASE);
    r = fx1_npu_neo_submit(&npu, &cases.neo[NPU_D1], ms_ticks(10));
    if (r == FX1_NPU_OK) r = fx1_npu_neo_submit(&npu, &cases.neo[NPU_D2], ms_ticks(10));
    if (r == FX1_NPU_OK) r = wait_job(0);
    else (void)wait_job(0);
    note(r);
    if (r == FX1_NPU_OK) {
        st_res.dfc_bad = diff((const volatile uint8_t*)ST_BASE, st_expect, ST_WIN);
        st_res.dfc = st_res.dfc_bad == 0;
    }
}

static void do_reset(void)
{
    rs_res.ran = 1;
    rs_res.recover = rs_res.soft = rs_res.open = FX1_NPU_OK;
    if (npu.unavailable) {
        rs_res.open = fx1_npu_init(&npu, FX1_NPU_VP_BASE, 0, 1, fx1_mtime, FX1_MTIME_HZ);
    } else {
        rs_res.recover = fx1_npu_recover(&npu, ms_ticks(20));
        if (rs_res.recover == FX1_NPU_OK) rs_res.soft = fx1_npu_soft_reset(&npu, ms_ticks(5));
        if (rs_res.recover == FX1_NPU_OK && rs_res.soft == FX1_NPU_OK)
            rs_res.open = fx1_npu_init(&npu, FX1_NPU_VP_BASE, 0, 1, fx1_mtime, FX1_MTIME_HZ);
    }
    rs_res.vp_status = fx1_npu_bank_read(&npu, FX1_NPU_VP_STATUS);
}

static void npu_task_fn(void* arg)
{
    (void)arg;
    uint32_t id = 0;
    if (fx1_probe_read32(FX1_NPU_VP_BASE + FX1_NPU_BANK(FX1_NPU_VP_ID), &id) || id != FX1_NPU_VP_ID_VALUE) {
        state = ST_ABSENT;
        vTaskDelete(NULL);
    }
    open_status = fx1_npu_init(&npu, FX1_NPU_VP_BASE, 0, 1, fx1_mtime, FX1_MTIME_HZ);
    bsp_irq_register(FX1_IRQ_NPU, npu_isr);
    state = open_status == FX1_NPU_OK ? ST_IDLE : ST_CLOSED;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        const int r = req;
        if (r == REQ_NONE) continue; /* an interrupt between jobs */
        if (r == REQ_RUN) do_run();
        else if (r == REQ_SELFTEST) do_selftest();
        if (r == REQ_RESET || reset_pending) {
            rs_res.stopped_job = r == REQ_RUN && reset_pending;
            do_reset();
        }
        taskENTER_CRITICAL();
        abort_req = reset_pending = 0;
        req = REQ_NONE;
        state = npu.unavailable ? ST_CLOSED : ST_IDLE;
        taskEXIT_CRITICAL();
        xSemaphoreGive(req_done);
    }
}

void npu_service_init(void)
{
    req_done = xSemaphoreCreateBinary();
    configASSERT(req_done);
    /* Priority 1, below the CLI (2): the job checks and CRCs run on the CPU
     * for a while, and the console must keep answering meanwhile. */
    configASSERT(xTaskCreate(npu_task_fn, "npu", 1024, NULL, 1, &npu_task) == pdPASS);
}

/* ---- commands ---------------------------------------------------------------------- */
static int post(int r)
{
    taskENTER_CRITICAL();
    const int ok = req == REQ_NONE && (state == ST_IDLE || state == ST_CLOSED);
    if (ok) {
        xSemaphoreTake(req_done, 0);
        req = r;
        if (r == REQ_RUN || r == REQ_SELFTEST) state = ST_RUNNING;
    }
    taskEXIT_CRITICAL();
    if (ok) xTaskNotifyGive(npu_task);
    return ok;
}

static int done_within(uint32_t ms) { return xSemaphoreTake(req_done, pdMS_TO_TICKS(ms)) == pdTRUE; }

static int absent(const char* what)
{
    /* The task (lower priority) probes once the CLI first blocks; wait for it. */
    for (unsigned k = 0; state == ST_PROBING && k < 100; ++k) vTaskDelay(1);
    if (state != ST_ABSENT && state != ST_PROBING) return 0;
    out(what);
    out_line(": no NPU (fx1_soc --no-npu, or a platform built without it): unsupported");
    return 1;
}

static void out_hex_named(const char* name, uint32_t v)
{
    out(name);
    out_hex(v);
}

static void out_err(const uint32_t* e)
{
    out_hex_named("  STATUS ", e[0]);
    out_hex_named(" LAST_ERROR ", e[1]);
    out_hex_named(" IRQ_STATUS ", e[2]);
    out_line("");
    out_hex_named("  RICH_STATUS ", e[3]);
    out_hex_named(" VP_ERROR ", e[4]);
    out_hex_named(" VP_ERROR_INFO ", e[5]);
    out_hex_named(" VP_STATUS ", e[6]);
    out_line("");
}

static uint32_t ticks_us(uint64_t t) { return (uint32_t)(t / (FX1_MTIME_HZ / 1000000u)); }

static void summary(void)
{
    const fx1_npu_job_result* r = &last;
    out("npu run: ");
    out(last_name);
    out(": ");
    out(fx1_npu_job_status_name(r->status));
    out(", ");
    out_dec(r->ops_done);
    out("/");
    out_dec(last_ops);
    out(" ops (rich ");
    out_dec(r->rich_instr);
    out(", neo ");
    out_dec(r->neo_instr);
    out(", GEMM bank ");
    out_dec(r->gemm_jobs);
    out("), accepted +");
    out_dec(r->accepted_delta);
    out(" retired +");
    out_dec(r->retired_delta);
    out(", outputs ");
    out_dec(r->outputs_ok);
    out("/");
    out_dec(r->n_outputs);
    out_line(" match");
    out("npu run: model exec cycles ");
    out_dec(r->exec_cycles);
    out(", MAC ops ");
    out_dec(r->mac_ops_lo);
    out(", GEMM-bank cycles ");
    out_dec(r->gemm_cycles);
    out(", interrupts ");
    out_dec(r->irqs);
    out(" (stale ");
    out_dec(r->stale_irqs);
    out("), simulated ");
    out_dec(ticks_us(r->ticks));
    out_line(" us (NPU model time, not FX1 end-to-end throughput)");
    switch (r->status) {
    case FX1_NPU_JOB_OK: break;
    case FX1_NPU_JOB_BAD_JOB:
        out("npu run: invalid job: ");
        out(fx1_npu_job_reason_name(r->detail >> 16));
        out(", index ");
        out_dec(r->detail & 0xFFFFu);
        out_line(" (nothing was sent to the NPU)");
        break;
    case FX1_NPU_JOB_INPUT_CRC:
        out("npu run: input tensor ");
        out_dec(r->detail);
        out(" as loaded has CRC-32 ");
        out_hex(r->out_crc[r->detail & 31u]);
        out_line(", not the job's (NPU not started)");
        break;
    case FX1_NPU_JOB_UNSUPPORTED:
        out("npu run: unsupported here: ");
        out_line(r->detail == 0x80000000u ? "model hash differs" :
                 r->detail == 0x40000000u ? "window larger than VP_WINDOW_MAX" : "missing FEATURE bits");
        out_hex_named("  detail ", r->detail);
        out_line("");
        break;
    case FX1_NPU_JOB_MISMATCH:
        out("npu run: tensor ");
        out_dec(r->detail);
        out(" CRC-32 ");
        out_hex(r->out_crc[r->detail & 31u]);
        out_line(" differs from the job's");
        break;
    default:
        out("npu run: stopped at op ");
        out_dec(r->op_index);
        out(", driver ");
        out(fx1_npu_status_name((fx1_npu_status_t)r->driver_status));
        out_line("");
        out_err(r->err);
        break;
    }
}

static int job_present(void)
{
    const volatile fx1_npu_job_header* h = (const volatile fx1_npu_job_header*)FX1_NPU_JOB_BASE;
    return h->magic == FX1_NPU_JOB_MAGIC;
}

static void cmd_info(void)
{
    if (absent("npu info")) return;
    out("npu: SAURIA v4.7 (VP aperture ");
    out_hex(FX1_NPU_VP_BASE);
    out(", IRQ ");
    out_dec(FX1_IRQ_NPU);
    out("), VP_ID ");
    out_hex(fx1_npu_bank_read(&npu, FX1_NPU_VP_ID));
    out(" ABI ");
    out_hex(npu.abi);
    out(" model hash ");
    out_hex(npu.model_hash);
    out_line("");
    out("npu: features ");
    out_hex(npu.feature);
    static const char* const fnames[] = {"GEMM-bank", "rich", "DFC", "DMA_TENSOR", "ABORT", "bounded-push", "queue", "attn/LN"};
    for (unsigned b = 0; b < 8; ++b)
        if (npu.feature & (1u << b)) {
            out(" ");
            out(fnames[b]);
        }
    out(", queue ");
    out_dec(npu.queue_depth);
    out(", window max ");
    out_dec(npu.window_max >> 20);
    out_line(" MiB");
    out("npu: arenas small ");
    out_hex(FX1_NPU_ARENA_SMALL_BASE);
    out(" + 4 MiB (selftest from +3 MiB), large ");
    out_hex(FX1_NPU_ARENA_LARGE_BASE);
    out_line(" + 32 MiB");
    out("npu: job format ");
    out_dec(FX1_NPU_JOB_VERSION);
    out(", image at ");
    out_hex(FX1_NPU_JOB_BASE);
    out(" (64 KiB), result at ");
    out_hex(FX1_NPU_RESULT_BASE);
    out(job_present() ? ", a job image is loaded" : ", no job image loaded");
    out_line("");
    out("npu: driver context ");
    out_line(state == ST_CLOSED ? "closed (UNAVAILABLE: `npu reset` reopens once the NPU is idle)" : "open");
}

static void cmd_status(void)
{
    if (absent("npu status")) return;
    out("npu: ");
    out(state_names[state]);
    if (state == ST_RUNNING && req == REQ_RUN) {
        const volatile fx1_npu_job_header* h = (const volatile fx1_npu_job_header*)job_copy;
        out(", job ");
        out(last_name);
        out(" op ");
        out_dec(cur_op);
        out("/");
        out_dec(h->n_ops);
        out(", ");
        out_dec(ticks_us(fx1_mtime() - run_t0) / 1000u);
        out(" ms");
        if (abort_req) out(", stop requested");
    }
    out_line("");
    out_hex_named("npu: VP_STATUS ", fx1_npu_bank_read(&npu, FX1_NPU_VP_STATUS));
    out_hex_named(" ACCEPTED ", fx1_npu_accepted(&npu));
    out_hex_named(" RETIRED ", fx1_npu_retired(&npu));
    out_hex_named(" QUEUE_FREE ", fx1_npu_bank_read(&npu, FX1_NPU_VP_QUEUE_FREE));
    out(", interrupts ");
    out_dec(irq_count);
    out(" (stale ");
    out_dec(stale_count);
    out_line(")");
    if (have_last && state != ST_RUNNING) {
        out("npu: last job ");
        out(last_name);
        out(": ");
        out_line(fx1_npu_job_status_name(last.status));
    }
}

static void cmd_run(void)
{
    if (absent("npu run")) return;
    if (!job_present()) {
        out("npu run: no job image at ");
        out_hex(FX1_NPU_JOB_BASE);
        out_line(" (load one with platforms/fx1_soc/demo/npu_run.py or fx1_soc --load)");
        return;
    }
    if (!post(REQ_RUN)) {
        out("npu run: busy (");
        out(state_names[state]);
        out_line("): `npu wait` or `npu reset` first");
        return;
    }
    out_line("npu run: started; `npu status`, `npu wait [MS]`, `npu reset`");
}

static void cmd_wait(int argc, char** argv)
{
    if (absent("npu wait")) return;
    uint32_t ms = 0;
    if (argc > 2 && !parse_u32(argv[2], &ms)) ms = 0;
    const TickType_t t0 = xTaskGetTickCount();
    while (state == ST_RUNNING) {
        if (ms && xTaskGetTickCount() - t0 >= pdMS_TO_TICKS(ms)) {
            out("npu wait: still running after ");
            out_dec(ms);
            out(" ms (op ");
            out_dec(cur_op);
            out_line("); the job goes on");
            return;
        }
        vTaskDelay(1);
    }
    if (!have_last) {
        out_line("npu wait: no job has run");
        return;
    }
    summary();
    report("npu run", last.status == FX1_NPU_JOB_OK);
}

static void cmd_selftest_npu(void)
{
    if (absent("npu selftest")) return;
    if (!post(REQ_SELFTEST)) {
        out("npu selftest: busy (");
        out(state_names[state]);
        out_line(")");
        report("npu selftest", 0);
        return;
    }
    if (!done_within(5000)) {
        out_line("npu selftest: no answer from the NPU task in 5 s");
        report("npu selftest", 0);
        return;
    }
    out("npu selftest: GEMM bank K=32, INT32 outputs differing from the oracle ");
    out_dec(st_res.gemm_bad);
    out_line(st_res.gemm ? " of 1024" : " of 1024 (or not run)");
    out("npu selftest: rich chain R1..R5, window bytes differing ");
    out_dec(st_res.rich_bad);
    out_line(st_res.rich ? " of 8192" : " of 8192 (or not run)");
    if (st_res.dfc_skipped) out_line("npu selftest: DFC: no DFC_WINDOW feature, skipped");
    else {
        out("npu selftest: DFC D1 MAX_POOL + D2 RESIDUAL, window bytes differing ");
        out_dec(st_res.dfc_bad);
        out_line(st_res.dfc ? " of 8192" : " of 8192 (or not run)");
    }
    if (st_res.first_err != FX1_NPU_OK) {
        out("npu selftest: first driver error ");
        out_line(fx1_npu_status_name(st_res.first_err));
    }
    report("npu selftest", st_res.gemm && st_res.rich && (st_res.dfc || st_res.dfc_skipped));
}

static void cmd_reset(void)
{
    if (absent("npu reset")) return;
    /* One decision in one critical section: post a reset to an idle task, or
     * stop the request it runs (a job ending meanwhile cannot slip between). */
    int stopping = 0, posted = 0;
    taskENTER_CRITICAL();
    if (req == REQ_NONE && (state == ST_IDLE || state == ST_CLOSED)) {
        xSemaphoreTake(req_done, 0);
        req = REQ_RESET;
        posted = 1;
    } else if (state == ST_RUNNING) {
        reset_pending = 1;
        abort_req = 1;
        stopping = 1;
    }
    taskEXIT_CRITICAL();
    if (!posted && !stopping) {
        out_line("npu reset: busy");
        report("npu reset", 0);
        return;
    }
    xTaskNotifyGive(npu_task);
    if (!done_within(2000)) {
        out("npu reset: no answer from the NPU task in 2 s (");
        out(state_names[state]);
        out_line(")");
        report("npu reset", 0);
        return;
    }
    if (stopping && rs_res.stopped_job) {
        out("npu reset: job ");
        out(last_name);
        out(" stopped: ");
        out(fx1_npu_job_status_name(last.status));
        out(" at op ");
        out_dec(last.op_index);
        out_line("");
    }
    out("npu reset: recovery ");
    out(fx1_npu_status_name(rs_res.recover));
    out(", soft reset ");
    out(fx1_npu_status_name(rs_res.soft));
    out(", open ");
    out(fx1_npu_status_name(rs_res.open));
    out_hex_named(", VP_STATUS ", rs_res.vp_status);
    out_line("");
    report("npu reset", rs_res.recover == FX1_NPU_OK && rs_res.soft == FX1_NPU_OK && rs_res.open == FX1_NPU_OK &&
                            state == ST_IDLE);
}

int cmd_npu(int argc, char** argv)
{
    if (argc < 2) return 1;
    if (!strcmp(argv[1], "info")) cmd_info();
    else if (!strcmp(argv[1], "status")) cmd_status();
    else if (!strcmp(argv[1], "selftest")) cmd_selftest_npu();
    else if (!strcmp(argv[1], "run")) cmd_run();
    else if (!strcmp(argv[1], "wait")) cmd_wait(argc, argv);
    else if (!strcmp(argv[1], "reset")) cmd_reset();
    else return 1;
    return 0;
}

int npu_present(void) { return state != ST_ABSENT && state != ST_PROBING; }
