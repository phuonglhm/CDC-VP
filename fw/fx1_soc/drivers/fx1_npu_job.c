/*
 * FX1 NPU job format (fx1_npu_job.h): CRC-32, the checks, the driver
 * descriptors of each operation. No MMIO: compiles for the hart and the host
 * (host/npu_job_gen.c makes the fixtures and checks them with this code).
 */
#include "fx1_npu_job.h"

#include "fx1/fx1_memory_map.h"

/* ---- CRC-32 (IEEE 802.3, reflected 0xEDB88320, as zlib) ------------------------- */
static uint32_t crc_table[256];
static int crc_ready;

static void crc_init(void)
{
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t c = i;
        for (int k = 0; k < 8; ++k) c = c & 1u ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        crc_table[i] = c;
    }
    crc_ready = 1;
}

uint32_t fx1_npu_job_crc32(uint32_t crc, const volatile void* data, uint32_t n)
{
    if (!crc_ready) crc_init();
    const volatile uint8_t* p = (const volatile uint8_t*)data;
    crc = ~crc;
    /* Word reads where the buffer is aligned: the hart reads DDR word by word. */
    while (n && ((uintptr_t)p & 3u)) {
        crc = crc_table[(crc ^ *p++) & 0xFFu] ^ (crc >> 8);
        --n;
    }
    for (; n >= 4; n -= 4, p += 4) {
        const uint32_t w = *(const volatile uint32_t*)p;
        crc = crc_table[(crc ^ w) & 0xFFu] ^ (crc >> 8);
        crc = crc_table[(crc ^ (w >> 8)) & 0xFFu] ^ (crc >> 8);
        crc = crc_table[(crc ^ (w >> 16)) & 0xFFu] ^ (crc >> 8);
        crc = crc_table[(crc ^ (w >> 24)) & 0xFFu] ^ (crc >> 8);
    }
    while (n--) crc = crc_table[(crc ^ *p++) & 0xFFu] ^ (crc >> 8);
    return ~crc;
}

/* The image's CRC with the job_crc word read as 0. */
static uint32_t image_crc(const fx1_npu_job_header* h)
{
    const uint8_t* b = (const uint8_t*)h;
    const uint32_t at = (uint32_t)((const uint8_t*)&h->job_crc - b);
    const uint32_t zero = 0;
    uint32_t c = fx1_npu_job_crc32(0, b, at);
    c = fx1_npu_job_crc32(c, &zero, 4);
    return fx1_npu_job_crc32(c, b + at + 4, h->total_bytes - at - 4);
}

/* ---- tables ------------------------------------------------------------------------ */
const fx1_npu_job_tensor* fx1_npu_job_tensors(const void* image)
{
    const fx1_npu_job_header* h = (const fx1_npu_job_header*)image;
    return (const fx1_npu_job_tensor*)((const uint8_t*)image + h->tensor_offset);
}
const fx1_npu_job_op* fx1_npu_job_ops(const void* image)
{
    const fx1_npu_job_header* h = (const fx1_npu_job_header*)image;
    return (const fx1_npu_job_op*)((const uint8_t*)image + h->op_offset);
}

static uint32_t arena_base(uint32_t a) { return a == FX1_NPU_JOB_ARENA_LARGE ? FX1_NPU_ARENA_LARGE_BASE : FX1_NPU_ARENA_SMALL_BASE; }
static uint32_t arena_size(uint32_t a) { return a == FX1_NPU_JOB_ARENA_LARGE ? FX1_NPU_ARENA_LARGE_SIZE : FX1_NPU_ARENA_SMALL_SIZE; }

uint32_t fx1_npu_job_window_base(const fx1_npu_job_header* h) { return arena_base(h->arena) + h->window_offset; }

static int is_rich(uint32_t kind) { return kind >= FX1_NPU_JOB_CONV && kind <= FX1_NPU_JOB_ADD; }
static int is_neo(uint32_t kind) { return kind == FX1_NPU_JOB_NEO_MAX_POOL || kind == FX1_NPU_JOB_NEO_RESIDUAL; }

