/* Shared NPU firmware test cases (npu_cases.h). */
#include "npu_cases.h"

#include "npu_oracle.h"

/* Offsets inside the window. */
#define O_R1_IN 0x0400u    /* [4][8][8]        256 B */
#define O_R1_W 0x0500u     /* [4][3][3][8]     288 B */
#define O_R1_BIAS 0x0640u  /* int32 [8] */
#define O_R1_SCALE 0x0680u
#define O_R1_SHIFT 0x06C0u
#define O_R1_OUT 0x0700u   /* [8][8][8]        512 B */
#define O_R2_W 0x0900u     /* [8][1][1][8]      64 B */
#define O_R2_BIAS 0x0940u
#define O_R2_SCALE 0x0980u
#define O_R2_SHIFT 0x09C0u
#define O_R2_SKIP 0x0A00u  /* [8][8][8] */
#define O_R2_OUT 0x0C00u
#define O_R3_OUT 0x0E00u   /* [8][4][4]        128 B */
#define O_R4_OUT 0x0E80u
#define O_R5_OUT 0x0F00u   /* [128] */
#define O_D1_IN 0x1000u    /* [8][10][10]      800 B */
#define O_D1_OUT 0x1400u   /* [8][8][8] */
#define O_D2_A 0x1600u     /* [256] */
#define O_D2_B 0x1700u
#define O_D2_OUT 0x1800u
#define O_D3_OUT 0x1900u   /* [8][5][5] (refused) */

static uint32_t rng_state;
static uint32_t rng(void)
{
    uint32_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return rng_state = x;
}
static int32_t rnd(int32_t lo, int32_t hi) { return lo + (int32_t)(rng() % (uint32_t)(hi - lo + 1)); }

static void put32(uint8_t* img, uint32_t off, int32_t v)
{
    const uint32_t u = (uint32_t)v;
    img[off] = (uint8_t)u;
    img[off + 1] = (uint8_t)(u >> 8);
    img[off + 2] = (uint8_t)(u >> 16);
    img[off + 3] = (uint8_t)(u >> 24);
}
static void fill8(uint8_t* img, uint32_t off, uint32_t n, int32_t lo, int32_t hi)
{
    for (uint32_t i = 0; i < n; ++i) img[off + i] = (uint8_t)(int8_t)rnd(lo, hi);
}

int npu_cases_build(npu_cases_t* t, uint32_t base)
{
    int ok = 1;
    t->r1 = (fx1_npu_conv_t){.in = base + O_R1_IN, .w = base + O_R1_W, .out = base + O_R1_OUT, .bias = base + O_R1_BIAS,
                             .scale = base + O_R1_SCALE, .shift = base + O_R1_SHIFT, .skip = 0, .cin = 4, .ih = 8,
                             .iw = 8, .cout = 8, .kh = 3, .kw = 3, .stride = 1, .pad = 1, .act = FX1_NPU_ACT_RELU,
                             .zp_out = 3};
    t->r2 = (fx1_npu_conv_t){.in = base + O_R1_OUT, .w = base + O_R2_W, .out = base + O_R2_OUT, .bias = base + O_R2_BIAS,
                             .scale = base + O_R2_SCALE, .shift = base + O_R2_SHIFT, .skip = base + O_R2_SKIP, .cin = 8,
                             .ih = 8, .iw = 8, .cout = 8, .kh = 1, .kw = 1, .stride = 1, .pad = 0,
                             .act = FX1_NPU_ACT_NONE, .zp_out = -2};
    t->r3 = (fx1_npu_pool_t){.in = base + O_R2_OUT, .out = base + O_R3_OUT, .c = 8, .h = 8, .w = 8, .k = 3, .pad = 1,
                             .stride = 2, .avg = 0};
    t->r4 = (fx1_npu_pool_t){.in = base + O_R1_OUT, .out = base + O_R4_OUT, .c = 8, .h = 8, .w = 8, .k = 2, .pad = 0,
                             .stride = 2, .avg = 1, .so = 1, .sho = 2};
    t->r5 = (fx1_npu_add_t){.a = base + O_R3_OUT, .b = base + O_R4_OUT, .out = base + O_R5_OUT, .n = 8 * 4 * 4,
                            .zp_a = 1, .zp_b = -2, .zp_o = 3, .sa = 12000, .sha = 4, .sb = 9000, .shb = 3, .so = 3,
                            .sho = 12};
    ok &= fx1_npu_build_conv(&t->instr[NPU_R1], &t->r1);
    ok &= fx1_npu_build_conv(&t->instr[NPU_R2], &t->r2);
    ok &= fx1_npu_build_pool(&t->instr[NPU_R3], &t->r3);
    ok &= fx1_npu_build_pool(&t->instr[NPU_R4], &t->r4);
    ok &= fx1_npu_build_add(&t->instr[NPU_R5], &t->r5);
    t->label[NPU_R1] = "R1 conv 3x3 ReLU";
    t->label[NPU_R2] = "R2 conv 1x1 residual";
    t->label[NPU_R3] = "R3 MAX_POOL 3 p1 s2";
    t->label[NPU_R4] = "R4 AVG_POOL 2 s2";
    t->label[NPU_R5] = "R5 ADD";

    t->d1 = (fx1_npu_pool_t){.in = base + O_D1_IN, .out = base + O_D1_OUT, .c = 8, .h = 10, .w = 10, .k = 3, .pad = 0,
                             .stride = 1, .avg = 0};
    /* SO odd and no per-operand shift: expressible as RESIDUAL (sa, sb, bias, sh). */
    t->d2 = (fx1_npu_add_t){.a = base + O_D2_A, .b = base + O_D2_B, .out = base + O_D2_OUT, .n = 256, .zp_a = 3,
                            .zp_b = -5, .zp_o = 2, .sa = 3000, .sha = 0, .sb = 5000, .shb = 0, .so = 1, .sho = 14};
    t->d3 = (fx1_npu_pool_t){.in = base + O_D1_IN, .out = base + O_D3_OUT, .c = 8, .h = 10, .w = 10, .k = 2, .pad = 0,
                             .stride = 2, .avg = 1, .so = 1, .sho = 2};
    t->neo_ok[NPU_D1] = fx1_npu_neo_pool(&t->neo[NPU_D1], &t->d1);
    t->neo_ok[NPU_D2] = fx1_npu_neo_residual(&t->neo[NPU_D2], &t->d2);
    t->neo_ok[NPU_D3] = fx1_npu_neo_pool(&t->neo[NPU_D3], &t->d3);
    ok &= t->neo_ok[NPU_D1] & t->neo_ok[NPU_D2] & t->neo_ok[NPU_D3];
    t->dfc_label[NPU_D1] = "D1 neo MAX_POOL 3 s1";
    t->dfc_label[NPU_D2] = "D2 neo RESIDUAL (ADD)";
    t->dfc_label[NPU_D3] = "D3 neo AVG_POOL (strict refusal)";
    return ok;
}

