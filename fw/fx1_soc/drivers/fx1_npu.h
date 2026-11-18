/*
 * FX1 NPU (SAURIA v4.7 in the FX1 VP) reference bare-metal driver, C11.
 *
 * Register ABI: VP_ABI 1.x (fx1_npu/fx1_npu_regs.h, behaviour in
 * components/FX1_Components/npu/README.md). The driver checks VP_ID, the ABI
 * major and the features it needs before any other access.
 *
 * Scope (plan FX1_NPU_V47 N2): the GEMM bank, lane-A rich instructions run on
 * HasNpuTop, the neo_lite register file (DFC doorbell), ABORT and recovery.
 * The raw native interface (SAURIA configuration and SRAM windows, raw START
 * at native offset 0) is NOT part of the driver: fx1_npu_native_read/write
 * are plain accessors for bring-up only. After an ABORT the driver marks the
 * native configuration invalid (fx1_npu_t.native_config_valid); only a
 * GEMM-bank job programs it again (review N1, N1-I).
 *
 * Ownership: one owner (one hart, later one FreeRTOS task) calls the driver;
 * nothing here is reentrant. Single producer for the rich queue: the driver
 * checks VP_QUEUE_FREE before every push and never relies on the bounded push
 * wait, which is a VP recovery path (a timed-out push is a store access fault).
 *
 * Buffers: every input and output of a submitted job belongs to the NPU until
 * its wait/poll returned FX1_NPU_OK or FX1_NPU_E_JOB, or until
 * fx1_npu_recover returned FX1_NPU_OK (QUIESCENT and every engine idle). A
 * FX1_NPU_E_TIMEOUT of a wait does NOT mean the NPU stopped: the buffers stay
 * the NPU's until a recovery succeeded. The VP has no data cache: no
 * maintenance is needed on this platform.
 *
 * Jobs and their completion (review N2 R1). The driver counts what it got
 * accepted and consumes each completion exactly once:
 *   - a GEMM-bank job runs alone: while it is outstanding (started, not yet
 *     consumed by fx1_npu_gemm_poll/wait) rich submissions and every
 *     configuration call return FX1_NPU_E_NOT_IDLE without any access;
 *   - rich instructions form a batch: everything accepted since the last
 *     consuming fx1_npu_rich_poll/wait; the batch passed when RETIRED
 *     advanced by the number of accepted instructions (an instruction that
 *     fails, is refused by the model or aborted never retires);
 *   - error latches (STATUS.ERROR, IRQ_STATUS, LAST_ERROR, RICH_STATUS,
 *     VP_ERROR) are shared; the driver clears them only when the NPU is
 *     quiet (no outstanding job, every engine idle), so a completion can
 *     never be cleared before its wait saw it. A refused push while a batch
 *     runs is reported at once (FX1_NPU_E_REFUSED, npu->last) and its latches
 *     stay until the batch is consumed.
 *
 * Interrupts: IRQ_STATUS.DONE once per GEMM-bank job, and once per rich batch
 * that ended with no error latched during it. After an ERROR interrupt, or a
 * refused push while the batch ran, the batch ends WITHOUT a DONE interrupt
 * (VP_ABI 1.0 behaviour, N2-I): an interrupt-driven owner then polls
 * fx1_npu_rich_poll. The handler only acknowledges (fx1_npu_irq_ack) and wakes
 * the owner; the verdict is always the poll's. A PLIC claim can find
 * IRQ_STATUS 0 (the level gateway latched a request the driver cleared since,
 * N2-B): handlers must accept it.
 *
 * The instruction builders (fx1_npu_build_*, fx1_npu_neo_*) touch no MMIO and
 * compile on the host as well (fw/fx1_soc host tests use them).
 */
#ifndef FX1_NPU_H
#define FX1_NPU_H

#include <stdint.h>