uint32_t fx1_npu_job_features(const void* image)
{
    const fx1_npu_job_header* h = (const fx1_npu_job_header*)image;
    const fx1_npu_job_op* op = fx1_npu_job_ops(image);
    uint32_t f = h->features;
    for (uint32_t i = 0; i < h->n_ops; ++i) {
        if (op[i].kind == FX1_NPU_JOB_GEMM_BANK) f |= FX1_NPU_FEATURE_NATIVE_GEMM;
        if (is_rich(op[i].kind)) f |= FX1_NPU_FEATURE_RICH_HAS;
        if (is_neo(op[i].kind)) f |= FX1_NPU_FEATURE_RICH_HAS | FX1_NPU_FEATURE_DFC_WINDOW;
    }
    return f;
}

/* ---- operation descriptors -------------------------------------------------------- */
static uint32_t addr_of(const fx1_npu_job_tensor* t, uint32_t base, uint8_t idx)
{
    return idx == FX1_NPU_JOB_NONE ? 0u : base + t[idx].offset;
}

static void conv_of(const fx1_npu_job_tensor* t, const fx1_npu_job_op* op, uint32_t base, fx1_npu_conv_t* c)
{
    const fx1_npu_job_tensor* in = &t[op->t[FX1_NPU_JOB_T_IN]];
    const fx1_npu_job_tensor* out = &t[op->t[FX1_NPU_JOB_T_OUT]];
    c->in = addr_of(t, base, op->t[FX1_NPU_JOB_T_IN]);
    c->w = addr_of(t, base, op->t[FX1_NPU_JOB_T_W]);
    c->bias = addr_of(t, base, op->t[FX1_NPU_JOB_T_BIAS]);
    c->scale = addr_of(t, base, op->t[FX1_NPU_JOB_T_SCALE]);
    c->shift = addr_of(t, base, op->t[FX1_NPU_JOB_T_SHIFT]);
    c->skip = addr_of(t, base, op->t[FX1_NPU_JOB_T_SKIP]);
    c->out = addr_of(t, base, op->t[FX1_NPU_JOB_T_OUT]);
    c->cin = in->dims[0];
    c->ih = in->dims[1];
    c->iw = in->dims[2];
    c->cout = out->dims[0];
    c->kh = (uint32_t)op->p[0];
    c->kw = (uint32_t)op->p[1];
    c->stride = (uint32_t)op->p[2];
    c->pad = (uint32_t)op->p[3];
    c->act = (uint32_t)op->p[4];
    c->zp_out = op->p[5];
}

static void pool_of(const fx1_npu_job_tensor* t, const fx1_npu_job_op* op, uint32_t base, fx1_npu_pool_t* p)
{
    const fx1_npu_job_tensor* in = &t[op->t[FX1_NPU_JOB_T_IN]];
    p->in = addr_of(t, base, op->t[FX1_NPU_JOB_T_IN]);
    p->out = addr_of(t, base, op->t[FX1_NPU_JOB_T_OUT]);
    p->c = in->dims[0];
    p->h = in->dims[1];
    p->w = in->dims[2];
    p->k = (uint32_t)op->p[0];
    p->pad = (uint32_t)op->p[1];
    p->stride = (uint32_t)op->p[2];
    p->avg = op->kind == FX1_NPU_JOB_AVG_POOL;
    p->so = p->avg ? op->p[3] : 0;
    p->sho = p->avg ? op->p[4] : 0;
}

static void add_of(const fx1_npu_job_tensor* t, const fx1_npu_job_op* op, uint32_t base, fx1_npu_add_t* a)
{
    a->a = addr_of(t, base, op->t[FX1_NPU_JOB_T_IN]);
    a->b = addr_of(t, base, op->t[FX1_NPU_JOB_T_W]);
    a->out = addr_of(t, base, op->t[FX1_NPU_JOB_T_OUT]);
    a->n = t[op->t[FX1_NPU_JOB_T_OUT]].bytes; /* int8 elements */
    a->zp_a = op->p[0];
    a->zp_b = op->p[1];
    a->zp_o = op->p[2];
    a->sa = op->p[3];
    a->sha = op->p[4];
    a->sb = op->p[5];
    a->shb = op->p[6];
    a->so = op->p[7];
    a->sho = op->p[8];
}

