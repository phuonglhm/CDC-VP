/*
 * Independent C oracle of the NPU arithmetic used by the FX1 NPU firmware
 * tests. It follows the documented arithmetic of the drop's numpy reference
 * (tools/has/ref_program.py, "Arithmetic (default configuration of the
 * model)"), not the model's C++. The host test fx1_fw_npu_oracle_xcheck runs
 * the same cases through ref_program.py and compares byte for byte.
 * Integer only (no floating point); compiles for the hart and the host.
 */
#ifndef NPU_ORACLE_H
#define NPU_ORACLE_H

#include <stdint.h>

#include "fx1_npu.h"

/* C[32][32] int32 = A[32][K] x W[K][32] (GEMM bank). */
void npu_oracle_gemm_bank(const int8_t* a, const int8_t* w, uint32_t k, int32_t* c);
/* Inputs read through `mem(addr)` style pointers: the caller passes host
 * pointers for every operand of the descriptor (addresses are ignored). */
void npu_oracle_conv(const fx1_npu_conv_t* c, const int8_t* in, const int8_t* w, const int32_t* bias, const int32_t* scale,
                     const int32_t* shift, const int8_t* skip, int8_t* out);
void npu_oracle_pool(const fx1_npu_pool_t* p, const int8_t* in, int8_t* out);
void npu_oracle_add(const fx1_npu_add_t* a, const int8_t* x, const int8_t* y, int8_t* out);

#endif