#include "fx1_npu/fx1_npu_regs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FX1_NPU_OK = 0,
    FX1_NPU_E_ABSENT,      /* VP_ID does not read FXNP */
    FX1_NPU_E_ABI,         /* ABI major is not 1 */
    FX1_NPU_E_FEATURE,     /* a required feature bit is clear */
    FX1_NPU_E_ARG,         /* bad argument (checked before any MMIO); poll without a job */
    FX1_NPU_E_TIMEOUT,     /* the driver's own wait limit expired (see each call) */
    FX1_NPU_E_REFUSED,     /* the NPU refused the push or the write (code in npu->last) */
    FX1_NPU_E_JOB,         /* the job ran and failed (DMA, model, aborted, ...) */
    FX1_NPU_E_UNAVAILABLE, /* closed: a recovery or an init failed; only init reopens */
    FX1_NPU_E_NOT_IDLE,    /* needs a quiet NPU (no outstanding job, engines idle) */
    FX1_NPU_PENDING,       /* poll: the job still runs (not an error, nothing to do) */
} fx1_npu_status_t;

/* Error registers, read one after the other (not an atomic image). */
typedef struct {
    uint32_t status, last_error, irq_status;   /* wrapper bank */
    uint32_t rich_status, vp_error, vp_error_info, vp_status;
} fx1_npu_err_t;

typedef struct {
    uintptr_t base;            /* FX1_NPU_VP_BASE */
    uint32_t feature, abi, model_hash, queue_depth, window_max;
    uint32_t win_base, win_size; /* current RICH window */
    uint32_t mtime_hz;         /* timeout unit: ticks of the time source */
    uint64_t (*now)(void);     /* time source (fx1_mtime on the VP) */
    int native_config_valid;   /* 0 after ABORT/reset until a GEMM-bank job */
    int unavailable;           /* closed: failed recovery or init (init reopens) */
    int gemm_active;           /* a GEMM-bank job is outstanding */
    uint32_t rich_inflight;    /* rich instructions accepted, not consumed yet */
    uint32_t retired_mark;     /* RETIRED when the rich side was last consumed */
    int latched;               /* reported errors still latched in the NPU */
    fx1_npu_err_t last;        /* snapshot of the last failure */
} fx1_npu_t;

/* ---- bring-up ----------------------------------------------------------------- */
/* Opens the context: checks VP_ID, ABI major 1, the `required` FEATURE bits
 * and that every engine is idle (QUIESCENT, nothing queued or running), then
 * enables the bank (CTRL.ENABLE, plus CTRL.IRQ_EN with IRQ_ENABLE = DONE |
 * ERROR when irq != 0) and clears the latches. `now`/`hz` are the time source
 * of every timeout below. On any failure the context is closed
 * (npu->unavailable = 1, every other call returns FX1_NPU_E_UNAVAILABLE) and
 * nothing else of *npu is read or changed: npu->last keeps the diagnostics of
 * an earlier failure. FX1_NPU_E_NOT_IDLE: an engine still runs (for instance
 * the aborted instruction after a failed recovery); call init again later.
 * Init never aborts: it does not prove a stuck engine recovered. */
fx1_npu_status_t fx1_npu_init(fx1_npu_t* npu, uintptr_t base, uint32_t required, int irq, uint64_t (*now)(void),
                              uint32_t hz);
void fx1_npu_read_errors(const fx1_npu_t* npu, fx1_npu_err_t* e);
/* Quiet NPU only (else FX1_NPU_E_NOT_IDLE, nothing written): VP_CMD.CLEAR_ERROR,
 * RICH_STATUS, STATUS / IRQ_STATUS W1C, neo DONE/ISTATUS. For latches set by
 * accesses below the driver; the driver's own calls clear theirs. */
fx1_npu_status_t fx1_npu_clear_errors(fx1_npu_t* npu);
/* IRQ handler helper: IRQ_STATUS, cleared (W1C). The level drops with it. */
uint32_t fx1_npu_irq_ack(fx1_npu_t* npu);
/* Bounded push wait (VP_PUSH_WAIT_NS). */
void fx1_npu_set_push_wait_ns(fx1_npu_t* npu, uint32_t ns);

