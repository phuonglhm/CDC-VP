/*
 * FX1 NPU driver: instruction builders. No MMIO, no platform header: this file
 * also compiles on the host (fw/fx1_soc host tests generate ref_program.py
 * programs from these builders and compare the neo words with the drop's
 * encoder).
 *
 * Lane-A registers: model host addresses (fx1_npu_regs.h). The parameter
 * registers are sticky across pushes, so every builder writes each parameter
 * its operation reads; the extension bank is one-shot (cleared by every
 * push) and is written in full for each instruction.
 *
 * neo_lite words: the encoding of has/has_host_neo.h plan() of the SAURIA v4.7
 * drop (MAXPOOL / AVGPOOL / RESIDUAL), and for RESIDUAL the parameter mapping
 * of has/rtl_ref_gvu_residual_hw.h ResidualHw::map (round half up, zero point
 * subtracted first). The host test fx1_fw_npu_neo_encoder checks every word
 * against the drop's C++ for a sweep of shapes.
 */
#include "fx1_npu.h"

static void put(fx1_npu_instr_t* i, uint32_t addr, uint32_t val)
{
    if (i->n < FX1_NPU_INSTR_MAX_WRITES) {
        i->addr[i->n] = addr;
        i->val[i->n] = val;
    }
    ++i->n;
}

static int finish(fx1_npu_instr_t* i, uint32_t opcode)
{
    i->push = opcode;
    return i->n <= FX1_NPU_INSTR_MAX_WRITES;
}

static void clear_instr(fx1_npu_instr_t* i)
{
    i->n = 0;
    i->push = 0;
    i->out = 0;
    i->out_bytes = 0;
}

uint32_t fx1_npu_conv_oh(const fx1_npu_conv_t* c)
{
    if (!c->stride || c->ih + 2 * c->pad < c->kh) return 0;
    return (c->ih + 2 * c->pad - c->kh) / c->stride + 1;
}
uint32_t fx1_npu_conv_ow(const fx1_npu_conv_t* c)
{
    if (!c->stride || c->iw + 2 * c->pad < c->kw) return 0;
    return (c->iw + 2 * c->pad - c->kw) / c->stride + 1;
}

int fx1_npu_build_conv(fx1_npu_instr_t* i, const fx1_npu_conv_t* c)
{
    clear_instr(i);
    const uint32_t oh = fx1_npu_conv_oh(c), ow = fx1_npu_conv_ow(c);
    if (!oh || !ow || !c->cin || !c->cout || !c->in || !c->w || !c->out || !c->bias || !c->scale || !c->shift ||
        c->act > FX1_NPU_ACT_RELU)
        return 0;
    put(i, FX1_NPU_M_IN_ADDR, c->in);
    put(i, FX1_NPU_M_W_ADDR, c->w);
    put(i, FX1_NPU_M_OUT_ADDR, c->out);
    put(i, FX1_NPU_M_BIAS_ADDR, c->bias);
    put(i, FX1_NPU_M_KERNEL_H, c->kh);
    put(i, FX1_NPU_M_KERNEL_W, c->kw);
    put(i, FX1_NPU_M_STRIDE, c->stride);
    put(i, FX1_NPU_M_PADDING, c->pad);
    put(i, FX1_NPU_M_ACT_TYPE, c->act);
    put(i, FX1_NPU_M_HAS_SKIP, c->skip ? 1u : 0u);
    put(i, FX1_NPU_M_SKIP_ADDR, c->skip);
    put(i, FX1_NPU_M_IN_C, c->cin);
    put(i, FX1_NPU_M_IN_H, c->ih);
    put(i, FX1_NPU_M_IN_W, c->iw);
    put(i, FX1_NPU_M_OUT_C, c->cout);
    put(i, FX1_NPU_M_OUT_H, oh);
    put(i, FX1_NPU_M_OUT_W, ow);
    put(i, FX1_NPU_M_TILE_COUT, c->cout);
    put(i, FX1_NPU_M_TILE_H, oh);
    put(i, FX1_NPU_M_TILE_W, ow);
    put(i, FX1_NPU_M_SCALE_ADDR, c->scale);
    put(i, FX1_NPU_M_SHIFT_ADDR, c->shift);
    put(i, FX1_NPU_M_ZP_OUT, (uint32_t)c->zp_out);
    put(i, FX1_NPU_M_FLAGS, 0);
    i->out = c->out;
    i->out_bytes = c->cout * oh * ow;
    return finish(i, FX1_NPU_OP_GEMM_FUSED);
}

