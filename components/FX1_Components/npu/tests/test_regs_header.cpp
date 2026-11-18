// SPDX-License-Identifier: Apache-2.0
//
// The C firmware header (fx1_npu_regs.h) must match the wrapper's C++ register
// definitions (fx1_npu_regmap.h). Every comparison is a static_assert: a drift
// fails the build of this test. The run itself only prints PASS.
#include "fx1_npu/fx1_npu_regs.h"
#include "fx1_npu/fx1_npu_regmap.h"
#include <cstdio>

namespace r = fx1::npu::reg;
using e = r::error_code;
using re = r::rich_error_code;
constexpr std::uint32_t ev(e v) { return static_cast<std::uint32_t>(v); }
constexpr std::uint32_t rv(re v) { return static_cast<std::uint32_t>(v); }
constexpr std::uint32_t alias(std::uint32_t model) { return FX1_NPU_RICH_OF_MODEL(model); }

// Same-name constants.
static_assert(FX1_NPU_MMIO_SIZE == r::MMIO_SIZE, "MMIO_SIZE");
static_assert(FX1_NPU_WRAPPER_BASE == r::WRAPPER_BASE, "WRAPPER_BASE");
static_assert(FX1_NPU_WRAPPER_WINDOW_SIZE == r::WRAPPER_WINDOW_SIZE, "WRAPPER_WINDOW_SIZE");
static_assert(FX1_NPU_RICH_ALIAS_BASE == r::RICH_ALIAS_BASE, "RICH_ALIAS_BASE");
static_assert(FX1_NPU_RICH_ALIAS_SIZE == r::RICH_ALIAS_SIZE, "RICH_ALIAS_SIZE");
static_assert(FX1_NPU_MODEL_RICH_BASE == r::MODEL_RICH_BASE, "MODEL_RICH_BASE");
static_assert(FX1_NPU_CTRL == r::CTRL, "CTRL");
static_assert(FX1_NPU_STATUS == r::STATUS, "STATUS");
static_assert(FX1_NPU_IRQ_ENABLE == r::IRQ_ENABLE, "IRQ_ENABLE");
static_assert(FX1_NPU_IRQ_STATUS == r::IRQ_STATUS, "IRQ_STATUS");
static_assert(FX1_NPU_SRC_ADDR == r::SRC_ADDR, "SRC_ADDR");
static_assert(FX1_NPU_DST_ADDR == r::DST_ADDR, "DST_ADDR");
static_assert(FX1_NPU_SRC_SIZE_BYTES == r::SRC_SIZE_BYTES, "SRC_SIZE_BYTES");
static_assert(FX1_NPU_DST_SIZE_BYTES == r::DST_SIZE_BYTES, "DST_SIZE_BYTES");
static_assert(FX1_NPU_WIDTH == r::WIDTH, "WIDTH");
static_assert(FX1_NPU_HEIGHT == r::HEIGHT, "HEIGHT");
static_assert(FX1_NPU_FORMAT == r::FORMAT, "FORMAT");
static_assert(FX1_NPU_OP_MODE == r::OP_MODE, "OP_MODE");
static_assert(FX1_NPU_WEIGHTS_ADDR == r::WEIGHTS_ADDR, "WEIGHTS_ADDR");
static_assert(FX1_NPU_WEIGHTS_SIZE_BYTES == r::WEIGHTS_SIZE_BYTES, "WEIGHTS_SIZE_BYTES");
static_assert(FX1_NPU_K_DIMENSION == r::K_DIMENSION, "K_DIMENSION");
static_assert(FX1_NPU_CYCLE_COUNT == r::CYCLE_COUNT, "CYCLE_COUNT");
static_assert(FX1_NPU_BYTES_READ == r::BYTES_READ, "BYTES_READ");
static_assert(FX1_NPU_BYTES_WRITTEN == r::BYTES_WRITTEN, "BYTES_WRITTEN");
static_assert(FX1_NPU_LAST_ERROR == r::LAST_ERROR, "LAST_ERROR");
static_assert(FX1_NPU_CORE_ID == r::CORE_ID, "CORE_ID");
static_assert(FX1_NPU_RICH_WINDOW_BASE == r::RICH_WINDOW_BASE, "RICH_WINDOW_BASE");
static_assert(FX1_NPU_RICH_WINDOW_SIZE == r::RICH_WINDOW_SIZE, "RICH_WINDOW_SIZE");
static_assert(FX1_NPU_PERF_EXEC_CYCLES == r::PERF_EXEC_CYCLES, "PERF_EXEC_CYCLES");
static_assert(FX1_NPU_PERF_MAC_OPS == r::PERF_MAC_OPS, "PERF_MAC_OPS");
static_assert(FX1_NPU_PERF_TOTAL_CYCLES == r::PERF_TOTAL_CYCLES, "PERF_TOTAL_CYCLES");
static_assert(FX1_NPU_PERF_DDR_READ_BYTES == r::PERF_DDR_READ_BYTES, "PERF_DDR_READ_BYTES");
static_assert(FX1_NPU_PERF_DDR_WRITE_BYTES == r::PERF_DDR_WRITE_BYTES, "PERF_DDR_WRITE_BYTES");
static_assert(FX1_NPU_CTRL_ENABLE == r::CTRL_ENABLE, "CTRL_ENABLE");
static_assert(FX1_NPU_CTRL_START == r::CTRL_START, "CTRL_START");
static_assert(FX1_NPU_CTRL_SOFT_RESET == r::CTRL_SOFT_RESET, "CTRL_SOFT_RESET");
static_assert(FX1_NPU_CTRL_IRQ_EN == r::CTRL_IRQ_EN, "CTRL_IRQ_EN");
static_assert(FX1_NPU_STATUS_BUSY == r::STATUS_BUSY, "STATUS_BUSY");
static_assert(FX1_NPU_STATUS_DONE == r::STATUS_DONE, "STATUS_DONE");
static_assert(FX1_NPU_STATUS_ERROR == r::STATUS_ERROR, "STATUS_ERROR");
static_assert(FX1_NPU_STATUS_IDLE == r::STATUS_IDLE, "STATUS_IDLE");
static_assert(FX1_NPU_IRQ_DONE == r::IRQ_DONE, "IRQ_DONE");
static_assert(FX1_NPU_IRQ_ERROR == r::IRQ_ERROR, "IRQ_ERROR");
static_assert(FX1_NPU_FORMAT_INT8_INT8_INT32 == r::FORMAT_INT8_INT8_INT32, "FORMAT_INT8_INT8_INT32");
static_assert(FX1_NPU_OP_GEMM == r::OP_GEMM, "OP_GEMM");
static_assert(FX1_NPU_CORE_ID_VALUE == r::CORE_ID_VALUE, "CORE_ID_VALUE");
static_assert(FX1_NPU_VP_ID == r::VP_ID, "VP_ID");
static_assert(FX1_NPU_VP_ABI == r::VP_ABI, "VP_ABI");
static_assert(FX1_NPU_VP_MODEL_HASH == r::VP_MODEL_HASH, "VP_MODEL_HASH");
static_assert(FX1_NPU_VP_FEATURE == r::VP_FEATURE, "VP_FEATURE");
static_assert(FX1_NPU_VP_QUEUE_DEPTH == r::VP_QUEUE_DEPTH, "VP_QUEUE_DEPTH");
static_assert(FX1_NPU_VP_QUEUE_FREE == r::VP_QUEUE_FREE, "VP_QUEUE_FREE");
static_assert(FX1_NPU_VP_CMD == r::VP_CMD, "VP_CMD");
static_assert(FX1_NPU_VP_STATUS == r::VP_STATUS, "VP_STATUS");
static_assert(FX1_NPU_VP_ERROR == r::VP_ERROR, "VP_ERROR");
static_assert(FX1_NPU_VP_ERROR_INFO == r::VP_ERROR_INFO, "VP_ERROR_INFO");
static_assert(FX1_NPU_VP_PUSH_WAIT_NS == r::VP_PUSH_WAIT_NS, "VP_PUSH_WAIT_NS");
static_assert(FX1_NPU_VP_ACCEPTED == r::VP_ACCEPTED, "VP_ACCEPTED");
static_assert(FX1_NPU_VP_ABORT_DROPPED == r::VP_ABORT_DROPPED, "VP_ABORT_DROPPED");
static_assert(FX1_NPU_VP_WINDOW_MAX == r::VP_WINDOW_MAX, "VP_WINDOW_MAX");
static_assert(FX1_NPU_VP_ID_VALUE == r::VP_ID_VALUE, "VP_ID_VALUE");
static_assert(FX1_NPU_VP_ABI_VALUE == r::VP_ABI_VALUE, "VP_ABI_VALUE");
static_assert(FX1_NPU_VP_CMD_ABORT == r::VP_CMD_ABORT, "VP_CMD_ABORT");
static_assert(FX1_NPU_VP_CMD_CLEAR_ERROR == r::VP_CMD_CLEAR_ERROR, "VP_CMD_CLEAR_ERROR");
static_assert(FX1_NPU_VP_STATUS_QUIESCENT == r::VP_STATUS_QUIESCENT, "VP_STATUS_QUIESCENT");
static_assert(FX1_NPU_VP_STATUS_ENGINE_BUSY == r::VP_STATUS_ENGINE_BUSY, "VP_STATUS_ENGINE_BUSY");
static_assert(FX1_NPU_VP_STATUS_ABORT_ACTIVE == r::VP_STATUS_ABORT_ACTIVE, "VP_STATUS_ABORT_ACTIVE");
static_assert(FX1_NPU_VP_STATUS_COPYBACK_ACTIVE == r::VP_STATUS_COPYBACK_ACTIVE, "VP_STATUS_COPYBACK_ACTIVE");
static_assert(FX1_NPU_VP_STATUS_NATIVE_BUSY == r::VP_STATUS_NATIVE_BUSY, "VP_STATUS_NATIVE_BUSY");
static_assert(FX1_NPU_VP_STATUS_QUEUED_SHIFT == r::VP_STATUS_QUEUED_SHIFT, "VP_STATUS_QUEUED_SHIFT");
static_assert(FX1_NPU_VP_STATUS_QUEUED_MASK == r::VP_STATUS_QUEUED_MASK, "VP_STATUS_QUEUED_MASK");
static_assert(FX1_NPU_VP_ERROR_PUSH_TIMEOUT == r::VP_ERROR_PUSH_TIMEOUT, "VP_ERROR_PUSH_TIMEOUT");
static_assert(FX1_NPU_VP_ERROR_BAD_ADDR == r::VP_ERROR_BAD_ADDR, "VP_ERROR_BAD_ADDR");
static_assert(FX1_NPU_VP_ERROR_ABORTED == r::VP_ERROR_ABORTED, "VP_ERROR_ABORTED");
static_assert(FX1_NPU_VP_ERROR_DMA == r::VP_ERROR_DMA, "VP_ERROR_DMA");
static_assert(FX1_NPU_VP_ERROR_REFUSED == r::VP_ERROR_REFUSED, "VP_ERROR_REFUSED");
static_assert(FX1_NPU_VP_ERROR_UNSUPPORTED == r::VP_ERROR_UNSUPPORTED, "VP_ERROR_UNSUPPORTED");
static_assert(FX1_NPU_VP_ERROR_MODEL == r::VP_ERROR_MODEL, "VP_ERROR_MODEL");
static_assert(FX1_NPU_VP_ERROR_BUSY == r::VP_ERROR_BUSY, "VP_ERROR_BUSY");
static_assert(FX1_NPU_VP_ERROR_ALL == r::VP_ERROR_ALL, "VP_ERROR_ALL");
static_assert(FX1_NPU_ELEM_ADD == r::RICH_ELEM_ADD, "ELEM_ADD");
static_assert(FX1_NPU_ELEM_MAX_POOL == r::RICH_ELEM_MAX_POOL, "ELEM_MAX_POOL");
static_assert(FX1_NPU_ELEM_AVG_POOL == r::RICH_ELEM_AVG_POOL, "ELEM_AVG_POOL");
static_assert(FX1_NPU_ACT_NONE == r::RICH_ACT_NONE, "ACT_NONE");
static_assert(FX1_NPU_ACT_RELU == r::RICH_ACT_RELU, "ACT_RELU");
static_assert(FX1_NPU_RICH_STATUS_BUSY == r::RICH_STATUS_BUSY, "RICH_STATUS_BUSY");
static_assert(FX1_NPU_RICH_STATUS_QUEUE_FULL == r::RICH_STATUS_QUEUE_FULL, "RICH_STATUS_QUEUE_FULL");
static_assert(FX1_NPU_RICH_STATUS_ERROR == r::RICH_STATUS_ERROR, "RICH_STATUS_ERROR");
static_assert(FX1_NPU_RICH_ERR_VP_BAD_ADDR == r::RICH_ERR_VP_BAD_ADDR, "RICH_ERR_VP_BAD_ADDR");
static_assert(FX1_NPU_RICH_ERR_VP_UNSUPPORTED == r::RICH_ERR_VP_UNSUPPORTED, "RICH_ERR_VP_UNSUPPORTED");
static_assert(FX1_NPU_RICH_ERR_VP_BUSY == r::RICH_ERR_VP_BUSY, "RICH_ERR_VP_BUSY");
static_assert(FX1_NPU_RICH_ERR_VP_MODEL == r::RICH_ERR_VP_MODEL, "RICH_ERR_VP_MODEL");
static_assert(FX1_NPU_RICH_ERR_VP_DMA == r::RICH_ERR_VP_DMA, "RICH_ERR_VP_DMA");
static_assert(FX1_NPU_RICH_ERR_VP_ABORTED == r::RICH_ERR_VP_ABORTED, "RICH_ERR_VP_ABORTED");
static_assert(FX1_NPU_NEO_CTRL_OFF == r::NEO_CTRL_OFF, "NEO_CTRL_OFF");
static_assert(FX1_NPU_NEO_IEN_OFF == r::NEO_IEN_OFF, "NEO_IEN_OFF");
static_assert(FX1_NPU_NEO_ISTATUS_OFF == r::NEO_ISTATUS_OFF, "NEO_ISTATUS_OFF");
static_assert(FX1_NPU_NEO_REG_OFF == r::NEO_REG_OFF, "NEO_REG_OFF");
static_assert(FX1_NPU_NEO_REG_COUNT == r::NEO_REG_COUNT, "NEO_REG_COUNT");
static_assert(FX1_NPU_NATIVE_CONTROL == r::NATIVE_CONTROL, "NATIVE_CONTROL");
static_assert(FX1_NPU_NATIVE_CFG_PROFILE == r::NATIVE_CFG_PROFILE, "NATIVE_CFG_PROFILE");
static_assert(FX1_NPU_NATIVE_SRAMA_OFFSET == r::NATIVE_SRAMA_OFFSET, "NATIVE_SRAMA_OFFSET");
static_assert(FX1_NPU_NATIVE_SRAMB_OFFSET == r::NATIVE_SRAMB_OFFSET, "NATIVE_SRAMB_OFFSET");
static_assert(FX1_NPU_NATIVE_SRAMC_OFFSET == r::NATIVE_SRAMC_OFFSET, "NATIVE_SRAMC_OFFSET");
// Error codes.
static_assert(FX1_NPU_ERR_NONE == ev(e::none) && FX1_NPU_ERR_DISABLED == ev(e::disabled) && FX1_NPU_ERR_BUSY == ev(e::busy), "ERR 0..2");
static_assert(FX1_NPU_ERR_INVALID_DIMENSIONS == ev(e::invalid_dimensions) && FX1_NPU_ERR_INVALID_FORMAT == ev(e::invalid_format), "ERR 3..4");
static_assert(FX1_NPU_ERR_INVALID_OPERATION == ev(e::invalid_operation) && FX1_NPU_ERR_INVALID_ADDRESS == ev(e::invalid_address), "ERR 5..6");
static_assert(FX1_NPU_ERR_INVALID_SIZE == ev(e::invalid_size) && FX1_NPU_ERR_DMA_READ == ev(e::dma_read), "ERR 7..8");
static_assert(FX1_NPU_ERR_DMA_WRITE == ev(e::dma_write) && FX1_NPU_ERR_CORE_DEADLOCK == ev(e::core_deadlock), "ERR 9..10");
static_assert(FX1_NPU_ERR_CORE_TIMEOUT == ev(e::core_timeout) && FX1_NPU_ERR_RESET_ABORTED == ev(e::reset_aborted), "ERR 11..12");
static_assert(FX1_NPU_RICH_ERR_Q_OVF == rv(re::queue_overflow) && FX1_NPU_RICH_ERR_LANE_B == rv(re::lane_b), "RICH ERR 1..2");
static_assert(FX1_NPU_RICH_ERR_LEGACY64 == rv(re::legacy_64bit) && FX1_NPU_RICH_ERR_UNSUPPORTED_OP == rv(re::unsupported_opcode), "RICH ERR 3..4");
static_assert(FX1_NPU_RICH_ERR_UNSUPPORTED_MODE == rv(re::unsupported_mode) && FX1_NPU_RICH_ERR_BROADCAST == rv(re::broadcast), "RICH ERR 5..6");
static_assert(FX1_NPU_RICH_ERR_NEED_LUT == rv(re::need_lut) && FX1_NPU_RICH_ERR_BAD_GEOM == rv(re::bad_geometry), "RICH ERR 7..8");
static_assert(FX1_NPU_RICH_ERR_SCALE_RANGE == rv(re::scale_range) && FX1_NPU_RICH_ERR_NO_TILING == rv(re::no_tiling), "RICH ERR 9..10");

