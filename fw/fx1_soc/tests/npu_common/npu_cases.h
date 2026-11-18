/*
 * NPU firmware test cases shared by the hart tests (npu_rich, npu_dfc,
 * npu_errors) and the host cross-check (fx1_fw_npu_oracle_xcheck): the same
 * descriptors, the same deterministic data, the same oracle.
 *
 * Window layout (offsets from the window base, NPU_CASE_WINDOW bytes):
 *   rich chain (lane A, HasNpuTop), each output feeds a later instruction:
 *     R1 conv 3x3 s1 pad 1  [4][8][8]    -> [8][8][8], ReLU, per-channel requant
 *     R2 conv 1x1 residual  R1 out       -> [8][8][8], + skip tensor
 *     R3 MAX_POOL 3 p1 s2   R2 out       -> [8][4][4]
 *     R4 AVG_POOL 2 s2      R1 out       -> [8][4][4]
 *     R5 ADD                R3 out, R4 out -> [128]
 *   DFC (neo_lite register file, strict):
 *     D1 MAX_POOL 3 s1 p0   [8][10][10]  -> [8][8][8]
 *     D2 ADD as RESIDUAL    [256] + [256] -> [256]
 *     D3 AVG_POOL 2 s2 p0   (the strict hardware refuses it: A-H3)
 * The shapes are small because the harts run every check (and the oracle)
 * out of DDR; the host cross-check covers the same arithmetic.
 * Every byte of the window that is no output starts with a known value and
 * must keep it (stray-write check).
 */
#ifndef NPU_CASES_H
#define NPU_CASES_H

#include <stdint.h>

#include "fx1_npu.h"

#define NPU_CASE_WINDOW 0x00002000u
#define NPU_CASE_SEED 0x5EED0001u

enum { NPU_R1, NPU_R2, NPU_R3, NPU_R4, NPU_R5, NPU_RICH_COUNT };
enum { NPU_D1, NPU_D2, NPU_D3, NPU_DFC_COUNT };

typedef struct {
    fx1_npu_conv_t r1, r2;
    fx1_npu_pool_t r3, r4;
    fx1_npu_add_t r5;
    fx1_npu_instr_t instr[NPU_RICH_COUNT];
    fx1_npu_pool_t d1, d3;
    fx1_npu_add_t d2;
    fx1_npu_neo_t neo[NPU_DFC_COUNT];
    int neo_ok[NPU_DFC_COUNT];
    const char* label[NPU_RICH_COUNT];
    const char* dfc_label[NPU_DFC_COUNT];
} npu_cases_t;

/* Descriptors and instructions for a window at bus address `base`. 1 when every
 * lane-A instruction was built. */
int npu_cases_build(npu_cases_t* t, uint32_t base);
/* Initial content of the window image (`img` = NPU_CASE_WINDOW bytes). */
void npu_cases_fill(uint8_t* img);
/* Oracle: applies R1..R5 in order (rich), or D1..D2 (DFC), to the image. */
void npu_cases_apply_rich(const npu_cases_t* t, uint8_t* img, uint32_t base);
void npu_cases_apply_dfc(const npu_cases_t* t, uint8_t* img, uint32_t base);

#endif