uint32_t fx1_npu_pool_oh(const fx1_npu_pool_t* p)
{
    if (!p->stride || p->h + 2 * p->pad < p->k) return 0;
    return (p->h + 2 * p->pad - p->k) / p->stride + 1;
}
uint32_t fx1_npu_pool_ow(const fx1_npu_pool_t* p)
{
    if (!p->stride || p->w + 2 * p->pad < p->k) return 0;
    return (p->w + 2 * p->pad - p->k) / p->stride + 1;
}

int fx1_npu_build_pool(fx1_npu_instr_t* i, const fx1_npu_pool_t* p)
{
    clear_instr(i);
    const uint32_t oh = fx1_npu_pool_oh(p), ow = fx1_npu_pool_ow(p);
    if (!oh || !ow || !p->c || !p->k || !p->in || !p->out || (p->avg && p->pad)) return 0;
    put(i, FX1_NPU_M_A_ADDR, p->in);
    put(i, FX1_NPU_M_OUT_ADDR, p->out);
    put(i, FX1_NPU_M_MODE, p->avg ? FX1_NPU_ELEM_AVG_POOL : FX1_NPU_ELEM_MAX_POOL);
    put(i, FX1_NPU_M_STRIDE, p->stride);
    put(i, FX1_NPU_M_IN_C, p->c);
    put(i, FX1_NPU_M_IN_H, p->h);
    put(i, FX1_NPU_M_IN_W, p->w);
    put(i, FX1_NPU_M_POOL_K, p->k);
    put(i, FX1_NPU_M_POOL_P, p->pad);
    if (p->avg) {
        put(i, FX1_NPU_M_SO, (uint32_t)p->so);
        put(i, FX1_NPU_M_SHO, (uint32_t)p->sho);
    }
    i->out = p->out;
    i->out_bytes = p->c * oh * ow;
    return finish(i, FX1_NPU_OP_ELEM_WISE);
}

int fx1_npu_build_add(fx1_npu_instr_t* i, const fx1_npu_add_t* a)
{
    clear_instr(i);
    if (!a->n || !a->a || !a->b || !a->out) return 0;
    put(i, FX1_NPU_M_A_ADDR, a->a);
    put(i, FX1_NPU_M_B_ADDR, a->b);
    put(i, FX1_NPU_M_OUT_ADDR, a->out);
    put(i, FX1_NPU_M_SEQ_LEN, a->n);
    put(i, FX1_NPU_M_A_LEN, 0);
    put(i, FX1_NPU_M_B_LEN, 0);
    put(i, FX1_NPU_M_MODE, FX1_NPU_ELEM_ADD);
    put(i, FX1_NPU_M_ZP_A, (uint32_t)a->zp_a);
    put(i, FX1_NPU_M_ZP_B, (uint32_t)a->zp_b);
    put(i, FX1_NPU_M_ZP_O, (uint32_t)a->zp_o);
    put(i, FX1_NPU_M_SA, (uint32_t)a->sa);
    put(i, FX1_NPU_M_SHA, (uint32_t)a->sha);
    put(i, FX1_NPU_M_SB, (uint32_t)a->sb);
    put(i, FX1_NPU_M_SHB, (uint32_t)a->shb);
    put(i, FX1_NPU_M_SO, (uint32_t)a->so);
    put(i, FX1_NPU_M_SHO, (uint32_t)a->sho);
    i->out = a->out;
    i->out_bytes = a->n;
    return finish(i, FX1_NPU_OP_ELEM_WISE);
}

int fx1_npu_build_raw(fx1_npu_instr_t* i, uint32_t opcode, const uint32_t* addr, const uint32_t* val, uint32_t n)
{
    clear_instr(i);
    for (uint32_t k = 0; k < n; ++k) put(i, addr[k], val[k]);
    return finish(i, opcode);
}