int fx1_npu_job_build(const void* image, uint32_t i, uint32_t base, fx1_npu_gemm_t* g, fx1_npu_instr_t* in,
                      fx1_npu_neo_t* n)
{
    const fx1_npu_job_tensor* t = fx1_npu_job_tensors(image);
    const fx1_npu_job_op* op = &fx1_npu_job_ops(image)[i];
    switch (op->kind) {
    case FX1_NPU_JOB_GEMM_BANK:
        g->a = addr_of(t, base, op->t[FX1_NPU_JOB_T_IN]);
        g->w = addr_of(t, base, op->t[FX1_NPU_JOB_T_W]);
        g->c = addr_of(t, base, op->t[FX1_NPU_JOB_T_OUT]);
        g->k = (uint32_t)op->p[0];
        return 1;
    case FX1_NPU_JOB_CONV: {
        fx1_npu_conv_t c;
        conv_of(t, op, base, &c);
        return fx1_npu_build_conv(in, &c);
    }
    case FX1_NPU_JOB_MAX_POOL:
    case FX1_NPU_JOB_AVG_POOL: {
        fx1_npu_pool_t p;
        pool_of(t, op, base, &p);
        return fx1_npu_build_pool(in, &p);
    }
    case FX1_NPU_JOB_ADD: {
        fx1_npu_add_t a;
        add_of(t, op, base, &a);
        return fx1_npu_build_add(in, &a);
    }
    case FX1_NPU_JOB_NEO_MAX_POOL: {
        fx1_npu_pool_t p;
        pool_of(t, op, base, &p);
        return fx1_npu_neo_pool(n, &p);
    }
    case FX1_NPU_JOB_NEO_RESIDUAL: {
        fx1_npu_add_t a;
        add_of(t, op, base, &a);
        return fx1_npu_neo_residual(n, &a);
    }
    }
    return 0;
}

/* ---- checks ------------------------------------------------------------------------ */
#define E(reason, index) (((uint32_t)(reason) << 16) | ((uint32_t)(index) & 0xFFFFu))

static uint32_t esize(uint8_t dtype) { return dtype == FX1_NPU_JOB_INT32 ? 4u : 1u; }

static int tensor_is(const fx1_npu_job_tensor* x, uint8_t dtype, uint8_t layout, uint32_t d0, uint32_t d1, uint32_t d2,
                     uint32_t d3)
{
    return x->dtype == dtype && x->layout == layout && x->dims[0] == d0 && x->dims[1] == d1 && x->dims[2] == d2 &&
           x->dims[3] == d3;
}

/* Slots each kind uses (bit k = slot k). */
static uint32_t slots_of(uint32_t kind)
{
    switch (kind) {
    case FX1_NPU_JOB_GEMM_BANK: return 1u << FX1_NPU_JOB_T_IN | 1u << FX1_NPU_JOB_T_W | 1u << FX1_NPU_JOB_T_OUT;
    case FX1_NPU_JOB_CONV:
        return 1u << FX1_NPU_JOB_T_IN | 1u << FX1_NPU_JOB_T_W | 1u << FX1_NPU_JOB_T_BIAS | 1u << FX1_NPU_JOB_T_SCALE |
               1u << FX1_NPU_JOB_T_SHIFT | 1u << FX1_NPU_JOB_T_SKIP | 1u << FX1_NPU_JOB_T_OUT;
    case FX1_NPU_JOB_MAX_POOL:
    case FX1_NPU_JOB_AVG_POOL:
    case FX1_NPU_JOB_NEO_MAX_POOL: return 1u << FX1_NPU_JOB_T_IN | 1u << FX1_NPU_JOB_T_OUT;
    case FX1_NPU_JOB_ADD:
    case FX1_NPU_JOB_NEO_RESIDUAL: return 1u << FX1_NPU_JOB_T_IN | 1u << FX1_NPU_JOB_T_W | 1u << FX1_NPU_JOB_T_OUT;
    }
    return 0;
}
/* Parameters each kind uses. */
static unsigned params_of(uint32_t kind)
{
    switch (kind) {
    case FX1_NPU_JOB_GEMM_BANK: return 1;
    case FX1_NPU_JOB_CONV: return 6;
    case FX1_NPU_JOB_MAX_POOL:
    case FX1_NPU_JOB_NEO_MAX_POOL: return 3;
    case FX1_NPU_JOB_AVG_POOL: return 5;
    case FX1_NPU_JOB_ADD:
    case FX1_NPU_JOB_NEO_RESIDUAL: return 9;
    }
    return 0;
}