// Feature bits.
static_assert(FX1_NPU_FEATURE_NATIVE_GEMM == r::VP_FEATURE_NATIVE_GEMM && FX1_NPU_FEATURE_RICH_HAS == r::VP_FEATURE_RICH_HAS, "FEATURE 0..1");
static_assert(FX1_NPU_FEATURE_DFC_WINDOW == r::VP_FEATURE_DFC_WINDOW && FX1_NPU_FEATURE_DMA_TENSOR == r::VP_FEATURE_DMA_TENSOR, "FEATURE 2..3");
static_assert(FX1_NPU_FEATURE_ABORT == r::VP_FEATURE_ABORT && FX1_NPU_FEATURE_BOUNDED_PUSH == r::VP_FEATURE_BOUNDED_PUSH, "FEATURE 4..5");
static_assert(FX1_NPU_FEATURE_QUEUE == r::VP_FEATURE_QUEUE && FX1_NPU_FEATURE_ATTN_LN == r::VP_FEATURE_ATTN_LN, "FEATURE 6..7");
static_assert(FX1_NPU_VP_ABI_MAJOR(FX1_NPU_VP_ABI_VALUE) == 1u, "ABI 1.x");

// Opcodes.
static_assert(FX1_NPU_OP_GEMM_FUSED == r::RICH_OPCODE_GEMM_FUSED && FX1_NPU_OP_FUSED_ATTN == r::RICH_OPCODE_FUSED_ATTN, "OP");
static_assert(FX1_NPU_OP_LAYERNORM == r::RICH_OPCODE_LAYERNORM && FX1_NPU_OP_ELEM_WISE == r::RICH_OPCODE_ELEM_WISE, "OP");
static_assert(FX1_NPU_OP_NEO_DOORBELL == r::NEO_DOORBELL_OPCODE && FX1_NPU_OP_DMA_TENSOR == 0x16u, "OP");