/* ---- GEMM bank: C[32][32] int32 = A[32][K] int8 x W[K][32] int8 ------------- */
typedef struct {
    uint32_t a, w, c; /* bus addresses in RAM */
    uint32_t k;       /* K >= 1 */
} fx1_npu_gemm_t;
/* Quiet NPU only (FX1_NPU_E_NOT_IDLE while rich work is outstanding). */
fx1_npu_status_t fx1_npu_gemm_start(fx1_npu_t* npu, const fx1_npu_gemm_t* g);
/* Without waiting: FX1_NPU_PENDING still running; FX1_NPU_OK done (DONE),
 * FX1_NPU_E_JOB failed (ERROR without DONE); both consume the job.
 * FX1_NPU_E_ARG: no GEMM-bank job outstanding. */
fx1_npu_status_t fx1_npu_gemm_poll(fx1_npu_t* npu);
/* fx1_npu_gemm_poll until it is not PENDING. FX1_NPU_E_TIMEOUT after
 * timeout_ticks: the job still runs and stays outstanding (npu->last holds
 * the state); before reusing its buffers or starting other work the caller
 * waits again or recovers. */
fx1_npu_status_t fx1_npu_gemm_wait(fx1_npu_t* npu, uint64_t timeout_ticks);

/* ---- instructions (no MMIO) ---------------------------------------------------- */
#define FX1_NPU_INSTR_MAX_WRITES 48
typedef struct {
    uint32_t n;
    uint32_t addr[FX1_NPU_INSTR_MAX_WRITES]; /* model host addresses */
    uint32_t val[FX1_NPU_INSTR_MAX_WRITES];
    uint32_t push;                           /* value of the PUSH_A write */
    uint32_t out, out_bytes;                 /* output region (bus address) */
} fx1_npu_instr_t;

/* GEMM_FUSED convolution [cin][ih][iw] -> [cout][oh][ow] with per-channel
 * requant (S, shift int32 arrays), int32 bias, ReLU or none, optional residual
 * add of an int8 skip tensor; one tile for the whole output. Weights laid out
 * as the model reads them: [cin][kh][kw][cout] (TILE_COUT = cout). */
typedef struct {
    uint32_t in, w, out, bias, scale, shift, skip; /* bus addresses; skip 0 = none */
    uint32_t cin, ih, iw, cout, kh, kw, stride, pad;
    uint32_t act;   /* FX1_NPU_ACT_NONE / RELU */
    int32_t zp_out;
} fx1_npu_conv_t;
uint32_t fx1_npu_conv_oh(const fx1_npu_conv_t* c);
uint32_t fx1_npu_conv_ow(const fx1_npu_conv_t* c);
int fx1_npu_build_conv(fx1_npu_instr_t* i, const fx1_npu_conv_t* c);

/* MAX_POOL (pad = -128 padding) or AVG_POOL (pad 0, out = clamp8(round(sum*so >> sho))). */
typedef struct {
    uint32_t in, out, c, h, w, k, pad, stride;
    int avg;
    int32_t so, sho; /* AVG_POOL only */
} fx1_npu_pool_t;
uint32_t fx1_npu_pool_oh(const fx1_npu_pool_t* p);
uint32_t fx1_npu_pool_ow(const fx1_npu_pool_t* p);
int fx1_npu_build_pool(fx1_npu_instr_t* i, const fx1_npu_pool_t* p);

/* ADD with integer parameters: out = requant(dequant(A) + dequant(B)). */
typedef struct {
    uint32_t a, b, out, n;
    int32_t zp_a, zp_b, zp_o, sa, sha, sb, shb, so, sho;
} fx1_npu_add_t;
int fx1_npu_build_add(fx1_npu_instr_t* i, const fx1_npu_add_t* a);

/* Any lane-A opcode with explicit writes (tests of refusals). */
int fx1_npu_build_raw(fx1_npu_instr_t* i, uint32_t opcode, const uint32_t* addr, const uint32_t* val, uint32_t n);

/* ---- neo_lite register file (DFC), strict neo_lite RTL revision 1 ----------- */
typedef struct {
    uint32_t word[FX1_NPU_NEO_REG_COUNT];
    uint32_t out, out_bytes;
} fx1_npu_neo_t;
/* MAX_POOL (pad must be 0: the hardware has no pooling padding) or AVG_POOL
 * (encoded, but the strict hardware refuses it: A-H3). 0 when not encodable. */
