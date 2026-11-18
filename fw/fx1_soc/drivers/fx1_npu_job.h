/*
 * FX1 NPU job format, version 1 (plan FX1_NPU_V47 N3), C11, little-endian.
 *
 * A job is a list of operations on tensors, all inside one RICH window of an
 * NPU arena. The host never sends register writes: the firmware builds every
 * NPU instruction itself with the reference driver's builders (fx1_npu.h) from
 * the operation and the tensors it names, after checking them. Relocation is
 * the window base: a tensor is (offset, bytes) inside the window.
 *
 *   FX1_NPU_JOB_BASE     job image: header, tensor table, op table (no payload)
 *   window               arena base + window_offset, window_size bytes; the
 *                        host loads the INPUT and CONST tensors there
 *   FX1_NPU_RESULT_BASE  result block (firmware -> host), magic written last
 *
 * Checks before the NPU is touched (fx1_npu_job_check): the header (magic,
 * version, sizes, CRC-32 of the image, ABI, arena, window), every tensor
 * (role, dtype, layout, dims x element size = bytes, 4-byte aligned, inside
 * the window, no two tensors overlap) and every operation (kind, tensors of
 * the right role / dtype / shape for its geometry, every source defined
 * before use, every OUTPUT / SCRATCH written by exactly one operation, the
 * instruction buildable). Then (fx1_npu_job_run): the CRC-32 of every INPUT /
 * CONST tensor as loaded, OUTPUT / SCRATCH tensors filled with a canary, the
 * operations in order, one at a time (each completes before the next is
 * pushed: the model counters, which cover one hardware batch, are exact per
 * operation, and a failure names its operation), every OUTPUT's CRC-32
 * compared.
 *
 * CRC-32: the IEEE 802.3 / zlib polynomial (Python's zlib.crc32).
 */
#ifndef FX1_NPU_JOB_H
#define FX1_NPU_JOB_H

#include <stdint.h>

#include "fx1_npu.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FX1_NPU_JOB_MAGIC       0x4A4E5846u /* "FXNJ" */
#define FX1_NPU_JOB_RESULT_MAGIC 0x524E5846u /* "FXNR" */
#define FX1_NPU_JOB_VERSION     1u
#define FX1_NPU_JOB_MAX_TENSORS 32u
#define FX1_NPU_JOB_MAX_OPS     32u
#define FX1_NPU_JOB_NONE        0xFFu  /* no tensor in an op slot */
#define FX1_NPU_JOB_TIMEOUT_MAX 60000u /* ms of simulated time */
#define FX1_NPU_JOB_CANARY      0xA5u  /* OUTPUT / SCRATCH bytes before the job runs */

enum { FX1_NPU_JOB_ARENA_SMALL = 0, FX1_NPU_JOB_ARENA_LARGE = 1 };

enum { /* tensor role */
    FX1_NPU_JOB_INPUT = 1,   /* loaded by the host; CRC checked before the run */
    FX1_NPU_JOB_CONST = 2,   /* weights, bias, requant tables: as INPUT */
    FX1_NPU_JOB_OUTPUT = 3,  /* written by one operation; CRC compared after the run */
    FX1_NPU_JOB_SCRATCH = 4, /* written by one operation, read by later ones; not compared */
};
enum { FX1_NPU_JOB_INT8 = 1, FX1_NPU_JOB_INT32 = 2 }; /* dtype */
enum {                                                 /* layout (dims order) */
    FX1_NPU_JOB_FLAT = 0,   /* [n] */
    FX1_NPU_JOB_CHW = 1,    /* [c][h][w] */
    FX1_NPU_JOB_MATRIX = 2, /* [rows][cols], row-major */
    FX1_NPU_JOB_CONV_W = 3, /* [cin][kh][kw][cout] (the model's GEMM_FUSED weight order) */
};
enum { /* operation kind */
    FX1_NPU_JOB_GEMM_BANK = 1,    /* t: a int8 [32][K], w int8 [K][32], c int32 [32][32]; p0 K (1..992) */
    FX1_NPU_JOB_CONV = 2,         /* t: in, w, bias, scale, shift, skip|NONE, out; p: kh kw stride pad act zp_out */
    FX1_NPU_JOB_MAX_POOL = 3,     /* t: in, out; p: k pad stride */
    FX1_NPU_JOB_AVG_POOL = 4,     /* t: in, out; p: k pad(0) stride so sho */
    FX1_NPU_JOB_ADD = 5,          /* t: a, b, out (int8, any layout, same size); p: zp_a zp_b zp_o sa sha sb shb so sho */
    FX1_NPU_JOB_NEO_MAX_POOL = 6, /* as MAX_POOL, through the neo_lite register file (pad 0) */
    FX1_NPU_JOB_NEO_RESIDUAL = 7, /* as ADD, through the neo_lite register file */
};
/* Op tensor slots. */
enum { FX1_NPU_JOB_T_IN = 0, FX1_NPU_JOB_T_W = 1, FX1_NPU_JOB_T_BIAS = 2, FX1_NPU_JOB_T_SCALE = 3, FX1_NPU_JOB_T_SHIFT = 4,
       FX1_NPU_JOB_T_SKIP = 5, FX1_NPU_JOB_T_OUT = 6, FX1_NPU_JOB_T_SLOTS = 8 };