static int zp_ok(int32_t z) { return z >= -128 && z <= 127; }
static int shift_ok(int32_t s) { return s >= 0 && s <= 31; }

/* Shapes, dtypes and parameters of op `op` (its tensors exist). */
static int op_shape_ok(const fx1_npu_job_tensor* t, const fx1_npu_job_op* op, uint32_t* reason)
{
    const fx1_npu_job_tensor* in = &t[op->t[FX1_NPU_JOB_T_IN]];
    const fx1_npu_job_tensor* out = &t[op->t[FX1_NPU_JOB_T_OUT]];
    const int32_t* p = op->p;
    *reason = FX1_NPU_JOB_E_OP_SHAPE;
    switch (op->kind) {
    case FX1_NPU_JOB_GEMM_BANK: {
        const uint32_t k = (uint32_t)p[0];
        if (p[0] < 1 || p[0] > 992) {
            *reason = FX1_NPU_JOB_E_OP_PARAM;
            return 0;
        }
        return tensor_is(in, FX1_NPU_JOB_INT8, FX1_NPU_JOB_MATRIX, FX1_NPU_GEMM_ROWS, k, 1, 1) &&
               tensor_is(&t[op->t[FX1_NPU_JOB_T_W]], FX1_NPU_JOB_INT8, FX1_NPU_JOB_MATRIX, k, FX1_NPU_GEMM_COLS, 1, 1) &&
               tensor_is(out, FX1_NPU_JOB_INT32, FX1_NPU_JOB_MATRIX, FX1_NPU_GEMM_ROWS, FX1_NPU_GEMM_COLS, 1, 1);
    }
    case FX1_NPU_JOB_CONV: {
        if (p[0] < 1 || p[1] < 1 || p[2] < 1 || p[3] < 0 || p[3] >= p[0] || p[3] >= p[1] ||
            (p[4] != (int32_t)FX1_NPU_ACT_NONE && p[4] != (int32_t)FX1_NPU_ACT_RELU) || !zp_ok(p[5])) {
            *reason = FX1_NPU_JOB_E_OP_PARAM;
            return 0;
        }
        fx1_npu_conv_t c;
        conv_of(t, op, 0, &c);
        const uint32_t oh = fx1_npu_conv_oh(&c), ow = fx1_npu_conv_ow(&c);
        const uint8_t skip = op->t[FX1_NPU_JOB_T_SKIP];
        return in->dtype == FX1_NPU_JOB_INT8 && in->layout == FX1_NPU_JOB_CHW && oh && ow &&
               tensor_is(&t[op->t[FX1_NPU_JOB_T_W]], FX1_NPU_JOB_INT8, FX1_NPU_JOB_CONV_W, c.cin, c.kh, c.kw, c.cout) &&
               tensor_is(&t[op->t[FX1_NPU_JOB_T_BIAS]], FX1_NPU_JOB_INT32, FX1_NPU_JOB_FLAT, c.cout, 1, 1, 1) &&
               tensor_is(&t[op->t[FX1_NPU_JOB_T_SCALE]], FX1_NPU_JOB_INT32, FX1_NPU_JOB_FLAT, c.cout, 1, 1, 1) &&
               tensor_is(&t[op->t[FX1_NPU_JOB_T_SHIFT]], FX1_NPU_JOB_INT32, FX1_NPU_JOB_FLAT, c.cout, 1, 1, 1) &&
               (skip == FX1_NPU_JOB_NONE || tensor_is(&t[skip], FX1_NPU_JOB_INT8, FX1_NPU_JOB_CHW, c.cout, oh, ow, 1)) &&
               tensor_is(out, FX1_NPU_JOB_INT8, FX1_NPU_JOB_CHW, c.cout, oh, ow, 1);
    }
    case FX1_NPU_JOB_MAX_POOL:
    case FX1_NPU_JOB_AVG_POOL:
    case FX1_NPU_JOB_NEO_MAX_POOL: {
        const int avg = op->kind == FX1_NPU_JOB_AVG_POOL;
        if (p[0] < 1 || p[1] < 0 || p[1] >= p[0] || p[2] < 1 || (avg && (p[1] != 0 || p[3] < 0 || !shift_ok(p[4]))) ||
            (op->kind == FX1_NPU_JOB_NEO_MAX_POOL && p[1] != 0)) {
            *reason = FX1_NPU_JOB_E_OP_PARAM;
            return 0;
        }
        fx1_npu_pool_t q;
        pool_of(t, op, 0, &q);
        const uint32_t oh = fx1_npu_pool_oh(&q), ow = fx1_npu_pool_ow(&q);
        return in->dtype == FX1_NPU_JOB_INT8 && in->layout == FX1_NPU_JOB_CHW && oh && ow &&
               tensor_is(out, FX1_NPU_JOB_INT8, FX1_NPU_JOB_CHW, q.c, oh, ow, 1);
    }
    case FX1_NPU_JOB_ADD:
    case FX1_NPU_JOB_NEO_RESIDUAL: {
        if (!zp_ok(p[0]) || !zp_ok(p[1]) || !zp_ok(p[2]) || p[3] < 0 || !shift_ok(p[4]) || p[5] < 0 || !shift_ok(p[6]) ||
            p[7] < 0 || !shift_ok(p[8])) {
            *reason = FX1_NPU_JOB_E_OP_PARAM;
            return 0;
        }
        /* Element-wise over the bytes: any layout, the same element count. */
        const fx1_npu_job_tensor* b = &t[op->t[FX1_NPU_JOB_T_W]];
        return in->dtype == FX1_NPU_JOB_INT8 && b->dtype == FX1_NPU_JOB_INT8 && out->dtype == FX1_NPU_JOB_INT8 &&
               in->bytes == out->bytes && b->bytes == out->bytes;
    }
    }
    *reason = FX1_NPU_JOB_E_OP_KIND;
    return 0;
}