// Model host addresses and their aperture aliases.
static_assert(FX1_NPU_M_PUSH_A == r::MODEL_PUSH_A && FX1_NPU_M_PUSH_B == r::MODEL_PUSH_B, "PUSH");
static_assert(FX1_NPU_M_STATUS == r::MODEL_STATUS && FX1_NPU_M_RETIRED == r::MODEL_RETIRED, "STATUS/RETIRED");
static_assert(FX1_NPU_M_PARAM_FIRST == r::MODEL_PARAM_LO && FX1_NPU_M_PARAM_LAST == r::MODEL_PARAM_HI, "PARAM range");
static_assert(FX1_NPU_M_EXT_FIRST == r::MODEL_EXT_LO && FX1_NPU_M_EXT_END == r::MODEL_EXT_END, "EXT range");
static_assert(FX1_NPU_M_NEO_BASE == r::MODEL_NEO_BASE && FX1_NPU_M_NEO_MARKER == r::MODEL_NEO_MARKER, "NEO");
static_assert(alias(FX1_NPU_M_PUSH_A) == r::RICH_PUSH_A && alias(FX1_NPU_M_STATUS) == r::RICH_STATUS &&
              alias(FX1_NPU_M_RETIRED) == r::RICH_RETIRED, "aliases");
static_assert(alias(FX1_NPU_M_NEO_BASE) == r::RICH_NEO_BASE, "neo alias");
static_assert(alias(FX1_NPU_M_IN_ADDR) == r::RICH_IN_ADDR && alias(FX1_NPU_M_W_ADDR) == r::RICH_WEIGHT_ADDR &&
              alias(FX1_NPU_M_OUT_ADDR) == r::RICH_OUT_ADDR && alias(FX1_NPU_M_BIAS_ADDR) == r::RICH_BIAS_ADDR, "param");