/* ADD / RESIDUAL: a = T_IN, b = T_W. GEMM bank: a = T_IN, w = T_W. */

typedef struct {
    uint32_t magic, version;
    uint32_t header_bytes;  /* sizeof(fx1_npu_job_header) = 128 */
    uint32_t total_bytes;   /* header + tables, <= FX1_NPU_JOB_SIZE */
    uint32_t job_crc;       /* CRC-32 of total_bytes with this word read as 0 */
    uint32_t abi_major;     /* VP_ABI major the job was made for (1) */
    uint32_t features;      /* FEATURE bits the job needs (the ops' own are added) */
    uint32_t model_hash;    /* VP_MODEL_HASH the goldens come from; 0: any model */
    uint32_t arena;         /* FX1_NPU_JOB_ARENA_* */
    uint32_t window_offset; /* from the arena base, 4 KiB aligned */
    uint32_t window_size;   /* bytes, multiple of 4 KiB */
    uint32_t n_tensors, tensor_offset; /* table offsets from the image start */
    uint32_t n_ops, op_offset;
    uint32_t timeout_ms;    /* execution budget, simulated time, 1..FX1_NPU_JOB_TIMEOUT_MAX: from the first
                             * push / start to the last completion; no op is pushed or started once it
                             * passed. The firmware's input CRCs / canaries before and output CRCs after
                             * are not in it (they take about 0.1 s per 64 KiB on the hart) */
    char name[32];          /* NUL-terminated, [A-Za-z0-9_.-] */
    uint32_t reserved[8];   /* 0 */
} fx1_npu_job_header;       /* 128 bytes */

typedef struct {
    uint32_t offset, bytes;      /* in the window */
    uint8_t role, dtype, layout, rank;
    uint32_t dims[4];            /* `rank` used, the rest 1 */
    uint32_t crc;                /* INPUT / CONST: as loaded; OUTPUT: expected; SCRATCH: 0 */
} fx1_npu_job_tensor;            /* 32 bytes */

typedef struct {
    uint32_t kind;
    uint8_t t[FX1_NPU_JOB_T_SLOTS]; /* tensor indices, FX1_NPU_JOB_NONE when unused */
    int32_t p[12];
    uint32_t reserved;              /* 0 */
} fx1_npu_job_op;                   /* 64 bytes */

/* fx1_npu_job_result.status */
enum {
    FX1_NPU_JOB_OK = 0,
    FX1_NPU_JOB_BAD_JOB = 1,        /* detail: FX1_NPU_JOB_E_* << 16 | index */
    FX1_NPU_JOB_NO_NPU = 2,         /* absent, or the context did not open (driver_status) */
    FX1_NPU_JOB_UNSUPPORTED = 3,    /* a FEATURE / ABI / model hash the job needs is missing */
    FX1_NPU_JOB_INPUT_CRC = 4,      /* detail: tensor index (out_crc[] holds the CRC found) */
    FX1_NPU_JOB_REFUSED = 5,        /* the NPU refused an instruction (op_index, err) */
    FX1_NPU_JOB_FAILED = 6,         /* an instruction ran and failed (op_index, err) */
    FX1_NPU_JOB_TIMEOUT = 7,        /* timeout_ms passed; recovered (or UNAVAILABLE) */
    FX1_NPU_JOB_ABORTED = 8,        /* `npu reset` while it ran; recovered */
    FX1_NPU_JOB_UNAVAILABLE = 9,    /* recovery failed: the NPU is closed until it is idle */
    FX1_NPU_JOB_MISMATCH = 10,      /* ran, an OUTPUT's CRC differs (out_crc[]) */
    FX1_NPU_JOB_STATUS_COUNT
};
/* fx1_npu_job_check reasons (detail >> 16) */
enum {
    FX1_NPU_JOB_E_NONE = 0,
    FX1_NPU_JOB_E_MAGIC, FX1_NPU_JOB_E_VERSION, FX1_NPU_JOB_E_SIZE, FX1_NPU_JOB_E_CRC, FX1_NPU_JOB_E_ABI,
    FX1_NPU_JOB_E_ARENA, FX1_NPU_JOB_E_WINDOW, FX1_NPU_JOB_E_TABLE, FX1_NPU_JOB_E_TIMEOUT, FX1_NPU_JOB_E_NAME,
    FX1_NPU_JOB_E_RESERVED,
    FX1_NPU_JOB_E_TENSOR_ROLE, FX1_NPU_JOB_E_TENSOR_TYPE, FX1_NPU_JOB_E_TENSOR_SHAPE, FX1_NPU_JOB_E_TENSOR_ALIGN,
    FX1_NPU_JOB_E_TENSOR_BOUNDS, FX1_NPU_JOB_E_TENSOR_OVERLAP, FX1_NPU_JOB_E_TENSOR_CRC,
    FX1_NPU_JOB_E_OP_KIND, FX1_NPU_JOB_E_OP_TENSOR, FX1_NPU_JOB_E_OP_ROLE, FX1_NPU_JOB_E_OP_SHAPE,
    FX1_NPU_JOB_E_OP_PARAM, FX1_NPU_JOB_E_OP_ORDER, FX1_NPU_JOB_E_OP_BUILD, FX1_NPU_JOB_E_UNWRITTEN,
    FX1_NPU_JOB_E_COUNT
};