int fx1_npu_neo_pool(fx1_npu_neo_t* n, const fx1_npu_pool_t* p);
/* ADD as RESIDUAL; needs sha = shb = 0 (or exactly reducible), n a multiple
 * of 32, non-negative scales. 0 when not expressible on the hardware. */
int fx1_npu_neo_residual(fx1_npu_neo_t* n, const fx1_npu_add_t* a);

/* ---- rich submission ----------------------------------------------------------------- */
/* RICH_WINDOW_BASE/SIZE; every address of a later instruction must be inside.
 * Quiet NPU only: FX1_NPU_E_NOT_IDLE (nothing written, cache unchanged) while
 * any job is outstanding or an engine runs (the bank is locked then). The
 * cached window is what the registers read back. */
fx1_npu_status_t fx1_npu_set_window(fx1_npu_t* npu, uint32_t base, uint32_t size);
/* Waits (timeout) for a queue credit, writes the instruction and pushes. OK
 * only when VP_ACCEPTED advanced: a refusal is a store that did not fault,
 * so the driver reads the refusal back (FX1_NPU_E_REFUSED, npu->last; the
 * batch already accepted is not affected and is still consumed by its wait).
 * FX1_NPU_E_TIMEOUT: no credit in time; the instruction was not written or
 * accepted, earlier instructions still run (wait for them, or recover the
 * whole batch). FX1_NPU_E_NOT_IDLE: a GEMM-bank job is outstanding. */
fx1_npu_status_t fx1_npu_rich_submit(fx1_npu_t* npu, const fx1_npu_instr_t* i, uint64_t timeout_ticks);
/* The 82 words, IEN, START, then the doorbell (push 0x17). As rich_submit. */
fx1_npu_status_t fx1_npu_neo_submit(fx1_npu_t* npu, const fx1_npu_neo_t* n, uint64_t timeout_ticks);
/* Without waiting: FX1_NPU_PENDING while the rich side is busy; once idle it
 * consumes the batch: FX1_NPU_OK when every accepted instruction retired,
 * else FX1_NPU_E_JOB (npu->last). OK at once when nothing was accepted. */
fx1_npu_status_t fx1_npu_rich_poll(fx1_npu_t* npu);
/* fx1_npu_rich_poll until it is not PENDING. FX1_NPU_E_TIMEOUT: the batch
 * still runs and stays outstanding (as fx1_npu_gemm_wait). */
fx1_npu_status_t fx1_npu_rich_wait(fx1_npu_t* npu, uint64_t timeout_ticks);
uint32_t fx1_npu_retired(const fx1_npu_t* npu);
uint32_t fx1_npu_accepted(const fx1_npu_t* npu);

/* ---- recovery --------------------------------------------------------------------- */
/* ABORT, then wait for QUIESCENT and every engine idle (QUIESCENT alone is
 * buffer safety, not recovery), record (npu->last) and clear the errors, mark
 * the native configuration invalid. Every outstanding job is lost: its output
 * is undefined, its buffers are the caller's again. FX1_NPU_E_UNAVAILABLE when
 * the engines do not go idle in time: the context is closed; a later init
 * reopens it once the engines went idle by themselves. */
fx1_npu_status_t fx1_npu_recover(fx1_npu_t* npu, uint64_t timeout_ticks);
/* CTRL.SOFT_RESET, quiet NPU only: every register back to reset, then the
 * bank enabled again with the same interrupt enables. */
fx1_npu_status_t fx1_npu_soft_reset(fx1_npu_t* npu, uint64_t timeout_ticks);

/* ---- low level (bring-up only, outside the driver contract) ---------------------- */
uint32_t fx1_npu_native_read(const fx1_npu_t* npu, uint32_t offset);
void fx1_npu_native_write(const fx1_npu_t* npu, uint32_t offset, uint32_t value);
uint32_t fx1_npu_bank_read(const fx1_npu_t* npu, uint32_t local);

const char* fx1_npu_status_name(fx1_npu_status_t s);

#ifdef __cplusplus
}
#endif

#endif /* FX1_NPU_H */