static_assert(alias(FX1_NPU_M_M) == r::RICH_M && alias(FX1_NPU_M_K) == r::RICH_K && alias(FX1_NPU_M_N) == r::RICH_N, "param");
static_assert(alias(FX1_NPU_M_KERNEL_H) == r::RICH_KERNEL_H && alias(FX1_NPU_M_KERNEL_W) == r::RICH_KERNEL_W &&
              alias(FX1_NPU_M_STRIDE) == r::RICH_STRIDE && alias(FX1_NPU_M_PADDING) == r::RICH_PADDING, "param");
static_assert(alias(FX1_NPU_M_ACT_TYPE) == r::RICH_ACT_TYPE && alias(FX1_NPU_M_HAS_SKIP) == r::RICH_HAS_SKIP &&
              alias(FX1_NPU_M_SKIP_ADDR) == r::RICH_SKIP_ADDR, "param");
static_assert(alias(FX1_NPU_M_A_ADDR) == r::RICH_Q_GAMMA_A_ADDR && alias(FX1_NPU_M_B_ADDR) == r::RICH_K_B_ADDR &&
              alias(FX1_NPU_M_SEQ_LEN) == r::RICH_SEQ_LEN && alias(FX1_NPU_M_MODE) == r::RICH_HEADS_DIM_MODE, "param");
