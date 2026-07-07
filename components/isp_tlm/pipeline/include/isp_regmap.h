#pragma once

#include <cstdint>

namespace cdc::components {

//  Global/Common ISP register map (0x0000 - 0x00FF)
constexpr std::uint32_t REG_CTRL = 0x0000;
constexpr std::uint32_t REG_STATUS = 0x0004;
constexpr std::uint32_t REG_IRQ_ENABLE = 0x0008;
constexpr std::uint32_t REG_IRQ_STATUS = 0x000C;

//  Buffer Descriptors (0x0100 - 0x01FF)
constexpr std::uint32_t REG_SRC_ADDR = 0x0100;
constexpr std::uint32_t REG_DST_ADDR = 0x0104;
constexpr std::uint32_t REG_SCRATCH_ADDR = 0x0108;
constexpr std::uint32_t REG_SRC_SIZE_BYTES = 0x010C;
constexpr std::uint32_t REG_DST_SIZE_BYTES = 0x0110;
constexpr std::uint32_t REG_WEIGHTS_ADDR = 0x0114;
constexpr std::uint32_t REG_PARAM_ADDR = 0x0118;

//  Global Parameters (0x0020 - 0x0050)
constexpr std::uint32_t REG_WIDTH = 0x0024;
constexpr std::uint32_t REG_HEIGHT = 0x0028;
constexpr std::uint32_t REG_STRIDE = 0x002C;
constexpr std::uint32_t REG_FORMAT = 0x0030;
constexpr std::uint32_t REG_OP_MODE = 0x0034;
constexpr std::uint32_t REG_BIT_DEPTH = 0x0040;
constexpr std::uint32_t REG_BAYER_PATTERN = 0x0044;

// BLC (Black Level Correction)
constexpr std::uint32_t REG_BLC_ENABLE = 0x1000;
constexpr std::uint32_t REG_BLC_LINEAR = 0x1004;
constexpr std::uint32_t REG_BLC_R_OFFSET = 0x1008;
constexpr std::uint32_t REG_BLC_GR_OFFSET = 0x100C;
constexpr std::uint32_t REG_BLC_GB_OFFSET = 0x1010;
constexpr std::uint32_t REG_BLC_B_OFFSET = 0x1014;
constexpr std::uint32_t REG_BLC_R_SAT = 0x1018;
constexpr std::uint32_t REG_BLC_GR_SAT = 0x101C;
constexpr std::uint32_t REG_BLC_GB_SAT = 0x1020;
constexpr std::uint32_t REG_BLC_B_SAT = 0x1024;

// DPC (Defect Pixel Correction)
constexpr std::uint32_t REG_DPC_ENABLE = 0x1080;
constexpr std::uint32_t REG_DPC_THRESH = 0x1084;

// LSC (Lens Shading Correction)
constexpr std::uint32_t REG_LSC_ENABLE = 0x1100;
constexpr std::uint32_t REG_LSC_GRID_W = 0x1104;
constexpr std::uint32_t REG_LSC_GRID_H = 0x1108;

// DG (Digital Gain)
constexpr std::uint32_t REG_DG_ENABLE = 0x1180;
constexpr std::uint32_t REG_DG_GAIN = 0x1184;
constexpr std::uint32_t REG_DG_AUTO = 0x1188;

// BNR (Bayer Noise Reduction)
constexpr std::uint32_t REG_BNR_ENABLE = 0x1200;
constexpr std::uint32_t REG_BNR_WINDOW = 0x1204;

// Demosaic
constexpr std::uint32_t REG_DEMOSAIC_ENABLE = 0x1280;

// AWB (Auto White Balance)
constexpr std::uint32_t REG_AWB_ENABLE = 0x1300;
constexpr std::uint32_t REG_AWB_ALGORITHM = 0x1304;
constexpr std::uint32_t REG_AWB_R_GAIN = 0x1308;
constexpr std::uint32_t REG_AWB_B_GAIN = 0x130C;
constexpr std::uint32_t REG_AWB_UNDER_PCT = 0x1310;
constexpr std::uint32_t REG_AWB_OVER_PCT = 0x1314;
constexpr std::uint32_t REG_AWB_PERCENT = 0x1318;

// WB (White Balance)
constexpr std::uint32_t REG_WB_ENABLE = 0x1380;
constexpr std::uint32_t REG_WB_R_GAIN = 0x1384;
constexpr std::uint32_t REG_WB_B_GAIN = 0x1388;

// CCM (Color Correction Matrix)
constexpr std::uint32_t REG_CCM_ENABLE = 0x1400;
constexpr std::uint32_t REG_CCM_MATRIX00 = 0x1404;
constexpr std::uint32_t REG_CCM_MATRIX01 = 0x1408;
constexpr std::uint32_t REG_CCM_MATRIX02 = 0x140C;
constexpr std::uint32_t REG_CCM_MATRIX10 = 0x1410;
constexpr std::uint32_t REG_CCM_MATRIX11 = 0x1414;
constexpr std::uint32_t REG_CCM_MATRIX12 = 0x1418;
constexpr std::uint32_t REG_CCM_MATRIX20 = 0x141C;
constexpr std::uint32_t REG_CCM_MATRIX21 = 0x1420;
constexpr std::uint32_t REG_CCM_MATRIX22 = 0x1424;

// GC (Gamma Correction)
constexpr std::uint32_t REG_GC_ENABLE    = 0x1480;
constexpr std::uint32_t REG_GC_GAMMA     = 0x1484;
constexpr std::uint32_t REG_GC_LUT_ADDR  = 0x1488;
constexpr std::uint32_t REG_GC_LUT_DATA  = 0x148C;

// AEC (Auto Exposure Control)
constexpr std::uint32_t REG_AEC_ENABLE = 0x1500;
constexpr std::uint32_t REG_AEC_FEEDBACK = 0x1504;
constexpr std::uint32_t REG_AEC_CENTER_ILLUM = 0x1508;
constexpr std::uint32_t REG_AEC_SKEWNESS = 0x150C;

// CSC (Color Space Conversion)
constexpr std::uint32_t REG_CSC_ENABLE = 0x1580;
constexpr std::uint32_t REG_CSC_STANDARD = 0x1584;

// CSE (Color Saturation Enhancement)
constexpr std::uint32_t REG_CSE_ENABLE = 0x1600;
constexpr std::uint32_t REG_CSE_SAT_GAIN = 0x1604;

// Sharpen
constexpr std::uint32_t REG_SHARPEN_ENABLE = 0x1680;
constexpr std::uint32_t REG_SHARPEN_SIGMA = 0x1684;
constexpr std::uint32_t REG_SHARPEN_STRENGTH = 0x1688;

// 2DNR
constexpr std::uint32_t REG_2DNR_ENABLE = 0x1700;
constexpr std::uint32_t REG_2DNR_WINDOW = 0x1704;
constexpr std::uint32_t REG_2DNR_PATCH = 0x1708;
constexpr std::uint32_t REG_2DNR_WTS = 0x170C;

// Scale
constexpr std::uint32_t REG_SCALE_ENABLE = 0x1780;
constexpr std::uint32_t REG_SCALE_OUT_W = 0x1784;
constexpr std::uint32_t REG_SCALE_OUT_H = 0x1788;

// YUV420
constexpr std::uint32_t REG_YUV420_ENABLE = 0x1800;

// max offset
constexpr std::uint32_t REG_MAX = 0x10000; // 64 KiB window

// Status flags
constexpr std::uint32_t STATUS_DONE = 0x01;
constexpr std::uint32_t STATUS_BUSY = 0x02;
constexpr std::uint32_t STATUS_ERROR = 0x04;
constexpr std::uint32_t STATUS_IDLE = 0x08;

} // namespace cdc::components
