// SPDX-License-Identifier: Apache-2.0
// FX1 ISP register table -- GENERATED FILE, DO NOT EDIT.
// Generator: tools/gen_csr.py v2.0, source SHA-256 444e72f8d7fd3f35d83a7ce47d455b87dc6df6cda833b11302c6a45fb8b721fa

#include "registers/csr_desc.h"

namespace cdc::components::fx1_isp::csr {
namespace {

constexpr field_desc fields_0[] = {  // COMMON_VER_DATE
   {"year", 0, 16, 0x0000FFFFu, access::ro, 0x2026u, true, 3},
   {"month", 16, 8, 0x00FF0000u, access::ro, 0x10u, true, 4},
   {"day", 24, 8, 0xFF000000u, access::ro, 0x18u, true, 5},
};
constexpr field_desc fields_1[] = {  // COMMON_VER_ID
   {"version", 0, 32, 0xFFFFFFFFu, access::ro, 0x10007u, true, 7},
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
   {"updated", 2, 1, 0x00000004u, access::rw, 0x0u, false, 163},
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
   {"lut_load_confirm", 8, 1, 0x00000100u, access::w1p, 0x0u, true, 193},
};
constexpr field_desc fields_81[] = {  // GAMMA_LUT_ADDR
   {"addr", 0, 8, 0x000000FFu, access::rw, 0x0u, true, 195},
};
constexpr field_desc fields_82[] = {  // GAMMA_LUT_DATA
   {"data", 0, 12, 0x00000FFFu, access::rw, 0x0u, true, 197},
};
constexpr field_desc fields_83[] = {  // GAMMA_LUT_RDATA
   {"rdata", 0, 12, 0x00000FFFu, access::ro, 0x0u, true, 199},
};
constexpr field_desc fields_84[] = {  // GAMMA_STATUS
   {"busy", 0, 1, 0x00000001u, access::ro, 0x0u, true, 201},
   {"lut_loaded", 1, 1, 0x00000002u, access::ro, 0x0u, true, 202},
};
constexpr field_desc fields_85[] = {  // CSC_CTRL
   {"std", 0, 1, 0x00000001u, access::rw, 0x0u, true, 204},
};
constexpr field_desc fields_86[] = {  // GTM_CTRL
   {"en", 0, 1, 0x00000001u, access::rw, 0x0u, true, 206},
   {"manual", 1, 1, 0x00000002u, access::rw, 0x0u, true, 207},
};
constexpr field_desc fields_87[] = {  // GTM_LUT_ADDR
   {"addr", 0, 7, 0x0000007Fu, access::rw, 0x0u, true, 209},
};
constexpr field_desc fields_88[] = {  // GTM_LUT_DATA
   {"data", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 211},
};
constexpr field_desc fields_89[] = {  // GTM_KEY
   {"key", 0, 8, 0x000000FFu, access::rw, 0x2Eu, true, 213},
};
constexpr field_desc fields_90[] = {  // GTM_LWHITE
   {"lwhite", 0, 16, 0x0000FFFFu, access::rw, 0x400u, true, 215},
};
constexpr field_desc fields_91[] = {  // GTM_ROI_LOG2
   {"roi_log2", 0, 5, 0x0000001Fu, access::rw, 0xBu, true, 217},
};
constexpr field_desc fields_92[] = {  // GTM_LUT_RDATA
   {"rdata", 0, 16, 0x0000FFFFu, access::ro, 0x0u, true, 219},
};
constexpr field_desc fields_93[] = {  // GTM_STATUS
   {"busy", 0, 1, 0x00000001u, access::ro, 0x0u, true, 221},
};
constexpr field_desc fields_94[] = {  // NR_2D_CTRL
   {"en", 0, 1, 0x00000001u, access::rw, 0x0u, true, 223},
};
constexpr field_desc fields_95[] = {  // NR_2D_EST_VAR
   {"est_var", 0, 16, 0x0000FFFFu, access::ro, 0x0u, true, 225},
};
constexpr field_desc fields_96[] = {  // NR_2D_STATUS
   {"busy", 0, 1, 0x00000001u, access::ro, 0x0u, true, 227},
};
constexpr field_desc fields_97[] = {  // TNR_3D_CTRL
   {"en", 0, 1, 0x00000001u, access::rw, 0x0u, false, 229},
};
constexpr field_desc fields_98[] = {  // TNR_3D_MTF_SEL
   {"mtf", 0, 3, 0x00000007u, access::rw, 0x0u, false, 231},
};
constexpr field_desc fields_99[] = {  // TNR_3D_HIST_BASE
   {"hist_base_addr", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, false, 233},
};
constexpr field_desc fields_100[] = {  // EE_CTRL
   {"en", 0, 1, 0x00000001u, access::rw, 0x0u, true, 235},
   {"reg_update", 1, 1, 0x00000002u, access::rw, 0x0u, false, 236},
   {"profile_sel", 2, 2, 0x0000000Cu, access::rw, 0x0u, true, 237},
   {"profile_wr_sel", 4, 2, 0x00000030u, access::rw, 0x0u, true, 238},
};
constexpr field_desc fields_101[] = {  // EE_ALPHA
   {"alpha", 0, 16, 0x0000FFFFu, access::rw, 0x4000u, true, 240},
};
constexpr field_desc fields_102[] = {  // EE_BETA
   {"beta", 0, 16, 0x0000FFFFu, access::rw, 0x2000u, true, 242},
};
constexpr field_desc fields_103[] = {  // EE_CLAMP
   {"clamp_pos", 0, 8, 0x000000FFu, access::rw, 0x10u, true, 244},
   {"clamp_neg", 8, 8, 0x0000FF00u, access::rw, 0x18u, true, 245},
};
constexpr field_desc fields_104[] = {  // EE_FEATURE_EN
   {"activity_en", 0, 1, 0x00000001u, access::rw, 0x0u, true, 247},
   {"contrast_en", 1, 1, 0x00000002u, access::rw, 0x0u, true, 248},
   {"radial_en", 2, 1, 0x00000004u, access::rw, 0x0u, true, 249},
};
constexpr field_desc fields_105[] = {  // EE_RADIAL_CENTER
   {"center_x2", 0, 13, 0x00001FFFu, access::rw, 0x0u, true, 251},
   {"center_y2", 16, 13, 0x1FFF0000u, access::rw, 0x0u, true, 252},
};
constexpr field_desc fields_106[] = {  // EE_RADIAL_R2_TH0
   {"r2_th0", 0, 27, 0x07FFFFFFu, access::rw, 0x0u, true, 254},
};
constexpr field_desc fields_107[] = {  // EE_RADIAL_R2_TH1
   {"r2_th1", 0, 27, 0x07FFFFFFu, access::rw, 0x0u, true, 256},
};
constexpr field_desc fields_108[] = {  // EE_RADIAL_R2_TH2
   {"r2_th2", 0, 27, 0x07FFFFFFu, access::rw, 0x0u, true, 258},
};
constexpr field_desc fields_109[] = {  // EE_RADIAL_GAIN_01
   {"gain0", 0, 16, 0x0000FFFFu, access::rw, 0x8000u, true, 260},
   {"gain1", 16, 16, 0xFFFF0000u, access::rw, 0x8000u, true, 261},
};
constexpr field_desc fields_110[] = {  // EE_RADIAL_GAIN_23
   {"gain2", 0, 16, 0x0000FFFFu, access::rw, 0x8000u, true, 263},
   {"gain3", 16, 16, 0xFFFF0000u, access::rw, 0x8000u, true, 264},
};
constexpr field_desc fields_111[] = {  // EE_LUT_CTRL
   {"lut_sel", 1, 2, 0x00000006u, access::rw, 0x0u, true, 266},
   {"profile_load_confirm", 8, 1, 0x00000100u, access::w1p, 0x0u, true, 267},
};
constexpr field_desc fields_112[] = {  // EE_LUT_ADDR
   {"lut_addr", 0, 6, 0x0000003Fu, access::rw, 0x0u, true, 269},
};
constexpr field_desc fields_113[] = {  // EE_LUT_WDATA
   {"lut_wdata", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 271},
};
constexpr field_desc fields_114[] = {  // EE_STATUS
   {"busy", 0, 1, 0x00000001u, access::ro, 0x0u, true, 273},
   {"error", 1, 1, 0x00000002u, access::w1c, 0x0u, true, 274},
   {"lut_valid", 4, 3, 0x00000070u, access::ro, 0x0u, true, 275},
   {"active_profile", 8, 2, 0x00000300u, access::ro, 0x0u, true, 276},
   {"active_valid", 10, 1, 0x00000400u, access::ro, 0x0u, true, 277},
};
constexpr field_desc fields_115[] = {  // EE_LUT_RDATA
   {"rdata", 0, 16, 0x0000FFFFu, access::ro, 0x0u, true, 279},
};
constexpr field_desc fields_116[] = {  // CNF_CTRL
   {"en", 0, 1, 0x00000001u, access::rw, 0x0u, true, 281},
   {"updated", 2, 1, 0x00000004u, access::rw, 0x0u, false, 282},
};
constexpr field_desc fields_117[] = {  // CNF_CHROMA_TH
   {"chroma_th", 0, 9, 0x000001FFu, access::rw, 0x0u, true, 284},
};
constexpr field_desc fields_118[] = {  // CNF_LUMA_TH
   {"luma_th", 0, 8, 0x000000FFu, access::rw, 0x0u, true, 286},
};
constexpr field_desc fields_119[] = {  // CNF_STRENGTH
   {"mode_strength", 0, 2, 0x00000003u, access::rw, 0x0u, false, 288},
};
constexpr field_desc fields_120[] = {  // CNF_STATUS
   {"busy", 0, 1, 0x00000001u, access::ro, 0x0u, true, 290},
};
constexpr field_desc fields_121[] = {  // RESIZER_CTRL
   {"en", 0, 1, 0x00000001u, access::rw, 0x0u, true, 292},
   {"updated", 2, 1, 0x00000004u, access::rw, 0x0u, true, 293},
   {"scale", 3, 5, 0x000000F8u, access::rw, 0x0u, true, 294},
};
constexpr field_desc fields_122[] = {  // RESIZER_CUSTOM_W
   {"RESERVED", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 296},
};
constexpr field_desc fields_123[] = {  // RESIZER_CUSTOM_H
   {"RESERVED", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 298},
};
constexpr field_desc fields_124[] = {  // RESIZER_OUT_W
   {"out_width", 0, 13, 0x00001FFFu, access::ro, 0x0u, true, 300},
};
constexpr field_desc fields_125[] = {  // RESIZER_OUT_H
   {"out_height", 0, 13, 0x00001FFFu, access::ro, 0x0u, true, 302},
};
constexpr field_desc fields_126[] = {  // RESIZER_STATUS
   {"busy", 0, 1, 0x00000001u, access::ro, 0x0u, true, 304},
};
constexpr field_desc fields_127[] = {  // OUTPUT_ENABLE
   {"channels", 0, 3, 0x00000007u, access::rw, 0x1u, true, 306},
};
constexpr field_desc fields_128[] = {  // OUTPUT_IRQ_EN
   {"done", 0, 3, 0x00000007u, access::rw, 0x0u, true, 308},
   {"error", 4, 3, 0x00000070u, access::rw, 0x0u, true, 309},
};
constexpr field_desc fields_129[] = {  // OUTPUT_IRQ_STATUS
   {"done", 0, 3, 0x00000007u, access::w1c, 0x0u, true, 311},
   {"error", 4, 3, 0x00000070u, access::w1c, 0x0u, true, 312},
};
constexpr field_desc fields_130[] = {  // AEC_CTRL
   {"en", 0, 1, 0x00000001u, access::rw, 0x0u, true, 314},
   {"commit", 1, 1, 0x00000002u, access::w1s, 0x0u, true, 315},
};
constexpr field_desc fields_131[] = {  // AEC_CONTEXT_ID
   {"context_id", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 317},
};
constexpr field_desc fields_132[] = {  // AEC_ZONE_CFG
   {"num_zone_x", 0, 6, 0x0000003Fu, access::rw, 0x20u, true, 319},
   {"num_zone_y", 8, 5, 0x00001F00u, access::rw, 0x18u, true, 320},
};
constexpr field_desc fields_133[] = {  // AEC_ZONE_SIZE
   {"zone_width", 0, 12, 0x00000FFFu, access::rw, 0x78u, true, 322},
   {"zone_height", 16, 12, 0x0FFF0000u, access::rw, 0x5Au, true, 323},
};
constexpr field_desc fields_134[] = {  // AEC_SAMPLE_CLIP
   {"sample_min_clip", 0, 12, 0x00000FFFu, access::rw, 0x0u, true, 325},
   {"sample_max_clip", 16, 12, 0x0FFF0000u, access::rw, 0xFFFu, true, 326},
};
constexpr field_desc fields_135[] = {  // AEC_THRESH
   {"th_ue", 0, 12, 0x00000FFFu, access::rw, 0x110u, true, 328},
   {"th_oe", 16, 12, 0x0FFF0000u, access::rw, 0xEA0u, true, 329},
};
constexpr field_desc fields_136[] = {  // AEC_STATUS
   {"busy", 0, 1, 0x00000001u, access::ro, 0x0u, true, 331},
   {"stat_done", 1, 1, 0x00000002u, access::w1c, 0x0u, true, 332},
   {"error", 2, 1, 0x00000004u, access::w1c, 0x0u, false, 333},
};
constexpr field_desc fields_137[] = {  // AEC_FRAME_ID
   {"frame_id", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 335},
};
constexpr field_desc fields_138[] = {  // AEC_RESULT_CONTEXT_ID
   {"context_id", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 337},
};
constexpr field_desc fields_139[] = {  // AEC_CHANNEL_SEL
   {"channel_sel", 0, 2, 0x00000003u, access::rw, 0x0u, true, 339},
};
constexpr field_desc fields_140[] = {  // AEC_GLOBAL_SUM_LO
   {"global_sum_lo", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 341},
};
constexpr field_desc fields_141[] = {  // AEC_GLOBAL_SUM_HI
   {"global_sum_hi", 0, 1, 0x00000001u, access::ro, 0x0u, true, 343},
};
constexpr field_desc fields_142[] = {  // AEC_GLOBAL_COUNT
   {"global_count", 0, 23, 0x007FFFFFu, access::ro, 0x0u, true, 345},
};
constexpr field_desc fields_143[] = {  // AEC_ZONE_ADDR
   {"zone_addr", 0, 10, 0x000003FFu, access::rw, 0x0u, true, 347},
};
constexpr field_desc fields_144[] = {  // AEC_ZONE_SUM
   {"zone_sum", 0, 28, 0x0FFFFFFFu, access::ro, 0x0u, true, 349},
};
constexpr field_desc fields_145[] = {  // AEC_ZONE_COUNT
   {"zone_count", 0, 16, 0x0000FFFFu, access::ro, 0x0u, true, 351},
};
constexpr field_desc fields_146[] = {  // AEC_ZONE_GREEN_OE_UE
   {"green_ue_count", 0, 16, 0x0000FFFFu, access::ro, 0x0u, true, 353},
   {"green_oe_count", 16, 16, 0xFFFF0000u, access::ro, 0x0u, true, 354},
};
constexpr field_desc fields_147[] = {  // AEC_ZONE_GREEN_MIN_MAX
   {"green_min", 0, 12, 0x00000FFFu, access::ro, 0x0u, true, 356},
   {"green_max", 12, 12, 0x00FFF000u, access::ro, 0x0u, true, 357},
};
constexpr field_desc fields_148[] = {  // AEC_HIST_ADDR
   {"hist_addr", 0, 6, 0x0000003Fu, access::rw, 0x0u, true, 359},
};
constexpr field_desc fields_149[] = {  // AEC_HIST_DATA
   {"hist_count", 0, 23, 0x007FFFFFu, access::ro, 0x0u, true, 361},
};
constexpr field_desc fields_150[] = {  // AWB_CTRL
   {"en", 0, 1, 0x00000001u, access::rw, 0x0u, true, 363},
   {"num_zone_x", 1, 7, 0x000000FEu, access::rw, 0x0u, true, 364},
   {"num_zone_y", 8, 6, 0x00003F00u, access::rw, 0x0u, true, 365},
};
constexpr field_desc fields_151[] = {  // AWB_X_BOUND_ADDR
   {"x_bound_addr", 0, 6, 0x0000003Fu, access::rw, 0x0u, false, 367},
};
constexpr field_desc fields_152[] = {  // AWB_Y_BOUND_ADDR
   {"y_bound_addr", 0, 5, 0x0000001Fu, access::rw, 0x0u, false, 369},
};
constexpr field_desc fields_153[] = {  // AWB_X_BOUND_DATA
   {"x_bound_data", 0, 12, 0x00000FFFu, access::rw, 0x0u, false, 371},
   {"x_bound_we", 12, 1, 0x00001000u, access::rw, 0x0u, false, 372},
};
constexpr field_desc fields_154[] = {  // AWB_Y_BOUND_DATA
   {"y_bound_data", 0, 12, 0x00000FFFu, access::rw, 0x0u, false, 374},
   {"y_bound_we", 12, 1, 0x00001000u, access::rw, 0x0u, false, 375},
};
constexpr field_desc fields_155[] = {  // AWB_GLOBAL_SUM_R_H
   {"global_sum_r_h", 0, 3, 0x00000007u, access::ro, 0x0u, true, 377},
};
constexpr field_desc fields_156[] = {  // AWB_GLOBAL_SUM_R_L
   {"global_sum_r_l", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 379},
};
constexpr field_desc fields_157[] = {  // AWB_GLOBAL_SUM_G_H
   {"global_sum_g_h", 0, 3, 0x00000007u, access::ro, 0x0u, true, 381},
};
constexpr field_desc fields_158[] = {  // AWB_GLOBAL_SUM_G_L
   {"global_sum_g_l", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 383},
};
constexpr field_desc fields_159[] = {  // AWB_GLOBAL_SUM_B_H
   {"global_sum_b_h", 0, 3, 0x00000007u, access::ro, 0x0u, true, 385},
};
constexpr field_desc fields_160[] = {  // AWB_GLOBAL_SUM_B_L
   {"global_sum_b_l", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 387},
};
constexpr field_desc fields_161[] = {  // AWB_GLOBAL_COUNT
   {"global_count", 0, 23, 0x007FFFFFu, access::ro, 0x0u, true, 389},
};
constexpr field_desc fields_162[] = {  // AWB_ZONE_ADDR
   {"zone_addr", 0, 11, 0x000007FFu, access::rw, 0x0u, true, 391},
};
constexpr field_desc fields_163[] = {  // AWB_ZONE_SUM_R_H
   {"zone_sum_r_h", 0, 3, 0x00000007u, access::ro, 0x0u, true, 393},
};
constexpr field_desc fields_164[] = {  // AWB_ZONE_SUM_R_L
   {"zone_sum_r_l", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 395},
};
constexpr field_desc fields_165[] = {  // AWB_ZONE_SUM_G_H
   {"zone_sum_g_h", 0, 3, 0x00000007u, access::ro, 0x0u, true, 397},
};
constexpr field_desc fields_166[] = {  // AWB_ZONE_SUM_G_L
   {"zone_sum_g_l", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 399},
};
constexpr field_desc fields_167[] = {  // AWB_ZONE_SUM_B_H
   {"zone_sum_b_h", 0, 3, 0x00000007u, access::ro, 0x0u, true, 401},
};
constexpr field_desc fields_168[] = {  // AWB_ZONE_SUM_B_L
   {"zone_sum_b_l", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 403},
};
constexpr field_desc fields_169[] = {  // AWB_ZONE_COUNT
   {"zone_count", 0, 23, 0x007FFFFFu, access::ro, 0x0u, true, 405},
};
constexpr field_desc fields_170[] = {  // AWB_STATUS
   {"stat_done", 0, 1, 0x00000001u, access::w1c, 0x0u, true, 407},
};
constexpr field_desc fields_171[] = {  // AWB_UNDEREXPOSED_LIMIT
   {"underexposed_limit", 0, 12, 0x00000FFFu, access::rw, 0x0u, true, 409},
};
constexpr field_desc fields_172[] = {  // AWB_SATURATION_LIMIT
   {"saturation_limit", 0, 12, 0x00000FFFu, access::rw, 0xFFFu, true, 411},
};
constexpr field_desc fields_173[] = {  // AWB_CONTEXT_ID
   {"context_id", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 413},
};
constexpr field_desc fields_174[] = {  // AWB_RESULT_CONTEXT_ID
   {"context_id", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 415},
};
constexpr field_desc fields_175[] = {  // AWB_FRAME_ID
   {"frame_id", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 417},
};
constexpr field_desc fields_176[] = {  // AWB_RESULT_CTRL
   {"acquire", 0, 1, 0x00000001u, access::w1p, 0x0u, true, 419},
   {"release", 1, 1, 0x00000002u, access::w1p, 0x0u, true, 420},
   {"clear_report", 2, 1, 0x00000004u, access::w1p, 0x0u, true, 421},
};
constexpr field_desc fields_177[] = {  // AWB_RESULT_STATUS
   {"ready_count", 0, 2, 0x00000003u, access::ro, 0x0u, true, 423},
   {"leased", 2, 1, 0x00000004u, access::ro, 0x0u, true, 424},
   {"read_valid", 3, 1, 0x00000008u, access::ro, 0x0u, true, 425},
   {"read_bank", 4, 1, 0x00000010u, access::ro, 0x0u, true, 426},
   {"write_busy", 5, 1, 0x00000020u, access::ro, 0x0u, true, 427},
   {"overrun", 6, 1, 0x00000040u, access::ro, 0x0u, true, 428},
   {"read_num_zone_x", 8, 7, 0x00007F00u, access::ro, 0x0u, true, 429},
   {"read_num_zone_y", 16, 6, 0x003F0000u, access::ro, 0x0u, true, 430},
};
constexpr field_desc fields_178[] = {  // AWB_DROP_COUNT
   {"drop_count", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 432},
};
constexpr field_desc fields_179[] = {  // AF_CTRL
   {"en", 0, 1, 0x00000001u, access::rw, 0x0u, true, 434},
   {"bypass", 1, 1, 0x00000002u, access::rw, 0x0u, false, 435},
   {"reg_update", 2, 1, 0x00000004u, access::rw, 0x0u, false, 436},
   {"search_start", 3, 1, 0x00000008u, access::w1s, 0x0u, false, 437},
   {"search_abort", 4, 1, 0x00000010u, access::w1s, 0x0u, false, 438},
   {"lut_en", 5, 1, 0x00000020u, access::rw, 0x0u, false, 439},
   {"metric_sel", 6, 2, 0x000000C0u, access::rw, 0x0u, false, 440},
   {"vcm_ack", 8, 1, 0x00000100u, access::rw, 0x0u, false, 441},
};
constexpr field_desc fields_180[] = {  // AF_ZONE_EN
   {"zone_en", 0, 16, 0x0000FFFFu, access::rw, 0xFFFFu, false, 443},
};
constexpr field_desc fields_181[] = {  // AF_METRIC_CFG
   {"min_score", 0, 16, 0x0000FFFFu, access::rw, 0x0u, false, 445},
   {"settle_frames", 16, 8, 0x00FF0000u, access::rw, 0x0u, false, 446},
};
constexpr field_desc fields_182[] = {  // AF_STAT_ADDR
   {"zone_addr", 0, 4, 0x0000000Fu, access::rw, 0x0u, true, 448},
};
constexpr field_desc fields_183[] = {  // AF_STAT_DATA
   {"focus_score", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 450},
};
constexpr field_desc fields_184[] = {  // AF_LUT_ADDR
   {"lut_addr", 0, 8, 0x000000FFu, access::rw, 0x0u, false, 452},
};
constexpr field_desc fields_185[] = {  // AF_LUT_DATA
   {"vcm_position", 0, 16, 0x0000FFFFu, access::rw, 0x0u, false, 454},
};
constexpr field_desc fields_186[] = {  // AF_VCM_CUR_POS
   {"vcm_cur_pos", 0, 16, 0x0000FFFFu, access::rw, 0x0u, false, 456},
};
constexpr field_desc fields_187[] = {  // AF_VCM_BEST_POS
   {"vcm_best_pos", 0, 16, 0x0000FFFFu, access::ro, 0x0u, false, 458},
};
constexpr field_desc fields_188[] = {  // AF_PEAK_SCORE
   {"peak_score", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, false, 460},
};
constexpr field_desc fields_189[] = {  // AF_FOCUS_RANGE
   {"accept_delta", 0, 16, 0x0000FFFFu, access::rw, 0x0u, false, 462},
   {"hof_range", 16, 8, 0x00FF0000u, access::rw, 0x0u, false, 463},
};
constexpr field_desc fields_190[] = {  // AF_STATUS
   {"busy", 0, 1, 0x00000001u, access::ro, 0x0u, true, 465},
   {"frame_done", 1, 1, 0x00000002u, access::w1c, 0x0u, true, 466},
   {"search_done", 2, 1, 0x00000004u, access::w1c, 0x0u, false, 467},
   {"vcm_pos_valid", 3, 1, 0x00000008u, access::w1c, 0x0u, false, 468},
   {"score_valid", 5, 1, 0x00000020u, access::ro, 0x0u, true, 469},
};
constexpr field_desc fields_191[] = {  // AF_CONTEXT_ID
   {"context_id", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 471},
};
constexpr field_desc fields_192[] = {  // AF_RESULT_CONTEXT_ID
   {"context_id", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 473},
};
constexpr field_desc fields_193[] = {  // AF_FRAME_ID
   {"frame_id", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 475},
};
constexpr field_desc fields_194[] = {  // OFMT_CTRL
   {"stride_en", 0, 1, 0x00000001u, access::rw, 0x0u, true, 477},
};
constexpr field_desc fields_195[] = {  // OFMT_Y_STRIDE
   {"y_stride", 0, 16, 0x0000FFFFu, access::rw, 0xF00u, true, 479},
};
constexpr field_desc fields_196[] = {  // OFMT_UV_STRIDE
   {"uv_stride", 0, 16, 0x0000FFFFu, access::rw, 0xF00u, true, 481},
};
constexpr field_desc fields_197[] = {  // DMA_CTRL
   {"idma_en", 0, 1, 0x00000001u, access::rw, 0x0u, true, 483},
   {"odma_en", 1, 1, 0x00000002u, access::rw, 0x0u, true, 484},
   {"soft_reset", 2, 1, 0x00000004u, access::w1sc, 0x0u, true, 485},
   {"max_burst_m1", 8, 8, 0x0000FF00u, access::rw, 0x3Fu, true, 486},
};
constexpr field_desc fields_198[] = {  // DMA_STAT
   {"idma_busy", 0, 1, 0x00000001u, access::ro, 0x0u, true, 488},
   {"odma_busy", 1, 1, 0x00000002u, access::ro, 0x0u, true, 489},
};
constexpr field_desc fields_199[] = {  // DMA_ERR
   {"err", 0, 6, 0x0000001Fu, access::w1c, 0x0u, true, 491},
};
constexpr field_desc fields_200[] = {  // DMA_IRQ_EN
   {"irq_en", 0, 7, 0x0000007Fu, access::rw, 0x0u, true, 493},
};
constexpr field_desc fields_201[] = {  // DMA_IRQ_STAT
   {"irq", 0, 7, 0x0000007Fu, access::w1c, 0x0u, true, 495},
};
constexpr field_desc fields_202[] = {  // IDMA_STRIDE
   {"idma_stride", 0, 32, 0xFFFFFFFFu, access::rw, 0x1E00u, true, 497},
};
constexpr field_desc fields_203[] = {  // IDMA_BUF_VALID
   {"idma_buf_valid", 0, 8, 0x0000000Fu, access::w1s, 0x0u, true, 499},
};
constexpr field_desc fields_204[] = {  // IDMA_FRAME_COUNT
   {"idma_frame_count", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 501},
};
constexpr field_desc fields_205[] = {  // IDMA_BUF_ADDR_L0
   {"idma_buf_addr0_lo", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 503},
};
constexpr field_desc fields_206[] = {  // IDMA_BUF_ADDR_H0
   {"idma_buf_addr0_hi", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 505},
};
constexpr field_desc fields_207[] = {  // IDMA_BUF_ADDR_L1
   {"idma_buf_addr1_lo", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 507},
};
constexpr field_desc fields_208[] = {  // IDMA_BUF_ADDR_H1
   {"idma_buf_addr1_hi", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 509},
};
constexpr field_desc fields_209[] = {  // IDMA_BUF_ADDR_L2
   {"idma_buf_addr2_lo", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 511},
};
constexpr field_desc fields_210[] = {  // IDMA_BUF_ADDR_H2
   {"idma_buf_addr2_hi", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 513},
};
constexpr field_desc fields_211[] = {  // IDMA_BUF_ADDR_L3
   {"idma_buf_addr3_lo", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 515},
};
constexpr field_desc fields_212[] = {  // IDMA_BUF_ADDR_H3
   {"idma_buf_addr3_hi", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 517},
};
constexpr field_desc fields_213[] = {  // ODMA_Y_STRIDE
   {"y_stride", 0, 32, 0xFFFFFFFFu, access::rw, 0xF00u, true, 519},
};
constexpr field_desc fields_214[] = {  // ODMA_UV_STRIDE
   {"uv_stride", 0, 32, 0xFFFFFFFFu, access::rw, 0xF00u, true, 521},
};
constexpr field_desc fields_215[] = {  // ODMA_BUF_FREE
   {"odma_buf_free", 0, 8, 0x0000000Fu, access::w1s, 0x0u, true, 523},
};
constexpr field_desc fields_216[] = {  // ODMA_BUF_DONE
   {"odma_buf_done", 0, 8, 0x0000000Fu, access::w1c, 0x0u, true, 525},
};
constexpr field_desc fields_217[] = {  // ODMA_FRAME_COUNT
   {"odma_frame_count", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 527},
};
constexpr field_desc fields_218[] = {  // ODMA_Y_ADDR_L0
   {"odma_y_addr0_lo", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 529},
};
constexpr field_desc fields_219[] = {  // ODMA_Y_ADDR_H0
   {"odma_y_addr0_hi", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 531},
};
constexpr field_desc fields_220[] = {  // ODMA_UV_ADDR_L0
   {"odma_uv_addr0_lo", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 533},
};
constexpr field_desc fields_221[] = {  // ODMA_UV_ADDR_H0
   {"odma_uv_addr0_hi", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 535},
};
constexpr field_desc fields_222[] = {  // ODMA_Y_ADDR_L1
   {"odma_y_addr1_lo", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 537},
};
constexpr field_desc fields_223[] = {  // ODMA_Y_ADDR_H1
   {"odma_y_addr1_hi", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 539},
};
constexpr field_desc fields_224[] = {  // ODMA_UV_ADDR_L1
   {"odma_uv_addr1_lo", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 541},
};
constexpr field_desc fields_225[] = {  // ODMA_UV_ADDR_H1
   {"odma_uv_addr1_hi", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 543},
};
constexpr field_desc fields_226[] = {  // ODMA_Y_ADDR_L2
   {"odma_y_addr2_lo", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 545},
};
constexpr field_desc fields_227[] = {  // ODMA_Y_ADDR_H2
   {"odma_y_addr2_hi", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 547},
};
constexpr field_desc fields_228[] = {  // ODMA_UV_ADDR_L2
   {"odma_uv_addr2_lo", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 549},
};
constexpr field_desc fields_229[] = {  // ODMA_UV_ADDR_H2
   {"odma_uv_addr2_hi", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 551},
};
constexpr field_desc fields_230[] = {  // ODMA_Y_ADDR_L3
   {"odma_y_addr3_lo", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 553},
};
constexpr field_desc fields_231[] = {  // ODMA_Y_ADDR_H3
   {"odma_y_addr3_hi", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 555},
};
constexpr field_desc fields_232[] = {  // ODMA_UV_ADDR_L3
   {"odma_uv_addr3_lo", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 557},
};
constexpr field_desc fields_233[] = {  // ODMA_UV_ADDR_H3
   {"odma_uv_addr3_hi", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 559},
};
constexpr field_desc fields_234[] = {  // IDMA_BUF_TAG0
   {"tag", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 561},
};
constexpr field_desc fields_235[] = {  // IDMA_BUF_TAG1
   {"tag", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 563},
};
constexpr field_desc fields_236[] = {  // IDMA_BUF_TAG2
   {"tag", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 565},
};
constexpr field_desc fields_237[] = {  // IDMA_BUF_TAG3
   {"tag", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 567},
};
constexpr field_desc fields_238[] = {  // IDMA_BUF_TS0
   {"timestamp", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 569},
};
constexpr field_desc fields_239[] = {  // IDMA_BUF_TS1
   {"timestamp", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 571},
};
constexpr field_desc fields_240[] = {  // IDMA_BUF_TS2
   {"timestamp", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 573},
};
constexpr field_desc fields_241[] = {  // IDMA_BUF_TS3
   {"timestamp", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 575},
};
constexpr field_desc fields_242[] = {  // ODMA_BUF_TAG0
   {"tag", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 577},
};
constexpr field_desc fields_243[] = {  // ODMA_BUF_TAG1
   {"tag", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 579},
};
constexpr field_desc fields_244[] = {  // ODMA_BUF_TAG2
   {"tag", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 581},
};
constexpr field_desc fields_245[] = {  // ODMA_BUF_TAG3
   {"tag", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 583},
};
constexpr field_desc fields_246[] = {  // ODMA_BUF_TS0
   {"timestamp", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 585},
};
constexpr field_desc fields_247[] = {  // ODMA_BUF_TS1
   {"timestamp", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 587},
};
constexpr field_desc fields_248[] = {  // ODMA_BUF_TS2
   {"timestamp", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 589},
};
constexpr field_desc fields_249[] = {  // ODMA_BUF_TS3
   {"timestamp", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 591},
};
constexpr field_desc fields_250[] = {  // ODMA_BUF_SOF_TS0
   {"timestamp", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 593},
};
constexpr field_desc fields_251[] = {  // ODMA_BUF_SOF_TS1
   {"timestamp", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 595},
};
constexpr field_desc fields_252[] = {  // ODMA_BUF_SOF_TS2
   {"timestamp", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 597},
};
constexpr field_desc fields_253[] = {  // ODMA_BUF_SOF_TS3
   {"timestamp", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 599},
};
constexpr field_desc fields_254[] = {  // ODMA_BUF_EOF_TS0
   {"timestamp", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 601},
};
constexpr field_desc fields_255[] = {  // ODMA_BUF_EOF_TS1
   {"timestamp", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 603},
};
constexpr field_desc fields_256[] = {  // ODMA_BUF_EOF_TS2
   {"timestamp", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 605},
};
constexpr field_desc fields_257[] = {  // ODMA_BUF_EOF_TS3
   {"timestamp", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 607},
};
constexpr field_desc fields_258[] = {  // FRAME_COUNTER
   {"counter", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 609},
};
constexpr field_desc fields_259[] = {  // CH1_RESIZER_CTRL
   {"en", 0, 1, 0x00000001u, access::rw, 0x0u, true, 611},
   {"updated", 2, 1, 0x00000004u, access::rw, 0x0u, true, 612},
   {"scale", 3, 5, 0x000000F8u, access::rw, 0x0u, true, 613},
};
constexpr field_desc fields_260[] = {  // CH1_RESIZER_CUSTOM_W
   {"RESERVED", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 615},
};
constexpr field_desc fields_261[] = {  // CH1_RESIZER_CUSTOM_H
   {"RESERVED", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 617},
};
constexpr field_desc fields_262[] = {  // CH1_RESIZER_OUT_W
   {"out_width", 0, 13, 0x00001FFFu, access::ro, 0x0u, true, 619},
};
constexpr field_desc fields_263[] = {  // CH1_RESIZER_OUT_H
   {"out_height", 0, 13, 0x00001FFFu, access::ro, 0x0u, true, 621},
};
constexpr field_desc fields_264[] = {  // CH1_RESIZER_STATUS
   {"busy", 0, 1, 0x00000001u, access::ro, 0x0u, true, 623},
};
constexpr field_desc fields_265[] = {  // CH1_ODMA_Y_STRIDE
   {"y_stride", 0, 32, 0xFFFFFFFFu, access::rw, 0xF00u, true, 625},
};
constexpr field_desc fields_266[] = {  // CH1_ODMA_UV_STRIDE
   {"uv_stride", 0, 32, 0xFFFFFFFFu, access::rw, 0xF00u, true, 627},
};
constexpr field_desc fields_267[] = {  // CH1_ODMA_BUF_FREE
   {"odma_buf_free", 0, 8, 0x0000000Fu, access::w1s, 0x0u, true, 629},
};
constexpr field_desc fields_268[] = {  // CH1_ODMA_BUF_DONE
   {"odma_buf_done", 0, 8, 0x0000000Fu, access::w1c, 0x0u, true, 631},
};
constexpr field_desc fields_269[] = {  // CH1_ODMA_FRAME_COUNT
   {"odma_frame_count", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 633},
};
constexpr field_desc fields_270[] = {  // CH1_ODMA_Y_ADDR_L0
   {"odma_y_addr0_lo", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 635},
};
constexpr field_desc fields_271[] = {  // CH1_ODMA_Y_ADDR_H0
   {"odma_y_addr0_hi", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 637},
};
constexpr field_desc fields_272[] = {  // CH1_ODMA_UV_ADDR_L0
   {"odma_uv_addr0_lo", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 639},
};
constexpr field_desc fields_273[] = {  // CH1_ODMA_UV_ADDR_H0
   {"odma_uv_addr0_hi", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 641},
};
constexpr field_desc fields_274[] = {  // CH1_ODMA_Y_ADDR_L1
   {"odma_y_addr1_lo", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 643},
};
constexpr field_desc fields_275[] = {  // CH1_ODMA_Y_ADDR_H1
   {"odma_y_addr1_hi", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 645},
};
constexpr field_desc fields_276[] = {  // CH1_ODMA_UV_ADDR_L1
   {"odma_uv_addr1_lo", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 647},
};
constexpr field_desc fields_277[] = {  // CH1_ODMA_UV_ADDR_H1
   {"odma_uv_addr1_hi", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 649},
};
constexpr field_desc fields_278[] = {  // CH1_ODMA_Y_ADDR_L2
   {"odma_y_addr2_lo", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 651},
};
constexpr field_desc fields_279[] = {  // CH1_ODMA_Y_ADDR_H2
   {"odma_y_addr2_hi", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 653},
};
constexpr field_desc fields_280[] = {  // CH1_ODMA_UV_ADDR_L2
   {"odma_uv_addr2_lo", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 655},
};
constexpr field_desc fields_281[] = {  // CH1_ODMA_UV_ADDR_H2
   {"odma_uv_addr2_hi", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 657},
};
constexpr field_desc fields_282[] = {  // CH1_ODMA_Y_ADDR_L3
   {"odma_y_addr3_lo", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 659},
};
constexpr field_desc fields_283[] = {  // CH1_ODMA_Y_ADDR_H3
   {"odma_y_addr3_hi", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 661},
};
constexpr field_desc fields_284[] = {  // CH1_ODMA_UV_ADDR_L3
   {"odma_uv_addr3_lo", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 663},
};
constexpr field_desc fields_285[] = {  // CH1_ODMA_UV_ADDR_H3
   {"odma_uv_addr3_hi", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 665},
};
constexpr field_desc fields_286[] = {  // CH1_ODMA_BUF_TAG0
   {"tag", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 667},
};
constexpr field_desc fields_287[] = {  // CH1_ODMA_BUF_TAG1
   {"tag", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 669},
};
constexpr field_desc fields_288[] = {  // CH1_ODMA_BUF_TAG2
   {"tag", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 671},
};
constexpr field_desc fields_289[] = {  // CH1_ODMA_BUF_TAG3
   {"tag", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 673},
};
constexpr field_desc fields_290[] = {  // CH1_ODMA_BUF_TS0
   {"timestamp", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 675},
};
constexpr field_desc fields_291[] = {  // CH1_ODMA_BUF_TS1
   {"timestamp", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 677},
};
constexpr field_desc fields_292[] = {  // CH1_ODMA_BUF_TS2
   {"timestamp", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 679},
};
constexpr field_desc fields_293[] = {  // CH1_ODMA_BUF_TS3
   {"timestamp", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 681},
};
constexpr field_desc fields_294[] = {  // CH1_ODMA_BUF_SOF_TS0
   {"timestamp", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 683},
};
constexpr field_desc fields_295[] = {  // CH1_ODMA_BUF_SOF_TS1
   {"timestamp", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 685},
};
constexpr field_desc fields_296[] = {  // CH1_ODMA_BUF_SOF_TS2
   {"timestamp", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 687},
};
constexpr field_desc fields_297[] = {  // CH1_ODMA_BUF_SOF_TS3
   {"timestamp", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 689},
};
constexpr field_desc fields_298[] = {  // CH1_ODMA_BUF_EOF_TS0
   {"timestamp", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 691},
};
constexpr field_desc fields_299[] = {  // CH1_ODMA_BUF_EOF_TS1
   {"timestamp", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 693},
};
constexpr field_desc fields_300[] = {  // CH1_ODMA_BUF_EOF_TS2
   {"timestamp", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 695},
};
constexpr field_desc fields_301[] = {  // CH1_ODMA_BUF_EOF_TS3
   {"timestamp", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 697},
};
constexpr field_desc fields_302[] = {  // CH2_RESIZER_CTRL
   {"en", 0, 1, 0x00000001u, access::rw, 0x0u, true, 699},
   {"updated", 2, 1, 0x00000004u, access::rw, 0x0u, true, 700},
   {"scale", 3, 5, 0x000000F8u, access::rw, 0x0u, true, 701},
};
constexpr field_desc fields_303[] = {  // CH2_RESIZER_CUSTOM_W
   {"RESERVED", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 703},
};
constexpr field_desc fields_304[] = {  // CH2_RESIZER_CUSTOM_H
   {"RESERVED", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 705},
};
constexpr field_desc fields_305[] = {  // CH2_RESIZER_OUT_W
   {"out_width", 0, 13, 0x00001FFFu, access::ro, 0x0u, true, 707},
};
constexpr field_desc fields_306[] = {  // CH2_RESIZER_OUT_H
   {"out_height", 0, 13, 0x00001FFFu, access::ro, 0x0u, true, 709},
};
constexpr field_desc fields_307[] = {  // CH2_RESIZER_STATUS
   {"busy", 0, 1, 0x00000001u, access::ro, 0x0u, true, 711},
};
constexpr field_desc fields_308[] = {  // CH2_ODMA_Y_STRIDE
   {"y_stride", 0, 32, 0xFFFFFFFFu, access::rw, 0xF00u, true, 713},
};
constexpr field_desc fields_309[] = {  // CH2_ODMA_BUF_FREE
   {"odma_buf_free", 0, 8, 0x0000000Fu, access::w1s, 0x0u, true, 715},
};
constexpr field_desc fields_310[] = {  // CH2_ODMA_BUF_DONE
   {"odma_buf_done", 0, 8, 0x0000000Fu, access::w1c, 0x0u, true, 717},
};
constexpr field_desc fields_311[] = {  // CH2_ODMA_FRAME_COUNT
   {"odma_frame_count", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 719},
};
constexpr field_desc fields_312[] = {  // CH2_ODMA_Y_ADDR_L0
   {"odma_y_addr0_lo", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 721},
};
constexpr field_desc fields_313[] = {  // CH2_ODMA_Y_ADDR_H0
   {"odma_y_addr0_hi", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 723},
};
constexpr field_desc fields_314[] = {  // CH2_ODMA_Y_ADDR_L1
   {"odma_y_addr1_lo", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 725},
};
constexpr field_desc fields_315[] = {  // CH2_ODMA_Y_ADDR_H1
   {"odma_y_addr1_hi", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 727},
};
constexpr field_desc fields_316[] = {  // CH2_ODMA_Y_ADDR_L2
   {"odma_y_addr2_lo", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 729},
};
constexpr field_desc fields_317[] = {  // CH2_ODMA_Y_ADDR_H2
   {"odma_y_addr2_hi", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 731},
};
constexpr field_desc fields_318[] = {  // CH2_ODMA_Y_ADDR_L3
   {"odma_y_addr3_lo", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 733},
};
constexpr field_desc fields_319[] = {  // CH2_ODMA_Y_ADDR_H3
   {"odma_y_addr3_hi", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 735},
};
constexpr field_desc fields_320[] = {  // CH2_ODMA_BUF_TAG0
   {"tag", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 737},
};
constexpr field_desc fields_321[] = {  // CH2_ODMA_BUF_TAG1
   {"tag", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 739},
};
constexpr field_desc fields_322[] = {  // CH2_ODMA_BUF_TAG2
   {"tag", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 741},
};
constexpr field_desc fields_323[] = {  // CH2_ODMA_BUF_TAG3
   {"tag", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 743},
};
constexpr field_desc fields_324[] = {  // CH2_ODMA_BUF_TS0
   {"timestamp", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 745},
};
constexpr field_desc fields_325[] = {  // CH2_ODMA_BUF_TS1
   {"timestamp", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 747},
};
constexpr field_desc fields_326[] = {  // CH2_ODMA_BUF_TS2
   {"timestamp", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 749},
};
constexpr field_desc fields_327[] = {  // CH2_ODMA_BUF_TS3
   {"timestamp", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 751},
};
constexpr field_desc fields_328[] = {  // CH2_ODMA_BUF_SOF_TS0
   {"timestamp", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 753},
};
constexpr field_desc fields_329[] = {  // CH2_ODMA_BUF_SOF_TS1
   {"timestamp", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 755},
};
constexpr field_desc fields_330[] = {  // CH2_ODMA_BUF_SOF_TS2
   {"timestamp", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 757},
};
constexpr field_desc fields_331[] = {  // CH2_ODMA_BUF_SOF_TS3
   {"timestamp", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 759},
};
constexpr field_desc fields_332[] = {  // CH2_ODMA_BUF_EOF_TS0
   {"timestamp", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 761},
};
constexpr field_desc fields_333[] = {  // CH2_ODMA_BUF_EOF_TS1
   {"timestamp", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 763},
};
constexpr field_desc fields_334[] = {  // CH2_ODMA_BUF_EOF_TS2
   {"timestamp", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 765},
};
constexpr field_desc fields_335[] = {  // CH2_ODMA_BUF_EOF_TS3
   {"timestamp", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 767},
};
constexpr field_desc fields_336[] = {  // TUNE_ID
   {"MAGIC", 0, 32, 0xFFFFFFFFu, access::ro, 0x54554E45u, true, 769},
};
constexpr field_desc fields_337[] = {  // TUNE_REV
   {"VALUE", 0, 32, 0xFFFFFFFFu, access::ro, 0x10007u, true, 771},
};
constexpr field_desc fields_338[] = {  // TUNE_CAPS
   {"PROFILE", 0, 1, 0x00000001u, access::ro, 0x1u, true, 773},
   {"NR", 1, 1, 0x00000002u, access::ro, 0x1u, true, 774},
   {"AWB_COLOR", 2, 1, 0x00000004u, access::ro, 0x1u, true, 775},
   {"AF_WINDOWS", 3, 1, 0x00000008u, access::ro, 0x1u, true, 776},
   {"HIST_WEIGHT", 4, 1, 0x00000010u, access::ro, 0x1u, true, 777},
   {"WB_GB", 5, 1, 0x00000020u, access::ro, 0x1u, true, 778},
   {"DEMOSAIC", 6, 1, 0x00000040u, access::ro, 0x1u, true, 779},
   {"CNF_BLEND", 7, 1, 0x00000080u, access::ro, 0x1u, true, 780},
   {"CSC_LOOK", 8, 1, 0x00000100u, access::ro, 0x1u, true, 781},
   {"AEC_LEASE", 9, 1, 0x00000200u, access::ro, 0x1u, true, 782},
   {"GAMMA_BANK", 10, 1, 0x00000400u, access::ro, 0x1u, true, 783},
   {"BPC_CAL", 11, 1, 0x00000800u, access::ro, 0x1u, true, 784},
   {"LSC_SECTORS", 12, 1, 0x00001000u, access::ro, 0x1u, true, 785},
   {"AF_LEASE", 13, 1, 0x00002000u, access::ro, 0x1u, true, 786},
   {"RAW_INPUT", 14, 1, 0x00004000u, access::ro, 0x1u, true, 787},
   {"BAYER_AE", 15, 1, 0x00008000u, access::ro, 0x1u, true, 788},
   {"CHANNEL_CROP", 16, 1, 0x00010000u, access::ro, 0x1u, true, 789},
   {"CSC_WIDE_RANGE", 17, 1, 0x00020000u, access::ro, 0x1u, true, 790},
   {"RECOVERY_REPORT", 18, 1, 0x00040000u, access::ro, 0x1u, true, 791},
   {"FRAME_ID", 19, 1, 0x00080000u, access::ro, 0x1u, true, 792},
   {"LEASE_GEOMETRY", 20, 1, 0x00100000u, access::ro, 0x1u, true, 793},
   {"RESIDENT_OWNERSHIP", 21, 1, 0x00200000u, access::ro, 0x1u, true, 794},
};
constexpr field_desc fields_339[] = {  // TUNE_MODE
   {"ENABLE", 0, 1, 0x00000001u, access::rw, 0x0u, true, 796},
};
constexpr field_desc fields_340[] = {  // TUNE_BANK
   {"VALUE", 0, 1, 0x00000001u, access::rw, 0x0u, true, 798},
};
constexpr field_desc fields_341[] = {  // TUNE_PROFILE_ID
   {"VALUE", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 800},
};
constexpr field_desc fields_342[] = {  // TUNE_APPLY_FRAME
   {"VALUE", 0, 32, 0xFFFFFFFFu, access::rw, 0x0u, true, 802},
};
constexpr field_desc fields_343[] = {  // TUNE_CMD
   {"INIT", 0, 1, 0x00000001u, access::w1p, 0x0u, true, 804},
   {"SEAL", 1, 1, 0x00000002u, access::w1p, 0x0u, true, 805},
   {"APPLY", 2, 1, 0x00000004u, access::w1p, 0x0u, true, 806},
   {"CANCEL", 3, 1, 0x00000008u, access::w1p, 0x0u, true, 807},
   {"UNSEAL", 4, 1, 0x00000010u, access::w1p, 0x0u, true, 808},
   {"EXACT", 8, 1, 0x00000100u, access::w1p, 0x0u, true, 809},
};
constexpr field_desc fields_344[] = {  // TUNE_STATUS
   {"ACTIVE_BANK", 0, 1, 0x00000001u, access::ro, 0x0u, true, 811},
   {"ACTIVE_VALID", 1, 1, 0x00000002u, access::ro, 0x0u, true, 812},
   {"PENDING", 2, 1, 0x00000004u, access::ro, 0x0u, true, 813},
   {"BANK0_BUSY", 3, 1, 0x00000008u, access::ro, 0x0u, true, 814},
   {"BANK1_BUSY", 4, 1, 0x00000010u, access::ro, 0x0u, true, 815},
   {"BANK0_VALID", 5, 1, 0x00000020u, access::ro, 0x0u, true, 816},
   {"BANK1_VALID", 6, 1, 0x00000040u, access::ro, 0x0u, true, 817},
   {"BANK0_PENDING", 7, 1, 0x00000080u, access::ro, 0x0u, true, 818},
   {"BANK1_PENDING", 8, 1, 0x00000100u, access::ro, 0x0u, true, 819},
   {"BANK0_REFERENCED", 9, 1, 0x00000200u, access::ro, 0x0u, true, 820},
   {"BANK1_REFERENCED", 10, 1, 0x00000400u, access::ro, 0x0u, true, 821},
};
constexpr field_desc fields_345[] = {  // TUNE_ACTIVE_ID
   {"VALUE", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 823},
};
constexpr field_desc fields_346[] = {  // TUNE_ACTIVE_FRAME
   {"VALUE", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 825},
};
constexpr field_desc fields_347[] = {  // TUNE_ERROR
   {"LOCKED", 0, 1, 0x00000001u, access::w1c, 0x0u, true, 827},
   {"INVALID", 1, 1, 0x00000002u, access::w1c, 0x0u, true, 828},
   {"NOT_READY", 2, 1, 0x00000004u, access::w1c, 0x0u, true, 829},
   {"PENDING", 3, 1, 0x00000008u, access::w1c, 0x0u, true, 830},
   {"MISSED", 4, 1, 0x00000010u, access::w1c, 0x0u, true, 831},
   {"MODE", 5, 1, 0x00000020u, access::w1c, 0x0u, true, 832},
   {"NO_LEASE", 6, 1, 0x00000040u, access::w1c, 0x0u, true, 833},
   {"OVERFLOW", 7, 1, 0x00000080u, access::w1c, 0x0u, true, 834},
};
constexpr field_desc fields_348[] = {  // TUNE_NEXT_FRAME
   {"VALUE", 0, 32, 0xFFFFFFFFu, access::ro, 0x1u, true, 836},
};
constexpr field_desc fields_349[] = {  // TUNE_CH0_BUF0_PROFILE
   {"VALUE", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 838},
};
constexpr field_desc fields_350[] = {  // TUNE_CH0_BUF1_PROFILE
   {"VALUE", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 840},
};
constexpr field_desc fields_351[] = {  // TUNE_CH0_BUF2_PROFILE
   {"VALUE", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 842},
};
constexpr field_desc fields_352[] = {  // TUNE_CH0_BUF3_PROFILE
   {"VALUE", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 844},
};
constexpr field_desc fields_353[] = {  // TUNE_CH1_BUF0_PROFILE
   {"VALUE", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 846},
};
constexpr field_desc fields_354[] = {  // TUNE_CH1_BUF1_PROFILE
   {"VALUE", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 848},
};
constexpr field_desc fields_355[] = {  // TUNE_CH1_BUF2_PROFILE
   {"VALUE", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 850},
};
constexpr field_desc fields_356[] = {  // TUNE_CH1_BUF3_PROFILE
   {"VALUE", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 852},
};
constexpr field_desc fields_357[] = {  // TUNE_CH2_BUF0_PROFILE
   {"VALUE", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 854},
};
constexpr field_desc fields_358[] = {  // TUNE_CH2_BUF1_PROFILE
   {"VALUE", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 856},
};
constexpr field_desc fields_359[] = {  // TUNE_CH2_BUF2_PROFILE
   {"VALUE", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 858},
};
constexpr field_desc fields_360[] = {  // TUNE_CH2_BUF3_PROFILE
   {"VALUE", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 860},
};
constexpr field_desc fields_361[] = {  // TUNE_NR_CTRL
   {"MODE", 0, 2, 0x00000003u, access::rw, 0x0u, true, 862},
   {"MEDIAN_EN", 2, 1, 0x00000004u, access::rw, 0x1u, true, 863},
};
constexpr field_desc fields_362[] = {  // TUNE_NR_VARIANCE
   {"VALUE", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 865},
};
constexpr field_desc fields_363[] = {  // TUNE_NR_SCALE
   {"VALUE", 0, 16, 0x0000FFFFu, access::rw, 0x100u, true, 867},
};
constexpr field_desc fields_364[] = {  // TUNE_NR_LIMITS
   {"MIN", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 869},
   {"MAX", 16, 16, 0xFFFF0000u, access::rw, 0xFFFFu, true, 870},
};
constexpr field_desc fields_365[] = {  // TUNE_NR_LL_SCALE
   {"VALUE", 0, 16, 0x0000FFFFu, access::rw, 0x100u, true, 872},
};
constexpr field_desc fields_366[] = {  // TUNE_NR_LH_SCALE
   {"VALUE", 0, 16, 0x0000FFFFu, access::rw, 0x100u, true, 874},
};
constexpr field_desc fields_367[] = {  // TUNE_NR_HL_SCALE
   {"VALUE", 0, 16, 0x0000FFFFu, access::rw, 0x100u, true, 876},
};
constexpr field_desc fields_368[] = {  // TUNE_NR_HH_SCALE
   {"VALUE", 0, 16, 0x0000FFFFu, access::rw, 0x100u, true, 878},
};
constexpr field_desc fields_369[] = {  // TUNE_NR_BLEND
   {"VALUE", 0, 9, 0x000001FFu, access::rw, 0x100u, true, 880},
};
constexpr field_desc fields_370[] = {  // TUNE_NR_LUT_ADDR
   {"VALUE", 0, 5, 0x0000001Fu, access::rw, 0x0u, true, 882},
};
constexpr field_desc fields_371[] = {  // TUNE_NR_LUT_DATA
   {"VALUE", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 884},
};
constexpr field_desc fields_372[] = {  // TUNE_NR_SEED
   {"VALUE", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 886},
};
constexpr field_desc fields_373[] = {  // TUNE_WB_GB
   {"VALUE", 0, 12, 0x00000FFFu, access::rw, 0x100u, true, 888},
};
constexpr field_desc fields_374[] = {  // TUNE_DEMOSAIC_MARGIN
   {"GREEN", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 890},
   {"RB", 16, 16, 0xFFFF0000u, access::rw, 0x0u, true, 891},
};
constexpr field_desc fields_375[] = {  // TUNE_CNF_BLEND
   {"VALUE", 0, 9, 0x000001FFu, access::rw, 0x100u, true, 893},
};
constexpr field_desc fields_376[] = {  // TUNE_CSC_CTRL
   {"LOOK_EN", 0, 1, 0x00000001u, access::rw, 0x0u, true, 895},
   {"FULL_RANGE", 1, 1, 0x00000002u, access::rw, 0x1u, true, 896},
};
constexpr field_desc fields_377[] = {  // TUNE_CSC_CONTRAST
   {"VALUE", 0, 12, 0x00000FFFu, access::rw, 0x100u, true, 898},
};
constexpr field_desc fields_378[] = {  // TUNE_CSC_BRIGHTNESS
   {"VALUE", 0, 10, 0x000003FFu, access::rw, 0x0u, true, 900},
};
constexpr field_desc fields_379[] = {  // TUNE_CSC_UU
   {"VALUE", 0, 18, 0x0003FFFFu, access::rw, 0x4000u, true, 902},
};
constexpr field_desc fields_380[] = {  // TUNE_CSC_UV
   {"VALUE", 0, 18, 0x0003FFFFu, access::rw, 0x0u, true, 904},
};
constexpr field_desc fields_381[] = {  // TUNE_CSC_VU
   {"VALUE", 0, 18, 0x0003FFFFu, access::rw, 0x0u, true, 906},
};
constexpr field_desc fields_382[] = {  // TUNE_CSC_VV
   {"VALUE", 0, 18, 0x0003FFFFu, access::rw, 0x4000u, true, 908},
};
constexpr field_desc fields_383[] = {  // TUNE_AWB_CTRL
   {"ROI_EN", 0, 1, 0x00000001u, access::rw, 0x0u, true, 910},
   {"COLOR_EN", 1, 1, 0x00000002u, access::rw, 0x0u, true, 911},
   {"ELLIPSE_MASK", 8, 8, 0x0000FF00u, access::rw, 0x0u, true, 912},
};
constexpr field_desc fields_384[] = {  // TUNE_AWB_ROI_START
   {"X", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 914},
   {"Y", 16, 16, 0xFFFF0000u, access::rw, 0x0u, true, 915},
};
constexpr field_desc fields_385[] = {  // TUNE_AWB_ROI_END
   {"X", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 917},
   {"Y", 16, 16, 0xFFFF0000u, access::rw, 0x0u, true, 918},
};
constexpr field_desc fields_386[] = {  // TUNE_AWB_R_LIMITS
   {"MIN", 0, 12, 0x00000FFFu, access::rw, 0x0u, true, 920},
   {"MAX", 16, 12, 0x0FFF0000u, access::rw, 0xFFFu, true, 921},
};
constexpr field_desc fields_387[] = {  // TUNE_AWB_G_LIMITS
   {"MIN", 0, 12, 0x00000FFFu, access::rw, 0x0u, true, 923},
   {"MAX", 16, 12, 0x0FFF0000u, access::rw, 0xFFFu, true, 924},
};
constexpr field_desc fields_388[] = {  // TUNE_AWB_B_LIMITS
   {"MIN", 0, 12, 0x00000FFFu, access::rw, 0x0u, true, 926},
   {"MAX", 16, 12, 0x0FFF0000u, access::rw, 0xFFFu, true, 927},
};
constexpr field_desc fields_389[] = {  // TUNE_AWB_MIN_G
   {"VALUE", 0, 12, 0x00000FFFu, access::rw, 0x1u, true, 929},
};
constexpr field_desc fields_390[] = {  // TUNE_AWB_E0_CENTER
   {"RG", 0, 16, 0x0000FFFFu, access::rw, 0x100u, true, 931},
   {"BG", 16, 16, 0xFFFF0000u, access::rw, 0x100u, true, 932},
};
constexpr field_desc fields_391[] = {  // TUNE_AWB_E0_A
   {"VALUE", 0, 16, 0x0000FFFFu, access::rw, 0x4000u, true, 934},
};
constexpr field_desc fields_392[] = {  // TUNE_AWB_E0_B
   {"VALUE", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 936},
};
constexpr field_desc fields_393[] = {  // TUNE_AWB_E0_C
   {"VALUE", 0, 16, 0x0000FFFFu, access::rw, 0x4000u, true, 938},
};
constexpr field_desc fields_394[] = {  // TUNE_AWB_E0_RADIUS2
   {"VALUE", 0, 32, 0xFFFFFFFFu, access::rw, 0x10000u, true, 940},
};
constexpr field_desc fields_395[] = {  // TUNE_AWB_E1_CENTER
   {"RG", 0, 16, 0x0000FFFFu, access::rw, 0x100u, true, 942},
   {"BG", 16, 16, 0xFFFF0000u, access::rw, 0x100u, true, 943},
};
constexpr field_desc fields_396[] = {  // TUNE_AWB_E1_A
   {"VALUE", 0, 16, 0x0000FFFFu, access::rw, 0x4000u, true, 945},
};
constexpr field_desc fields_397[] = {  // TUNE_AWB_E1_B
   {"VALUE", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 947},
};
constexpr field_desc fields_398[] = {  // TUNE_AWB_E1_C
   {"VALUE", 0, 16, 0x0000FFFFu, access::rw, 0x4000u, true, 949},
};
constexpr field_desc fields_399[] = {  // TUNE_AWB_E1_RADIUS2
   {"VALUE", 0, 32, 0xFFFFFFFFu, access::rw, 0x10000u, true, 951},
};
constexpr field_desc fields_400[] = {  // TUNE_AWB_E2_CENTER
   {"RG", 0, 16, 0x0000FFFFu, access::rw, 0x100u, true, 953},
   {"BG", 16, 16, 0xFFFF0000u, access::rw, 0x100u, true, 954},
};
constexpr field_desc fields_401[] = {  // TUNE_AWB_E2_A
   {"VALUE", 0, 16, 0x0000FFFFu, access::rw, 0x4000u, true, 956},
};
constexpr field_desc fields_402[] = {  // TUNE_AWB_E2_B
   {"VALUE", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 958},
};
constexpr field_desc fields_403[] = {  // TUNE_AWB_E2_C
   {"VALUE", 0, 16, 0x0000FFFFu, access::rw, 0x4000u, true, 960},
};
constexpr field_desc fields_404[] = {  // TUNE_AWB_E2_RADIUS2
   {"VALUE", 0, 32, 0xFFFFFFFFu, access::rw, 0x10000u, true, 962},
};
constexpr field_desc fields_405[] = {  // TUNE_AWB_E3_CENTER
   {"RG", 0, 16, 0x0000FFFFu, access::rw, 0x100u, true, 964},
   {"BG", 16, 16, 0xFFFF0000u, access::rw, 0x100u, true, 965},
};
constexpr field_desc fields_406[] = {  // TUNE_AWB_E3_A
   {"VALUE", 0, 16, 0x0000FFFFu, access::rw, 0x4000u, true, 967},
};
constexpr field_desc fields_407[] = {  // TUNE_AWB_E3_B
   {"VALUE", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 969},
};
constexpr field_desc fields_408[] = {  // TUNE_AWB_E3_C
   {"VALUE", 0, 16, 0x0000FFFFu, access::rw, 0x4000u, true, 971},
};
constexpr field_desc fields_409[] = {  // TUNE_AWB_E3_RADIUS2
   {"VALUE", 0, 32, 0xFFFFFFFFu, access::rw, 0x10000u, true, 973},
};
constexpr field_desc fields_410[] = {  // TUNE_AWB_E4_CENTER
   {"RG", 0, 16, 0x0000FFFFu, access::rw, 0x100u, true, 975},
   {"BG", 16, 16, 0xFFFF0000u, access::rw, 0x100u, true, 976},
};
constexpr field_desc fields_411[] = {  // TUNE_AWB_E4_A
   {"VALUE", 0, 16, 0x0000FFFFu, access::rw, 0x4000u, true, 978},
};
constexpr field_desc fields_412[] = {  // TUNE_AWB_E4_B
   {"VALUE", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 980},
};
constexpr field_desc fields_413[] = {  // TUNE_AWB_E4_C
   {"VALUE", 0, 16, 0x0000FFFFu, access::rw, 0x4000u, true, 982},
};
constexpr field_desc fields_414[] = {  // TUNE_AWB_E4_RADIUS2
   {"VALUE", 0, 32, 0xFFFFFFFFu, access::rw, 0x10000u, true, 984},
};
constexpr field_desc fields_415[] = {  // TUNE_AWB_E5_CENTER
   {"RG", 0, 16, 0x0000FFFFu, access::rw, 0x100u, true, 986},
   {"BG", 16, 16, 0xFFFF0000u, access::rw, 0x100u, true, 987},
};
constexpr field_desc fields_416[] = {  // TUNE_AWB_E5_A
   {"VALUE", 0, 16, 0x0000FFFFu, access::rw, 0x4000u, true, 989},
};
constexpr field_desc fields_417[] = {  // TUNE_AWB_E5_B
   {"VALUE", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 991},
};
constexpr field_desc fields_418[] = {  // TUNE_AWB_E5_C
   {"VALUE", 0, 16, 0x0000FFFFu, access::rw, 0x4000u, true, 993},
};
constexpr field_desc fields_419[] = {  // TUNE_AWB_E5_RADIUS2
   {"VALUE", 0, 32, 0xFFFFFFFFu, access::rw, 0x10000u, true, 995},
};
constexpr field_desc fields_420[] = {  // TUNE_AWB_E6_CENTER
   {"RG", 0, 16, 0x0000FFFFu, access::rw, 0x100u, true, 997},
   {"BG", 16, 16, 0xFFFF0000u, access::rw, 0x100u, true, 998},
};
constexpr field_desc fields_421[] = {  // TUNE_AWB_E6_A
   {"VALUE", 0, 16, 0x0000FFFFu, access::rw, 0x4000u, true, 1000},
};
constexpr field_desc fields_422[] = {  // TUNE_AWB_E6_B
   {"VALUE", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 1002},
};
constexpr field_desc fields_423[] = {  // TUNE_AWB_E6_C
   {"VALUE", 0, 16, 0x0000FFFFu, access::rw, 0x4000u, true, 1004},
};
constexpr field_desc fields_424[] = {  // TUNE_AWB_E6_RADIUS2
   {"VALUE", 0, 32, 0xFFFFFFFFu, access::rw, 0x10000u, true, 1006},
};
constexpr field_desc fields_425[] = {  // TUNE_AWB_E7_CENTER
   {"RG", 0, 16, 0x0000FFFFu, access::rw, 0x100u, true, 1008},
   {"BG", 16, 16, 0xFFFF0000u, access::rw, 0x100u, true, 1009},
};
constexpr field_desc fields_426[] = {  // TUNE_AWB_E7_A
   {"VALUE", 0, 16, 0x0000FFFFu, access::rw, 0x4000u, true, 1011},
};
constexpr field_desc fields_427[] = {  // TUNE_AWB_E7_B
   {"VALUE", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 1013},
};
constexpr field_desc fields_428[] = {  // TUNE_AWB_E7_C
   {"VALUE", 0, 16, 0x0000FFFFu, access::rw, 0x4000u, true, 1015},
};
constexpr field_desc fields_429[] = {  // TUNE_AWB_E7_RADIUS2
   {"VALUE", 0, 32, 0xFFFFFFFFu, access::rw, 0x10000u, true, 1017},
};
constexpr field_desc fields_430[] = {  // TUNE_AF_CTRL
   {"WINDOW_MODE", 0, 1, 0x00000001u, access::rw, 0x0u, true, 1019},
   {"WINDOW_MASK", 16, 16, 0xFFFF0000u, access::rw, 0xFFFFu, true, 1020},
};
constexpr field_desc fields_431[] = {  // TUNE_AF_NOISE
   {"VALUE", 0, 12, 0x00000FFFu, access::rw, 0x0u, true, 1022},
};
constexpr field_desc fields_432[] = {  // TUNE_AF_W0_START
   {"X", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 1024},
   {"Y", 16, 16, 0xFFFF0000u, access::rw, 0x0u, true, 1025},
};
constexpr field_desc fields_433[] = {  // TUNE_AF_W0_END
   {"X", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 1027},
   {"Y", 16, 16, 0xFFFF0000u, access::rw, 0x0u, true, 1028},
};
constexpr field_desc fields_434[] = {  // TUNE_AF_W1_START
   {"X", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 1030},
   {"Y", 16, 16, 0xFFFF0000u, access::rw, 0x0u, true, 1031},
};
constexpr field_desc fields_435[] = {  // TUNE_AF_W1_END
   {"X", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 1033},
   {"Y", 16, 16, 0xFFFF0000u, access::rw, 0x0u, true, 1034},
};
constexpr field_desc fields_436[] = {  // TUNE_AF_W2_START
   {"X", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 1036},
   {"Y", 16, 16, 0xFFFF0000u, access::rw, 0x0u, true, 1037},
};
constexpr field_desc fields_437[] = {  // TUNE_AF_W2_END
   {"X", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 1039},
   {"Y", 16, 16, 0xFFFF0000u, access::rw, 0x0u, true, 1040},
};
constexpr field_desc fields_438[] = {  // TUNE_AF_W3_START
   {"X", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 1042},
   {"Y", 16, 16, 0xFFFF0000u, access::rw, 0x0u, true, 1043},
};
constexpr field_desc fields_439[] = {  // TUNE_AF_W3_END
   {"X", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 1045},
   {"Y", 16, 16, 0xFFFF0000u, access::rw, 0x0u, true, 1046},
};
constexpr field_desc fields_440[] = {  // TUNE_AF_W4_START
   {"X", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 1048},
   {"Y", 16, 16, 0xFFFF0000u, access::rw, 0x0u, true, 1049},
};
constexpr field_desc fields_441[] = {  // TUNE_AF_W4_END
   {"X", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 1051},
   {"Y", 16, 16, 0xFFFF0000u, access::rw, 0x0u, true, 1052},
};
constexpr field_desc fields_442[] = {  // TUNE_AF_W5_START
   {"X", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 1054},
   {"Y", 16, 16, 0xFFFF0000u, access::rw, 0x0u, true, 1055},
};
constexpr field_desc fields_443[] = {  // TUNE_AF_W5_END
   {"X", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 1057},
   {"Y", 16, 16, 0xFFFF0000u, access::rw, 0x0u, true, 1058},
};
constexpr field_desc fields_444[] = {  // TUNE_AF_W6_START
   {"X", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 1060},
   {"Y", 16, 16, 0xFFFF0000u, access::rw, 0x0u, true, 1061},
};
constexpr field_desc fields_445[] = {  // TUNE_AF_W6_END
   {"X", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 1063},
   {"Y", 16, 16, 0xFFFF0000u, access::rw, 0x0u, true, 1064},
};
constexpr field_desc fields_446[] = {  // TUNE_AF_W7_START
   {"X", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 1066},
   {"Y", 16, 16, 0xFFFF0000u, access::rw, 0x0u, true, 1067},
};
constexpr field_desc fields_447[] = {  // TUNE_AF_W7_END
   {"X", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 1069},
   {"Y", 16, 16, 0xFFFF0000u, access::rw, 0x0u, true, 1070},
};
constexpr field_desc fields_448[] = {  // TUNE_AF_W8_START
   {"X", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 1072},
   {"Y", 16, 16, 0xFFFF0000u, access::rw, 0x0u, true, 1073},
};
constexpr field_desc fields_449[] = {  // TUNE_AF_W8_END
   {"X", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 1075},
   {"Y", 16, 16, 0xFFFF0000u, access::rw, 0x0u, true, 1076},
};
constexpr field_desc fields_450[] = {  // TUNE_AF_W9_START
   {"X", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 1078},
   {"Y", 16, 16, 0xFFFF0000u, access::rw, 0x0u, true, 1079},
};
constexpr field_desc fields_451[] = {  // TUNE_AF_W9_END
   {"X", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 1081},
   {"Y", 16, 16, 0xFFFF0000u, access::rw, 0x0u, true, 1082},
};
constexpr field_desc fields_452[] = {  // TUNE_AF_W10_START
   {"X", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 1084},
   {"Y", 16, 16, 0xFFFF0000u, access::rw, 0x0u, true, 1085},
};
constexpr field_desc fields_453[] = {  // TUNE_AF_W10_END
   {"X", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 1087},
   {"Y", 16, 16, 0xFFFF0000u, access::rw, 0x0u, true, 1088},
};
constexpr field_desc fields_454[] = {  // TUNE_AF_W11_START
   {"X", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 1090},
   {"Y", 16, 16, 0xFFFF0000u, access::rw, 0x0u, true, 1091},
};
constexpr field_desc fields_455[] = {  // TUNE_AF_W11_END
   {"X", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 1093},
   {"Y", 16, 16, 0xFFFF0000u, access::rw, 0x0u, true, 1094},
};
constexpr field_desc fields_456[] = {  // TUNE_AF_W12_START
   {"X", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 1096},
   {"Y", 16, 16, 0xFFFF0000u, access::rw, 0x0u, true, 1097},
};
constexpr field_desc fields_457[] = {  // TUNE_AF_W12_END
   {"X", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 1099},
   {"Y", 16, 16, 0xFFFF0000u, access::rw, 0x0u, true, 1100},
};
constexpr field_desc fields_458[] = {  // TUNE_AF_W13_START
   {"X", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 1102},
   {"Y", 16, 16, 0xFFFF0000u, access::rw, 0x0u, true, 1103},
};
constexpr field_desc fields_459[] = {  // TUNE_AF_W13_END
   {"X", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 1105},
   {"Y", 16, 16, 0xFFFF0000u, access::rw, 0x0u, true, 1106},
};
constexpr field_desc fields_460[] = {  // TUNE_AF_W14_START
   {"X", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 1108},
   {"Y", 16, 16, 0xFFFF0000u, access::rw, 0x0u, true, 1109},
};
constexpr field_desc fields_461[] = {  // TUNE_AF_W14_END
   {"X", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 1111},
   {"Y", 16, 16, 0xFFFF0000u, access::rw, 0x0u, true, 1112},
};
constexpr field_desc fields_462[] = {  // TUNE_AF_W15_START
   {"X", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 1114},
   {"Y", 16, 16, 0xFFFF0000u, access::rw, 0x0u, true, 1115},
};
constexpr field_desc fields_463[] = {  // TUNE_AF_W15_END
   {"X", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 1117},
   {"Y", 16, 16, 0xFFFF0000u, access::rw, 0x0u, true, 1118},
};
constexpr field_desc fields_464[] = {  // TUNE_HIST_CTRL
   {"ENABLE", 0, 1, 0x00000001u, access::rw, 0x0u, true, 1120},
   {"ROI_EN", 1, 1, 0x00000002u, access::rw, 0x0u, true, 1121},
};
constexpr field_desc fields_465[] = {  // TUNE_HIST_START
   {"X", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 1123},
   {"Y", 16, 16, 0xFFFF0000u, access::rw, 0x0u, true, 1124},
};
constexpr field_desc fields_466[] = {  // TUNE_HIST_END
   {"X", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 1126},
   {"Y", 16, 16, 0xFFFF0000u, access::rw, 0x0u, true, 1127},
};
constexpr field_desc fields_467[] = {  // TUNE_HIST_STEP
   {"X", 0, 8, 0x000000FFu, access::rw, 0x1u, true, 1129},
   {"Y", 8, 8, 0x0000FF00u, access::rw, 0x1u, true, 1130},
};
constexpr field_desc fields_468[] = {  // TUNE_HIST_WEIGHT0
   {"VALUE", 0, 8, 0x000000FFu, access::rw, 0x1u, true, 1132},
};
constexpr field_desc fields_469[] = {  // TUNE_HIST_WEIGHT1
   {"VALUE", 0, 8, 0x000000FFu, access::rw, 0x1u, true, 1134},
};
constexpr field_desc fields_470[] = {  // TUNE_HIST_WEIGHT2
   {"VALUE", 0, 8, 0x000000FFu, access::rw, 0x1u, true, 1136},
};
constexpr field_desc fields_471[] = {  // TUNE_HIST_WEIGHT3
   {"VALUE", 0, 8, 0x000000FFu, access::rw, 0x1u, true, 1138},
};
constexpr field_desc fields_472[] = {  // TUNE_HIST_WEIGHT4
   {"VALUE", 0, 8, 0x000000FFu, access::rw, 0x1u, true, 1140},
};
constexpr field_desc fields_473[] = {  // TUNE_HIST_WEIGHT5
   {"VALUE", 0, 8, 0x000000FFu, access::rw, 0x1u, true, 1142},
};
constexpr field_desc fields_474[] = {  // TUNE_HIST_WEIGHT6
   {"VALUE", 0, 8, 0x000000FFu, access::rw, 0x1u, true, 1144},
};
constexpr field_desc fields_475[] = {  // TUNE_HIST_WEIGHT7
   {"VALUE", 0, 8, 0x000000FFu, access::rw, 0x1u, true, 1146},
};
constexpr field_desc fields_476[] = {  // TUNE_HIST_WEIGHT8
   {"VALUE", 0, 8, 0x000000FFu, access::rw, 0x1u, true, 1148},
};
constexpr field_desc fields_477[] = {  // TUNE_HIST_WEIGHT9
   {"VALUE", 0, 8, 0x000000FFu, access::rw, 0x1u, true, 1150},
};
constexpr field_desc fields_478[] = {  // TUNE_HIST_WEIGHT10
   {"VALUE", 0, 8, 0x000000FFu, access::rw, 0x1u, true, 1152},
};
constexpr field_desc fields_479[] = {  // TUNE_HIST_WEIGHT11
   {"VALUE", 0, 8, 0x000000FFu, access::rw, 0x1u, true, 1154},
};
constexpr field_desc fields_480[] = {  // TUNE_HIST_WEIGHT12
   {"VALUE", 0, 8, 0x000000FFu, access::rw, 0x1u, true, 1156},
};
constexpr field_desc fields_481[] = {  // TUNE_HIST_WEIGHT13
   {"VALUE", 0, 8, 0x000000FFu, access::rw, 0x1u, true, 1158},
};
constexpr field_desc fields_482[] = {  // TUNE_HIST_WEIGHT14
   {"VALUE", 0, 8, 0x000000FFu, access::rw, 0x1u, true, 1160},
};
constexpr field_desc fields_483[] = {  // TUNE_HIST_WEIGHT15
   {"VALUE", 0, 8, 0x000000FFu, access::rw, 0x1u, true, 1162},
};
constexpr field_desc fields_484[] = {  // TUNE_HIST_WEIGHT16
   {"VALUE", 0, 8, 0x000000FFu, access::rw, 0x1u, true, 1164},
};
constexpr field_desc fields_485[] = {  // TUNE_HIST_WEIGHT17
   {"VALUE", 0, 8, 0x000000FFu, access::rw, 0x1u, true, 1166},
};
constexpr field_desc fields_486[] = {  // TUNE_HIST_WEIGHT18
   {"VALUE", 0, 8, 0x000000FFu, access::rw, 0x1u, true, 1168},
};
constexpr field_desc fields_487[] = {  // TUNE_HIST_WEIGHT19
   {"VALUE", 0, 8, 0x000000FFu, access::rw, 0x1u, true, 1170},
};
constexpr field_desc fields_488[] = {  // TUNE_HIST_WEIGHT20
   {"VALUE", 0, 8, 0x000000FFu, access::rw, 0x1u, true, 1172},
};
constexpr field_desc fields_489[] = {  // TUNE_HIST_WEIGHT21
   {"VALUE", 0, 8, 0x000000FFu, access::rw, 0x1u, true, 1174},
};
constexpr field_desc fields_490[] = {  // TUNE_HIST_WEIGHT22
   {"VALUE", 0, 8, 0x000000FFu, access::rw, 0x1u, true, 1176},
};
constexpr field_desc fields_491[] = {  // TUNE_HIST_WEIGHT23
   {"VALUE", 0, 8, 0x000000FFu, access::rw, 0x1u, true, 1178},
};
constexpr field_desc fields_492[] = {  // TUNE_HIST_WEIGHT24
   {"VALUE", 0, 8, 0x000000FFu, access::rw, 0x1u, true, 1180},
};
constexpr field_desc fields_493[] = {  // TUNE_AEC_CMD
   {"ACQUIRE", 0, 1, 0x00000001u, access::w1p, 0x0u, true, 1182},
   {"RELEASE", 1, 1, 0x00000002u, access::w1p, 0x0u, true, 1183},
};
constexpr field_desc fields_494[] = {  // TUNE_AEC_STATUS
   {"LEASE_VALID", 0, 1, 0x00000001u, access::ro, 0x0u, true, 1185},
   {"READY_COUNT", 1, 2, 0x00000006u, access::ro, 0x0u, true, 1186},
   {"BUSY", 3, 1, 0x00000008u, access::ro, 0x0u, true, 1187},
};
constexpr field_desc fields_495[] = {  // TUNE_AEC_DROPS
   {"VALUE", 0, 32, 0xFFFFFFFFu, access::w1c, 0x0u, true, 1189},
};
constexpr field_desc fields_496[] = {  // TUNE_AEC_FRAME
   {"VALUE", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 1191},
};
constexpr field_desc fields_497[] = {  // TUNE_AEC_PROFILE
   {"VALUE", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 1193},
};
constexpr field_desc fields_498[] = {  // TUNE_AEC_INDEX
   {"VALUE", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 1195},
};
constexpr field_desc fields_499[] = {  // TUNE_AEC_DATA_LO
   {"VALUE", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 1197},
};
constexpr field_desc fields_500[] = {  // TUNE_AEC_DATA_HI
   {"VALUE", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 1199},
};
constexpr field_desc fields_501[] = {  // TUNE_AF_CMD
   {"ACQUIRE", 0, 1, 0x00000001u, access::w1p, 0x0u, true, 1201},
   {"RELEASE", 1, 1, 0x00000002u, access::w1p, 0x0u, true, 1202},
};
constexpr field_desc fields_502[] = {  // TUNE_AF_STATUS
   {"LEASE_VALID", 0, 1, 0x00000001u, access::ro, 0x0u, true, 1204},
   {"READY_COUNT", 1, 2, 0x00000006u, access::ro, 0x0u, true, 1205},
   {"BUSY", 3, 1, 0x00000008u, access::ro, 0x0u, true, 1206},
};
constexpr field_desc fields_503[] = {  // TUNE_AF_DROPS
   {"VALUE", 0, 32, 0xFFFFFFFFu, access::w1c, 0x0u, true, 1208},
};
constexpr field_desc fields_504[] = {  // TUNE_AF_FRAME
   {"VALUE", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 1210},
};
constexpr field_desc fields_505[] = {  // TUNE_AF_PROFILE
   {"VALUE", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 1212},
};
constexpr field_desc fields_506[] = {  // TUNE_AF_INDEX
   {"VALUE", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 1214},
};
constexpr field_desc fields_507[] = {  // TUNE_AF_DATA_LO
   {"VALUE", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 1216},
};
constexpr field_desc fields_508[] = {  // TUNE_AF_DATA_HI
   {"VALUE", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 1218},
};
constexpr field_desc fields_509[] = {  // TUNE_AWB_PROFILE
   {"VALUE", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 1220},
};
constexpr field_desc fields_510[] = {  // TUNE_GAMMA_PORT
   {"INDEX", 0, 8, 0x000000FFu, access::rw, 0x0u, true, 1222},
};
constexpr field_desc fields_511[] = {  // TUNE_GAMMA_DATA
   {"VALUE", 0, 12, 0x00000FFFu, access::rw, 0x0u, true, 1224},
};
constexpr field_desc fields_512[] = {  // TUNE_TABLE_CMD
   {"GAMMA_CONFIRM", 0, 1, 0x00000001u, access::w1p, 0x0u, true, 1226},
   {"NR_CONFIRM", 1, 1, 0x00000002u, access::w1p, 0x0u, true, 1227},
   {"BPC_CONFIRM", 2, 1, 0x00000004u, access::w1p, 0x0u, true, 1228},
   {"LSC_CONFIRM", 3, 1, 0x00000008u, access::w1p, 0x0u, true, 1229},
};
constexpr field_desc fields_513[] = {  // TUNE_TABLE_STATUS
   {"GAMMA", 0, 1, 0x00000001u, access::ro, 0x0u, true, 1231},
   {"NR", 1, 1, 0x00000002u, access::ro, 0x0u, true, 1232},
   {"BPC", 2, 1, 0x00000004u, access::ro, 0x0u, true, 1233},
   {"LSC", 3, 1, 0x00000008u, access::ro, 0x0u, true, 1234},
};
constexpr field_desc fields_514[] = {  // TUNE_BPC_CTRL
   {"COLOR_THRESH_EN", 0, 1, 0x00000001u, access::rw, 0x0u, true, 1236},
   {"FACTORY_EN", 1, 1, 0x00000002u, access::rw, 0x0u, true, 1237},
};
constexpr field_desc fields_515[] = {  // TUNE_BPC_G_THRESH
   {"K", 0, 12, 0x00000FFFu, access::rw, 0x100u, true, 1239},
   {"FLOOR", 16, 12, 0x0FFF0000u, access::rw, 0x0u, true, 1240},
};
constexpr field_desc fields_516[] = {  // TUNE_BPC_RB_THRESH
   {"K", 0, 12, 0x00000FFFu, access::rw, 0x100u, true, 1242},
   {"FLOOR", 16, 12, 0x0FFF0000u, access::rw, 0x0u, true, 1243},
};
constexpr field_desc fields_517[] = {  // TUNE_BPC_COUNT
   {"VALUE", 0, 11, 0x000007FFu, access::rw, 0x0u, true, 1245},
};
constexpr field_desc fields_518[] = {  // TUNE_BPC_ADDR
   {"VALUE", 0, 10, 0x000003FFu, access::rw, 0x0u, true, 1247},
};
constexpr field_desc fields_519[] = {  // TUNE_BPC_DATA
   {"X", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 1249},
   {"Y", 16, 16, 0xFFFF0000u, access::rw, 0x0u, true, 1250},
};
constexpr field_desc fields_520[] = {  // TUNE_LSC_CTRL
   {"SECTOR_EN", 0, 1, 0x00000001u, access::rw, 0x0u, true, 1252},
};
constexpr field_desc fields_521[] = {  // TUNE_LSC_SECTOR_ADDR
   {"INDEX", 0, 7, 0x0000007Fu, access::rw, 0x0u, true, 1254},
   {"AXIS", 8, 1, 0x00000100u, access::rw, 0x0u, true, 1255},
};
constexpr field_desc fields_522[] = {  // TUNE_LSC_SECTOR_DATA
   {"VALUE", 0, 16, 0x0000FFFFu, access::rw, 0x0u, true, 1257},
};
constexpr field_desc fields_523[] = {  // COMMON_INPUT_FORMAT
   {"DEPTH", 0, 2, 0x00000003u, access::rw, 0x0u, true, 1259},
};
constexpr field_desc fields_524[] = {  // TUNE_AEC_SOURCE
   {"SOURCE", 0, 1, 0x00000001u, access::rw, 0x0u, true, 1261},
};
constexpr field_desc fields_525[] = {  // TUNE_AEC_RESULT_META
   {"SOURCE", 0, 1, 0x00000001u, access::ro, 0x0u, true, 1263},
   {"DOMAIN", 4, 4, 0x000000F0u, access::ro, 0x0u, true, 1264},
   {"NATIVE_DEPTH", 8, 2, 0x00000300u, access::ro, 0x0u, true, 1265},
   {"LEASE_VALID", 31, 1, 0x80000000u, access::ro, 0x0u, true, 1266},
};
constexpr field_desc fields_526[] = {  // CH0_CROP_CTRL
   {"ENABLE", 0, 1, 0x00000001u, access::rw, 0x0u, true, 1268},
};
constexpr field_desc fields_527[] = {  // CH0_CROP_ORIGIN
   {"X", 0, 13, 0x00001FFFu, access::rw, 0x0u, true, 1270},
   {"Y", 16, 13, 0x1FFF0000u, access::rw, 0x0u, true, 1271},
};
constexpr field_desc fields_528[] = {  // CH0_CROP_SIZE
   {"W", 0, 13, 0x00001FFFu, access::rw, 0x0u, true, 1273},
   {"H", 16, 13, 0x1FFF0000u, access::rw, 0x0u, true, 1274},
};
constexpr field_desc fields_529[] = {  // CH0_GEOMETRY_STATUS
   {"VALID", 0, 1, 0x00000001u, access::ro, 0x0u, true, 1276},
   {"BUSY", 1, 1, 0x00000002u, access::ro, 0x0u, true, 1277},
   {"ERROR", 4, 4, 0x000000F0u, access::ro, 0x0u, true, 1278},
};
constexpr field_desc fields_530[] = {  // CH0_ACTIVE_CROP_ORIGIN
   {"X", 0, 13, 0x00001FFFu, access::ro, 0x0u, true, 1280},
   {"Y", 16, 13, 0x1FFF0000u, access::ro, 0x0u, true, 1281},
};
constexpr field_desc fields_531[] = {  // CH0_ACTIVE_CROP_SIZE
   {"W", 0, 13, 0x00001FFFu, access::ro, 0x0u, true, 1283},
   {"H", 16, 13, 0x1FFF0000u, access::ro, 0x0u, true, 1284},
};
constexpr field_desc fields_532[] = {  // CH0_OUTPUT_LIMIT
   {"MAX_W", 0, 13, 0x00001FFFu, access::ro, 0xF00u, true, 1286},
   {"MAX_H", 16, 13, 0x1FFF0000u, access::ro, 0x870u, true, 1287},
};
constexpr field_desc fields_533[] = {  // CH1_CROP_CTRL
   {"ENABLE", 0, 1, 0x00000001u, access::rw, 0x0u, true, 1289},
};
constexpr field_desc fields_534[] = {  // CH1_CROP_ORIGIN
   {"X", 0, 13, 0x00001FFFu, access::rw, 0x0u, true, 1291},
   {"Y", 16, 13, 0x1FFF0000u, access::rw, 0x0u, true, 1292},
};
constexpr field_desc fields_535[] = {  // CH1_CROP_SIZE
   {"W", 0, 13, 0x00001FFFu, access::rw, 0x0u, true, 1294},
   {"H", 16, 13, 0x1FFF0000u, access::rw, 0x0u, true, 1295},
};
constexpr field_desc fields_536[] = {  // CH1_GEOMETRY_STATUS
   {"VALID", 0, 1, 0x00000001u, access::ro, 0x0u, true, 1297},
   {"BUSY", 1, 1, 0x00000002u, access::ro, 0x0u, true, 1298},
   {"ERROR", 4, 4, 0x000000F0u, access::ro, 0x0u, true, 1299},
};
constexpr field_desc fields_537[] = {  // CH1_ACTIVE_CROP_ORIGIN
   {"X", 0, 13, 0x00001FFFu, access::ro, 0x0u, true, 1301},
   {"Y", 16, 13, 0x1FFF0000u, access::ro, 0x0u, true, 1302},
};
constexpr field_desc fields_538[] = {  // CH1_ACTIVE_CROP_SIZE
   {"W", 0, 13, 0x00001FFFu, access::ro, 0x0u, true, 1304},
   {"H", 16, 13, 0x1FFF0000u, access::ro, 0x0u, true, 1305},
};
constexpr field_desc fields_539[] = {  // CH1_OUTPUT_LIMIT
   {"MAX_W", 0, 13, 0x00001FFFu, access::ro, 0xF00u, true, 1307},
   {"MAX_H", 16, 13, 0x1FFF0000u, access::ro, 0x870u, true, 1308},
};
constexpr field_desc fields_540[] = {  // CH2_CROP_CTRL
   {"ENABLE", 0, 1, 0x00000001u, access::rw, 0x0u, true, 1310},
};
constexpr field_desc fields_541[] = {  // CH2_CROP_ORIGIN
   {"X", 0, 13, 0x00001FFFu, access::rw, 0x0u, true, 1312},
   {"Y", 16, 13, 0x1FFF0000u, access::rw, 0x0u, true, 1313},
};
constexpr field_desc fields_542[] = {  // CH2_CROP_SIZE
   {"W", 0, 13, 0x00001FFFu, access::rw, 0x0u, true, 1315},
   {"H", 16, 13, 0x1FFF0000u, access::rw, 0x0u, true, 1316},
};
constexpr field_desc fields_543[] = {  // CH2_GEOMETRY_STATUS
   {"VALID", 0, 1, 0x00000001u, access::ro, 0x0u, true, 1318},
   {"BUSY", 1, 1, 0x00000002u, access::ro, 0x0u, true, 1319},
   {"ERROR", 4, 4, 0x000000F0u, access::ro, 0x0u, true, 1320},
};
constexpr field_desc fields_544[] = {  // CH2_ACTIVE_CROP_ORIGIN
   {"X", 0, 13, 0x00001FFFu, access::ro, 0x0u, true, 1322},
   {"Y", 16, 13, 0x1FFF0000u, access::ro, 0x0u, true, 1323},
};
constexpr field_desc fields_545[] = {  // CH2_ACTIVE_CROP_SIZE
   {"W", 0, 13, 0x00001FFFu, access::ro, 0x0u, true, 1325},
   {"H", 16, 13, 0x1FFF0000u, access::ro, 0x0u, true, 1326},
};
constexpr field_desc fields_546[] = {  // CH2_OUTPUT_LIMIT
   {"MAX_W", 0, 13, 0x00001FFFu, access::ro, 0x280u, true, 1328},
   {"MAX_H", 16, 13, 0x1FFF0000u, access::ro, 0x280u, true, 1329},
};
constexpr field_desc fields_547[] = {  // COMMON_RECOVERY_STATUS
   {"BUSY", 0, 1, 0x00000001u, access::ro, 0x0u, true, 1331},
   {"DONE", 1, 1, 0x00000002u, access::w1c, 0x0u, true, 1332},
   {"SCOPE", 4, 2, 0x00000030u, access::ro, 0x0u, true, 1333},
   {"PHASE", 8, 2, 0x00000300u, access::ro, 0x0u, true, 1334},
   {"COUNT", 16, 3, 0x00070000u, access::ro, 0x0u, true, 1335},
};
constexpr field_desc fields_548[] = {  // COMMON_RECOVERY_SEQUENCE
   {"VALUE", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 1337},
};
constexpr field_desc fields_549[] = {  // COMMON_ABORT0_FRAME
   {"FRAME", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 1339},
};
constexpr field_desc fields_550[] = {  // COMMON_ABORT0_OUTCOME
   {"FAILED_OUTPUTS", 0, 3, 0x00000007u, access::ro, 0x0u, true, 1341},
   {"CAUSE", 8, 3, 0x00000700u, access::ro, 0x0u, true, 1342},
};
constexpr field_desc fields_551[] = {  // COMMON_ABORT1_FRAME
   {"FRAME", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 1344},
};
constexpr field_desc fields_552[] = {  // COMMON_ABORT1_OUTCOME
   {"FAILED_OUTPUTS", 0, 3, 0x00000007u, access::ro, 0x0u, true, 1346},
   {"CAUSE", 8, 3, 0x00000700u, access::ro, 0x0u, true, 1347},
};
constexpr field_desc fields_553[] = {  // COMMON_ABORT2_FRAME
   {"FRAME", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 1349},
};
constexpr field_desc fields_554[] = {  // COMMON_ABORT2_OUTCOME
   {"FAILED_OUTPUTS", 0, 3, 0x00000007u, access::ro, 0x0u, true, 1351},
   {"CAUSE", 8, 3, 0x00000700u, access::ro, 0x0u, true, 1352},
};
constexpr field_desc fields_555[] = {  // COMMON_ABORT3_FRAME
   {"FRAME", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 1354},
};
constexpr field_desc fields_556[] = {  // COMMON_ABORT3_OUTCOME
   {"FAILED_OUTPUTS", 0, 3, 0x00000007u, access::ro, 0x0u, true, 1356},
   {"CAUSE", 8, 3, 0x00000700u, access::ro, 0x0u, true, 1357},
};
constexpr field_desc fields_557[] = {  // TUNE_CH0_BUF0_FRAME
   {"FRAME", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 1359},
};
constexpr field_desc fields_558[] = {  // TUNE_CH0_BUF1_FRAME
   {"FRAME", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 1361},
};
constexpr field_desc fields_559[] = {  // TUNE_CH0_BUF2_FRAME
   {"FRAME", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 1363},
};
constexpr field_desc fields_560[] = {  // TUNE_CH0_BUF3_FRAME
   {"FRAME", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 1365},
};
constexpr field_desc fields_561[] = {  // TUNE_CH1_BUF0_FRAME
   {"FRAME", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 1367},
};
constexpr field_desc fields_562[] = {  // TUNE_CH1_BUF1_FRAME
   {"FRAME", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 1369},
};
constexpr field_desc fields_563[] = {  // TUNE_CH1_BUF2_FRAME
   {"FRAME", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 1371},
};
constexpr field_desc fields_564[] = {  // TUNE_CH1_BUF3_FRAME
   {"FRAME", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 1373},
};
constexpr field_desc fields_565[] = {  // TUNE_CH2_BUF0_FRAME
   {"FRAME", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 1375},
};
constexpr field_desc fields_566[] = {  // TUNE_CH2_BUF1_FRAME
   {"FRAME", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 1377},
};
constexpr field_desc fields_567[] = {  // TUNE_CH2_BUF2_FRAME
   {"FRAME", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 1379},
};
constexpr field_desc fields_568[] = {  // TUNE_CH2_BUF3_FRAME
   {"FRAME", 0, 32, 0xFFFFFFFFu, access::ro, 0x0u, true, 1381},
};
constexpr field_desc fields_569[] = {  // TUNE_AE_FRAME_SIZE
   {"WIDTH", 0, 13, 0x00001FFFu, access::ro, 0x0u, true, 1383},
   {"HEIGHT", 16, 13, 0x1FFF0000u, access::ro, 0x0u, true, 1384},
};
constexpr field_desc fields_570[] = {  // TUNE_AE_GRID
   {"NX", 0, 6, 0x0000003Fu, access::ro, 0x0u, true, 1386},
   {"NY", 16, 5, 0x001F0000u, access::ro, 0x0u, true, 1387},
};
constexpr field_desc fields_571[] = {  // TUNE_AE_ZONE_SIZE
   {"WIDTH", 0, 16, 0x0000FFFFu, access::ro, 0x0u, true, 1389},
   {"HEIGHT", 16, 16, 0xFFFF0000u, access::ro, 0x0u, true, 1390},
};
constexpr field_desc fields_572[] = {  // TUNE_AWB_FRAME_SIZE
   {"WIDTH", 0, 13, 0x00001FFFu, access::ro, 0x0u, true, 1392},
   {"HEIGHT", 16, 13, 0x1FFF0000u, access::ro, 0x0u, true, 1393},
};

}  // namespace

const reg_desc registers[] = {
   {0x0000u, "COMMON_VER_DATE", 0x18102026u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_0, 3, 2},
   {0x0004u, "COMMON_VER_ID", 0x00010007u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_1, 1, 6},
   {0x0008u, "COMMON_CTRL", 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000004u, 0x00000002u, 0x00000000u, fields_2, 3, 8},
   {0x000Cu, "COMMON_STATUS", 0x00000000u, 0x00000000u, 0x00000007u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_3, 3, 12},
   {0x0010u, "COMMON_IRQ_STATUS", 0x00000000u, 0x00000000u, 0x00000000u, 0x00000007u, 0x00000000u, 0x00000000u, 0x00000000u, fields_4, 3, 16},
   {0x0014u, "COMMON_IRQ_EN", 0x00000000u, 0x00000007u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_5, 3, 20},
   {0x0018u, "COMMON_FRAME_WIDTH", 0x00000780u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_6, 1, 24},
   {0x001Cu, "COMMON_FRAME_HEIGHT", 0x00000438u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_7, 1, 26},
   {0x0020u, "COMMON_BAYER", 0x00000000u, 0x00000003u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_8, 1, 28},
   {0x0028u, "COMMON_SCRATCH", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_9, 1, 30},
   {0x0200u, "BLC_CTRL", 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_10, 1, 32},
   {0x0204u, "BLC_GAIN_SEL", 0x00000100u, 0x00000103u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_11, 2, 34},
   {0x0208u, "BLC_OFS_G0_DFT", 0x000000C8u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_12, 1, 37},
   {0x020Cu, "BLC_OFS_G0_R", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_13, 1, 39},
   {0x0210u, "BLC_OFS_G0_GR", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_14, 1, 41},
   {0x0214u, "BLC_OFS_G0_GB", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_15, 1, 43},
   {0x0218u, "BLC_OFS_G0_B", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_16, 1, 45},
   {0x021Cu, "BLC_OFS_G1_DFT", 0x000000C8u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_17, 1, 47},
   {0x0220u, "BLC_OFS_G1_R", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_18, 1, 49},
   {0x0224u, "BLC_OFS_G1_GR", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_19, 1, 51},
   {0x0228u, "BLC_OFS_G1_GB", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_20, 1, 53},
   {0x022Cu, "BLC_OFS_G1_B", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_21, 1, 55},
   {0x0230u, "BLC_OFS_G2_DFT", 0x000000C8u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_22, 1, 57},
   {0x0234u, "BLC_OFS_G2_R", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_23, 1, 59},
   {0x0238u, "BLC_OFS_G2_GR", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_24, 1, 61},
   {0x023Cu, "BLC_OFS_G2_GB", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_25, 1, 63},
   {0x0240u, "BLC_OFS_G2_B", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_26, 1, 65},
   {0x0244u, "BLC_OFS_G3_DFT", 0x000000C8u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_27, 1, 67},
   {0x0248u, "BLC_OFS_G3_R", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_28, 1, 69},
   {0x024Cu, "BLC_OFS_G3_GR", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_29, 1, 71},
   {0x0250u, "BLC_OFS_G3_GB", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_30, 1, 73},
   {0x0254u, "BLC_OFS_G3_B", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_31, 1, 75},
   {0x0258u, "BLC_SCALE_G0", 0x00010000u, 0x0003FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_32, 1, 77},
   {0x025Cu, "BLC_SCALE_G1", 0x00010000u, 0x0003FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_33, 1, 79},
   {0x0260u, "BLC_SCALE_G2", 0x00010000u, 0x0003FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_34, 1, 81},
   {0x0264u, "BLC_SCALE_G3", 0x00010000u, 0x0003FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_35, 1, 83},
   {0x0400u, "LSC_CTRL", 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_36, 1, 85},
   {0x0404u, "LSC_MESH_NODES", 0x00000000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_37, 2, 87},
   {0x0408u, "LSC_STRENGTH", 0x00000000u, 0x0001FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_38, 1, 90},
   {0x040Cu, "LSC_PROFILE_SEL", 0x00000000u, 0x00000003u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_39, 1, 92},
   {0x0410u, "LSC_LOAD_CTRL", 0x00000000u, 0x00000003u, 0x00000000u, 0x00000000u, 0x00000700u, 0x00000000u, 0x00000000u, fields_40, 4, 94},
   {0x0414u, "LSC_COEF_DATA", 0x00000000u, 0x001FFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_41, 1, 99},
   {0x0418u, "LSC_LOAD_STATUS", 0x00000000u, 0x00000000u, 0x000FFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_42, 4, 101},
   {0x041Cu, "LSC_PROFILE_STATUS", 0x00000000u, 0x00000000u, 0x000000F7u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_43, 4, 106},
   {0x0420u, "LSC_ERROR", 0x00000000u, 0x00000000u, 0x00000000u, 0x0000003Fu, 0x00000000u, 0x00000000u, 0x00000000u, fields_44, 6, 111},
   {0x0424u, "LSC_STAT_OVF_CNT_R", 0x00000000u, 0x00000000u, 0x001FFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_45, 1, 118},
   {0x0428u, "LSC_STAT_OVF_CNT_GR", 0x00000000u, 0x00000000u, 0x001FFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_46, 1, 120},
   {0x042Cu, "LSC_STAT_OVF_CNT_GB", 0x00000000u, 0x00000000u, 0x001FFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_47, 1, 122},
   {0x0430u, "LSC_STAT_OVF_CNT_B", 0x00000000u, 0x00000000u, 0x001FFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_48, 1, 124},
   {0x0600u, "BPC_CTRL", 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_49, 1, 126},
   {0x0604u, "BPC_MODE", 0x00000001u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_50, 1, 128},
   {0x0608u, "BPC_TEMPORAL_VAR", 0x00000100u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_51, 1, 130},
   {0x060Cu, "BPC_THRESH", 0x004D0192u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_52, 2, 132},
   {0x0610u, "BPC_PIXEL_AGE", 0x000004B0u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_53, 1, 135},
   {0x0614u, "BPC_NUM_CANDIDATES", 0x00000000u, 0x00000000u, 0x00003FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_54, 1, 137},
   {0x0618u, "BPC_NUM_DEFECTIVE", 0x00000000u, 0x00000000u, 0x00003FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_55, 1, 139},
   {0x061Cu, "BPC_STATUS", 0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, fields_56, 1, 141},
   {0x0800u, "WB_CTRL", 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_57, 1, 143},
   {0x0804u, "WB_GAIN_R", 0x00000100u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_58, 1, 145},
   {0x0808u, "WB_GAIN_G", 0x00000100u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_59, 1, 147},
   {0x080Cu, "WB_GAIN_B", 0x00000100u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_60, 1, 149},
   {0x0A00u, "DG_CTRL", 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_61, 1, 151},
   {0x0A04u, "DG_GAIN", 0x00000100u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_62, 1, 153},
   {0x0C00u, "D_WDR_CTRL", 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_63, 1, 155},
   {0x0C04u, "D_WDR_STRENGTH", 0x00000000u, 0x000000FFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_64, 1, 157},
   {0x0E00u, "DEMOSAIC_STATUS", 0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_65, 1, 159},
   {0x1000u, "CCM_CTRL", 0x00000000u, 0x00000005u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_66, 2, 161},
   {0x1004u, "CCM_CRR", 0x00000200u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_67, 1, 164},
   {0x1008u, "CCM_CRG", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_68, 1, 166},
   {0x100Cu, "CCM_CRB", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_69, 1, 168},
   {0x1010u, "CCM_CGR", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_70, 1, 170},
   {0x1014u, "CCM_CGG", 0x00000200u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_71, 1, 172},
   {0x1018u, "CCM_CGB", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_72, 1, 174},
   {0x101Cu, "CCM_CBR", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_73, 1, 176},
   {0x1020u, "CCM_CBG", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_74, 1, 178},
   {0x1024u, "CCM_CBB", 0x00000200u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_75, 1, 180},
   {0x1028u, "CCM_OFS_R", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_76, 1, 182},
   {0x102Cu, "CCM_OFS_G", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_77, 1, 184},
   {0x1030u, "CCM_OFS_B", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_78, 1, 186},
   {0x1034u, "CCM_STATUS", 0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_79, 1, 188},
   {0x1200u, "GAMMA_CTRL", 0x00000000u, 0x00000071u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000100u, fields_80, 3, 190},
   {0x1204u, "GAMMA_LUT_ADDR", 0x00000000u, 0x000000FFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_81, 1, 194},
   {0x1208u, "GAMMA_LUT_DATA", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_82, 1, 196},
   {0x120Cu, "GAMMA_LUT_RDATA", 0x00000000u, 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_83, 1, 198},
   {0x1210u, "GAMMA_STATUS", 0x00000000u, 0x00000000u, 0x00000003u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_84, 2, 200},
   {0x1400u, "CSC_CTRL", 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_85, 1, 203},
   {0x1600u, "GTM_CTRL", 0x00000000u, 0x00000003u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_86, 2, 205},
   {0x1604u, "GTM_LUT_ADDR", 0x00000000u, 0x0000007Fu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_87, 1, 208},
   {0x1608u, "GTM_LUT_DATA", 0x00000000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_88, 1, 210},
   {0x160Cu, "GTM_KEY", 0x0000002Eu, 0x000000FFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_89, 1, 212},
   {0x1610u, "GTM_LWHITE", 0x00000400u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_90, 1, 214},
   {0x1614u, "GTM_ROI_LOG2", 0x0000000Bu, 0x0000001Fu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_91, 1, 216},
   {0x1618u, "GTM_LUT_RDATA", 0x00000000u, 0x00000000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_92, 1, 218},
   {0x161Cu, "GTM_STATUS", 0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_93, 1, 220},
   {0x1800u, "NR_2D_CTRL", 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_94, 1, 222},
   {0x1804u, "NR_2D_EST_VAR", 0x00000000u, 0x00000000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_95, 1, 224},
   {0x1808u, "NR_2D_STATUS", 0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_96, 1, 226},
   {0x1A00u, "TNR_3D_CTRL", 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_97, 1, 228},
   {0x1A04u, "TNR_3D_MTF_SEL", 0x00000000u, 0x00000007u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_98, 1, 230},
   {0x1A08u, "TNR_3D_HIST_BASE", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_99, 1, 232},
   {0x1C00u, "EE_CTRL", 0x00000000u, 0x0000003Fu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_100, 4, 234},
   {0x1C04u, "EE_ALPHA", 0x00004000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_101, 1, 239},
   {0x1C08u, "EE_BETA", 0x00002000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_102, 1, 241},
   {0x1C0Cu, "EE_CLAMP", 0x00001810u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_103, 2, 243},
   {0x1C10u, "EE_FEATURE_EN", 0x00000000u, 0x00000007u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_104, 3, 246},
   {0x1C14u, "EE_RADIAL_CENTER", 0x00000000u, 0x1FFF1FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_105, 2, 250},
   {0x1C18u, "EE_RADIAL_R2_TH0", 0x00000000u, 0x07FFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_106, 1, 253},
   {0x1C1Cu, "EE_RADIAL_R2_TH1", 0x00000000u, 0x07FFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_107, 1, 255},
   {0x1C20u, "EE_RADIAL_R2_TH2", 0x00000000u, 0x07FFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_108, 1, 257},
   {0x1C24u, "EE_RADIAL_GAIN_01", 0x80008000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_109, 2, 259},
   {0x1C28u, "EE_RADIAL_GAIN_23", 0x80008000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_110, 2, 262},
   {0x1C2Cu, "EE_LUT_CTRL", 0x00000000u, 0x00000006u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000100u, fields_111, 2, 265},
   {0x1C30u, "EE_LUT_ADDR", 0x00000000u, 0x0000003Fu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_112, 1, 268},
   {0x1C34u, "EE_LUT_WDATA", 0x00000000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_113, 1, 270},
   {0x1C38u, "EE_STATUS", 0x00000000u, 0x00000000u, 0x00000771u, 0x00000002u, 0x00000000u, 0x00000000u, 0x00000000u, fields_114, 5, 272},
   {0x1C3Cu, "EE_LUT_RDATA", 0x00000000u, 0x00000000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_115, 1, 278},
   {0x1E00u, "CNF_CTRL", 0x00000000u, 0x00000005u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_116, 2, 280},
   {0x1E04u, "CNF_CHROMA_TH", 0x00000000u, 0x000001FFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_117, 1, 283},
   {0x1E08u, "CNF_LUMA_TH", 0x00000000u, 0x000000FFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_118, 1, 285},
   {0x1E0Cu, "CNF_STRENGTH", 0x00000000u, 0x00000003u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_119, 1, 287},
   {0x1E10u, "CNF_STATUS", 0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_120, 1, 289},
   {0x2000u, "RESIZER_CTRL", 0x00000000u, 0x000000FDu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_121, 3, 291},
   {0x2004u, "RESIZER_CUSTOM_W", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_122, 1, 295},
   {0x2008u, "RESIZER_CUSTOM_H", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_123, 1, 297},
   {0x2014u, "RESIZER_OUT_W", 0x00000000u, 0x00000000u, 0x00001FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_124, 1, 299},
   {0x2018u, "RESIZER_OUT_H", 0x00000000u, 0x00000000u, 0x00001FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_125, 1, 301},
   {0x201Cu, "RESIZER_STATUS", 0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_126, 1, 303},
   {0x2020u, "OUTPUT_ENABLE", 0x00000001u, 0x00000007u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_127, 1, 305},
   {0x2024u, "OUTPUT_IRQ_EN", 0x00000000u, 0x00000077u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_128, 2, 307},
   {0x2028u, "OUTPUT_IRQ_STATUS", 0x00000000u, 0x00000000u, 0x00000000u, 0x00000077u, 0x00000000u, 0x00000000u, 0x00000000u, fields_129, 2, 310},
   {0x2200u, "AEC_CTRL", 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000002u, 0x00000000u, 0x00000000u, fields_130, 2, 313},
   {0x2204u, "AEC_CONTEXT_ID", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_131, 1, 316},
   {0x2208u, "AEC_ZONE_CFG", 0x00001820u, 0x00001F3Fu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_132, 2, 318},
   {0x220Cu, "AEC_ZONE_SIZE", 0x005A0078u, 0x0FFF0FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_133, 2, 321},
   {0x2210u, "AEC_SAMPLE_CLIP", 0x0FFF0000u, 0x0FFF0FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_134, 2, 324},
   {0x2214u, "AEC_THRESH", 0x0EA00110u, 0x0FFF0FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_135, 2, 327},
   {0x2218u, "AEC_STATUS", 0x00000000u, 0x00000000u, 0x00000001u, 0x00000006u, 0x00000000u, 0x00000000u, 0x00000000u, fields_136, 3, 330},
   {0x221Cu, "AEC_FRAME_ID", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_137, 1, 334},
   {0x2220u, "AEC_RESULT_CONTEXT_ID", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_138, 1, 336},
   {0x2224u, "AEC_CHANNEL_SEL", 0x00000000u, 0x00000003u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_139, 1, 338},
   {0x2228u, "AEC_GLOBAL_SUM_LO", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_140, 1, 340},
   {0x222Cu, "AEC_GLOBAL_SUM_HI", 0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_141, 1, 342},
   {0x2230u, "AEC_GLOBAL_COUNT", 0x00000000u, 0x00000000u, 0x007FFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_142, 1, 344},
   {0x2234u, "AEC_ZONE_ADDR", 0x00000000u, 0x000003FFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_143, 1, 346},
   {0x2238u, "AEC_ZONE_SUM", 0x00000000u, 0x00000000u, 0x0FFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_144, 1, 348},
   {0x223Cu, "AEC_ZONE_COUNT", 0x00000000u, 0x00000000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_145, 1, 350},
   {0x2240u, "AEC_ZONE_GREEN_OE_UE", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_146, 2, 352},
   {0x2244u, "AEC_ZONE_GREEN_MIN_MAX", 0x00000000u, 0x00000000u, 0x00FFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_147, 2, 355},
   {0x2248u, "AEC_HIST_ADDR", 0x00000000u, 0x0000003Fu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_148, 1, 358},
   {0x224Cu, "AEC_HIST_DATA", 0x00000000u, 0x00000000u, 0x007FFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_149, 1, 360},
   {0x2400u, "AWB_CTRL", 0x00000000u, 0x00003FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_150, 3, 362},
   {0x2404u, "AWB_X_BOUND_ADDR", 0x00000000u, 0x0000003Fu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_151, 1, 366},
   {0x2408u, "AWB_Y_BOUND_ADDR", 0x00000000u, 0x0000001Fu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_152, 1, 368},
   {0x240Cu, "AWB_X_BOUND_DATA", 0x00000000u, 0x00001FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_153, 2, 370},
   {0x2410u, "AWB_Y_BOUND_DATA", 0x00000000u, 0x00001FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_154, 2, 373},
   {0x2414u, "AWB_GLOBAL_SUM_R_H", 0x00000000u, 0x00000000u, 0x00000007u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_155, 1, 376},
   {0x2418u, "AWB_GLOBAL_SUM_R_L", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_156, 1, 378},
   {0x241Cu, "AWB_GLOBAL_SUM_G_H", 0x00000000u, 0x00000000u, 0x00000007u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_157, 1, 380},
   {0x2420u, "AWB_GLOBAL_SUM_G_L", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_158, 1, 382},
   {0x2424u, "AWB_GLOBAL_SUM_B_H", 0x00000000u, 0x00000000u, 0x00000007u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_159, 1, 384},
   {0x2428u, "AWB_GLOBAL_SUM_B_L", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_160, 1, 386},
   {0x242Cu, "AWB_GLOBAL_COUNT", 0x00000000u, 0x00000000u, 0x007FFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_161, 1, 388},
   {0x2430u, "AWB_ZONE_ADDR", 0x00000000u, 0x000007FFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_162, 1, 390},
   {0x2434u, "AWB_ZONE_SUM_R_H", 0x00000000u, 0x00000000u, 0x00000007u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_163, 1, 392},
   {0x2438u, "AWB_ZONE_SUM_R_L", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_164, 1, 394},
   {0x243Cu, "AWB_ZONE_SUM_G_H", 0x00000000u, 0x00000000u, 0x00000007u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_165, 1, 396},
   {0x2440u, "AWB_ZONE_SUM_G_L", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_166, 1, 398},
   {0x2444u, "AWB_ZONE_SUM_B_H", 0x00000000u, 0x00000000u, 0x00000007u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_167, 1, 400},
   {0x2448u, "AWB_ZONE_SUM_B_L", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_168, 1, 402},
   {0x244Cu, "AWB_ZONE_COUNT", 0x00000000u, 0x00000000u, 0x007FFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_169, 1, 404},
   {0x2450u, "AWB_STATUS", 0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, fields_170, 1, 406},
   {0x2454u, "AWB_UNDEREXPOSED_LIMIT", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_171, 1, 408},
   {0x2458u, "AWB_SATURATION_LIMIT", 0x00000FFFu, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_172, 1, 410},
   {0x245Cu, "AWB_CONTEXT_ID", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_173, 1, 412},
   {0x2460u, "AWB_RESULT_CONTEXT_ID", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_174, 1, 414},
   {0x2464u, "AWB_FRAME_ID", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_175, 1, 416},
   {0x2468u, "AWB_RESULT_CTRL", 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000007u, fields_176, 3, 418},
   {0x246Cu, "AWB_RESULT_STATUS", 0x00000000u, 0x00000000u, 0x003F7F7Fu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_177, 8, 422},
   {0x2470u, "AWB_DROP_COUNT", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_178, 1, 431},
   {0x2600u, "AF_CTRL", 0x00000000u, 0x000001E7u, 0x00000000u, 0x00000000u, 0x00000018u, 0x00000000u, 0x00000000u, fields_179, 8, 433},
   {0x2604u, "AF_ZONE_EN", 0x0000FFFFu, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_180, 1, 442},
   {0x2608u, "AF_METRIC_CFG", 0x00000000u, 0x00FFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_181, 2, 444},
   {0x260Cu, "AF_STAT_ADDR", 0x00000000u, 0x0000000Fu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_182, 1, 447},
   {0x2610u, "AF_STAT_DATA", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_183, 1, 449},
   {0x2614u, "AF_LUT_ADDR", 0x00000000u, 0x000000FFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_184, 1, 451},
   {0x2618u, "AF_LUT_DATA", 0x00000000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_185, 1, 453},
   {0x261Cu, "AF_VCM_CUR_POS", 0x00000000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_186, 1, 455},
   {0x2620u, "AF_VCM_BEST_POS", 0x00000000u, 0x00000000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_187, 1, 457},
   {0x2624u, "AF_PEAK_SCORE", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_188, 1, 459},
   {0x2628u, "AF_FOCUS_RANGE", 0x00000000u, 0x00FFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_189, 2, 461},
   {0x262Cu, "AF_STATUS", 0x00000000u, 0x00000000u, 0x00000021u, 0x0000000Eu, 0x00000000u, 0x00000000u, 0x00000000u, fields_190, 5, 464},
   {0x2630u, "AF_CONTEXT_ID", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_191, 1, 470},
   {0x2634u, "AF_RESULT_CONTEXT_ID", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_192, 1, 472},
   {0x2638u, "AF_FRAME_ID", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_193, 1, 474},
   {0x2800u, "OFMT_CTRL", 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_194, 1, 476},
   {0x2804u, "OFMT_Y_STRIDE", 0x00000F00u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_195, 1, 478},
   {0x2808u, "OFMT_UV_STRIDE", 0x00000F00u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_196, 1, 480},
   {0x3000u, "DMA_CTRL", 0x00003F00u, 0x0000FF03u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000004u, 0x00000000u, fields_197, 4, 482},
   {0x3004u, "DMA_STAT", 0x00000000u, 0x00000000u, 0x00000003u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_198, 2, 487},
   {0x3008u, "DMA_ERR", 0x00000000u, 0x00000000u, 0x00000000u, 0x0000001Fu, 0x00000000u, 0x00000000u, 0x00000000u, fields_199, 1, 490},
   {0x300Cu, "DMA_IRQ_EN", 0x00000000u, 0x0000007Fu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_200, 1, 492},
   {0x3010u, "DMA_IRQ_STAT", 0x00000000u, 0x00000000u, 0x00000000u, 0x0000007Fu, 0x00000000u, 0x00000000u, 0x00000000u, fields_201, 1, 494},
   {0x3020u, "IDMA_STRIDE", 0x00001E00u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_202, 1, 496},
   {0x3024u, "IDMA_BUF_VALID", 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x0000000Fu, 0x00000000u, 0x00000000u, fields_203, 1, 498},
   {0x3028u, "IDMA_FRAME_COUNT", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_204, 1, 500},
   {0x3040u, "IDMA_BUF_ADDR_L0", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_205, 1, 502},
   {0x3044u, "IDMA_BUF_ADDR_H0", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_206, 1, 504},
   {0x3048u, "IDMA_BUF_ADDR_L1", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_207, 1, 506},
   {0x304Cu, "IDMA_BUF_ADDR_H1", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_208, 1, 508},
   {0x3050u, "IDMA_BUF_ADDR_L2", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_209, 1, 510},
   {0x3054u, "IDMA_BUF_ADDR_H2", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_210, 1, 512},
   {0x3058u, "IDMA_BUF_ADDR_L3", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_211, 1, 514},
   {0x305Cu, "IDMA_BUF_ADDR_H3", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_212, 1, 516},
   {0x3080u, "ODMA_Y_STRIDE", 0x00000F00u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_213, 1, 518},
   {0x3084u, "ODMA_UV_STRIDE", 0x00000F00u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_214, 1, 520},
   {0x3088u, "ODMA_BUF_FREE", 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x0000000Fu, 0x00000000u, 0x00000000u, fields_215, 1, 522},
   {0x308Cu, "ODMA_BUF_DONE", 0x00000000u, 0x00000000u, 0x00000000u, 0x0000000Fu, 0x00000000u, 0x00000000u, 0x00000000u, fields_216, 1, 524},
   {0x3090u, "ODMA_FRAME_COUNT", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_217, 1, 526},
   {0x30A0u, "ODMA_Y_ADDR_L0", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_218, 1, 528},
   {0x30A4u, "ODMA_Y_ADDR_H0", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_219, 1, 530},
   {0x30A8u, "ODMA_UV_ADDR_L0", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_220, 1, 532},
   {0x30ACu, "ODMA_UV_ADDR_H0", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_221, 1, 534},
   {0x30B0u, "ODMA_Y_ADDR_L1", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_222, 1, 536},
   {0x30B4u, "ODMA_Y_ADDR_H1", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_223, 1, 538},
   {0x30B8u, "ODMA_UV_ADDR_L1", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_224, 1, 540},
   {0x30BCu, "ODMA_UV_ADDR_H1", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_225, 1, 542},
   {0x30C0u, "ODMA_Y_ADDR_L2", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_226, 1, 544},
   {0x30C4u, "ODMA_Y_ADDR_H2", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_227, 1, 546},
   {0x30C8u, "ODMA_UV_ADDR_L2", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_228, 1, 548},
   {0x30CCu, "ODMA_UV_ADDR_H2", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_229, 1, 550},
   {0x30D0u, "ODMA_Y_ADDR_L3", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_230, 1, 552},
   {0x30D4u, "ODMA_Y_ADDR_H3", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_231, 1, 554},
   {0x30D8u, "ODMA_UV_ADDR_L3", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_232, 1, 556},
   {0x30DCu, "ODMA_UV_ADDR_H3", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_233, 1, 558},
   {0x3100u, "IDMA_BUF_TAG0", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_234, 1, 560},
   {0x3104u, "IDMA_BUF_TAG1", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_235, 1, 562},
   {0x3108u, "IDMA_BUF_TAG2", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_236, 1, 564},
   {0x310Cu, "IDMA_BUF_TAG3", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_237, 1, 566},
   {0x3110u, "IDMA_BUF_TS0", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_238, 1, 568},
   {0x3114u, "IDMA_BUF_TS1", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_239, 1, 570},
   {0x3118u, "IDMA_BUF_TS2", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_240, 1, 572},
   {0x311Cu, "IDMA_BUF_TS3", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_241, 1, 574},
   {0x3120u, "ODMA_BUF_TAG0", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_242, 1, 576},
   {0x3124u, "ODMA_BUF_TAG1", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_243, 1, 578},
   {0x3128u, "ODMA_BUF_TAG2", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_244, 1, 580},
   {0x312Cu, "ODMA_BUF_TAG3", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_245, 1, 582},
   {0x3130u, "ODMA_BUF_TS0", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_246, 1, 584},
   {0x3134u, "ODMA_BUF_TS1", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_247, 1, 586},
   {0x3138u, "ODMA_BUF_TS2", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_248, 1, 588},
   {0x313Cu, "ODMA_BUF_TS3", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_249, 1, 590},
   {0x3140u, "ODMA_BUF_SOF_TS0", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_250, 1, 592},
   {0x3144u, "ODMA_BUF_SOF_TS1", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_251, 1, 594},
   {0x3148u, "ODMA_BUF_SOF_TS2", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_252, 1, 596},
   {0x314Cu, "ODMA_BUF_SOF_TS3", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_253, 1, 598},
   {0x3150u, "ODMA_BUF_EOF_TS0", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_254, 1, 600},
   {0x3154u, "ODMA_BUF_EOF_TS1", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_255, 1, 602},
   {0x3158u, "ODMA_BUF_EOF_TS2", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_256, 1, 604},
   {0x315Cu, "ODMA_BUF_EOF_TS3", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_257, 1, 606},
   {0x3160u, "FRAME_COUNTER", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_258, 1, 608},
   {0x3200u, "CH1_RESIZER_CTRL", 0x00000000u, 0x000000FDu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_259, 3, 610},
   {0x3204u, "CH1_RESIZER_CUSTOM_W", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_260, 1, 614},
   {0x3208u, "CH1_RESIZER_CUSTOM_H", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_261, 1, 616},
   {0x3214u, "CH1_RESIZER_OUT_W", 0x00000000u, 0x00000000u, 0x00001FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_262, 1, 618},
   {0x3218u, "CH1_RESIZER_OUT_H", 0x00000000u, 0x00000000u, 0x00001FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_263, 1, 620},
   {0x321Cu, "CH1_RESIZER_STATUS", 0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_264, 1, 622},
   {0x3280u, "CH1_ODMA_Y_STRIDE", 0x00000F00u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_265, 1, 624},
   {0x3284u, "CH1_ODMA_UV_STRIDE", 0x00000F00u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_266, 1, 626},
   {0x3288u, "CH1_ODMA_BUF_FREE", 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x0000000Fu, 0x00000000u, 0x00000000u, fields_267, 1, 628},
   {0x328Cu, "CH1_ODMA_BUF_DONE", 0x00000000u, 0x00000000u, 0x00000000u, 0x0000000Fu, 0x00000000u, 0x00000000u, 0x00000000u, fields_268, 1, 630},
   {0x3290u, "CH1_ODMA_FRAME_COUNT", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_269, 1, 632},
   {0x32A0u, "CH1_ODMA_Y_ADDR_L0", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_270, 1, 634},
   {0x32A4u, "CH1_ODMA_Y_ADDR_H0", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_271, 1, 636},
   {0x32A8u, "CH1_ODMA_UV_ADDR_L0", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_272, 1, 638},
   {0x32ACu, "CH1_ODMA_UV_ADDR_H0", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_273, 1, 640},
   {0x32B0u, "CH1_ODMA_Y_ADDR_L1", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_274, 1, 642},
   {0x32B4u, "CH1_ODMA_Y_ADDR_H1", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_275, 1, 644},
   {0x32B8u, "CH1_ODMA_UV_ADDR_L1", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_276, 1, 646},
   {0x32BCu, "CH1_ODMA_UV_ADDR_H1", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_277, 1, 648},
   {0x32C0u, "CH1_ODMA_Y_ADDR_L2", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_278, 1, 650},
   {0x32C4u, "CH1_ODMA_Y_ADDR_H2", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_279, 1, 652},
   {0x32C8u, "CH1_ODMA_UV_ADDR_L2", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_280, 1, 654},
   {0x32CCu, "CH1_ODMA_UV_ADDR_H2", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_281, 1, 656},
   {0x32D0u, "CH1_ODMA_Y_ADDR_L3", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_282, 1, 658},
   {0x32D4u, "CH1_ODMA_Y_ADDR_H3", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_283, 1, 660},
   {0x32D8u, "CH1_ODMA_UV_ADDR_L3", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_284, 1, 662},
   {0x32DCu, "CH1_ODMA_UV_ADDR_H3", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_285, 1, 664},
   {0x3300u, "CH1_ODMA_BUF_TAG0", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_286, 1, 666},
   {0x3304u, "CH1_ODMA_BUF_TAG1", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_287, 1, 668},
   {0x3308u, "CH1_ODMA_BUF_TAG2", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_288, 1, 670},
   {0x330Cu, "CH1_ODMA_BUF_TAG3", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_289, 1, 672},
   {0x3310u, "CH1_ODMA_BUF_TS0", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_290, 1, 674},
   {0x3314u, "CH1_ODMA_BUF_TS1", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_291, 1, 676},
   {0x3318u, "CH1_ODMA_BUF_TS2", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_292, 1, 678},
   {0x331Cu, "CH1_ODMA_BUF_TS3", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_293, 1, 680},
   {0x3320u, "CH1_ODMA_BUF_SOF_TS0", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_294, 1, 682},
   {0x3324u, "CH1_ODMA_BUF_SOF_TS1", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_295, 1, 684},
   {0x3328u, "CH1_ODMA_BUF_SOF_TS2", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_296, 1, 686},
   {0x332Cu, "CH1_ODMA_BUF_SOF_TS3", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_297, 1, 688},
   {0x3330u, "CH1_ODMA_BUF_EOF_TS0", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_298, 1, 690},
   {0x3334u, "CH1_ODMA_BUF_EOF_TS1", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_299, 1, 692},
   {0x3338u, "CH1_ODMA_BUF_EOF_TS2", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_300, 1, 694},
   {0x333Cu, "CH1_ODMA_BUF_EOF_TS3", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_301, 1, 696},
   {0x3400u, "CH2_RESIZER_CTRL", 0x00000000u, 0x000000FDu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_302, 3, 698},
   {0x3404u, "CH2_RESIZER_CUSTOM_W", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_303, 1, 702},
   {0x3408u, "CH2_RESIZER_CUSTOM_H", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_304, 1, 704},
   {0x3414u, "CH2_RESIZER_OUT_W", 0x00000000u, 0x00000000u, 0x00001FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_305, 1, 706},
   {0x3418u, "CH2_RESIZER_OUT_H", 0x00000000u, 0x00000000u, 0x00001FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_306, 1, 708},
   {0x341Cu, "CH2_RESIZER_STATUS", 0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_307, 1, 710},
   {0x3480u, "CH2_ODMA_Y_STRIDE", 0x00000F00u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_308, 1, 712},
   {0x3488u, "CH2_ODMA_BUF_FREE", 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x0000000Fu, 0x00000000u, 0x00000000u, fields_309, 1, 714},
   {0x348Cu, "CH2_ODMA_BUF_DONE", 0x00000000u, 0x00000000u, 0x00000000u, 0x0000000Fu, 0x00000000u, 0x00000000u, 0x00000000u, fields_310, 1, 716},
   {0x3490u, "CH2_ODMA_FRAME_COUNT", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_311, 1, 718},
   {0x34A0u, "CH2_ODMA_Y_ADDR_L0", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_312, 1, 720},
   {0x34A4u, "CH2_ODMA_Y_ADDR_H0", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_313, 1, 722},
   {0x34B0u, "CH2_ODMA_Y_ADDR_L1", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_314, 1, 724},
   {0x34B4u, "CH2_ODMA_Y_ADDR_H1", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_315, 1, 726},
   {0x34C0u, "CH2_ODMA_Y_ADDR_L2", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_316, 1, 728},
   {0x34C4u, "CH2_ODMA_Y_ADDR_H2", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_317, 1, 730},
   {0x34D0u, "CH2_ODMA_Y_ADDR_L3", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_318, 1, 732},
   {0x34D4u, "CH2_ODMA_Y_ADDR_H3", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_319, 1, 734},
   {0x3500u, "CH2_ODMA_BUF_TAG0", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_320, 1, 736},
   {0x3504u, "CH2_ODMA_BUF_TAG1", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_321, 1, 738},
   {0x3508u, "CH2_ODMA_BUF_TAG2", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_322, 1, 740},
   {0x350Cu, "CH2_ODMA_BUF_TAG3", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_323, 1, 742},
   {0x3510u, "CH2_ODMA_BUF_TS0", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_324, 1, 744},
   {0x3514u, "CH2_ODMA_BUF_TS1", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_325, 1, 746},
   {0x3518u, "CH2_ODMA_BUF_TS2", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_326, 1, 748},
   {0x351Cu, "CH2_ODMA_BUF_TS3", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_327, 1, 750},
   {0x3520u, "CH2_ODMA_BUF_SOF_TS0", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_328, 1, 752},
   {0x3524u, "CH2_ODMA_BUF_SOF_TS1", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_329, 1, 754},
   {0x3528u, "CH2_ODMA_BUF_SOF_TS2", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_330, 1, 756},
   {0x352Cu, "CH2_ODMA_BUF_SOF_TS3", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_331, 1, 758},
   {0x3530u, "CH2_ODMA_BUF_EOF_TS0", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_332, 1, 760},
   {0x3534u, "CH2_ODMA_BUF_EOF_TS1", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_333, 1, 762},
   {0x3538u, "CH2_ODMA_BUF_EOF_TS2", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_334, 1, 764},
   {0x353Cu, "CH2_ODMA_BUF_EOF_TS3", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_335, 1, 766},
   {0x8000u, "TUNE_ID", 0x54554E45u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_336, 1, 768},
   {0x8004u, "TUNE_REV", 0x00010007u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_337, 1, 770},
   {0x8008u, "TUNE_CAPS", 0x003FFFFFu, 0x00000000u, 0x003FFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_338, 22, 772},
   {0x800Cu, "TUNE_MODE", 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_339, 1, 795},
   {0x8010u, "TUNE_BANK", 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_340, 1, 797},
   {0x8014u, "TUNE_PROFILE_ID", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_341, 1, 799},
   {0x8018u, "TUNE_APPLY_FRAME", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_342, 1, 801},
   {0x801Cu, "TUNE_CMD", 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x0000011Fu, fields_343, 6, 803},
   {0x8020u, "TUNE_STATUS", 0x00000000u, 0x00000000u, 0x000007FFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_344, 11, 810},
   {0x8024u, "TUNE_ACTIVE_ID", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_345, 1, 822},
   {0x8028u, "TUNE_ACTIVE_FRAME", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_346, 1, 824},
   {0x802Cu, "TUNE_ERROR", 0x00000000u, 0x00000000u, 0x00000000u, 0x000000FFu, 0x00000000u, 0x00000000u, 0x00000000u, fields_347, 8, 826},
   {0x8030u, "TUNE_NEXT_FRAME", 0x00000001u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_348, 1, 835},
   {0x8040u, "TUNE_CH0_BUF0_PROFILE", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_349, 1, 837},
   {0x8044u, "TUNE_CH0_BUF1_PROFILE", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_350, 1, 839},
   {0x8048u, "TUNE_CH0_BUF2_PROFILE", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_351, 1, 841},
   {0x804Cu, "TUNE_CH0_BUF3_PROFILE", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_352, 1, 843},
   {0x8050u, "TUNE_CH1_BUF0_PROFILE", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_353, 1, 845},
   {0x8054u, "TUNE_CH1_BUF1_PROFILE", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_354, 1, 847},
   {0x8058u, "TUNE_CH1_BUF2_PROFILE", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_355, 1, 849},
   {0x805Cu, "TUNE_CH1_BUF3_PROFILE", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_356, 1, 851},
   {0x8060u, "TUNE_CH2_BUF0_PROFILE", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_357, 1, 853},
   {0x8064u, "TUNE_CH2_BUF1_PROFILE", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_358, 1, 855},
   {0x8068u, "TUNE_CH2_BUF2_PROFILE", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_359, 1, 857},
   {0x806Cu, "TUNE_CH2_BUF3_PROFILE", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_360, 1, 859},
   {0x8100u, "TUNE_NR_CTRL", 0x00000004u, 0x00000007u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_361, 2, 861},
   {0x8104u, "TUNE_NR_VARIANCE", 0x00000000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_362, 1, 864},
   {0x8108u, "TUNE_NR_SCALE", 0x00000100u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_363, 1, 866},
   {0x810Cu, "TUNE_NR_LIMITS", 0xFFFF0000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_364, 2, 868},
   {0x8110u, "TUNE_NR_LL_SCALE", 0x00000100u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_365, 1, 871},
   {0x8114u, "TUNE_NR_LH_SCALE", 0x00000100u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_366, 1, 873},
   {0x8118u, "TUNE_NR_HL_SCALE", 0x00000100u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_367, 1, 875},
   {0x811Cu, "TUNE_NR_HH_SCALE", 0x00000100u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_368, 1, 877},
   {0x8120u, "TUNE_NR_BLEND", 0x00000100u, 0x000001FFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_369, 1, 879},
   {0x8124u, "TUNE_NR_LUT_ADDR", 0x00000000u, 0x0000001Fu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_370, 1, 881},
   {0x8128u, "TUNE_NR_LUT_DATA", 0x00000000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_371, 1, 883},
   {0x812Cu, "TUNE_NR_SEED", 0x00000000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_372, 1, 885},
   {0x8200u, "TUNE_WB_GB", 0x00000100u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_373, 1, 887},
   {0x8204u, "TUNE_DEMOSAIC_MARGIN", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_374, 2, 889},
   {0x8208u, "TUNE_CNF_BLEND", 0x00000100u, 0x000001FFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_375, 1, 892},
   {0x8210u, "TUNE_CSC_CTRL", 0x00000002u, 0x00000003u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_376, 2, 894},
   {0x8214u, "TUNE_CSC_CONTRAST", 0x00000100u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_377, 1, 897},
   {0x8218u, "TUNE_CSC_BRIGHTNESS", 0x00000000u, 0x000003FFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_378, 1, 899},
   {0x8220u, "TUNE_CSC_UU", 0x00004000u, 0x0003FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_379, 1, 901},
   {0x8224u, "TUNE_CSC_UV", 0x00000000u, 0x0003FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_380, 1, 903},
   {0x8228u, "TUNE_CSC_VU", 0x00000000u, 0x0003FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_381, 1, 905},
   {0x822Cu, "TUNE_CSC_VV", 0x00004000u, 0x0003FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_382, 1, 907},
   {0x8300u, "TUNE_AWB_CTRL", 0x00000000u, 0x0000FF03u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_383, 3, 909},
   {0x8304u, "TUNE_AWB_ROI_START", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_384, 2, 913},
   {0x8308u, "TUNE_AWB_ROI_END", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_385, 2, 916},
   {0x8310u, "TUNE_AWB_R_LIMITS", 0x0FFF0000u, 0x0FFF0FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_386, 2, 919},
   {0x8314u, "TUNE_AWB_G_LIMITS", 0x0FFF0000u, 0x0FFF0FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_387, 2, 922},
   {0x8318u, "TUNE_AWB_B_LIMITS", 0x0FFF0000u, 0x0FFF0FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_388, 2, 925},
   {0x831Cu, "TUNE_AWB_MIN_G", 0x00000001u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_389, 1, 928},
   {0x8340u, "TUNE_AWB_E0_CENTER", 0x01000100u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_390, 2, 930},
   {0x8344u, "TUNE_AWB_E0_A", 0x00004000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_391, 1, 933},
   {0x8348u, "TUNE_AWB_E0_B", 0x00000000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_392, 1, 935},
   {0x834Cu, "TUNE_AWB_E0_C", 0x00004000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_393, 1, 937},
   {0x8350u, "TUNE_AWB_E0_RADIUS2", 0x00010000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_394, 1, 939},
   {0x8360u, "TUNE_AWB_E1_CENTER", 0x01000100u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_395, 2, 941},
   {0x8364u, "TUNE_AWB_E1_A", 0x00004000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_396, 1, 944},
   {0x8368u, "TUNE_AWB_E1_B", 0x00000000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_397, 1, 946},
   {0x836Cu, "TUNE_AWB_E1_C", 0x00004000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_398, 1, 948},
   {0x8370u, "TUNE_AWB_E1_RADIUS2", 0x00010000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_399, 1, 950},
   {0x8380u, "TUNE_AWB_E2_CENTER", 0x01000100u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_400, 2, 952},
   {0x8384u, "TUNE_AWB_E2_A", 0x00004000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_401, 1, 955},
   {0x8388u, "TUNE_AWB_E2_B", 0x00000000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_402, 1, 957},
   {0x838Cu, "TUNE_AWB_E2_C", 0x00004000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_403, 1, 959},
   {0x8390u, "TUNE_AWB_E2_RADIUS2", 0x00010000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_404, 1, 961},
   {0x83A0u, "TUNE_AWB_E3_CENTER", 0x01000100u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_405, 2, 963},
   {0x83A4u, "TUNE_AWB_E3_A", 0x00004000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_406, 1, 966},
   {0x83A8u, "TUNE_AWB_E3_B", 0x00000000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_407, 1, 968},
   {0x83ACu, "TUNE_AWB_E3_C", 0x00004000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_408, 1, 970},
   {0x83B0u, "TUNE_AWB_E3_RADIUS2", 0x00010000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_409, 1, 972},
   {0x83C0u, "TUNE_AWB_E4_CENTER", 0x01000100u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_410, 2, 974},
   {0x83C4u, "TUNE_AWB_E4_A", 0x00004000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_411, 1, 977},
   {0x83C8u, "TUNE_AWB_E4_B", 0x00000000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_412, 1, 979},
   {0x83CCu, "TUNE_AWB_E4_C", 0x00004000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_413, 1, 981},
   {0x83D0u, "TUNE_AWB_E4_RADIUS2", 0x00010000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_414, 1, 983},
   {0x83E0u, "TUNE_AWB_E5_CENTER", 0x01000100u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_415, 2, 985},
   {0x83E4u, "TUNE_AWB_E5_A", 0x00004000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_416, 1, 988},
   {0x83E8u, "TUNE_AWB_E5_B", 0x00000000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_417, 1, 990},
   {0x83ECu, "TUNE_AWB_E5_C", 0x00004000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_418, 1, 992},
   {0x83F0u, "TUNE_AWB_E5_RADIUS2", 0x00010000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_419, 1, 994},
   {0x8400u, "TUNE_AWB_E6_CENTER", 0x01000100u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_420, 2, 996},
   {0x8404u, "TUNE_AWB_E6_A", 0x00004000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_421, 1, 999},
   {0x8408u, "TUNE_AWB_E6_B", 0x00000000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_422, 1, 1001},
   {0x840Cu, "TUNE_AWB_E6_C", 0x00004000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_423, 1, 1003},
   {0x8410u, "TUNE_AWB_E6_RADIUS2", 0x00010000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_424, 1, 1005},
   {0x8420u, "TUNE_AWB_E7_CENTER", 0x01000100u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_425, 2, 1007},
   {0x8424u, "TUNE_AWB_E7_A", 0x00004000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_426, 1, 1010},
   {0x8428u, "TUNE_AWB_E7_B", 0x00000000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_427, 1, 1012},
   {0x842Cu, "TUNE_AWB_E7_C", 0x00004000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_428, 1, 1014},
   {0x8430u, "TUNE_AWB_E7_RADIUS2", 0x00010000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_429, 1, 1016},
   {0x8500u, "TUNE_AF_CTRL", 0xFFFF0000u, 0xFFFF0001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_430, 2, 1018},
   {0x8504u, "TUNE_AF_NOISE", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_431, 1, 1021},
   {0x8540u, "TUNE_AF_W0_START", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_432, 2, 1023},
   {0x8544u, "TUNE_AF_W0_END", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_433, 2, 1026},
   {0x8548u, "TUNE_AF_W1_START", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_434, 2, 1029},
   {0x854Cu, "TUNE_AF_W1_END", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_435, 2, 1032},
   {0x8550u, "TUNE_AF_W2_START", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_436, 2, 1035},
   {0x8554u, "TUNE_AF_W2_END", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_437, 2, 1038},
   {0x8558u, "TUNE_AF_W3_START", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_438, 2, 1041},
   {0x855Cu, "TUNE_AF_W3_END", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_439, 2, 1044},
   {0x8560u, "TUNE_AF_W4_START", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_440, 2, 1047},
   {0x8564u, "TUNE_AF_W4_END", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_441, 2, 1050},
   {0x8568u, "TUNE_AF_W5_START", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_442, 2, 1053},
   {0x856Cu, "TUNE_AF_W5_END", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_443, 2, 1056},
   {0x8570u, "TUNE_AF_W6_START", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_444, 2, 1059},
   {0x8574u, "TUNE_AF_W6_END", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_445, 2, 1062},
   {0x8578u, "TUNE_AF_W7_START", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_446, 2, 1065},
   {0x857Cu, "TUNE_AF_W7_END", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_447, 2, 1068},
   {0x8580u, "TUNE_AF_W8_START", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_448, 2, 1071},
   {0x8584u, "TUNE_AF_W8_END", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_449, 2, 1074},
   {0x8588u, "TUNE_AF_W9_START", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_450, 2, 1077},
   {0x858Cu, "TUNE_AF_W9_END", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_451, 2, 1080},
   {0x8590u, "TUNE_AF_W10_START", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_452, 2, 1083},
   {0x8594u, "TUNE_AF_W10_END", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_453, 2, 1086},
   {0x8598u, "TUNE_AF_W11_START", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_454, 2, 1089},
   {0x859Cu, "TUNE_AF_W11_END", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_455, 2, 1092},
   {0x85A0u, "TUNE_AF_W12_START", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_456, 2, 1095},
   {0x85A4u, "TUNE_AF_W12_END", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_457, 2, 1098},
   {0x85A8u, "TUNE_AF_W13_START", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_458, 2, 1101},
   {0x85ACu, "TUNE_AF_W13_END", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_459, 2, 1104},
   {0x85B0u, "TUNE_AF_W14_START", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_460, 2, 1107},
   {0x85B4u, "TUNE_AF_W14_END", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_461, 2, 1110},
   {0x85B8u, "TUNE_AF_W15_START", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_462, 2, 1113},
   {0x85BCu, "TUNE_AF_W15_END", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_463, 2, 1116},
   {0x8600u, "TUNE_HIST_CTRL", 0x00000000u, 0x00000003u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_464, 2, 1119},
   {0x8604u, "TUNE_HIST_START", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_465, 2, 1122},
   {0x8608u, "TUNE_HIST_END", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_466, 2, 1125},
   {0x860Cu, "TUNE_HIST_STEP", 0x00000101u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_467, 2, 1128},
   {0x8640u, "TUNE_HIST_WEIGHT0", 0x00000001u, 0x000000FFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_468, 1, 1131},
   {0x8644u, "TUNE_HIST_WEIGHT1", 0x00000001u, 0x000000FFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_469, 1, 1133},
   {0x8648u, "TUNE_HIST_WEIGHT2", 0x00000001u, 0x000000FFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_470, 1, 1135},
   {0x864Cu, "TUNE_HIST_WEIGHT3", 0x00000001u, 0x000000FFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_471, 1, 1137},
   {0x8650u, "TUNE_HIST_WEIGHT4", 0x00000001u, 0x000000FFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_472, 1, 1139},
   {0x8654u, "TUNE_HIST_WEIGHT5", 0x00000001u, 0x000000FFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_473, 1, 1141},
   {0x8658u, "TUNE_HIST_WEIGHT6", 0x00000001u, 0x000000FFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_474, 1, 1143},
   {0x865Cu, "TUNE_HIST_WEIGHT7", 0x00000001u, 0x000000FFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_475, 1, 1145},
   {0x8660u, "TUNE_HIST_WEIGHT8", 0x00000001u, 0x000000FFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_476, 1, 1147},
   {0x8664u, "TUNE_HIST_WEIGHT9", 0x00000001u, 0x000000FFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_477, 1, 1149},
   {0x8668u, "TUNE_HIST_WEIGHT10", 0x00000001u, 0x000000FFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_478, 1, 1151},
   {0x866Cu, "TUNE_HIST_WEIGHT11", 0x00000001u, 0x000000FFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_479, 1, 1153},
   {0x8670u, "TUNE_HIST_WEIGHT12", 0x00000001u, 0x000000FFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_480, 1, 1155},
   {0x8674u, "TUNE_HIST_WEIGHT13", 0x00000001u, 0x000000FFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_481, 1, 1157},
   {0x8678u, "TUNE_HIST_WEIGHT14", 0x00000001u, 0x000000FFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_482, 1, 1159},
   {0x867Cu, "TUNE_HIST_WEIGHT15", 0x00000001u, 0x000000FFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_483, 1, 1161},
   {0x8680u, "TUNE_HIST_WEIGHT16", 0x00000001u, 0x000000FFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_484, 1, 1163},
   {0x8684u, "TUNE_HIST_WEIGHT17", 0x00000001u, 0x000000FFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_485, 1, 1165},
   {0x8688u, "TUNE_HIST_WEIGHT18", 0x00000001u, 0x000000FFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_486, 1, 1167},
   {0x868Cu, "TUNE_HIST_WEIGHT19", 0x00000001u, 0x000000FFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_487, 1, 1169},
   {0x8690u, "TUNE_HIST_WEIGHT20", 0x00000001u, 0x000000FFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_488, 1, 1171},
   {0x8694u, "TUNE_HIST_WEIGHT21", 0x00000001u, 0x000000FFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_489, 1, 1173},
   {0x8698u, "TUNE_HIST_WEIGHT22", 0x00000001u, 0x000000FFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_490, 1, 1175},
   {0x869Cu, "TUNE_HIST_WEIGHT23", 0x00000001u, 0x000000FFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_491, 1, 1177},
   {0x86A0u, "TUNE_HIST_WEIGHT24", 0x00000001u, 0x000000FFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_492, 1, 1179},
   {0x8700u, "TUNE_AEC_CMD", 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000003u, fields_493, 2, 1181},
   {0x8704u, "TUNE_AEC_STATUS", 0x00000000u, 0x00000000u, 0x0000000Fu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_494, 3, 1184},
   {0x8708u, "TUNE_AEC_DROPS", 0x00000000u, 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, fields_495, 1, 1188},
   {0x870Cu, "TUNE_AEC_FRAME", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_496, 1, 1190},
   {0x8710u, "TUNE_AEC_PROFILE", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_497, 1, 1192},
   {0x8714u, "TUNE_AEC_INDEX", 0x00000000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_498, 1, 1194},
   {0x8718u, "TUNE_AEC_DATA_LO", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_499, 1, 1196},
   {0x871Cu, "TUNE_AEC_DATA_HI", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_500, 1, 1198},
   {0x8740u, "TUNE_AF_CMD", 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000003u, fields_501, 2, 1200},
   {0x8744u, "TUNE_AF_STATUS", 0x00000000u, 0x00000000u, 0x0000000Fu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_502, 3, 1203},
   {0x8748u, "TUNE_AF_DROPS", 0x00000000u, 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, fields_503, 1, 1207},
   {0x874Cu, "TUNE_AF_FRAME", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_504, 1, 1209},
   {0x8750u, "TUNE_AF_PROFILE", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_505, 1, 1211},
   {0x8754u, "TUNE_AF_INDEX", 0x00000000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_506, 1, 1213},
   {0x8758u, "TUNE_AF_DATA_LO", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_507, 1, 1215},
   {0x875Cu, "TUNE_AF_DATA_HI", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_508, 1, 1217},
   {0x8780u, "TUNE_AWB_PROFILE", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_509, 1, 1219},
   {0x8800u, "TUNE_GAMMA_PORT", 0x00000000u, 0x000000FFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_510, 1, 1221},
   {0x8804u, "TUNE_GAMMA_DATA", 0x00000000u, 0x00000FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_511, 1, 1223},
   {0x8808u, "TUNE_TABLE_CMD", 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x0000000Fu, fields_512, 4, 1225},
   {0x880Cu, "TUNE_TABLE_STATUS", 0x00000000u, 0x00000000u, 0x0000000Fu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_513, 4, 1230},
   {0x8900u, "TUNE_BPC_CTRL", 0x00000000u, 0x00000003u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_514, 2, 1235},
   {0x8904u, "TUNE_BPC_G_THRESH", 0x00000100u, 0x0FFF0FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_515, 2, 1238},
   {0x8908u, "TUNE_BPC_RB_THRESH", 0x00000100u, 0x0FFF0FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_516, 2, 1241},
   {0x8910u, "TUNE_BPC_COUNT", 0x00000000u, 0x000007FFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_517, 1, 1244},
   {0x8914u, "TUNE_BPC_ADDR", 0x00000000u, 0x000003FFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_518, 1, 1246},
   {0x8918u, "TUNE_BPC_DATA", 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_519, 2, 1248},
   {0x8A00u, "TUNE_LSC_CTRL", 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_520, 1, 1251},
   {0x8A04u, "TUNE_LSC_SECTOR_ADDR", 0x00000000u, 0x0000017Fu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_521, 2, 1253},
   {0x8A08u, "TUNE_LSC_SECTOR_DATA", 0x00000000u, 0x0000FFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_522, 1, 1256},
   {0x8B00u, "COMMON_INPUT_FORMAT", 0x00000000u, 0x00000003u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_523, 1, 1258},
   {0x8B08u, "TUNE_AEC_SOURCE", 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_524, 1, 1260},
   {0x8B0Cu, "TUNE_AEC_RESULT_META", 0x00000000u, 0x00000000u, 0x800003F1u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_525, 4, 1262},
   {0x8B40u, "CH0_CROP_CTRL", 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_526, 1, 1267},
   {0x8B44u, "CH0_CROP_ORIGIN", 0x00000000u, 0x1FFF1FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_527, 2, 1269},
   {0x8B48u, "CH0_CROP_SIZE", 0x00000000u, 0x1FFF1FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_528, 2, 1272},
   {0x8B4Cu, "CH0_GEOMETRY_STATUS", 0x00000000u, 0x00000000u, 0x000000F3u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_529, 3, 1275},
   {0x8B50u, "CH0_ACTIVE_CROP_ORIGIN", 0x00000000u, 0x00000000u, 0x1FFF1FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_530, 2, 1279},
   {0x8B54u, "CH0_ACTIVE_CROP_SIZE", 0x00000000u, 0x00000000u, 0x1FFF1FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_531, 2, 1282},
   {0x8B58u, "CH0_OUTPUT_LIMIT", 0x08700F00u, 0x00000000u, 0x1FFF1FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_532, 2, 1285},
   {0x8B80u, "CH1_CROP_CTRL", 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_533, 1, 1288},
   {0x8B84u, "CH1_CROP_ORIGIN", 0x00000000u, 0x1FFF1FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_534, 2, 1290},
   {0x8B88u, "CH1_CROP_SIZE", 0x00000000u, 0x1FFF1FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_535, 2, 1293},
   {0x8B8Cu, "CH1_GEOMETRY_STATUS", 0x00000000u, 0x00000000u, 0x000000F3u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_536, 3, 1296},
   {0x8B90u, "CH1_ACTIVE_CROP_ORIGIN", 0x00000000u, 0x00000000u, 0x1FFF1FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_537, 2, 1300},
   {0x8B94u, "CH1_ACTIVE_CROP_SIZE", 0x00000000u, 0x00000000u, 0x1FFF1FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_538, 2, 1303},
   {0x8B98u, "CH1_OUTPUT_LIMIT", 0x08700F00u, 0x00000000u, 0x1FFF1FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_539, 2, 1306},
   {0x8BC0u, "CH2_CROP_CTRL", 0x00000000u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_540, 1, 1309},
   {0x8BC4u, "CH2_CROP_ORIGIN", 0x00000000u, 0x1FFF1FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_541, 2, 1311},
   {0x8BC8u, "CH2_CROP_SIZE", 0x00000000u, 0x1FFF1FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_542, 2, 1314},
   {0x8BCCu, "CH2_GEOMETRY_STATUS", 0x00000000u, 0x00000000u, 0x000000F3u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_543, 3, 1317},
   {0x8BD0u, "CH2_ACTIVE_CROP_ORIGIN", 0x00000000u, 0x00000000u, 0x1FFF1FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_544, 2, 1321},
   {0x8BD4u, "CH2_ACTIVE_CROP_SIZE", 0x00000000u, 0x00000000u, 0x1FFF1FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_545, 2, 1324},
   {0x8BD8u, "CH2_OUTPUT_LIMIT", 0x02800280u, 0x00000000u, 0x1FFF1FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_546, 2, 1327},
   {0x8C00u, "COMMON_RECOVERY_STATUS", 0x00000000u, 0x00000000u, 0x00070331u, 0x00000002u, 0x00000000u, 0x00000000u, 0x00000000u, fields_547, 5, 1330},
   {0x8C04u, "COMMON_RECOVERY_SEQUENCE", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_548, 1, 1336},
   {0x8C10u, "COMMON_ABORT0_FRAME", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_549, 1, 1338},
   {0x8C14u, "COMMON_ABORT0_OUTCOME", 0x00000000u, 0x00000000u, 0x00000707u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_550, 2, 1340},
   {0x8C18u, "COMMON_ABORT1_FRAME", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_551, 1, 1343},
   {0x8C1Cu, "COMMON_ABORT1_OUTCOME", 0x00000000u, 0x00000000u, 0x00000707u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_552, 2, 1345},
   {0x8C20u, "COMMON_ABORT2_FRAME", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_553, 1, 1348},
   {0x8C24u, "COMMON_ABORT2_OUTCOME", 0x00000000u, 0x00000000u, 0x00000707u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_554, 2, 1350},
   {0x8C28u, "COMMON_ABORT3_FRAME", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_555, 1, 1353},
   {0x8C2Cu, "COMMON_ABORT3_OUTCOME", 0x00000000u, 0x00000000u, 0x00000707u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_556, 2, 1355},
   {0x8C40u, "TUNE_CH0_BUF0_FRAME", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_557, 1, 1358},
   {0x8C44u, "TUNE_CH0_BUF1_FRAME", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_558, 1, 1360},
   {0x8C48u, "TUNE_CH0_BUF2_FRAME", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_559, 1, 1362},
   {0x8C4Cu, "TUNE_CH0_BUF3_FRAME", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_560, 1, 1364},
   {0x8C50u, "TUNE_CH1_BUF0_FRAME", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_561, 1, 1366},
   {0x8C54u, "TUNE_CH1_BUF1_FRAME", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_562, 1, 1368},
   {0x8C58u, "TUNE_CH1_BUF2_FRAME", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_563, 1, 1370},
   {0x8C5Cu, "TUNE_CH1_BUF3_FRAME", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_564, 1, 1372},
   {0x8C60u, "TUNE_CH2_BUF0_FRAME", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_565, 1, 1374},
   {0x8C64u, "TUNE_CH2_BUF1_FRAME", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_566, 1, 1376},
   {0x8C68u, "TUNE_CH2_BUF2_FRAME", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_567, 1, 1378},
   {0x8C6Cu, "TUNE_CH2_BUF3_FRAME", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_568, 1, 1380},
   {0x8C80u, "TUNE_AE_FRAME_SIZE", 0x00000000u, 0x00000000u, 0x1FFF1FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_569, 2, 1382},
   {0x8C84u, "TUNE_AE_GRID", 0x00000000u, 0x00000000u, 0x001F003Fu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_570, 2, 1385},
   {0x8C88u, "TUNE_AE_ZONE_SIZE", 0x00000000u, 0x00000000u, 0xFFFFFFFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_571, 2, 1388},
   {0x8C90u, "TUNE_AWB_FRAME_SIZE", 0x00000000u, 0x00000000u, 0x1FFF1FFFu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, fields_572, 2, 1391},
};

const std::size_t num_registers = sizeof(registers) / sizeof(registers[0]);

}  // namespace cdc::components::fx1_isp::csr