typedef struct {
    uint32_t magic, version;
    uint32_t status, detail;        /* FX1_NPU_JOB_* ; reason / index */
    uint32_t op_index;              /* failing operation, 0xFFFFFFFF none */
    uint32_t driver_status;         /* fx1_npu_status_t of the failing driver call */
    uint32_t err[7];                /* npu->last then: status last_error irq_status rich_status vp_error vp_error_info vp_status */
    uint32_t ops_done, rich_instr, neo_instr, gemm_jobs;
    uint32_t accepted_delta, retired_delta;
    uint32_t exec_cycles, mac_ops_lo, mac_ops_hi; /* model counters (rich / neo ops), summed over the ops */
    uint32_t gemm_cycles;           /* GEMM-bank CYCLE_COUNT, summed */
    uint32_t ticks;                 /* mtime ticks (50 ns) from the first submit to the last completion */
    uint32_t irqs, stale_irqs;      /* NPU interrupts taken during the job; claims with IRQ_STATUS 0 */
    uint32_t n_outputs, outputs_ok;
    uint32_t job_crc, model_hash;   /* the job's image CRC, the NPU's VP_MODEL_HASH */
    uint32_t out_crc[FX1_NPU_JOB_MAX_TENSORS]; /* CRC-32 found per tensor index (OUTPUT; INPUT on a bad CRC) */
    uint32_t reserved[2];
} fx1_npu_job_result;               /* 256 bytes */

/* ---- no MMIO: host and hart ----------------------------------------------------- */
uint32_t fx1_npu_job_crc32(uint32_t crc, const volatile void* data, uint32_t n); /* crc 0 to start */
/* Checks an image of `len` bytes (at most FX1_NPU_JOB_SIZE). Returns 0 when
 * valid, else FX1_NPU_JOB_E_* << 16 | index (field, tensor or op index). */
uint32_t fx1_npu_job_check(const void* image, uint32_t len);
const fx1_npu_job_tensor* fx1_npu_job_tensors(const void* image);
const fx1_npu_job_op* fx1_npu_job_ops(const void* image);
uint32_t fx1_npu_job_window_base(const fx1_npu_job_header* h);
/* FEATURE bits the job needs: header.features plus those of its op kinds. */
uint32_t fx1_npu_job_features(const void* image);
/* The driver descriptors of op `i` for a window at bus address `base`, from a
 * checked image. kind GEMM_BANK fills *g, NEO_* fills *n, the others *in.
 * 0 when the op cannot be built (fx1_npu_job_check already rejects those). */
int fx1_npu_job_build(const void* image, uint32_t i, uint32_t base, fx1_npu_gemm_t* g, fx1_npu_instr_t* in,
                      fx1_npu_neo_t* n);
const char* fx1_npu_job_status_name(uint32_t status);
const char* fx1_npu_job_reason_name(uint32_t reason);

/* ---- executor (MMIO through the driver) ------------------------------------------ */
typedef struct {
    void* ctx;
    /* Blocks for at most about 1 ms or until the NPU interrupt (an RTOS task);
     * may be 0 (busy polling). */
    void (*wait)(void* ctx);
    /* Nonzero when the job must stop (`npu reset`). */
    int (*abort)(void* ctx);
    /* Called once per operation before it is issued (status, may be 0). */
    void (*progress)(void* ctx, uint32_t op_index);
} fx1_npu_job_hooks;

/* Runs a checked job image on an open context (quiet: nothing outstanding):
 * set_window, input CRCs, canaries, the operations, output CRCs. Fills *r
 * (magic last; status FX1_NPU_JOB_*). On a timeout or an abort the job is
 * recovered (fx1_npu_recover); a failed recovery leaves the context closed
 * (FX1_NPU_JOB_UNAVAILABLE). */
void fx1_npu_job_run(fx1_npu_t* npu, const void* image, const fx1_npu_job_hooks* hooks, fx1_npu_job_result* r);

#ifdef __cplusplus
}
#endif

#endif /* FX1_NPU_JOB_H */
