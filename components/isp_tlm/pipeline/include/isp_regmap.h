#pragma once

#include <cstdint>

namespace cdc::components {

// ── Global ISP register map ──────────────────────────────────────────────────

constexpr std::uint32_t REG_ISP_ENABLE     = 0x000;
constexpr std::uint32_t REG_STATUS          = 0x004;
constexpr std::uint32_t REG_TRIGGER         = 0x008;
constexpr std::uint32_t REG_WIDTH           = 0x00C;
constexpr std::uint32_t REG_HEIGHT          = 0x010;
constexpr std::uint32_t REG_BIT_DEPTH        = 0x014;
constexpr std::uint32_t REG_BAYER_PATTERN   = 0x018;

constexpr std::uint32_t REG_BLC_ENABLE      = 0x020;
constexpr std::uint32_t REG_BLC_R_OFFSET    = 0x024;
constexpr std::uint32_t REG_BLC_GR_OFFSET   = 0x028;
constexpr std::uint32_t REG_BLC_GB_OFFSET   = 0x02C;
constexpr std::uint32_t REG_BLC_B_OFFSET    = 0x030;

constexpr std::uint32_t REG_DPC_ENABLE      = 0x040;
constexpr std::uint32_t REG_DPC_THRESH      = 0x044;

constexpr std::uint32_t REG_LSC_ENABLE      = 0x050;
constexpr std::uint32_t REG_LSC_GRID_W      = 0x054;
constexpr std::uint32_t REG_LSC_GRID_H      = 0x058;

constexpr std::uint32_t REG_DG_ENABLE       = 0x060;
constexpr std::uint32_t REG_DG_GAIN         = 0x064;

constexpr std::uint32_t REG_BNR_ENABLE      = 0x070;
constexpr std::uint32_t REG_BNR_WINDOW      = 0x074;

constexpr std::uint32_t REG_DEMOSAIC_ENABLE = 0x080;

constexpr std::uint32_t REG_AWB_ENABLE      = 0x085;
constexpr std::uint32_t REG_AWB_ALGORITHM   = 0x086;
constexpr std::uint32_t REG_AWB_R_GAIN      = 0x087;
constexpr std::uint32_t REG_AWB_B_GAIN      = 0x088;

constexpr std::uint32_t REG_WB_ENABLE       = 0x090;
constexpr std::uint32_t REG_WB_R_GAIN       = 0x094;
constexpr std::uint32_t REG_WB_B_GAIN       = 0x098;

constexpr std::uint32_t REG_CCM_ENABLE      = 0x0A0;
constexpr std::uint32_t REG_CCM_MATRIX00    = 0x0A4;
constexpr std::uint32_t REG_CCM_MATRIX01    = 0x0A8;
constexpr std::uint32_t REG_CCM_MATRIX02    = 0x0AC;
constexpr std::uint32_t REG_CCM_MATRIX10    = 0x0B0;
constexpr std::uint32_t REG_CCM_MATRIX11    = 0x0B4;
constexpr std::uint32_t REG_CCM_MATRIX12    = 0x0B8;
constexpr std::uint32_t REG_CCM_MATRIX20    = 0x0BC;
constexpr std::uint32_t REG_CCM_MATRIX21    = 0x0C0;
constexpr std::uint32_t REG_CCM_MATRIX22    = 0x0C4;

constexpr std::uint32_t REG_GC_ENABLE       = 0x0D0;

constexpr std::uint32_t REG_CSC_ENABLE      = 0x0DC;
constexpr std::uint32_t REG_CSC_STANDARD     = 0x0E0;

constexpr std::uint32_t REG_CSE_ENABLE      = 0x0F0;
constexpr std::uint32_t REG_CSE_SAT_GAIN    = 0x0F4;

constexpr std::uint32_t REG_SHARPEN_ENABLE  = 0x100;
constexpr std::uint32_t REG_SHARPEN_SIGMA   = 0x104;
constexpr std::uint32_t REG_SHARPEN_STRENGTH = 0x108;

constexpr std::uint32_t REG_2DNR_ENABLE     = 0x110;
constexpr std::uint32_t REG_2DNR_WINDOW     = 0x114;
constexpr std::uint32_t REG_2DNR_PATCH      = 0x118;
constexpr std::uint32_t REG_2DNR_WTS        = 0x11C;

constexpr std::uint32_t REG_SCALE_ENABLE    = 0x120;
constexpr std::uint32_t REG_SCALE_OUT_W     = 0x124;
constexpr std::uint32_t REG_SCALE_OUT_H     = 0x128;

constexpr std::uint32_t REG_YUV420_ENABLE   = 0x130;

constexpr std::uint32_t REG_LUT_ADDR        = 0x200;
constexpr std::uint32_t REG_LUT_DATA        = 0x204;

constexpr std::uint32_t REG_RAW_FRAME_ADDR  = 0x300;
constexpr std::uint32_t REG_YUV_FRAME_ADDR  = 0x304;

constexpr std::uint32_t REG_MAX             = 0x400;

// ── Status flags ──────────────────────────────────────────────────────────────

constexpr std::uint32_t STATUS_DONE = 0x01;
constexpr std::uint32_t STATUS_BUSY = 0x02;

} // namespace cdc::components
