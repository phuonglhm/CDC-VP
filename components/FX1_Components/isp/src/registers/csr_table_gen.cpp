// SPDX-License-Identifier: Apache-2.0
// FX1 ISP register table -- GENERATED FILE, DO NOT EDIT.
// Generator: tools/gen_csr.py v1.0, source SHA-256 0980b61ded06a386f0cc64e0445f93fd85b0b1f967d37f0cd41869c0753cd6bf

#include "registers/csr_desc.h"

namespace cdc::components::fx1_isp::csr {
namespace {

constexpr field_desc fields_0[] = {  // COMMON_VER_DATE
   {"year", 0, 16, 0x0000FFFFu, access::ro, 0x2026u, true, 3},
   {"month", 16, 8, 0x00FF0000u, access::ro, 0x7u, true, 4},
   {"day", 24, 8, 0xFF000000u, access::ro, 0x18u, true, 5},
};
constexpr field_desc fields_1[] = {  // COMMON_VER_ID
   {"version", 0, 32, 0xFFFFFFFFu, access::ro, 0x10002u, true, 7},
};
constexpr field_desc fields_2[] = {  // COMMON_CTRL
   {"isp_en", 0, 1, 0x00000001u, access::rw, 0x0u, true, 9},
   {"soft_rst", 1, 1, 0x00000002u, access::w1sc, 0x0u, true, 10},
   {"frame_start", 2, 1, 0x00000004u, access::w1s, 0x0u, true, 11},
};
constexpr field_desc fields_3[] = {  // COMMON_STATUS
   {"busy", 0, 1, 0x00000001u, access::ro, 0x0u, true, 13},
   {"frame_done", 1, 1, 0x00000002u, access::ro, 0x0u, true, 14},
   {"error", 2, 1, 0x00000004u, access::ro, 0x0u, true, 15},
};
constexpr field_desc fields_4[] = {  // COMMON_IRQ_STATUS
   {"frame_done_irq", 0, 1, 0x00000001u, access::w1c, 0x0u, true, 17},
   {"error_irq", 1, 1, 0x00000002u, access::w1c, 0x0u, true, 18},
   {"stats_ready_irq", 2, 1, 0x00000004u, access::w1c, 0x0u, true, 19},
};
constexpr field_desc fields_5[] = {  // COMMON_IRQ_EN
   {"frame_done_en", 0, 1, 0x00000001u, access::rw, 0x0u, true, 21},
   {"error_en", 1, 1, 0x00000002u, access::rw, 0x0u, true, 22},
   {"stats_ready_en", 2, 1, 0x00000004u, access::rw, 0x0u, true, 23},
};
constexpr field_desc fields_6[] = {  // COMMON_FRAME_WIDTH
   {"h_active", 0, 16, 0x0000FFFFu, access::rw, 0x780u, true, 25},
};
constexpr field_desc fields_7[] = {  // COMMON_FRAME_HEIGHT
   {"v_active", 0, 16, 0x0000FFFFu, access::rw, 0x438u, true, 27},
};
constexpr field_desc fields_8[] = {  // COMMON_BAYER
   {"pattern", 0, 2, 0x00000003u, access::rw, 0x0u, true, 29},
};
constexpr field_desc fields_9[] = {  // COMMON_SCRATCH
   {"scratch", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 31},
};
constexpr field_desc fields_10[] = {  // BLC_CTRL
   {"en", 0, 1, 0x00000001u, access::rw, 0x0u, true, 33},
};
constexpr field_desc fields_11[] = {  // BLC_GAIN_SEL
   {"gain_level", 0, 2, 0x00000003u, access::rw, 0x0u, true, 35},
   {"range_scale_en", 8, 1, 0x00000100u, access::rw, 0x1u, true, 36},
};
constexpr field_desc fields_12[] = {  // BLC_OFS_G0_DFT
   {"ofs_g0_dft", 0, 12, 0x00000FFFu, access::rw, 0xC8u, true, 38},
};
constexpr field_desc fields_13[] = {  // BLC_OFS_G0_R
   {"ofs_g0_r", 0, 12, 0x00000FFFu, access::rw, 0x0u, true, 40},
};
constexpr field_desc fields_14[] = {  // BLC_OFS_G0_GR
   {"ofs_g0_gr", 0, 12, 0x00000FFFu, access::rw, 0x0u, true, 42},
};
constexpr field_desc fields_15[] = {  // BLC_OFS_G0_GB
   {"ofs_g0_gb", 0, 12, 0x00000FFFu, access::rw, 0x0u, true, 44},
};
constexpr field_desc fields_16[] = {  // BLC_OFS_G0_B
   {"ofs_g0_b", 0, 12, 0x00000FFFu, access::rw, 0x0u, true, 46},
};
constexpr field_desc fields_17[] = {  // BLC_OFS_G1_DFT
   {"ofs_g1_dft", 0, 12, 0x00000FFFu, access::rw, 0xC8u, true, 48},
};
constexpr field_desc fields_18[] = {  // BLC_OFS_G1_R
   {"ofs_g1_r", 0, 12, 0x00000FFFu, access::rw, 0x0u, true, 50},
};
constexpr field_desc fields_19[] = {  // BLC_OFS_G1_GR
   {"ofs_g1_gr", 0, 12, 0x00000FFFu, access::rw, 0x0u, true, 52},
};
constexpr field_desc fields_20[] = {  // BLC_OFS_G1_GB
   {"ofs_g1_gb", 0, 12, 0x00000FFFu, access::rw, 0x0u, true, 54},
};
constexpr field_desc fields_21[] = {  // BLC_OFS_G1_B
   {"ofs_g1_b", 0, 12, 0x00000FFFu, access::rw, 0x0u, true, 56},
};
constexpr field_desc fields_22[] = {  // BLC_OFS_G2_DFT
   {"ofs_g2_dft", 0, 12, 0x00000FFFu, access::rw, 0xC8u, true, 58},
};
constexpr field_desc fields_23[] = {  // BLC_OFS_G2_R
   {"ofs_g2_r", 0, 12, 0x00000FFFu, access::rw, 0x0u, true, 60},
};
constexpr field_desc fields_24[] = {  // BLC_OFS_G2_GR
   {"ofs_g2_gr", 0, 12, 0x00000FFFu, access::rw, 0x0u, true, 62},
};
constexpr field_desc fields_25[] = {  // BLC_OFS_G2_GB
   {"ofs_g2_gb", 0, 12, 0x00000FFFu, access::rw, 0x0u, true, 64},
};
constexpr field_desc fields_26[] = {  // BLC_OFS_G2_B
   {"ofs_g2_b", 0, 12, 0x00000FFFu, access::rw, 0x0u, true, 66},
};
constexpr field_desc fields_27[] = {  // BLC_OFS_G3_DFT
   {"ofs_g3_dft", 0, 12, 0x00000FFFu, access::rw, 0xC8u, true, 68},
};
constexpr field_desc fields_28[] = {  // BLC_OFS_G3_R
   {"ofs_g3_r", 0, 12, 0x00000FFFu, access::rw, 0x0u, true, 70},
};
constexpr field_desc fields_29[] = {  // BLC_OFS_G3_GR
   {"ofs_g3_gr", 0, 12, 0x00000FFFu, access::rw, 0x0u, true, 72},
};
constexpr field_desc fields_30[] = {  // BLC_OFS_G3_GB
   {"ofs_g3_gb", 0, 12, 0x00000FFFu, access::rw, 0x0u, true, 74},
};
constexpr field_desc fields_31[] = {  // BLC_OFS_G3_B
   {"ofs_g3_b", 0, 12, 0x00000FFFu, access::rw, 0x0u, true, 76},
};
constexpr field_desc fields_32[] = {  // BLC_SCALE_G0
   {"scale_g0", 0, 18, 0x0003FFFFu, access::rw, 0x10000u, true, 78},
};
constexpr field_desc fields_33[] = {  // BLC_SCALE_G1
   {"scale_g1", 0, 18, 0x0003FFFFu, access::rw, 0x10000u, true, 80},
};
constexpr field_desc fields_34[] = {  // BLC_SCALE_G2
   {"scale_g2", 0, 18, 0x0003FFFFu, access::rw, 0x10000u, true, 82},
};
constexpr field_desc fields_35[] = {  // BLC_SCALE_G3
   {"scale_g3", 0, 18, 0x0003FFFFu, access::rw, 0x10000u, true, 84},
};
constexpr field_desc fields_36[] = {  // LSC_CTRL
   {"en", 0, 1, 0x00000001u, access::rw, 0x0u, true, 86},
};
constexpr field_desc fields_37[] = {  // LSC_MESH_NODES
   {"mesh_nx", 0, 8, 0x000000FFu, access::rw, 0x0u, true, 88},
   {"mesh_ny", 8, 8, 0x0000FF00u, access::rw, 0x0u, true, 89},
};
constexpr field_desc fields_38[] = {  // LSC_STRENGTH
   {"strength", 0, 17, 0x0001FFFFu, access::rw, 0x0u, true, 91},
};
constexpr field_desc fields_39[] = {  // LSC_PROFILE_SEL
   {"profile_sel", 0, 2, 0x00000003u, access::rw, 0x0u, true, 93},
};
constexpr field_desc fields_40[] = {  // LSC_LOAD_CTRL
   {"load_profile", 0, 2, 0x00000003u, access::rw, 0x0u, true, 95},
   {"load_begin", 8, 1, 0x00000100u, access::w1s, 0x0u, true, 96},
   {"load_validate", 9, 1, 0x00000200u, access::w1s, 0x0u, true, 97},
   {"load_abort", 10, 1, 0x00000400u, access::w1s, 0x0u, true, 98},
};
constexpr field_desc fields_41[] = {  // LSC_COEF_DATA
   {"coef_data", 0, 21, 0x001FFFFFu, access::rw, 0x0u, true, 100},
};
constexpr field_desc fields_42[] = {  // LSC_LOAD_STATUS
   {"load_addr", 0, 16, 0x0000FFFFu, access::ro, 0x0u, true, 102},
   {"load_full", 16, 1, 0x00010000u, access::ro, 0x0u, true, 103},
   {"load_busy", 17, 1, 0x00020000u, access::ro, 0x0u, true, 104},
   {"load_profile", 18, 2, 0x000C0000u, access::ro, 0x0u, true, 105},
};
constexpr field_desc fields_43[] = {  // LSC_PROFILE_STATUS
   {"profile_valid", 0, 3, 0x00000007u, access::ro, 0x0u, true, 107},
   {"active_profile", 4, 2, 0x00000030u, access::ro, 0x0u, true, 108},
   {"active_valid", 6, 1, 0x00000040u, access::ro, 0x0u, true, 109},
   {"geometry_valid", 7, 1, 0x00000080u, access::ro, 0x0u, true, 110},
};
constexpr field_desc fields_44[] = {  // LSC_ERROR
   {"active_load_reject", 0, 1, 0x00000001u, access::w1c, 0x0u, true, 112},
   {"profile_sel_reject", 1, 1, 0x00000002u, access::w1c, 0x0u, true, 113},
   {"coef_range", 2, 1, 0x00000004u, access::w1c, 0x0u, true, 114},
   {"coef_count", 3, 1, 0x00000008u, access::w1c, 0x0u, true, 115},
   {"geometry_invalid", 4, 1, 0x00000010u, access::w1c, 0x0u, true, 116},
   {"load_protocol", 5, 1, 0x00000020u, access::w1c, 0x0u, true, 117},
};
constexpr field_desc fields_45[] = {  // LSC_STAT_OVF_CNT_R
   {"ovf_cnt_r", 0, 21, 0x001FFFFFu, access::ro, 0x0u, true, 119},
};
constexpr field_desc fields_46[] = {  // LSC_STAT_OVF_CNT_GR
   {"ovf_cnt_gr", 0, 21, 0x001FFFFFu, access::ro, 0x0u, true, 121},
};
constexpr field_desc fields_47[] = {  // LSC_STAT_OVF_CNT_GB
   {"ovf_cnt_gb", 0, 21, 0x001FFFFFu, access::ro, 0x0u, true, 123},
};
constexpr field_desc fields_48[] = {  // LSC_STAT_OVF_CNT_B
   {"ovf_cnt_b", 0, 21, 0x001FFFFFu, access::ro, 0x0u, true, 125},
};
constexpr field_desc fields_49[] = {  // BPC_CTRL
   {"en", 0, 1, 0x00000001u, access::rw, 0x0u, true, 127},
};
constexpr field_desc fields_50[] = {  // BPC_MODE
   {"dynamic_det_en", 0, 1, 0x00000001u, access::rw, 0x1u, true, 129},
};
constexpr field_desc fields_51[] = {  // BPC_TEMPORAL_VAR
   {"temporal_var", 0, 16, 0x0000FFFFu, access::rw, 0x100u, true, 131},
};
constexpr field_desc fields_52[] = {  // BPC_THRESH
   {"thresh_k", 16, 16, 0xFFFF0000u, access::rw, 0x4Du, true, 133},
   {"thresh_floor", 0, 16, 0x0000FFFFu, access::rw, 0x192u, true, 134},
};
constexpr field_desc fields_53[] = {  // BPC_PIXEL_AGE
   {"pixel_age", 0, 16, 0x0000FFFFu, access::rw, 0x4B0u, true, 136},
};
constexpr field_desc fields_54[] = {  // BPC_NUM_CANDIDATES
   {"candidates", 0, 14, 0x00003FFFu, access::ro, 0x0u, true, 138},
};
constexpr field_desc fields_55[] = {  // BPC_NUM_DEFECTIVE
   {"defective", 0, 14, 0x00003FFFu, access::ro, 0x0u, true, 140},
};
constexpr field_desc fields_56[] = {  // BPC_STATUS
   {"cand_reject_ovf", 0, 1, 0x00000001u, access::w1c, 0x0u, true, 142},
};
constexpr field_desc fields_57[] = {  // WB_CTRL
   {"en", 0, 1, 0x00000001u, access::rw, 0x0u, true, 144},
};
constexpr field_desc fields_58[] = {  // WB_GAIN_R
   {"gain_r", 0, 12, 0x00000FFFu, access::rw, 0x100u, true, 146},
};
constexpr field_desc fields_59[] = {  // WB_GAIN_G
   {"gain_g", 0, 12, 0x00000FFFu, access::rw, 0x100u, true, 148},
};
constexpr field_desc fields_60[] = {  // WB_GAIN_B
   {"gain_b", 0, 12, 0x00000FFFu, access::rw, 0x100u, true, 150},
};
constexpr field_desc fields_61[] = {  // DG_CTRL
   {"en", 0, 1, 0x00000001u, access::rw, 0x0u, true, 152},
};
constexpr field_desc fields_62[] = {  // DG_GAIN
   {"gain", 0, 12, 0x00000FFFu, access::rw, 0x100u, true, 154},
};
constexpr field_desc fields_63[] = {  // D_WDR_CTRL
   {"en", 0, 1, 0x00000001u, access::rw, 0x0u, false, 156},
};
constexpr field_desc fields_64[] = {  // D_WDR_STRENGTH
   {"strength", 0, 8, 0x000000FFu, access::rw, 0x0u, false, 158},
};
constexpr field_desc fields_65[] = {  // DEMOSAIC_STATUS
   {"busy", 0, 1, 0x00000001u, access::ro, 0x0u, true, 160},
};
constexpr field_desc fields_66[] = {  // CCM_CTRL
   {"en", 0, 1, 0x00000001u, access::rw, 0x0u, true, 162},
   {"updated", 2, 1, 0x00000004u, access::rw, 0x0u, true, 163},
};
constexpr field_desc fields_67[] = {  // CCM_CRR
   {"coefrr", 0, 12, 0x00000FFFu, access::rw, 0x200u, true, 165},
};
constexpr field_desc fields_68[] = {  // CCM_CRG
   {"coefrg", 0, 12, 0x00000FFFu, access::rw, 0x0u, true, 167},
};
constexpr field_desc fields_69[] = {  // CCM_CRB
   {"coefrb", 0, 12, 0x00000FFFu, access::rw, 0x0u, true, 169},
};
constexpr field_desc fields_70[] = {  // CCM_CGR
   {"coefgr", 0, 12, 0x00000FFFu, access::rw, 0x0u, true, 171},
};
constexpr field_desc fields_71[] = {  // CCM_CGG
   {"coefgg", 0, 12, 0x00000FFFu, access::rw, 0x200u, true, 173},
};
constexpr field_desc fields_72[] = {  // CCM_CGB
   {"coefgb", 0, 12, 0x00000FFFu, access::rw, 0x0u, true, 175},
};
constexpr field_desc fields_73[] = {  // CCM_CBR
   {"coefbr", 0, 12, 0x00000FFFu, access::rw, 0x0u, true, 177},
};
constexpr field_desc fields_74[] = {  // CCM_CBG
   {"coefbg", 0, 12, 0x00000FFFu, access::rw, 0x0u, true, 179},
};
constexpr field_desc fields_75[] = {  // CCM_CBB
   {"coefbb", 0, 12, 0x00000FFFu, access::rw, 0x200u, true, 181},
};
constexpr field_desc fields_76[] = {  // CCM_OFS_R
   {"offset_r", 0, 12, 0x00000FFFu, access::rw, 0x0u, true, 183},
};
constexpr field_desc fields_77[] = {  // CCM_OFS_G
   {"offset_g", 0, 12, 0x00000FFFu, access::rw, 0x0u, true, 185},
};
constexpr field_desc fields_78[] = {  // CCM_OFS_B
   {"offset_b", 0, 12, 0x00000FFFu, access::rw, 0x0u, true, 187},
};
constexpr field_desc fields_79[] = {  // CCM_STATUS
   {"busy", 0, 1, 0x00000001u, access::ro, 0x0u, true, 189},
};
constexpr field_desc fields_80[] = {  // GAMMA_CTRL
   {"en", 0, 1, 0x00000001u, access::rw, 0x0u, true, 191},
   {"gamma_value", 4, 3, 0x00000070u, access::rw, 0x0u, false, 192},
};
constexpr field_desc fields_81[] = {  // GAMMA_LUT_ADDR
   {"addr", 0, 8, 0x000000FFu, access::rw, 0x0u, true, 194},
};
constexpr field_desc fields_82[] = {  // GAMMA_LUT_DATA
   {"data", 0, 12, 0x00000FFFu, access::rw, 0x0u, true, 196},
};
constexpr field_desc fields_83[] = {  // GAMMA_LUT_RDATA
   {"rdata", 0, 12, 0x00000FFFu, access::ro, 0x0u, true, 198},
};
constexpr field_desc fields_84[] = {  // GAMMA_STATUS
   {"busy", 0, 1, 0x00000001u, access::ro, 0x0u, true, 200},
};
constexpr field_desc fields_85[] = {  // CSC_CTRL
   {"std", 0, 1, 0x00000001u, access::rw, 0x0u, true, 202},
};
constexpr field_desc fields_86[] = {  // GTM_CTRL
   {"en", 0, 1, 0x00000001u, access::rw, 0x0u, true, 204},
   {"manual", 1, 1, 0x00000002u, access::rw, 0x0u, true, 205},
};
constexpr field_desc fields_87[] = {  // GTM_LUT_ADDR
   {"addr", 0, 7, 0x0000007Fu, access::rw, 0x0u, true, 207},
};
constexpr field_desc fields_88[] = {  // GTM_LUT_DATA
   {"data", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 209},
};
constexpr field_desc fields_89[] = {  // GTM_KEY
   {"key", 0, 8, 0x000000FFu, access::rw, 0x2Eu, true, 211},
};
constexpr field_desc fields_90[] = {  // GTM_LWHITE
   {"lwhite", 0, 16, 0x0000FFFFu, access::rw, 0x400u, true, 213},
};
constexpr field_desc fields_91[] = {  // GTM_ROI_LOG2
   {"roi_log2", 0, 5, 0x0000001Fu, access::rw, 0xBu, true, 215},
};
constexpr field_desc fields_92[] = {  // GTM_LUT_RDATA
   {"rdata", 0, 16, 0x0000FFFFu, access::ro, 0x0u, true, 217},
};
constexpr field_desc fields_93[] = {  // GTM_STATUS
   {"busy", 0, 1, 0x00000001u, access::ro, 0x0u, true, 219},
};
constexpr field_desc fields_94[] = {  // NR_2D_CTRL
   {"en", 0, 1, 0x00000001u, access::rw, 0x0u, true, 221},
};
constexpr field_desc fields_95[] = {  // NR_2D_EST_VAR
   {"est_var", 0, 16, 0x0000FFFFu, access::ro, 0x0u, true, 223},
};
constexpr field_desc fields_96[] = {  // NR_2D_STATUS
   {"busy", 0, 1, 0x00000001u, access::ro, 0x0u, true, 225},
};
constexpr field_desc fields_97[] = {  // TNR_3D_CTRL
   {"en", 0, 1, 0x00000001u, access::rw, 0x0u, false, 227},
};
constexpr field_desc fields_98[] = {  // TNR_3D_MTF_SEL
   {"mtf", 0, 3, 0x00000007u, access::rw, 0x0u, false, 229},
};
constexpr field_desc fields_99[] = {  // TNR_3D_HIST_BASE
   {"hist_base_addr", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, false, 231},
};
constexpr field_desc fields_100[] = {  // EE_CTRL
   {"en", 0, 1, 0x00000001u, access::rw, 0x0u, true, 233},
   {"reg_update", 1, 1, 0x00000002u, access::rw, 0x0u, false, 234},
   {"profile_sel", 2, 2, 0x0000000Cu, access::rw, 0x0u, false, 235},
};
constexpr field_desc fields_101[] = {  // EE_ALPHA
   {"alpha", 0, 16, 0x0000FFFFu, access::rw, 0x4000u, true, 237},
};
constexpr field_desc fields_102[] = {  // EE_BETA
   {"beta", 0, 16, 0x0000FFFFu, access::rw, 0x2000u, true, 239},
};
constexpr field_desc fields_103[] = {  // EE_CLAMP
   {"clamp_pos", 0, 8, 0x000000FFu, access::rw, 0x10u, true, 241},
   {"clamp_neg", 8, 8, 0x0000FF00u, access::rw, 0x18u, true, 242},
};
constexpr field_desc fields_104[] = {  // EE_FEATURE_EN
   {"activity_en", 0, 1, 0x00000001u, access::rw, 0x0u, true, 244},
   {"contrast_en", 1, 1, 0x00000002u, access::rw, 0x0u, true, 245},
   {"radial_en", 2, 1, 0x00000004u, access::rw, 0x0u, true, 246},
};
constexpr field_desc fields_105[] = {  // EE_RADIAL_CENTER
   {"center_x2", 0, 13, 0x00001FFFu, access::rw, 0x0u, true, 248},
   {"center_y2", 16, 13, 0x1FFF0000u, access::rw, 0x0u, true, 249},
};
constexpr field_desc fields_106[] = {  // EE_RADIAL_R2_TH0
   {"r2_th0", 0, 27, 0x07FFFFFFu, access::rw, 0x0u, true, 251},
};
constexpr field_desc fields_107[] = {  // EE_RADIAL_R2_TH1
   {"r2_th1", 0, 27, 0x07FFFFFFu, access::rw, 0x0u, true, 253},
};
constexpr field_desc fields_108[] = {  // EE_RADIAL_R2_TH2
   {"r2_th2", 0, 27, 0x07FFFFFFu, access::rw, 0x0u, true, 255},
};
constexpr field_desc fields_109[] = {  // EE_RADIAL_GAIN_01
   {"gain0", 0, 16, 0x0000FFFFu, access::rw, 0x8000u, true, 257},
   {"gain1", 16, 16, 0xFFFF0000u, access::rw, 0x8000u, true, 258},
};
constexpr field_desc fields_110[] = {  // EE_RADIAL_GAIN_23
   {"gain2", 0, 16, 0x0000FFFFu, access::rw, 0x8000u, true, 260},
   {"gain3", 16, 16, 0xFFFF0000u, access::rw, 0x8000u, true, 261},
};
constexpr field_desc fields_111[] = {  // EE_LUT_CTRL
   {"lut_sel", 1, 2, 0x00000006u, access::rw, 0x0u, true, 263},
};
constexpr field_desc fields_112[] = {  // EE_LUT_ADDR
   {"lut_addr", 0, 6, 0x0000003Fu, access::rw, 0x0u, true, 265},
};
constexpr field_desc fields_113[] = {  // EE_LUT_WDATA
   {"lut_wdata", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 267},
};
constexpr field_desc fields_114[] = {  // EE_STATUS
   {"busy", 0, 1, 0x00000001u, access::ro, 0x0u, true, 269},
   {"error", 1, 1, 0x00000002u, access::w1c, 0x0u, true, 270},
};
constexpr field_desc fields_115[] = {  // EE_LUT_RDATA
   {"rdata", 0, 16, 0x0000FFFFu, access::ro, 0x0u, true, 272},
};
constexpr field_desc fields_116[] = {  // CNF_CTRL
   {"en", 0, 1, 0x00000001u, access::rw, 0x0u, true, 274},
   {"updated", 2, 1, 0x00000004u, access::rw, 0x0u, true, 275},
};
constexpr field_desc fields_117[] = {  // CNF_CHROMA_TH
   {"chroma_th", 0, 9, 0x000001FFu, access::rw, 0x0u, true, 277},
};
constexpr field_desc fields_118[] = {  // CNF_LUMA_TH
   {"luma_th", 0, 8, 0x000000FFu, access::rw, 0x0u, true, 279},
};
constexpr field_desc fields_119[] = {  // CNF_STRENGTH
   {"mode_strength", 0, 2, 0x00000003u, access::rw, 0x0u, false, 281},
};
constexpr field_desc fields_120[] = {  // CNF_STATUS
   {"busy", 0, 1, 0x00000001u, access::ro, 0x0u, true, 283},
};
constexpr field_desc fields_121[] = {  // RESIZER_CTRL
   {"en", 0, 1, 0x00000001u, access::rw, 0x0u, true, 285},
   {"updated", 2, 1, 0x00000004u, access::rw, 0x0u, true, 286},
   {"scale", 3, 4, 0x00000078u, access::rw, 0x0u, true, 287},
};
constexpr field_desc fields_122[] = {  // RESIZER_OUT_W
   {"out_width", 0, 13, 0x00001FFFu, access::ro, 0x780u, true, 289},
};
constexpr field_desc fields_123[] = {  // RESIZER_OUT_H
   {"out_height", 0, 13, 0x00001FFFu, access::ro, 0x438u, true, 291},
};
constexpr field_desc fields_124[] = {  // RESIZER_STATUS
   {"busy", 0, 1, 0x00000001u, access::ro, 0x0u, true, 293},
};
constexpr field_desc fields_125[] = {  // AEC_CTRL
   {"en", 0, 1, 0x00000001u, access::rw, 0x0u, true, 295},
   {"commit", 1, 1, 0x00000002u, access::w1s, 0x0u, true, 296},
};
constexpr field_desc fields_126[] = {  // AEC_CONTEXT_ID
   {"context_id", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 298},
};
constexpr field_desc fields_127[] = {  // AEC_ZONE_CFG
   {"num_zone_x", 0, 6, 0x0000003Fu, access::rw, 0x20u, true, 300},
   {"num_zone_y", 8, 5, 0x00001F00u, access::rw, 0x18u, true, 301},
};
constexpr field_desc fields_128[] = {  // AEC_ZONE_SIZE
   {"zone_width", 0, 12, 0x00000FFFu, access::rw, 0x78u, true, 303},
   {"zone_height", 16, 12, 0x0FFF0000u, access::rw, 0x5Au, true, 304},
};
constexpr field_desc fields_129[] = {  // AEC_SAMPLE_CLIP
   {"sample_min_clip", 0, 12, 0x00000FFFu, access::rw, 0x0u, true, 306},
   {"sample_max_clip", 16, 12, 0x0FFF0000u, access::rw, 0xFFFu, true, 307},
};
constexpr field_desc fields_130[] = {  // AEC_THRESH
   {"th_ue", 0, 12, 0x00000FFFu, access::rw, 0x100u, true, 309},
   {"th_oe", 16, 12, 0x0FFF0000u, access::rw, 0xF00u, true, 310},
};
constexpr field_desc fields_131[] = {  // AEC_STATUS
   {"busy", 0, 1, 0x00000001u, access::ro, 0x0u, true, 312},
   {"stat_done", 1, 1, 0x00000002u, access::w1c, 0x0u, true, 313},
   {"error", 2, 1, 0x00000004u, access::w1c, 0x0u, false, 314},
};
constexpr field_desc fields_132[] = {  // AEC_FRAME_ID
   {"frame_id", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 316},
};
constexpr field_desc fields_133[] = {  // AEC_RESULT_CONTEXT_ID
   {"context_id", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 318},
};
constexpr field_desc fields_134[] = {  // AEC_CHANNEL_SEL
   {"channel_sel", 0, 2, 0x00000003u, access::rw, 0x0u, true, 320},
};
constexpr field_desc fields_135[] = {  // AEC_GLOBAL_SUM_LO
   {"global_sum_lo", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 322},
};
constexpr field_desc fields_136[] = {  // AEC_GLOBAL_SUM_HI
   {"global_sum_hi", 0, 1, 0x00000001u, access::ro, 0x0u, true, 324},
};
constexpr field_desc fields_137[] = {  // AEC_GLOBAL_COUNT
   {"global_count", 0, 21, 0x001FFFFFu, access::ro, 0x0u, true, 326},
};
constexpr field_desc fields_138[] = {  // AEC_ZONE_ADDR
   {"zone_addr", 0, 10, 0x000003FFu, access::rw, 0x0u, true, 328},
};
constexpr field_desc fields_139[] = {  // AEC_ZONE_SUM
   {"zone_sum", 0, 28, 0x0FFFFFFFu, access::ro, 0x0u, true, 330},
};
constexpr field_desc fields_140[] = {  // AEC_ZONE_COUNT
   {"zone_count", 0, 16, 0x0000FFFFu, access::ro, 0x0u, true, 332},
};
constexpr field_desc fields_141[] = {  // AEC_ZONE_GREEN_OE_UE
   {"green_ue_count", 0, 16, 0x0000FFFFu, access::ro, 0x0u, true, 334},
   {"green_oe_count", 16, 16, 0xFFFF0000u, access::ro, 0x0u, true, 335},
};
constexpr field_desc fields_142[] = {  // AEC_ZONE_GREEN_MIN_MAX
   {"green_min", 0, 12, 0x00000FFFu, access::ro, 0x0u, true, 337},
   {"green_max", 12, 12, 0x00FFF000u, access::ro, 0x0u, true, 338},
};
constexpr field_desc fields_143[] = {  // AEC_HIST_ADDR
   {"hist_addr", 0, 6, 0x0000003Fu, access::rw, 0x0u, true, 340},
};
constexpr field_desc fields_144[] = {  // AEC_HIST_DATA
   {"hist_count", 0, 22, 0x003FFFFFu, access::ro, 0x0u, true, 342},
};
constexpr field_desc fields_145[] = {  // AWB_CTRL
   {"en", 0, 1, 0x00000001u, access::rw, 0x0u, true, 344},
   {"num_zone_x", 1, 7, 0x000000FEu, access::rw, 0x0u, true, 345},
   {"num_zone_y", 8, 6, 0x00003F00u, access::rw, 0x0u, true, 346},
};
constexpr field_desc fields_146[] = {  // AWB_X_BOUND_ADDR
   {"x_bound_addr", 0, 6, 0x0000003Fu, access::rw, 0x0u, false, 348},
};
constexpr field_desc fields_147[] = {  // AWB_Y_BOUND_ADDR
   {"y_bound_addr", 0, 5, 0x0000001Fu, access::rw, 0x0u, false, 350},
};
constexpr field_desc fields_148[] = {  // AWB_X_BOUND_DATA
   {"x_bound_data", 0, 12, 0x00000FFFu, access::rw, 0x0u, false, 352},
   {"x_bound_we", 12, 1, 0x00001000u, access::rw, 0x0u, false, 353},
};
constexpr field_desc fields_149[] = {  // AWB_Y_BOUND_DATA
   {"y_bound_data", 0, 12, 0x00000FFFu, access::rw, 0x0u, false, 355},
   {"y_bound_we", 12, 1, 0x00001000u, access::rw, 0x0u, false, 356},
};
constexpr field_desc fields_150[] = {  // AWB_GLOBAL_SUM_R_H
   {"global_sum_r_h", 0, 3, 0x00000007u, access::ro, 0x0u, true, 358},
};
constexpr field_desc fields_151[] = {  // AWB_GLOBAL_SUM_R_L
   {"global_sum_r_l", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 360},
};
constexpr field_desc fields_152[] = {  // AWB_GLOBAL_SUM_G_H
   {"global_sum_g_h", 0, 3, 0x00000007u, access::ro, 0x0u, true, 362},
};
constexpr field_desc fields_153[] = {  // AWB_GLOBAL_SUM_G_L
   {"global_sum_g_l", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 364},
};
constexpr field_desc fields_154[] = {  // AWB_GLOBAL_SUM_B_H
   {"global_sum_b_h", 0, 3, 0x00000007u, access::ro, 0x0u, true, 366},
};
constexpr field_desc fields_155[] = {  // AWB_GLOBAL_SUM_B_L
   {"global_sum_b_l", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 368},
};
constexpr field_desc fields_156[] = {  // AWB_GLOBAL_COUNT
   {"global_count", 0, 23, 0x007FFFFFu, access::ro, 0x0u, true, 370},
};
constexpr field_desc fields_157[] = {  // AWB_ZONE_ADDR
   {"zone_addr", 0, 11, 0x000007FFu, access::rw, 0x0u, true, 372},
};
constexpr field_desc fields_158[] = {  // AWB_ZONE_SUM_R_H
   {"zone_sum_r_h", 0, 3, 0x00000007u, access::ro, 0x0u, true, 374},
};
constexpr field_desc fields_159[] = {  // AWB_ZONE_SUM_R_L
   {"zone_sum_r_l", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 376},
};
constexpr field_desc fields_160[] = {  // AWB_ZONE_SUM_G_H
   {"zone_sum_g_h", 0, 3, 0x00000007u, access::ro, 0x0u, true, 378},
};
constexpr field_desc fields_161[] = {  // AWB_ZONE_SUM_G_L
   {"zone_sum_g_l", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 380},
};
constexpr field_desc fields_162[] = {  // AWB_ZONE_SUM_B_H
   {"zone_sum_b_h", 0, 3, 0x00000007u, access::ro, 0x0u, true, 382},
};
constexpr field_desc fields_163[] = {  // AWB_ZONE_SUM_B_L
   {"zone_sum_b_l", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 384},
};
constexpr field_desc fields_164[] = {  // AWB_ZONE_COUNT
   {"zone_count", 0, 23, 0x007FFFFFu, access::ro, 0x0u, true, 386},
};
constexpr field_desc fields_165[] = {  // AWB_STATUS
   {"stat_done", 0, 1, 0x00000001u, access::w1c, 0x0u, true, 388},
};
constexpr field_desc fields_166[] = {  // AWB_UNDEREXPOSED_LIMIT
   {"underexposed_limit", 0, 12, 0x00000FFFu, access::rw, 0x0u, true, 390},
};
constexpr field_desc fields_167[] = {  // AWB_SATURATION_LIMIT
   {"saturation_limit", 0, 12, 0x00000FFFu, access::rw, 0xFFFu, true, 392},
};
constexpr field_desc fields_168[] = {  // AWB_CONTEXT_ID
   {"context_id", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 394},
};
constexpr field_desc fields_169[] = {  // AWB_RESULT_CONTEXT_ID
   {"context_id", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 396},
};
constexpr field_desc fields_170[] = {  // AWB_FRAME_ID
   {"frame_id", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 398},
};
constexpr field_desc fields_171[] = {  // AF_CTRL
   {"en", 0, 1, 0x00000001u, access::rw, 0x0u, true, 400},
   {"bypass", 1, 1, 0x00000002u, access::rw, 0x0u, false, 401},
   {"reg_update", 2, 1, 0x00000004u, access::rw, 0x0u, false, 402},
   {"search_start", 3, 1, 0x00000008u, access::w1s, 0x0u, false, 403},
   {"search_abort", 4, 1, 0x00000010u, access::w1s, 0x0u, false, 404},
   {"lut_en", 5, 1, 0x00000020u, access::rw, 0x0u, false, 405},
   {"metric_sel", 6, 2, 0x000000C0u, access::rw, 0x0u, false, 406},
   {"vcm_ack", 8, 1, 0x00000100u, access::rw, 0x0u, false, 407},
};
constexpr field_desc fields_172[] = {  // AF_ZONE_EN
   {"zone_en", 0, 16, 0x0000FFFFu, access::rw, 0xFFFFu, false, 409},
};
constexpr field_desc fields_173[] = {  // AF_METRIC_CFG
   {"min_score", 0, 16, 0x0000FFFFu, access::rw, 0x0u, false, 411},
   {"settle_frames", 16, 8, 0x00FF0000u, access::rw, 0x0u, false, 412},
};
constexpr field_desc fields_174[] = {  // AF_STAT_ADDR
   {"zone_addr", 0, 4, 0x0000000Fu, access::rw, 0x0u, true, 414},
};
constexpr field_desc fields_175[] = {  // AF_STAT_DATA
   {"focus_score", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 416},
};
constexpr field_desc fields_176[] = {  // AF_LUT_ADDR
   {"lut_addr", 0, 8, 0x000000FFu, access::rw, 0x0u, false, 418},
};
constexpr field_desc fields_177[] = {  // AF_LUT_DATA
   {"vcm_position", 0, 16, 0x0000FFFFu, access::rw, 0x0u, false, 420},
};
constexpr field_desc fields_178[] = {  // AF_VCM_CUR_POS
   {"vcm_cur_pos", 0, 16, 0x0000FFFFu, access::rw, 0x0u, false, 422},
};
constexpr field_desc fields_179[] = {  // AF_VCM_BEST_POS
   {"vcm_best_pos", 0, 16, 0x0000FFFFu, access::ro, 0x0u, false, 424},
};
constexpr field_desc fields_180[] = {  // AF_PEAK_SCORE
   {"peak_score", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, false, 426},
};
constexpr field_desc fields_181[] = {  // AF_FOCUS_RANGE
   {"accept_delta", 0, 16, 0x0000FFFFu, access::rw, 0x0u, false, 428},
   {"hof_range", 16, 8, 0x00FF0000u, access::rw, 0x0u, false, 429},
};
constexpr field_desc fields_182[] = {  // AF_STATUS
   {"busy", 0, 1, 0x00000001u, access::ro, 0x0u, true, 431},
   {"frame_done", 1, 1, 0x00000002u, access::w1c, 0x0u, true, 432},
   {"search_done", 2, 1, 0x00000004u, access::w1c, 0x0u, false, 433},
   {"vcm_pos_valid", 3, 1, 0x00000008u, access::w1c, 0x0u, false, 434},
   {"score_valid", 5, 1, 0x00000020u, access::ro, 0x0u, true, 435},
};
constexpr field_desc fields_183[] = {  // AF_CONTEXT_ID
   {"context_id", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 437},
};
constexpr field_desc fields_184[] = {  // AF_RESULT_CONTEXT_ID
   {"context_id", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 439},
};
constexpr field_desc fields_185[] = {  // AF_FRAME_ID
   {"frame_id", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 441},
};
constexpr field_desc fields_186[] = {  // OFMT_CTRL
   {"stride_en", 0, 1, 0x00000001u, access::rw, 0x0u, true, 443},
};
constexpr field_desc fields_187[] = {  // OFMT_Y_STRIDE
   {"y_stride", 0, 16, 0x0000FFFFu, access::rw, 0xF00u, true, 445},
};
constexpr field_desc fields_188[] = {  // OFMT_UV_STRIDE
   {"uv_stride", 0, 16, 0x0000FFFFu, access::rw, 0xF00u, true, 447},
};
constexpr field_desc fields_189[] = {  // DMA_CTRL
   {"idma_en", 0, 1, 0x00000001u, access::rw, 0x0u, true, 449},
   {"odma_en", 1, 1, 0x00000002u, access::rw, 0x0u, true, 450},
   {"soft_reset", 2, 1, 0x00000004u, access::w1sc, 0x0u, true, 451},
   {"max_burst_m1", 8, 8, 0x0000FF00u, access::rw, 0x3Fu, true, 452},
};
constexpr field_desc fields_190[] = {  // DMA_STAT
   {"idma_busy", 0, 1, 0x00000001u, access::ro, 0x0u, true, 454},
   {"odma_busy", 1, 1, 0x00000002u, access::ro, 0x0u, true, 455},
};
constexpr field_desc fields_191[] = {  // DMA_ERR
   {"err", 0, 6, 0x0000001Fu, access::w1c, 0x0u, true, 457},
};
constexpr field_desc fields_192[] = {  // DMA_IRQ_EN
   {"irq_en", 0, 7, 0x0000007Eu, access::rw, 0x0u, true, 459},
};
constexpr field_desc fields_193[] = {  // DMA_IRQ_STAT
   {"irq", 0, 7, 0x0000007Eu, access::w1c, 0x0u, true, 461},
};
constexpr field_desc fields_194[] = {  // IDMA_STRIDE
   {"idma_stride", 0, 32, 0xFFFFFFFFu, access::rw, 0x1E00u, true, 463},
};
constexpr field_desc fields_195[] = {  // IDMA_BUF_VALID
   {"idma_buf_valid", 0, 8, 0x000000FFu, access::w1s, 0x0u, true, 465},
};
constexpr field_desc fields_196[] = {  // IDMA_FRAME_COUNT
   {"idma_frame_count", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 467},
};
constexpr field_desc fields_197[] = {  // IDMA_BUF_ADDR_L0
   {"idma_buf_addr0_lo", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 469},
};
constexpr field_desc fields_198[] = {  // IDMA_BUF_ADDR_H0
   {"idma_buf_addr0_hi", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 471},
};
constexpr field_desc fields_199[] = {  // IDMA_BUF_ADDR_L1
   {"idma_buf_addr1_lo", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 473},
};
constexpr field_desc fields_200[] = {  // IDMA_BUF_ADDR_H1
   {"idma_buf_addr1_hi", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 475},
};
constexpr field_desc fields_201[] = {  // IDMA_BUF_ADDR_L2
   {"idma_buf_addr2_lo", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 477},
};
constexpr field_desc fields_202[] = {  // IDMA_BUF_ADDR_H2
   {"idma_buf_addr2_hi", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 479},
};
constexpr field_desc fields_203[] = {  // IDMA_BUF_ADDR_L3
   {"idma_buf_addr3_lo", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 481},
};
constexpr field_desc fields_204[] = {  // IDMA_BUF_ADDR_H3
   {"idma_buf_addr3_hi", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 483},
};
constexpr field_desc fields_205[] = {  // ODMA_Y_STRIDE
   {"y_stride", 0, 32, 0xFFFFFFFFu, access::rw, 0xF00u, true, 485},
};
constexpr field_desc fields_206[] = {  // ODMA_UV_STRIDE
   {"uv_stride", 0, 32, 0xFFFFFFFFu, access::rw, 0xF00u, true, 487},
};
constexpr field_desc fields_207[] = {  // ODMA_BUF_FREE
   {"odma_buf_free", 0, 8, 0x000000FFu, access::w1s, 0x0u, true, 489},
};
constexpr field_desc fields_208[] = {  // ODMA_BUF_DONE
   {"odma_buf_done", 0, 8, 0x000000FFu, access::w1c, 0x0u, true, 491},
};
constexpr field_desc fields_209[] = {  // ODMA_FRAME_COUNT
   {"odma_frame_count", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 493},
};
constexpr field_desc fields_210[] = {  // ODMA_Y_ADDR_L0
   {"odma_y_addr0_lo", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 495},
};
constexpr field_desc fields_211[] = {  // ODMA_Y_ADDR_H0
   {"odma_y_addr0_hi", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 497},
};
constexpr field_desc fields_212[] = {  // ODMA_UV_ADDR_L0
   {"odma_uv_addr0_lo", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 499},
};
constexpr field_desc fields_213[] = {  // ODMA_UV_ADDR_H0
   {"odma_uv_addr0_hi", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 501},
};
constexpr field_desc fields_214[] = {  // ODMA_Y_ADDR_L1
   {"odma_y_addr1_lo", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 503},
};
constexpr field_desc fields_215[] = {  // ODMA_Y_ADDR_H1
   {"odma_y_addr1_hi", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 505},
};
constexpr field_desc fields_216[] = {  // ODMA_UV_ADDR_L1
   {"odma_uv_addr1_lo", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 507},
};
constexpr field_desc fields_217[] = {  // ODMA_UV_ADDR_H1
   {"odma_uv_addr1_hi", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 509},
};
constexpr field_desc fields_218[] = {  // ODMA_Y_ADDR_L2
   {"odma_y_addr2_lo", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 511},
};
constexpr field_desc fields_219[] = {  // ODMA_Y_ADDR_H2
   {"odma_y_addr2_hi", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 513},
};
constexpr field_desc fields_220[] = {  // ODMA_UV_ADDR_L2
   {"odma_uv_addr2_lo", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 515},
};
constexpr field_desc fields_221[] = {  // ODMA_UV_ADDR_H2
   {"odma_uv_addr2_hi", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 517},
};
constexpr field_desc fields_222[] = {  // ODMA_Y_ADDR_L3
   {"odma_y_addr3_lo", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 519},
};
constexpr field_desc fields_223[] = {  // ODMA_Y_ADDR_H3
   {"odma_y_addr3_hi", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 521},
};
constexpr field_desc fields_224[] = {  // ODMA_UV_ADDR_L3
   {"odma_uv_addr3_lo", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 523},
};
constexpr field_desc fields_225[] = {  // ODMA_UV_ADDR_H3
   {"odma_uv_addr3_hi", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 525},
};

}  // namespace

const reg_desc registers[] = {
   {0x0000u, "COMMON_VER_DATE", 0x18072026u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, fields_0, 3, 2},
   {0x0004u, "COMMON_VER_ID", 0x00010002u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, fields_1, 1, 6},
   {0x0008u, "COMMON_CTRL", 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000004u, 0x00000002u, fields_2, 3, 8},
   {0x000Cu, "COMMON_STATUS", 0x00000000u, 0x00000000u, 0x00000007u, 0x00000000u, 0x00000000u, 0x00000000u, fields_3, 3, 12},
   {0x0010u, "COMMON_IRQ_STATUS", 0x00000000u, 0x00000000u, 0x00000000u, 0x00000007u, 0x00000000u, 0x00000000u, fields_4, 3, 16},
   {0x0014u, "COMMON_IRQ_EN", 0x00000000u, 0x00000007u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_5, 3, 20},
   {0x0018u, "COMMON_FRAME_WIDTH", 0x00000780u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_6, 1, 24},
   {0x001Cu, "COMMON_FRAME_HEIGHT", 0x00000438u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_7, 1, 26},
   {0x0020u, "COMMON_BAYER", 0x00000000u, 0x00000003u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_8, 1, 28},
   {0x0028u, "COMMON_SCRATCH", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_9, 1, 30},
   {0x0200u, "BLC_CTRL", 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_10, 1, 32},
   {0x0204u, "BLC_GAIN_SEL", 0x00000100u, 0x00000103u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_11, 2, 34},
   {0x0208u, "BLC_OFS_G0_DFT", 0x000000C8u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_12, 1, 37},
   {0x020Cu, "BLC_OFS_G0_R", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_13, 1, 39},
   {0x0210u, "BLC_OFS_G0_GR", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_14, 1, 41},
   {0x0214u, "BLC_OFS_G0_GB", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_15, 1, 43},
   {0x0218u, "BLC_OFS_G0_B", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_16, 1, 45},
   {0x021Cu, "BLC_OFS_G1_DFT", 0x000000C8u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_17, 1, 47},
   {0x0220u, "BLC_OFS_G1_R", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_18, 1, 49},
   {0x0224u, "BLC_OFS_G1_GR", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_19, 1, 51},
   {0x0228u, "BLC_OFS_G1_GB", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_20, 1, 53},
   {0x022Cu, "BLC_OFS_G1_B", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_21, 1, 55},
   {0x0230u, "BLC_OFS_G2_DFT", 0x000000C8u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_22, 1, 57},
   {0x0234u, "BLC_OFS_G2_R", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_23, 1, 59},
   {0x0238u, "BLC_OFS_G2_GR", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_24, 1, 61},
   {0x023Cu, "BLC_OFS_G2_GB", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_25, 1, 63},
   {0x0240u, "BLC_OFS_G2_B", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_26, 1, 65},
   {0x0244u, "BLC_OFS_G3_DFT", 0x000000C8u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_27, 1, 67},
   {0x0248u, "BLC_OFS_G3_R", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_28, 1, 69},
   {0x024Cu, "BLC_OFS_G3_GR", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_29, 1, 71},
   {0x0250u, "BLC_OFS_G3_GB", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_30, 1, 73},
   {0x0254u, "BLC_OFS_G3_B", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_31, 1, 75},
   {0x0258u, "BLC_SCALE_G0", 0x00010000u, 0x0003FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_32, 1, 77},
   {0x025Cu, "BLC_SCALE_G1", 0x00010000u, 0x0003FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_33, 1, 79},
   {0x0260u, "BLC_SCALE_G2", 0x00010000u, 0x0003FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_34, 1, 81},
   {0x0264u, "BLC_SCALE_G3", 0x00010000u, 0x0003FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_35, 1, 83},
   {0x0400u, "LSC_CTRL", 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_36, 1, 85},
   {0x0404u, "LSC_MESH_NODES", 0x00000000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_37, 2, 87},
   {0x0408u, "LSC_STRENGTH", 0x00000000u, 0x0001FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_38, 1, 90},
   {0x040Cu, "LSC_PROFILE_SEL", 0x00000000u, 0x00000003u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_39, 1, 92},
   {0x0410u, "LSC_LOAD_CTRL", 0x00000000u, 0x00000003u, 0x00000000u, 0x00000000u, 0x00000700u, 0x00000000u, fields_40, 4, 94},
   {0x0414u, "LSC_COEF_DATA", 0x00000000u, 0x001FFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_41, 1, 99},
   {0x0418u, "LSC_LOAD_STATUS", 0x00000000u, 0x00000000u, 0x000FFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, fields_42, 4, 101},
   {0x041Cu, "LSC_PROFILE_STATUS", 0x00000000u, 0x00000000u, 0x000000F7u, 0x00000000u, 0x00000000u, 0x00000000u, fields_43, 4, 106},
   {0x0420u, "LSC_ERROR", 0x00000000u, 0x00000000u, 0x00000000u, 0x0000003Fu, 0x00000000u, 0x00000000u, fields_44, 6, 111},
   {0x0424u, "LSC_STAT_OVF_CNT_R", 0x00000000u, 0x00000000u, 0x001FFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, fields_45, 1, 118},
   {0x0428u, "LSC_STAT_OVF_CNT_GR", 0x00000000u, 0x00000000u, 0x001FFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, fields_46, 1, 120},
   {0x042Cu, "LSC_STAT_OVF_CNT_GB", 0x00000000u, 0x00000000u, 0x001FFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, fields_47, 1, 122},
   {0x0430u, "LSC_STAT_OVF_CNT_B", 0x00000000u, 0x00000000u, 0x001FFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, fields_48, 1, 124},
   {0x0600u, "BPC_CTRL", 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_49, 1, 126},
   {0x0604u, "BPC_MODE", 0x00000001u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_50, 1, 128},
   {0x0608u, "BPC_TEMPORAL_VAR", 0x00000100u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_51, 1, 130},
   {0x060Cu, "BPC_THRESH", 0x004D0192u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_52, 2, 132},
   {0x0610u, "BPC_PIXEL_AGE", 0x000004B0u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_53, 1, 135},
   {0x0614u, "BPC_NUM_CANDIDATES", 0x00000000u, 0x00000000u, 0x00003FFFu, 0x00000000u, 0x00000000u, 0x00000000u, fields_54, 1, 137},
   {0x0618u, "BPC_NUM_DEFECTIVE", 0x00000000u, 0x00000000u, 0x00003FFFu, 0x00000000u, 0x00000000u, 0x00000000u, fields_55, 1, 139},
   {0x061Cu, "BPC_STATUS", 0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, fields_56, 1, 141},
   {0x0800u, "WB_CTRL", 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_57, 1, 143},
   {0x0804u, "WB_GAIN_R", 0x00000100u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_58, 1, 145},
   {0x0808u, "WB_GAIN_G", 0x00000100u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_59, 1, 147},
   {0x080Cu, "WB_GAIN_B", 0x00000100u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_60, 1, 149},
   {0x0A00u, "DG_CTRL", 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_61, 1, 151},
   {0x0A04u, "DG_GAIN", 0x00000100u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_62, 1, 153},
   {0x0C00u, "D_WDR_CTRL", 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_63, 1, 155},
   {0x0C04u, "D_WDR_STRENGTH", 0x00000000u, 0x000000FFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_64, 1, 157},
   {0x0E00u, "DEMOSAIC_STATUS", 0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, fields_65, 1, 159},
   {0x1000u, "CCM_CTRL", 0x00000000u, 0x00000005u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_66, 2, 161},
   {0x1004u, "CCM_CRR", 0x00000200u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_67, 1, 164},
   {0x1008u, "CCM_CRG", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_68, 1, 166},
   {0x100Cu, "CCM_CRB", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_69, 1, 168},
   {0x1010u, "CCM_CGR", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_70, 1, 170},
   {0x1014u, "CCM_CGG", 0x00000200u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_71, 1, 172},
   {0x1018u, "CCM_CGB", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_72, 1, 174},
   {0x101Cu, "CCM_CBR", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_73, 1, 176},
   {0x1020u, "CCM_CBG", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_74, 1, 178},
   {0x1024u, "CCM_CBB", 0x00000200u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_75, 1, 180},
   {0x1028u, "CCM_OFS_R", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_76, 1, 182},
   {0x102Cu, "CCM_OFS_G", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_77, 1, 184},
   {0x1030u, "CCM_OFS_B", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_78, 1, 186},
   {0x1034u, "CCM_STATUS", 0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, fields_79, 1, 188},
   {0x1200u, "GAMMA_CTRL", 0x00000000u, 0x00000071u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_80, 2, 190},
   {0x1204u, "GAMMA_LUT_ADDR", 0x00000000u, 0x000000FFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_81, 1, 193},
   {0x1208u, "GAMMA_LUT_DATA", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_82, 1, 195},
   {0x120Cu, "GAMMA_LUT_RDATA", 0x00000000u, 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, fields_83, 1, 197},
   {0x1210u, "GAMMA_STATUS", 0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, fields_84, 1, 199},
   {0x1400u, "CSC_CTRL", 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_85, 1, 201},
   {0x1600u, "GTM_CTRL", 0x00000000u, 0x00000003u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_86, 2, 203},
   {0x1604u, "GTM_LUT_ADDR", 0x00000000u, 0x0000007Fu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_87, 1, 206},
   {0x1608u, "GTM_LUT_DATA", 0x00000000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_88, 1, 208},
   {0x160Cu, "GTM_KEY", 0x0000002Eu, 0x000000FFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_89, 1, 210},
   {0x1610u, "GTM_LWHITE", 0x00000400u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_90, 1, 212},
   {0x1614u, "GTM_ROI_LOG2", 0x0000000Bu, 0x0000001Fu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_91, 1, 214},
   {0x1618u, "GTM_LUT_RDATA", 0x00000000u, 0x00000000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, fields_92, 1, 216},
   {0x161Cu, "GTM_STATUS", 0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, fields_93, 1, 218},
   {0x1800u, "NR_2D_CTRL", 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_94, 1, 220},
   {0x1804u, "NR_2D_EST_VAR", 0x00000000u, 0x00000000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, fields_95, 1, 222},
   {0x1808u, "NR_2D_STATUS", 0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, fields_96, 1, 224},
   {0x1A00u, "TNR_3D_CTRL", 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_97, 1, 226},
   {0x1A04u, "TNR_3D_MTF_SEL", 0x00000000u, 0x00000007u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_98, 1, 228},
   {0x1A08u, "TNR_3D_HIST_BASE", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_99, 1, 230},
   {0x1C00u, "EE_CTRL", 0x00000000u, 0x0000000Fu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_100, 3, 232},
   {0x1C04u, "EE_ALPHA", 0x00004000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_101, 1, 236},
   {0x1C08u, "EE_BETA", 0x00002000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_102, 1, 238},
   {0x1C0Cu, "EE_CLAMP", 0x00001810u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_103, 2, 240},
   {0x1C10u, "EE_FEATURE_EN", 0x00000000u, 0x00000007u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_104, 3, 243},
   {0x1C14u, "EE_RADIAL_CENTER", 0x00000000u, 0x1FFF1FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_105, 2, 247},
   {0x1C18u, "EE_RADIAL_R2_TH0", 0x00000000u, 0x07FFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_106, 1, 250},
   {0x1C1Cu, "EE_RADIAL_R2_TH1", 0x00000000u, 0x07FFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_107, 1, 252},
   {0x1C20u, "EE_RADIAL_R2_TH2", 0x00000000u, 0x07FFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_108, 1, 254},
   {0x1C24u, "EE_RADIAL_GAIN_01", 0x80008000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_109, 2, 256},
   {0x1C28u, "EE_RADIAL_GAIN_23", 0x80008000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_110, 2, 259},
   {0x1C2Cu, "EE_LUT_CTRL", 0x00000000u, 0x00000006u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_111, 1, 262},
   {0x1C30u, "EE_LUT_ADDR", 0x00000000u, 0x0000003Fu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_112, 1, 264},
   {0x1C34u, "EE_LUT_WDATA", 0x00000000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_113, 1, 266},
   {0x1C38u, "EE_STATUS", 0x00000000u, 0x00000000u, 0x00000001u, 0x00000002u, 0x00000000u, 0x00000000u, fields_114, 2, 268},
   {0x1C3Cu, "EE_LUT_RDATA", 0x00000000u, 0x00000000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, fields_115, 1, 271},
   {0x1E00u, "CNF_CTRL", 0x00000000u, 0x00000005u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_116, 2, 273},
   {0x1E04u, "CNF_CHROMA_TH", 0x00000000u, 0x000001FFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_117, 1, 276},
   {0x1E08u, "CNF_LUMA_TH", 0x00000000u, 0x000000FFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_118, 1, 278},
   {0x1E0Cu, "CNF_STRENGTH", 0x00000000u, 0x00000003u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_119, 1, 280},
   {0x1E10u, "CNF_STATUS", 0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, fields_120, 1, 282},
   {0x2000u, "RESIZER_CTRL", 0x00000000u, 0x0000007Du, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_121, 3, 284},
   {0x2014u, "RESIZER_OUT_W", 0x00000780u, 0x00000000u, 0x00001FFFu, 0x00000000u, 0x00000000u, 0x00000000u, fields_122, 1, 288},
   {0x2018u, "RESIZER_OUT_H", 0x00000438u, 0x00000000u, 0x00001FFFu, 0x00000000u, 0x00000000u, 0x00000000u, fields_123, 1, 290},
   {0x201Cu, "RESIZER_STATUS", 0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, fields_124, 1, 292},
   {0x2200u, "AEC_CTRL", 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000002u, 0x00000000u, fields_125, 2, 294},
   {0x2204u, "AEC_CONTEXT_ID", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_126, 1, 297},
   {0x2208u, "AEC_ZONE_CFG", 0x00001820u, 0x00001F3Fu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_127, 2, 299},
   {0x220Cu, "AEC_ZONE_SIZE", 0x005A0078u, 0x0FFF0FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_128, 2, 302},
   {0x2210u, "AEC_SAMPLE_CLIP", 0x0FFF0000u, 0x0FFF0FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_129, 2, 305},
   {0x2214u, "AEC_THRESH", 0x0F000100u, 0x0FFF0FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_130, 2, 308},
   {0x2218u, "AEC_STATUS", 0x00000000u, 0x00000000u, 0x00000001u, 0x00000006u, 0x00000000u, 0x00000000u, fields_131, 3, 311},
   {0x221Cu, "AEC_FRAME_ID", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, fields_132, 1, 315},
   {0x2220u, "AEC_RESULT_CONTEXT_ID", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, fields_133, 1, 317},
   {0x2224u, "AEC_CHANNEL_SEL", 0x00000000u, 0x00000003u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_134, 1, 319},
   {0x2228u, "AEC_GLOBAL_SUM_LO", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, fields_135, 1, 321},
   {0x222Cu, "AEC_GLOBAL_SUM_HI", 0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, fields_136, 1, 323},
   {0x2230u, "AEC_GLOBAL_COUNT", 0x00000000u, 0x00000000u, 0x001FFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, fields_137, 1, 325},
   {0x2234u, "AEC_ZONE_ADDR", 0x00000000u, 0x000003FFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_138, 1, 327},
   {0x2238u, "AEC_ZONE_SUM", 0x00000000u, 0x00000000u, 0x0FFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, fields_139, 1, 329},
   {0x223Cu, "AEC_ZONE_COUNT", 0x00000000u, 0x00000000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, fields_140, 1, 331},
   {0x2240u, "AEC_ZONE_GREEN_OE_UE", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, fields_141, 2, 333},
   {0x2244u, "AEC_ZONE_GREEN_MIN_MAX", 0x00000000u, 0x00000000u, 0x00FFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, fields_142, 2, 336},
   {0x2248u, "AEC_HIST_ADDR", 0x00000000u, 0x0000003Fu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_143, 1, 339},
   {0x224Cu, "AEC_HIST_DATA", 0x00000000u, 0x00000000u, 0x003FFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, fields_144, 1, 341},
   {0x2400u, "AWB_CTRL", 0x00000000u, 0x00003FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_145, 3, 343},
   {0x2404u, "AWB_X_BOUND_ADDR", 0x00000000u, 0x0000003Fu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_146, 1, 347},
   {0x2408u, "AWB_Y_BOUND_ADDR", 0x00000000u, 0x0000001Fu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_147, 1, 349},
   {0x240Cu, "AWB_X_BOUND_DATA", 0x00000000u, 0x00001FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_148, 2, 351},
   {0x2410u, "AWB_Y_BOUND_DATA", 0x00000000u, 0x00001FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_149, 2, 354},
   {0x2414u, "AWB_GLOBAL_SUM_R_H", 0x00000000u, 0x00000000u, 0x00000007u, 0x00000000u, 0x00000000u, 0x00000000u, fields_150, 1, 357},
   {0x2418u, "AWB_GLOBAL_SUM_R_L", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, fields_151, 1, 359},
   {0x241Cu, "AWB_GLOBAL_SUM_G_H", 0x00000000u, 0x00000000u, 0x00000007u, 0x00000000u, 0x00000000u, 0x00000000u, fields_152, 1, 361},
   {0x2420u, "AWB_GLOBAL_SUM_G_L", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, fields_153, 1, 363},
   {0x2424u, "AWB_GLOBAL_SUM_B_H", 0x00000000u, 0x00000000u, 0x00000007u, 0x00000000u, 0x00000000u, 0x00000000u, fields_154, 1, 365},
   {0x2428u, "AWB_GLOBAL_SUM_B_L", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, fields_155, 1, 367},
   {0x242Cu, "AWB_GLOBAL_COUNT", 0x00000000u, 0x00000000u, 0x007FFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, fields_156, 1, 369},
   {0x2430u, "AWB_ZONE_ADDR", 0x00000000u, 0x000007FFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_157, 1, 371},
   {0x2434u, "AWB_ZONE_SUM_R_H", 0x00000000u, 0x00000000u, 0x00000007u, 0x00000000u, 0x00000000u, 0x00000000u, fields_158, 1, 373},
   {0x2438u, "AWB_ZONE_SUM_R_L", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, fields_159, 1, 375},
   {0x243Cu, "AWB_ZONE_SUM_G_H", 0x00000000u, 0x00000000u, 0x00000007u, 0x00000000u, 0x00000000u, 0x00000000u, fields_160, 1, 377},
   {0x2440u, "AWB_ZONE_SUM_G_L", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, fields_161, 1, 379},
   {0x2444u, "AWB_ZONE_SUM_B_H", 0x00000000u, 0x00000000u, 0x00000007u, 0x00000000u, 0x00000000u, 0x00000000u, fields_162, 1, 381},
   {0x2448u, "AWB_ZONE_SUM_B_L", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, fields_163, 1, 383},
   {0x244Cu, "AWB_ZONE_COUNT", 0x00000000u, 0x00000000u, 0x007FFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, fields_164, 1, 385},
   {0x2450u, "AWB_STATUS", 0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, fields_165, 1, 387},
   {0x2454u, "AWB_UNDEREXPOSED_LIMIT", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_166, 1, 389},
   {0x2458u, "AWB_SATURATION_LIMIT", 0x00000FFFu, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_167, 1, 391},
   {0x245Cu, "AWB_CONTEXT_ID", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_168, 1, 393},
   {0x2460u, "AWB_RESULT_CONTEXT_ID", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, fields_169, 1, 395},
   {0x2464u, "AWB_FRAME_ID", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, fields_170, 1, 397},
   {0x2600u, "AF_CTRL", 0x00000000u, 0x000001E7u, 0x00000000u, 0x00000000u, 0x00000018u, 0x00000000u, fields_171, 8, 399},
   {0x2604u, "AF_ZONE_EN", 0x0000FFFFu, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_172, 1, 408},
   {0x2608u, "AF_METRIC_CFG", 0x00000000u, 0x00FFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_173, 2, 410},
   {0x260Cu, "AF_STAT_ADDR", 0x00000000u, 0x0000000Fu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_174, 1, 413},
   {0x2610u, "AF_STAT_DATA", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, fields_175, 1, 415},
   {0x2614u, "AF_LUT_ADDR", 0x00000000u, 0x000000FFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_176, 1, 417},
   {0x2618u, "AF_LUT_DATA", 0x00000000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_177, 1, 419},
   {0x261Cu, "AF_VCM_CUR_POS", 0x00000000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_178, 1, 421},
   {0x2620u, "AF_VCM_BEST_POS", 0x00000000u, 0x00000000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, fields_179, 1, 423},
   {0x2624u, "AF_PEAK_SCORE", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, fields_180, 1, 425},
   {0x2628u, "AF_FOCUS_RANGE", 0x00000000u, 0x00FFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_181, 2, 427},
   {0x262Cu, "AF_STATUS", 0x00000000u, 0x00000000u, 0x00000021u, 0x0000000Eu, 0x00000000u, 0x00000000u, fields_182, 5, 430},
   {0x2630u, "AF_CONTEXT_ID", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_183, 1, 436},
   {0x2634u, "AF_RESULT_CONTEXT_ID", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, fields_184, 1, 438},
   {0x2638u, "AF_FRAME_ID", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, fields_185, 1, 440},
   {0x2800u, "OFMT_CTRL", 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_186, 1, 442},
   {0x2804u, "OFMT_Y_STRIDE", 0x00000F00u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_187, 1, 444},
   {0x2808u, "OFMT_UV_STRIDE", 0x00000F00u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_188, 1, 446},
   {0x3000u, "DMA_CTRL", 0x00003F00u, 0x0000FF03u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000004u, fields_189, 4, 448},
   {0x3004u, "DMA_STAT", 0x00000000u, 0x00000000u, 0x00000003u, 0x00000000u, 0x00000000u, 0x00000000u, fields_190, 2, 453},
   {0x3008u, "DMA_ERR", 0x00000000u, 0x00000000u, 0x00000000u, 0x0000001Fu, 0x00000000u, 0x00000000u, fields_191, 1, 456},
   {0x300Cu, "DMA_IRQ_EN", 0x00000000u, 0x0000007Eu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_192, 1, 458},
   {0x3010u, "DMA_IRQ_STAT", 0x00000000u, 0x00000000u, 0x00000000u, 0x0000007Eu, 0x00000000u, 0x00000000u, fields_193, 1, 460},
   {0x3020u, "IDMA_STRIDE", 0x00001E00u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_194, 1, 462},
   {0x3024u, "IDMA_BUF_VALID", 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x000000FFu, 0x00000000u, fields_195, 1, 464},
   {0x3028u, "IDMA_FRAME_COUNT", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, fields_196, 1, 466},
   {0x3040u, "IDMA_BUF_ADDR_L0", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_197, 1, 468},
   {0x3044u, "IDMA_BUF_ADDR_H0", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_198, 1, 470},
   {0x3048u, "IDMA_BUF_ADDR_L1", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_199, 1, 472},
   {0x304Cu, "IDMA_BUF_ADDR_H1", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_200, 1, 474},
   {0x3050u, "IDMA_BUF_ADDR_L2", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_201, 1, 476},
   {0x3054u, "IDMA_BUF_ADDR_H2", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_202, 1, 478},
   {0x3058u, "IDMA_BUF_ADDR_L3", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_203, 1, 480},
   {0x305Cu, "IDMA_BUF_ADDR_H3", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_204, 1, 482},
   {0x3080u, "ODMA_Y_STRIDE", 0x00000F00u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_205, 1, 484},
   {0x3084u, "ODMA_UV_STRIDE", 0x00000F00u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_206, 1, 486},
   {0x3088u, "ODMA_BUF_FREE", 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x000000FFu, 0x00000000u, fields_207, 1, 488},
   {0x308Cu, "ODMA_BUF_DONE", 0x00000000u, 0x00000000u, 0x00000000u, 0x000000FFu, 0x00000000u, 0x00000000u, fields_208, 1, 490},
   {0x3090u, "ODMA_FRAME_COUNT", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, fields_209, 1, 492},
   {0x30A0u, "ODMA_Y_ADDR_L0", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_210, 1, 494},
   {0x30A4u, "ODMA_Y_ADDR_H0", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_211, 1, 496},
   {0x30A8u, "ODMA_UV_ADDR_L0", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_212, 1, 498},
   {0x30ACu, "ODMA_UV_ADDR_H0", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_213, 1, 500},
   {0x30B0u, "ODMA_Y_ADDR_L1", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_214, 1, 502},
   {0x30B4u, "ODMA_Y_ADDR_H1", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_215, 1, 504},
   {0x30B8u, "ODMA_UV_ADDR_L1", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_216, 1, 506},
   {0x30BCu, "ODMA_UV_ADDR_H1", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_217, 1, 508},
   {0x30C0u, "ODMA_Y_ADDR_L2", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_218, 1, 510},
   {0x30C4u, "ODMA_Y_ADDR_H2", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_219, 1, 512},
   {0x30C8u, "ODMA_UV_ADDR_L2", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_220, 1, 514},
   {0x30CCu, "ODMA_UV_ADDR_H2", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_221, 1, 516},
   {0x30D0u, "ODMA_Y_ADDR_L3", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_222, 1, 518},
   {0x30D4u, "ODMA_Y_ADDR_H3", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_223, 1, 520},
   {0x30D8u, "ODMA_UV_ADDR_L3", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_224, 1, 522},
   {0x30DCu, "ODMA_UV_ADDR_H3", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_225, 1, 524},
};

const std::size_t num_registers = sizeof(registers) / sizeof(registers[0]);

}  // namespace cdc::components::fx1_isp::csr