/* ---- neo_lite words (has/has_host_neo.h plan()) ------------------------------ */
enum {
    NEO_G_TILE_LIM = 21, NEO_V0_BTT = 24, NEO_V0_STRIDE = 25, NEO_V0_REP = 26, NEO_V0_MOVE = 27, NEO_V1_BTT = 28,
    NEO_V1_MOVE = 29, NEO_V2_BTT = 30, NEO_V2_STRIDE = 31, NEO_V2_REP = 32, NEO_V2_MOVE = 33, NEO_START_V0 = 35,
    NEO_START_V1 = 36, NEO_START_V2 = 37, NEO_OPCODE = 39,
    NEO_IDX_GVU = 69, /* 40 DFC words + 29 core words */
};
enum { NEO_OP_RESIDUAL = 4, NEO_OP_MAXPOOL = 5, NEO_OP_AVGPOOL = 6 };
enum { GVU_RESIDUAL = 6, GVU_MAXPOOL = 7, GVU_AVGPOOL = 8 };
enum { G_OPCODE = 1, G_RES0 = 7, G_RES1 = 8, G_RES2 = 9, G_RES3 = 10, G_RES4 = 11, G_POOL0 = 12, G_POOL1 = 13 };
#define NEO_GVU(r) (NEO_IDX_GVU + (r) - 1)
#define NEO_VBANK0_ROWS 800

static void neo_clear(fx1_npu_neo_t* n)
{
    for (unsigned k = 0; k < FX1_NPU_NEO_REG_COUNT; ++k) n->word[k] = 0;
    n->out = 0;
    n->out_bytes = 0;
}
static int cdiv(int a, int b) { return (a + b - 1) / b; }

int fx1_npu_neo_pool(fx1_npu_neo_t* n, const fx1_npu_pool_t* p)
{
    neo_clear(n);
    const int c = (int)p->c, pp = (int)p->pad, hp = (int)p->h + 2 * pp, wp = (int)p->w + 2 * pp, k = (int)p->k,
              s = (int)p->stride;
    if (c <= 0 || hp < k || wp < k || s <= 0 || k <= 0 || pp != 0 || k < 2 || k > 5 || s > 7) return 0;
    const int nw = cdiv(wp, 32), oh = (hp - k) / s + 1, ow = (wp - k) / s + 1, now = cdiv(ow, 32);
    if (oh > 255 || nw > 255 || nw * hp > 0x3FF || nw * s > 0x3FF) return 0;
    int ct = 1;
    for (int t = c < 255 ? c : 255; t >= 1; t--)
        if (c % t == 0 && t * hp * nw <= NEO_VBANK0_ROWS) {
            ct = t;
            break;
        }
    n->word[NEO_OPCODE] = p->avg ? NEO_OP_AVGPOOL : NEO_OP_MAXPOOL;
    n->word[NEO_GVU(G_OPCODE)] = p->avg ? GVU_AVGPOOL : GVU_MAXPOOL;
    n->word[NEO_GVU(G_POOL0)] = ((uint32_t)(k - 2) & 3u) | ((uint32_t)s & 7u) << 2 | ((uint32_t)nw & 0xFFu) << 8 |
                                ((uint32_t)oh & 0xFFu) << 16 | ((uint32_t)ct & 0xFFu) << 24;
    n->word[NEO_GVU(G_POOL1)] = ((uint32_t)(nw * s) & 0x3FFu) | ((uint32_t)(nw * hp) & 0x3FFu) << 16;
    n->word[NEO_G_TILE_LIM] = (uint32_t)(c / ct - 1);
    n->word[NEO_V0_BTT] = (uint32_t)wp;
    n->word[NEO_V0_STRIDE] = (uint32_t)(nw * 32);
    n->word[NEO_V0_REP] = (uint32_t)(ct * hp - 1);
    n->word[NEO_V0_MOVE] = (uint32_t)(ct * hp * wp);
    n->word[NEO_V2_BTT] = (uint32_t)ow;
    n->word[NEO_V2_STRIDE] = (uint32_t)(now * 32);
    n->word[NEO_V2_REP] = (uint32_t)(ct * oh - 1);
    n->word[NEO_V2_MOVE] = (uint32_t)(ct * oh * ow);
    n->word[NEO_START_V0] = p->in;
    n->word[NEO_START_V2] = p->out;
    n->out = p->out;
    n->out_bytes = (uint32_t)(c * oh * ow);
    return 1;
}

/* ResidualHw::map (has/rtl_ref_gvu_residual_hw.h), round half up, zero point first. */
typedef struct {
    int32_t sa, sb;
    int64_t bias;
    int sh, z;
} residual_hw_t;