static_assert(alias(FX1_NPU_M_A_LEN) == r::RICH_A_LEN && alias(FX1_NPU_M_B_LEN) == r::RICH_B_LEN, "param");
static_assert(alias(FX1_NPU_M_IN_C) == r::RICH_IN_C && alias(FX1_NPU_M_IN_H) == r::RICH_IN_H && alias(FX1_NPU_M_IN_W) == r::RICH_IN_W, "ext");
static_assert(alias(FX1_NPU_M_OUT_C) == r::RICH_OUT_C && alias(FX1_NPU_M_OUT_H) == r::RICH_OUT_H && alias(FX1_NPU_M_OUT_W) == r::RICH_OUT_W, "ext");
static_assert(alias(FX1_NPU_M_TILE_COUT) == r::RICH_TILE_COUT && alias(FX1_NPU_M_TILE_H) == r::RICH_TILE_H &&
              alias(FX1_NPU_M_TILE_W) == r::RICH_TILE_W, "ext");
static_assert(alias(FX1_NPU_M_SCALE_ADDR) == r::RICH_SCALE_ADDR && alias(FX1_NPU_M_SHIFT_ADDR) == r::RICH_SHIFT_ADDR &&
              alias(FX1_NPU_M_LUT_ADDR) == r::RICH_LUT_ADDR && alias(FX1_NPU_M_ZP_OUT) == r::RICH_ZP_OUT &&
              alias(FX1_NPU_M_FLAGS) == r::RICH_FLAGS, "ext");