static int name_ok(const char* s)
{
    unsigned k = 0;
    for (; k < 32 && s[k]; ++k) {
        const char c = s[k];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '.' ||
              c == '-'))
            return 0;
    }
    return k > 0 && k < 32;
}

uint32_t fx1_npu_job_check(const void* image, uint32_t len)
{
    const fx1_npu_job_header* h = (const fx1_npu_job_header*)image;
    if (!image || len < sizeof *h) return E(FX1_NPU_JOB_E_SIZE, 0);
    if (h->magic != FX1_NPU_JOB_MAGIC) return E(FX1_NPU_JOB_E_MAGIC, 0);
    if (h->version != FX1_NPU_JOB_VERSION) return E(FX1_NPU_JOB_E_VERSION, 0);
    if (h->header_bytes != sizeof *h || h->total_bytes < sizeof *h || h->total_bytes > len ||
        h->total_bytes > FX1_NPU_JOB_SIZE || (h->total_bytes & 3u))
        return E(FX1_NPU_JOB_E_SIZE, 0);
    if (image_crc(h) != h->job_crc) return E(FX1_NPU_JOB_E_CRC, 0);
    if (h->abi_major != FX1_NPU_VP_ABI_MAJOR(FX1_NPU_VP_ABI_VALUE)) return E(FX1_NPU_JOB_E_ABI, 0);
    for (unsigned k = 0; k < 8; ++k)
        if (h->reserved[k]) return E(FX1_NPU_JOB_E_RESERVED, k);
    if (!name_ok(h->name)) return E(FX1_NPU_JOB_E_NAME, 0);
    if (h->timeout_ms < 1 || h->timeout_ms > FX1_NPU_JOB_TIMEOUT_MAX) return E(FX1_NPU_JOB_E_TIMEOUT, 0);
    if (h->arena > FX1_NPU_JOB_ARENA_LARGE) return E(FX1_NPU_JOB_E_ARENA, 0);
    const uint32_t asize = arena_size(h->arena), wsize = h->window_size;
    if ((h->window_offset & 0xFFFu) || (wsize & 0xFFFu) || !wsize || h->window_offset > asize ||
        wsize > asize - h->window_offset)
        return E(FX1_NPU_JOB_E_WINDOW, 0);
    if (h->n_tensors < 1 || h->n_tensors > FX1_NPU_JOB_MAX_TENSORS || h->n_ops < 1 || h->n_ops > FX1_NPU_JOB_MAX_OPS ||
        h->tensor_offset != sizeof *h || h->op_offset != h->tensor_offset + h->n_tensors * sizeof(fx1_npu_job_tensor) ||
        h->total_bytes != h->op_offset + h->n_ops * sizeof(fx1_npu_job_op))
        return E(FX1_NPU_JOB_E_TABLE, 0);

    const fx1_npu_job_tensor* t = fx1_npu_job_tensors(image);
    uint32_t outputs = 0;
    for (uint32_t i = 0; i < h->n_tensors; ++i) {
        const fx1_npu_job_tensor* x = &t[i];
        if (x->role < FX1_NPU_JOB_INPUT || x->role > FX1_NPU_JOB_SCRATCH) return E(FX1_NPU_JOB_E_TENSOR_ROLE, i);
        if ((x->dtype != FX1_NPU_JOB_INT8 && x->dtype != FX1_NPU_JOB_INT32) || x->layout > FX1_NPU_JOB_CONV_W)
            return E(FX1_NPU_JOB_E_TENSOR_TYPE, i);
        static const uint8_t rank_of[] = {1, 3, 2, 4}; /* FLAT, CHW, MATRIX, CONV_W */
        if (x->rank != rank_of[x->layout]) return E(FX1_NPU_JOB_E_TENSOR_SHAPE, i);
        uint64_t elems = 1;
        for (unsigned k = 0; k < 4; ++k) {
            if (!x->dims[k] || (k >= x->rank && x->dims[k] != 1)) return E(FX1_NPU_JOB_E_TENSOR_SHAPE, i);
            elems *= x->dims[k];
            if (elems > 0xFFFFFFFFull) return E(FX1_NPU_JOB_E_TENSOR_SHAPE, i);
        }
        if (elems * esize(x->dtype) != x->bytes) return E(FX1_NPU_JOB_E_TENSOR_SHAPE, i);
        if (x->offset & 3u) return E(FX1_NPU_JOB_E_TENSOR_ALIGN, i);
        if (!x->bytes || x->offset >= wsize || x->bytes > wsize - x->offset) return E(FX1_NPU_JOB_E_TENSOR_BOUNDS, i);
        if (x->role == FX1_NPU_JOB_SCRATCH && x->crc) return E(FX1_NPU_JOB_E_TENSOR_CRC, i);
        for (uint32_t j = 0; j < i; ++j)
            if (!(x->offset + x->bytes <= t[j].offset || t[j].offset + t[j].bytes <= x->offset))
                return E(FX1_NPU_JOB_E_TENSOR_OVERLAP, i);
        outputs += x->role == FX1_NPU_JOB_OUTPUT;
    }
    if (!outputs) return E(FX1_NPU_JOB_E_UNWRITTEN, 0xFFFFu);

    const fx1_npu_job_op* op = fx1_npu_job_ops(image);
    uint32_t written = 0; /* bit i: tensor i written by an earlier op (32 tensors at most) */
    const uint32_t base = fx1_npu_job_window_base(h);
    for (uint32_t i = 0; i < h->n_ops; ++i) {
        const fx1_npu_job_op* o = &op[i];
        const uint32_t used = slots_of(o->kind);
        if (!used) return E(FX1_NPU_JOB_E_OP_KIND, i);
        if (o->reserved) return E(FX1_NPU_JOB_E_OP_PARAM, i);
        for (unsigned k = params_of(o->kind); k < 12; ++k)
            if (o->p[k]) return E(FX1_NPU_JOB_E_OP_PARAM, i);
        for (unsigned k = 0; k < FX1_NPU_JOB_T_SLOTS; ++k) {
            const uint8_t s = o->t[k];
            const int optional = o->kind == FX1_NPU_JOB_CONV && k == FX1_NPU_JOB_T_SKIP;
            if (!(used & (1u << k))) {
                if (s != FX1_NPU_JOB_NONE) return E(FX1_NPU_JOB_E_OP_TENSOR, i);
                continue;
            }
            if (s == FX1_NPU_JOB_NONE) {
                if (optional) continue;
                return E(FX1_NPU_JOB_E_OP_TENSOR, i);
            }
            if (s >= h->n_tensors) return E(FX1_NPU_JOB_E_OP_TENSOR, i);
            const uint8_t role = t[s].role;
            if (k == FX1_NPU_JOB_T_OUT) {
                if (role != FX1_NPU_JOB_OUTPUT && role != FX1_NPU_JOB_SCRATCH) return E(FX1_NPU_JOB_E_OP_ROLE, i);
                if (written & (1u << s)) return E(FX1_NPU_JOB_E_OP_ORDER, i);
            } else if ((role == FX1_NPU_JOB_OUTPUT || role == FX1_NPU_JOB_SCRATCH) && !(written & (1u << s))) {
                return E(FX1_NPU_JOB_E_OP_ORDER, i); /* read before an earlier op wrote it */
            }
        }
        uint32_t reason;
        if (!op_shape_ok(t, o, &reason)) return E(reason, i);
        fx1_npu_gemm_t g;
        fx1_npu_instr_t in;
        fx1_npu_neo_t n;
        if (!fx1_npu_job_build(image, i, base, &g, &in, &n)) return E(FX1_NPU_JOB_E_OP_BUILD, i);
        written |= 1u << o->t[FX1_NPU_JOB_T_OUT];
    }
    for (uint32_t i = 0; i < h->n_tensors; ++i)
        if ((t[i].role == FX1_NPU_JOB_OUTPUT || t[i].role == FX1_NPU_JOB_SCRATCH) && !(written & (1u << i)))
            return E(FX1_NPU_JOB_E_UNWRITTEN, i);
    return 0;
}

/* ---- names ------------------------------------------------------------------------- */
const char* fx1_npu_job_status_name(uint32_t status)
{
    static const char* const n[] = {"OK",      "BAD_JOB", "NO_NPU",  "UNSUPPORTED", "INPUT_CRC",  "REFUSED",
                                    "FAILED",  "TIMEOUT", "ABORTED", "UNAVAILABLE", "MISMATCH"};
    return status < sizeof n / sizeof n[0] ? n[status] : "?";
}

const char* fx1_npu_job_reason_name(uint32_t reason)
{
    static const char* const n[] = {"none",           "magic",          "version",       "size",          "crc",
                                    "abi",            "arena",          "window",        "table",         "timeout",
                                    "name",           "reserved",       "tensor role",   "tensor type",   "tensor shape",
                                    "tensor align",   "tensor bounds",  "tensor overlap", "tensor crc",   "op kind",
                                    "op tensor",      "op role",        "op shape",      "op param",      "op order",
                                    "op build",       "unwritten"};
    return reason < sizeof n / sizeof n[0] ? n[reason] : "?";
}