static int residual_map(int zpA, int zpB, int zpO, int64_t SA, int64_t SB, int64_t SO, int sA, int sB, int sO,
                        residual_hw_t* out)
{
    if (sO < 0 || sO > 63 || sA < 0 || sB < 0) return 0;
    if (SA < 0 || SB < 0 || SO < 0 || SO > 0x7FFFFFFF || SA > 0x7FFFFFFF || SB > 0x7FFFFFFF) return 0;
    if (sA > 0 && sA < 31 && (SA & (((int64_t)1 << sA) - 1)) == 0) {
        SA >>= sA;
        sA = 0;
    }
    if (sB > 0 && sB < 31 && (SB & (((int64_t)1 << sB) - 1)) == 0) {
        SB >>= sB;
        sB = 0;
    }
    if (sA != 0 || sB != 0) return 0;
    if (SO > 0) {
        int t = 0;
        while (t < sO && (SO & ((int64_t)1 << t)) == 0) t++;
        SO >>= t;
        sO -= t;
    }
    if (SA > 4000000 || SB > 4000000) return 0;
    if (zpO < -128 || zpO > 127) return 0;
    const int64_t sa = SA * SO, sb = SB * SO;
    if (sa > 0x7FFFFFFF || sb > 0x7FFFFFFF) return 0;
    if (SO > 0 && (SA > 0x7FFFFFFF / SO || SB > 0x7FFFFFFF / SO)) return 0;
    out->sa = (int32_t)sa;
    out->sb = (int32_t)sb;
    out->bias = (sO > 0 ? ((int64_t)1 << (sO - 1)) : 0) - ((int64_t)zpA * SA + (int64_t)zpB * SB) * SO;
    out->sh = sO;
    out->z = zpO;
    return 1;
}

int fx1_npu_neo_residual(fx1_npu_neo_t* n, const fx1_npu_add_t* a)
{
    neo_clear(n);
    if (!a->n || (a->n % 32u) != 0) return 0;
    residual_hw_t hp;
    if (!residual_map(a->zp_a, a->zp_b, a->zp_o, a->sa, a->sb, a->so, a->sha, a->shb, a->sho, &hp)) return 0;
    const int rows = cdiv((int)a->n, 32);
    int tr = 1;
    for (int t = rows < NEO_VBANK0_ROWS ? rows : NEO_VBANK0_ROWS; t >= 1; t--)
        if (rows % t == 0) {
            tr = t;
            break;
        }
    const uint32_t T = (uint32_t)tr * 32u;
    n->word[NEO_OPCODE] = NEO_OP_RESIDUAL;
    n->word[NEO_GVU(G_OPCODE)] = GVU_RESIDUAL;
    n->word[NEO_G_TILE_LIM] = (uint32_t)(rows / tr - 1);
    n->word[NEO_V0_BTT] = n->word[NEO_V0_MOVE] = n->word[NEO_V1_BTT] = n->word[NEO_V1_MOVE] = n->word[NEO_V2_BTT] =
        n->word[NEO_V2_MOVE] = T;
    n->word[NEO_START_V0] = a->a;
    n->word[NEO_START_V1] = a->b;
    n->word[NEO_START_V2] = a->out;
    n->word[NEO_GVU(G_RES0)] = (uint32_t)tr | ((uint32_t)hp.z & 0xFFu) << 16 | ((uint32_t)hp.sh & 0x3Fu) << 24;
    n->word[NEO_GVU(G_RES1)] = (uint32_t)hp.sa;
    n->word[NEO_GVU(G_RES2)] = (uint32_t)hp.sb;
    n->word[NEO_GVU(G_RES3)] = (uint32_t)(uint64_t)hp.bias;
    n->word[NEO_GVU(G_RES4)] = (uint32_t)((uint64_t)hp.bias >> 32);
    n->out = a->out;
    n->out_bytes = a->n;
    return 1;
}

const char* fx1_npu_status_name(fx1_npu_status_t s)
{
    switch (s) {
    case FX1_NPU_OK: return "OK";
    case FX1_NPU_E_ABSENT: return "ABSENT";
    case FX1_NPU_E_ABI: return "ABI";
    case FX1_NPU_E_FEATURE: return "FEATURE";
    case FX1_NPU_E_ARG: return "ARG";
    case FX1_NPU_E_TIMEOUT: return "TIMEOUT";
    case FX1_NPU_E_REFUSED: return "REFUSED";
    case FX1_NPU_E_JOB: return "JOB";
    case FX1_NPU_E_UNAVAILABLE: return "UNAVAILABLE";
    case FX1_NPU_E_NOT_IDLE: return "NOT_IDLE";
    case FX1_NPU_PENDING: return "PENDING";
    case FX1_NPU_E_BUSY: return "BUSY";
    case FX1_NPU_E_NOT_OWNER: return "NOT_OWNER";
    }
    return "?";
}