void npu_cases_fill(uint8_t* img)
{
    rng_state = NPU_CASE_SEED;
    /* Background: a fixed pattern every output differs from somewhere. */
    for (uint32_t i = 0; i < NPU_CASE_WINDOW; ++i) img[i] = (uint8_t)(0xA5u ^ (i * 7u) ^ (i >> 8));
    fill8(img, O_R1_IN, 4 * 8 * 8, -128, 127);
    fill8(img, O_R1_W, 4 * 9 * 8, -8, 8);
    for (uint32_t c = 0; c < 8; ++c) {
        put32(img, O_R1_BIAS + 4 * c, rnd(-2000, 2000));
        put32(img, O_R1_SCALE + 4 * c, rnd(1500, 2500));
        put32(img, O_R1_SHIFT + 4 * c, 16);
        put32(img, O_R2_BIAS + 4 * c, rnd(-500, 500));
        put32(img, O_R2_SCALE + 4 * c, rnd(1200, 2800));
        put32(img, O_R2_SHIFT + 4 * c, 16);
    }
    fill8(img, O_R2_W, 8 * 8, -16, 16);
    fill8(img, O_R2_SKIP, 8 * 8 * 8, -64, 64);
    fill8(img, O_D1_IN, 8 * 10 * 10, -128, 127);
    fill8(img, O_D2_A, 256, -128, 127);
    fill8(img, O_D2_B, 256, -128, 127);
}

#define P8(off) ((int8_t*)(img + (off)))
#define C8(off) ((const int8_t*)(img + (off)))
#define C32(off) ((const int32_t*)(const void*)(img + (off)))

void npu_cases_apply_rich(const npu_cases_t* t, uint8_t* img, uint32_t base)
{
    (void)base;
    npu_oracle_conv(&t->r1, C8(O_R1_IN), C8(O_R1_W), C32(O_R1_BIAS), C32(O_R1_SCALE), C32(O_R1_SHIFT), 0, P8(O_R1_OUT));
    npu_oracle_conv(&t->r2, C8(O_R1_OUT), C8(O_R2_W), C32(O_R2_BIAS), C32(O_R2_SCALE), C32(O_R2_SHIFT), C8(O_R2_SKIP),
                    P8(O_R2_OUT));
    npu_oracle_pool(&t->r3, C8(O_R2_OUT), P8(O_R3_OUT));
    npu_oracle_pool(&t->r4, C8(O_R1_OUT), P8(O_R4_OUT));
    npu_oracle_add(&t->r5, C8(O_R3_OUT), C8(O_R4_OUT), P8(O_R5_OUT));
}

void npu_cases_apply_dfc(const npu_cases_t* t, uint8_t* img, uint32_t base)
{
    (void)base;
    npu_oracle_pool(&t->d1, C8(O_D1_IN), P8(O_D1_OUT));
    npu_oracle_add(&t->d2, C8(O_D2_A), C8(O_D2_B), P8(O_D2_OUT));
}