static_assert(alias(FX1_NPU_M_ZP_A) == r::RICH_ZP_A && alias(FX1_NPU_M_ZP_B) == r::RICH_ZP_B && alias(FX1_NPU_M_ZP_O) == r::RICH_ZP_O, "ext");
static_assert(alias(FX1_NPU_M_SA) == r::RICH_SA && alias(FX1_NPU_M_SHA) == r::RICH_SHA && alias(FX1_NPU_M_SB) == r::RICH_SB &&
              alias(FX1_NPU_M_SHB) == r::RICH_SHB && alias(FX1_NPU_M_SO) == r::RICH_SO && alias(FX1_NPU_M_SHO) == r::RICH_SHO, "ext");
static_assert(alias(FX1_NPU_M_POOL_K) == r::RICH_POOL_K && alias(FX1_NPU_M_POOL_P) == r::RICH_POOL_P &&
              alias(FX1_NPU_M_TILE_CIN) == r::RICH_TILE_CIN && alias(FX1_NPU_M_Y_USED) == r::RICH_Y_USED, "ext");
static_assert(FX1_NPU_NEO_CTRL_START == 1u && FX1_NPU_NEO_CTRL_DONE == 2u, "neo CONTROL bits (has_host_neo_port.h)");
static_assert(FX1_NPU_GEMM_ROWS == 32u && FX1_NPU_GEMM_COLS == 32u, "GEMM bank 32 x 32");
static_assert(FX1_NPU_BANK(FX1_NPU_VP_ID) == r::WRAPPER_BASE + r::VP_ID, "FX1_NPU_BANK");

int main()
{
    std::printf("fx1_npu_regs.h matches fx1_npu_regmap.h (checked at compile time)\nRESULT PASS\n");
    return 0;
}
