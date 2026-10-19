//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_dphyplldig
//--------------------------------------------------------------------------------------------------
#include "mipi_pll_dphy_dphyplldig.h"

static const char* const k_nets[1618] = {
    "n_0", "n_1", "n_2", "n_3", "m_0", "m_1", "m_2", "m_3",
    "m_4", "m_5", "m_6", "m_7", "m_8", "m_9", "th1_0", "th1_1",
    "th1_2", "th1_3", "th1_4", "th1_5", "th1_6", "th1_7", "th1_8", "th1_9",
    "th2_0", "th2_1", "th2_2", "th2_3", "th2_4", "th2_5", "th2_6", "th2_7",
    "th3_0", "th3_1", "th3_2", "th3_3", "th3_4", "th3_5", "th3_6", "th3_7",
    "tstplldig_0", "tstplldig_1", "tstplldig_2", "clksel_0", "clksel_1", "th_delay_0", "th_delay_1", "th_delay_2",
    "th_delay_3", "th_delay_4", "th_delay_5", "phy_prop_cntrl_0", "phy_prop_cntrl_1", "phy_prop_cntrl_2", "phy_prop_cntrl_3", "phy_prop_cntrl_4",
    "phy_prop_cntrl_5", "phy_int_cntrl_0", "phy_int_cntrl_1", "phy_int_cntrl_2", "phy_int_cntrl_3", "phy_int_cntrl_4", "phy_int_cntrl_5", "phy_vco_cntrl_0",
    "phy_vco_cntrl_1", "phy_vco_cntrl_2", "phy_vco_cntrl_3", "phy_vco_cntrl_4", "phy_vco_cntrl_5", "phy_cpbias_cntrl_0", "phy_cpbias_cntrl_1", "phy_cpbias_cntrl_2",
    "phy_cpbias_cntrl_3", "phy_cpbias_cntrl_4", "phy_cpbias_cntrl_5", "phy_cpbias_cntrl_6", "phy_gmp_cntrl_0", "phy_gmp_cntrl_1", "phy_n_0", "phy_n_1",
    "phy_n_2", "phy_n_3", "phy_m_0", "phy_m_1", "phy_m_2", "phy_m_3", "phy_m_4", "phy_m_5",
    "phy_m_6", "phy_m_7", "phy_m_8", "phy_m_9", "int_cntrl_int_0", "int_cntrl_int_1", "int_cntrl_int_2", "int_cntrl_int_3",
    "int_cntrl_int_4", "int_cntrl_int_5", "phy_in_clksel_0", "phy_in_clksel_1", "prop_cntrl_int_0", "prop_cntrl_int_1", "prop_cntrl_int_2", "prop_cntrl_int_3",
    "prop_cntrl_int_4", "prop_cntrl_int_5", "gmp_cntrl_int_0", "gmp_cntrl_int_1", "vco_cntrl_int_0", "vco_cntrl_int_1", "vco_cntrl_int_2", "vco_cntrl_int_3",
    "vco_cntrl_int_4", "vco_cntrl_int_5", "clksel_int_0", "clksel_int_1", "cpbias_cntrl_int_0", "cpbias_cntrl_int_1", "cpbias_cntrl_int_2", "cpbias_cntrl_int_3",
    "cpbias_cntrl_int_4", "cpbias_cntrl_int_5", "cpbias_cntrl_int_6", "m_obs_0", "m_obs_1", "m_obs_2", "m_obs_3", "m_obs_4",
    "m_obs_5", "m_obs_6", "m_obs_7", "m_obs_8", "m_obs_9", "n_obs_0", "n_obs_1", "n_obs_2",
    "n_obs_3", "int_cntrl_obs_0", "int_cntrl_obs_1", "int_cntrl_obs_2", "int_cntrl_obs_3", "int_cntrl_obs_4", "int_cntrl_obs_5", "prop_cntrl_obs_0",
    "prop_cntrl_obs_1", "prop_cntrl_obs_2", "prop_cntrl_obs_3", "prop_cntrl_obs_4", "prop_cntrl_obs_5", "vco_cntrl_obs_0", "vco_cntrl_obs_1", "vco_cntrl_obs_2",
    "vco_cntrl_obs_3", "vco_cntrl_obs_4", "vco_cntrl_obs_5", "cpbias_cntrl_obs_0", "cpbias_cntrl_obs_1", "cpbias_cntrl_obs_2", "cpbias_cntrl_obs_3", "cpbias_cntrl_obs_4",
    "cpbias_cntrl_obs_5", "cpbias_cntrl_obs_6", "gmp_cntrl_obs_0", "gmp_cntrl_obs_1", "int_cntrl_0", "int_cntrl_1", "int_cntrl_2", "int_cntrl_3",
    "int_cntrl_4", "int_cntrl_5", "vco_cntrl_0", "vco_cntrl_1", "vco_cntrl_2", "vco_cntrl_3", "vco_cntrl_4", "vco_cntrl_5",
    "prop_cntrl_0", "prop_cntrl_1", "prop_cntrl_2", "prop_cntrl_3", "prop_cntrl_4", "prop_cntrl_5", "cpbias_cntrl_0", "cpbias_cntrl_1",
    "cpbias_cntrl_2", "cpbias_cntrl_3", "cpbias_cntrl_4", "cpbias_cntrl_5", "cpbias_cntrl_6", "gmp_cntrl_0", "gmp_cntrl_1", "n320",
    "n1129", "n278", "n319", "n318", "n317", "n89", "n316", "shadow_clear_reset_s",
    "I01LOCKDETxN83", "n315", "n314", "I01PROGDIVIDERxN11", "I01LOCKDETxN82", "n313", "updatePLL_sync_clock_in_delay", "n312",
    "n311", "n310", "I01PROGDIVIDERxcounterx2x", "n309", "n158", "n308", "n156", "n307",
    "n_shadow_clock_inx2x", "n1269", "n306", "n302", "n1035", "n301", "n300", "n1215",
    "n299", "n1033", "n298", "n159", "n295", "n242", "n294", "n240",
    "n157", "n293", "n292", "I01PROGDIVIDERxcounterx0x", "n291", "isoNet_in_testlock", "n160", "n290",
    "n289", "I01DPHYPLLrelockxcounterx3x", "n288", "I01DPHYPLLrelockxcounterx2x", "n287", "I01DPHYPLLrelockxcounterx0x", "n286", "I01DPHYPLLrelockxcounterx1x",
    "n162", "n285", "isoNet_in_lock_phy", "n284", "n283", "n163", "n164", "n282",
    "n281", "n166", "n277", "n170", "n171", "n276", "n174", "n275",
    "n274", "n273", "n272", "n178", "n180", "n271", "n270", "n269",
    "n260", "n268", "n_shadow_clock_inx0x", "n267", "n182", "n266", "n93", "n1207",
    "n265", "n1374", "n264", "n1373", "n263", "n261", "n262", "n204",
    "n592", "n195", "isoNet_in_scanen", "n194", "n203", "shadow_clear_s", "isoNet_in_pll_shadow_control", "n957",
    "isoNet_in_phy_int_cntrlx4x", "int_cntrl_shadowx4x", "n179", "isoNet_in_phy_cpbias_cntrlx6x", "cpbias_cntrl_shadowx6x", "n177", "isoNet_in_phy_int_cntrlx3x", "int_cntrl_shadowx3x",
    "n176", "isoNet_in_phy_prop_cntrlx4x", "prop_cntrl_shadowx4x", "n175", "isoNet_in_phy_int_cntrlx1x", "int_cntrl_shadowx1x", "n173", "isoNet_in_phy_int_cntrlx5x",
    "int_cntrl_shadowx5x", "isoNet_in_phy_prop_cntrlx0x", "prop_cntrl_shadowx0x", "isoNet_in_pll_shadow_control_obs", "isoNet_in_phy_gmp_cntrlx0x", "gmp_cntrl_shadowx0x", "n168", "isoNet_in_phy_vco_cntrlx4x",
    "vco_cntrl_shadowx4x", "isoNet_in_phy_vco_cntrlx2x", "vco_cntrl_shadowx2x", "isoNet_in_phy_prop_cntrlx3x", "prop_cntrl_shadowx3x", "isoNet_in_phy_prop_cntrlx2x", "prop_cntrl_shadowx2x", "n197",
    "n56", "isoNet_in_th_delayx1x", "n972", "n1226", "n1227", "n1225", "n917", "I03PROGDIVIDER_RELOCKxcounterx4x",
    "I03PROGDIVIDER_RELOCKxN16", "n1213", "n866", "n305", "n296", "n297", "n161", "isoNet_in_phy_prop_cntrlx1x",
    "prop_cntrl_shadowx1x", "n196", "isoNet_in_phy_int_cntrlx2x", "int_cntrl_shadowx2x", "n1231", "n1230", "n1229", "n1232",
    "n1201", "updatepll_test", "n1039", "n188", "n973", "n974", "n975", "I01DPHYPLLrelockxwait_for_lock_detector",
    "n153", "n154", "syncrstzclkin", "isoNet_in_phy_nx0x", "n1041", "n1038", "n1037", "n1240",
    "isoNet_in_phy_vco_cntrlx0x", "vco_cntrl_shadowx0x", "n916", "isoNet_in_tstplldigx1x", "I01DPHYPLLrelockxlock_to_phy_s", "n243", "n152", "n74",
    "isoNet_in_phy_nx2x", "I01LOCKDETxchange_edge_fsmx0x", "I01LOCKDETxlockdet_mode_sync_with_vco", "I01LOCKDETxchange_edge_fsmx1x", "I01LOCKDETxN94", "isoNet_in_phy_vco_cntrlx3x", "vco_cntrl_shadowx3x", "n961",
    "n960", "n959", "n1195", "n1194", "n1216", "I01PROGDIVIDERxcounterx1x", "n1214", "isoNet_in_scanmode",
    "shadow_clear_vco", "n199", "n859", "n1372", "n247", "I03PROGDIVIDER_RELOCKxdivout_G3IP", "I03PROGDIVIDER_RELOCKxcounterx3x", "n1204",
    "I03PROGDIVIDER_RELOCKxdivout", "n1175", "n1176", "n138", "I01LOCKDETxnsclkcounterx2x", "n1174", "I01LOCKDETxnsclkcounterx3x", "isoNet_in_scanrstz",
    "n858", "N5", "isoNet_in_shadow_clear", "n1217", "n1040", "n_shadow_clock_inx3x", "isoNet_in_phy_nx3x", "n_shadow_clock_inx1x",
    "isoNet_in_phy_nx1x", "n1031", "n899", "hand_lock_det_on_vcoclk_syncxreset_async_ff", "hand_lock_det_on_vcoclk_syncxsyncrstx2x", "hand_lock_det_on_vcoclk_syncxsyncrstx0x", "hand_lock_det_on_vcoclk_syncxsyncrstx1x", "n1173",
    "n1189", "n1159", "I01LOCKDETxnsclkcounterx9x", "n1228", "n241", "n172", "n167", "n165",
    "n181", "n155", "isoNet_in_clkselx1x", "isoNet_in_clkselx0x", "isoNet_in_n_obsx3x", "isoNet_in_n_obsx2x", "isoNet_in_n_obsx1x", "isoNet_in_n_obsx0x",
    "n1138", "n1136", "n218", "isoNet_in_m_obsx6x", "n210", "isoNet_in_m_obsx4x", "n234", "n1144",
    "n144", "n169", "isoNet_in_scanout", "updatePLL_s", "scanluctrl_cell_out", "SEM_TIE1_1xU19_net", "SEM_TIE1_1xU18_net", "UNCONNECTED_0",
    "SEM_TIE0_1xU27_net", "SEM_TIE0_1xU26_net", "SEM_TIE0_1xU25_net", "SEM_TIE1_1xU27_net", "SEM_TIE1_1xU26_net", "UNCONNECTED_1", "SEM_TIE0_1xU41_net", "SEM_TIE0_1xU40_net",
    "SEM_TIE0_1xU39_net", "SEM_TIE1_1xU1_net", "SEM_TIE1_1xU0_net", "UNCONNECTED_2", "SEM_TIE0_1xU2_net", "SEM_TIE0_1xU1_net", "SEM_TIE0_1xU0_net", "SEM_TIE1_1xU7_net",
    "SEM_TIE1_1xU6_net", "UNCONNECTED_3", "SEM_TIE0_1xU12_net", "SEM_TIE0_1xU11_net", "SEM_TIE0_1xU10_net", "SEM_TIE1_1xU12_net", "SEM_TIE1_1xU11_net", "UNCONNECTED_4",
    "SEM_TIE0_1xU18_net", "SEM_TIE0_1xU17_net", "SEM_TIE0_1xU16_net", "SEM_TIE1_1xU28_net", "UNCONNECTED_5", "SEM_TIE1_1xU14_net", "UNCONNECTED_6", "SEM_TIE1_1xU9_net",
    "UNCONNECTED_7", "SEM_TIE1_1xU31_net", "UNCONNECTED_8", "SEM_TIE1_1xU22_net", "UNCONNECTED_9", "UNCONNECTED_10", "SEM_TIE0_1xU13_net", "UNCONNECTED_11",
    "SEM_TIE0_1xU38_net", "UNCONNECTED_12", "SEM_TIE0_1xU51_net", "UNCONNECTED_13", "SEM_TIE0_1xU14_net", "UNCONNECTED_14", "SEM_TIE0_1xU28_net", "SEM_TIE0_1xU31_net",
    "SEM_TIE0_1xU30_net", "UNCONNECTED_15", "SEM_TIE0_1xU43_net", "SEM_TIE0_1xU42_net", "UNCONNECTED_16", "SEM_TIE0_1xU4_net", "SEM_TIE0_1xU3_net", "UNCONNECTED_17",
    "SEM_TIE0_1xU33_net", "SEM_TIE0_1xU32_net", "UNCONNECTED_18", "SEM_TIE0_1xU45_net", "SEM_TIE0_1xU44_net", "UNCONNECTED_19", "SEM_TIE1_1xU17_net", "SEM_TIE0_1xU24_net",
    "UNCONNECTED_20", "SEM_TIE1_1xU10_net", "SEM_TIE0_1xU15_net", "UNCONNECTED_21", "SEM_TIE1_1xU5_net", "SEM_TIE0_1xU9_net", "UNCONNECTED_22", "SEM_TIE1_1xU20_net",
    "SEM_TIE0_1xU29_net", "UNCONNECTED_23", "SEM_TIE1_1xU13_net", "SEM_TIE0_1xU19_net", "UNCONNECTED_24", "SEM_TIE1_1xU3_net", "SEM_TIE0_1xU6_net", "UNCONNECTED_25",
    "SEM_TIE0_1xU5_net", "SEM_TIE1_1xU23_net", "SEM_TIE0_1xU35_net", "UNCONNECTED_26", "SEM_TIE0_1xU34_net", "SEM_TIE1_1xU15_net", "SEM_TIE0_1xU22_net", "UNCONNECTED_27",
    "SEM_TIE0_1xU21_net", "SEM_TIE1_1xU4_net", "SEM_TIE0_1xU8_net", "UNCONNECTED_28", "SEM_TIE0_1xU7_net", "SEM_TIE1_1xU32_net", "SEM_TIE0_1xU48_net", "UNCONNECTED_29",
    "SEM_TIE0_1xU47_net", "SEM_TIE0_1xU50_net", "UNCONNECTED_30", "SEM_TIE1_1xU34_net", "SEM_TIE0_1xU37_net", "UNCONNECTED_31", "SEM_TIE1_1xU25_net", "SEM_TIE0_1xU23_net",
    "UNCONNECTED_32", "SEM_TIE1_1xU16_net", "SEM_TIE0_1xU49_net", "UNCONNECTED_33", "SEM_TIE1_1xU33_net", "SEM_TIE0_1xU36_net", "UNCONNECTED_34", "SEM_TIE1_1xU24_net",
    "isoNet_in_phy_vco_cntrlx5x", "isoNet_in_phy_vco_cntrlx1x", "isoNet_in_phy_gmp_cntrlx1x", "isoNet_in_phy_cpbias_cntrlx5x", "isoNet_in_phy_cpbias_cntrlx4x", "isoNet_in_phy_cpbias_cntrlx3x", "isoNet_in_phy_cpbias_cntrlx2x", "isoNet_in_phy_cpbias_cntrlx1x",
    "isoNet_in_phy_cpbias_cntrlx0x", "isoNet_in_phy_mx9x", "isoNet_in_phy_mx8x", "isoNet_in_phy_mx7x", "isoNet_in_phy_mx6x", "isoNet_in_phy_mx5x", "isoNet_in_phy_mx4x", "isoNet_in_phy_mx3x",
    "isoNet_in_phy_mx2x", "isoNet_in_phy_mx1x", "isoNet_in_phy_mx0x", "isoNet_in_int_cntrlx5x", "isoNet_in_int_cntrlx4x", "isoNet_in_int_cntrlx3x", "isoNet_in_int_cntrlx2x", "isoNet_in_int_cntrlx1x",
    "isoNet_in_int_cntrlx0x", "isoNet_in_prop_cntrlx5x", "isoNet_in_prop_cntrlx4x", "isoNet_in_prop_cntrlx3x", "isoNet_in_prop_cntrlx2x", "isoNet_in_prop_cntrlx1x", "isoNet_in_prop_cntrlx0x", "isoNet_in_vco_cntrlx5x",
    "isoNet_in_vco_cntrlx4x", "isoNet_in_vco_cntrlx3x", "isoNet_in_vco_cntrlx2x", "isoNet_in_vco_cntrlx1x", "isoNet_in_vco_cntrlx0x", "isoNet_in_gmp_cntrlx1x", "isoNet_in_gmp_cntrlx0x", "isoNet_in_cpbias_cntrlx6x",
    "isoNet_in_cpbias_cntrlx5x", "isoNet_in_cpbias_cntrlx4x", "isoNet_in_cpbias_cntrlx3x", "isoNet_in_cpbias_cntrlx2x", "isoNet_in_cpbias_cntrlx1x", "isoNet_in_cpbias_cntrlx0x", "isoNet_in_lock_sel", "isoNet_in_force_lock_phy",
    "isoNet_in_lockdet_mode", "isoNet_in_lock_det_on", "isoNet_in_scanin", "isoNet_in_scanclk", "n206", "isoNet_in_clkfb", "isoNet_in_phy_in_clkselx0x", "n84",
    "n83", "n85", "n_shadowx1x", "I01LOCKDETxN80", "I01LOCKDETxnsclkcounterx0x", "I01LOCKDETxregfbclk", "n1262", "n1255",
    "n1256", "n1257", "I01LOCKDETxstatehfx1x", "I01LOCKDETxN44", "I01LOCKDETxregrefclk", "n1267", "n1258", "n1259",
    "n1260", "n1266", "n1261", "n1263", "n1265", "n1268", "I01LOCKDETxN43", "eco_net_124_0",
    "SEM_TIE1_1xU8_net", "SEM_TIE1_1xU21_net", "SEM_TIE1_1xU29_net", "n1375", "SEM_TIE1_1xU30_net", "n1376", "SEM_TIE1_1xU2_net", "n1377",
    "SEM_TIE0_1xU20_net", "isoNet_in_clkin", "isoNet_in_vcoclk", "isoNet_in_onpll", "isoNet_in_nx3x", "isoNet_in_nx2x", "isoNet_in_nx1x", "isoNet_in_nx0x",
    "isoNet_in_mx9x", "isoNet_in_mx8x", "isoNet_in_mx7x", "isoNet_in_mx6x", "isoNet_in_mx5x", "isoNet_in_mx4x", "isoNet_in_mx3x", "isoNet_in_mx2x",
    "isoNet_in_mx1x", "isoNet_in_mx0x", "isoNet_in_th1x9x", "isoNet_in_th1x8x", "isoNet_in_th1x7x", "isoNet_in_th1x6x", "isoNet_in_th1x5x", "isoNet_in_th1x4x",
    "isoNet_in_th1x3x", "isoNet_in_th1x2x", "isoNet_in_th1x1x", "isoNet_in_th1x0x", "isoNet_in_th2x7x", "isoNet_in_th2x6x", "isoNet_in_th2x5x", "isoNet_in_th2x4x",
    "isoNet_in_th2x3x", "isoNet_in_th2x2x", "isoNet_in_th2x1x", "isoNet_in_th2x0x", "isoNet_in_th3x7x", "isoNet_in_th3x6x", "isoNet_in_th3x5x", "isoNet_in_th3x4x",
    "isoNet_in_th3x3x", "isoNet_in_th3x2x", "isoNet_in_th3x1x", "isoNet_in_th3x0x", "isoNet_in_tstplldigx2x", "isoNet_in_tstplldigx0x", "isoNet_in_force_lock", "isoNet_in_th_delayx5x",
    "isoNet_in_th_delayx4x", "isoNet_in_th_delayx3x", "isoNet_in_th_delayx2x", "isoNet_in_th_delayx0x", "isoNet_in_updatepll", "isoNet_in_phy_int_cntrlx0x", "isoNet_in_phy_prop_cntrlx5x", "n86",
    "n87", "m_shadowx4x", "n88", "n1108", "n1111", "n1110", "n1120", "n1109",
    "n1113", "I01LOCKDETxnsclkcounterx10x", "n1119", "n1112", "n1116", "n1117", "n1118", "n1114",
    "n1115", "n1122", "n231", "n1121", "n1123", "n1153", "n1124", "I01LOCKDETxnpclkcounterx5x",
    "n1127", "n1246", "n1125", "n1126", "I01LOCKDETxnpclkcounterx4x", "n1151", "n873", "n1130",
    "n230", "n1131", "n1150", "n1132", "n1133", "n1140", "n1137", "I02PROGDIVIDERxcounterx5x",
    "n1139", "I02PROGDIVIDERxcounterx7x", "n1142", "n1141", "n1148", "n1146", "I02PROGDIVIDERxcounterx3x", "n1143",
    "n1145", "n1147", "I02PROGDIVIDERxcounterx8x", "I02PROGDIVIDERxcounterx9x", "n1149", "n397", "n1253", "n1152",
    "I01LOCKDETxnpclkcounterx7x", "I01LOCKDETxnpclkcounterx6x", "n1156", "n1252", "I01LOCKDETxreset_cond", "n1155", "n254", "n1248",
    "n229", "I02PROGDIVIDERxcounterx6x", "n1160", "n1168", "n1183", "n1161", "I02PROGDIVIDERxN25", "n1185",
    "I02PROGDIVIDERxcounterx0x", "I02PROGDIVIDERxN16", "I02PROGDIVIDERxN17", "n985", "n1162", "n1178", "n1164", "n1165",
    "n1163", "n1180", "n1167", "n1166", "I02PROGDIVIDERxN23", "I02PROGDIVIDERxN24", "n1170", "n1169",
    "I01LOCKDETxN90", "n1171", "n1172", "n1177", "I02PROGDIVIDERxcounterx2x", "n1179", "I02PROGDIVIDERxN18", "n1181",
    "n1182", "I02PROGDIVIDERxN22", "I02PROGDIVIDERxcounterx4x", "n1184", "n1186", "I02PROGDIVIDERxN20", "I01LOCKDETxN88", "n1187",
    "n223", "n1193", "n141", "I01LOCKDETxnpclkcounterx0x", "n1190", "n1192", "n259", "n1191",
    "n226", "n1197", "n1196", "I01LOCKDETxN86", "n1198", "n211", "n1199", "lockpll_detector_out",
    "n1202", "n303", "n304", "n1203", "updatePLL_pulse_catcher_d2x", "updatePLL_pulse_catcher", "updatePLL_pulse_catcher_d3x", "I03PROGDIVIDER_RELOCKxN14",
    "n1206", "n185", "I01PROGDIVIDERxN10", "n1222", "n1209", "n1221", "I01PROGDIVIDERxN12", "n1208",
    "n256", "n249", "n1212", "n1211", "n1219", "n1218", "n1220", "n1223",
    "I01PROGDIVIDERxdivout", "n246", "I01PROGDIVIDERxcounterx3x", "n245", "n1234", "n1237", "n1233", "n1235",
    "n1236", "n189", "n238", "n1242", "I01DPHYPLLrelockxcounterx4x", "n1238", "n1241", "I01DPHYPLLrelockxcounterx5x",
    "n237", "n228", "n1243", "n1245", "n1244", "I01LOCKDETxnpclkcounterx3x", "n1250", "n1247",
    "n1249", "n225", "n1251", "n222", "update_pll_sync_into_clkinxpulse_in_ff", "n90", "n91", "ack",
    "n92", "hand_syncrstzvcoclk_syncxsyncrstx1x", "hand_syncrstzvcoclk_syncxsyncrstx2x", "hand_syncrstzvcoclk_syncxsyncrstx0x", "n1032", "hand_syncrstzvcoclk_syncxreset_async_ff", "n861", "hand_syncrstzclkin_syncxsyncrstx1x",
    "hand_syncrstzclkin_syncxsyncrstx2x", "hand_syncrstzclkin_syncxsyncrstx0x", "hand_syncrstzclkin_syncxreset_async_ff", "n860", "hand_shadow_clear_reset_vco_syncxsyncrstx1x", "hand_shadow_clear_reset_vco_syncxsyncrstx2x", "hand_shadow_clear_reset_vco_syncxsyncrstx0x", "n1034",
    "hand_shadow_clear_reset_vco_syncxreset_async_ff", "hand_shadow_clear_reset_syncxsyncrstx1x", "hand_shadow_clear_reset_syncxsyncrstx2x", "hand_shadow_clear_reset_syncxsyncrstx0x", "hand_shadow_clear_reset_syncxreset_async_ff", "n872", "I03PROGDIVIDER_RELOCKxcounterx0x", "n244",
    "I01DPHYPLLrelockxfirst_lock_phy_detected", "I03PROGDIVIDER_RELOCKxcounterx1x", "I03PROGDIVIDER_RELOCKxN13", "I03PROGDIVIDER_RELOCKxcounterx2x", "I03PROGDIVIDER_RELOCKxN15", "n1036", "n239", "n1047",
    "n1045", "n1044", "n1043", "n1042", "n1046", "n1048", "n868", "n1052",
    "I01LOCKDETxnsclkcounterx1x", "n1051", "n1050", "n1060", "n1134", "n1053", "n1054", "n1056",
    "n1059", "n1057", "n1055", "n1058", "n1077", "n1061", "I01LOCKDETxnsclkcounterx4x", "n909",
    "n1065", "I01LOCKDETxnsclkcounterx5x", "n1063", "n1068", "n1071", "I01LOCKDETxnsclkcounterx7x", "n1067", "n1062",
    "I01LOCKDETxnsclkcounterx6x", "n1073", "n1076", "n1066", "n1064", "n1074", "n1070", "n1069",
    "n1072", "n1075", "n1128", "n1078", "n1080", "n1079", "n1081", "n1084",
    "n1082", "n1092", "n1090", "n1210", "I01LOCKDETxth1flag", "n1091", "n1094", "n1093",
    "n1099", "I01LOCKDETxN84", "n1095", "n1097", "n1096", "I01LOCKDETxN85", "n1098", "n1100",
    "n1101", "n1102", "n227", "I01LOCKDETxN87", "n1103", "n137", "n1104", "n1107",
    "n1106", "n224", "update_pll_sync_into_clock_in_domainxpulse_in_ff", "n94", "n95", "n1363", "n96", "n97",
    "n98", "n99", "hand_updatePLL_pulse_catcherxU1xU_SYNCxsample_metax0x", "n100", "n101", "update_pll_sync_into_clock_in_domainxU1xU_SYNCxsample_metax0x", "n102", "n103",
    "updatePLL_sync_clkin", "n104", "n105", "update_pll_sync_into_clkinxU1xU_SYNCxsample_metax0x", "n106", "n107", "n108", "n109",
    "n110", "n111", "n112", "n113", "n114", "n115", "gmp_cntrl_shadowx1x", "n116",
    "n117", "n120", "n118", "n119", "n121", "n124", "n122", "n123",
    "n125", "n1", "n126", "n127", "n130", "n958", "m_shadow_vcox0x", "lock_to_phy",
    "n976", "n979", "n969", "n967", "n968", "n977", "n978", "n990",
    "n135", "n1004", "I02PROGDIVIDERxcounterx1x", "n1003", "n1005", "n1328", "n1007", "I02PROGDIVIDERxN21",
    "n1009", "n1010", "I02PROGDIVIDERxN19", "n1012", "n1024", "n1014", "n1013", "n1023",
    "n1022", "n1016", "n1015", "n1020", "n1018", "n1017", "n1019", "n1021",
    "n1087", "n1088", "n1086", "I01LOCKDETxnpclkcounterx1x", "n1025", "n1028", "n1027", "n1026",
    "n1030", "n1085", "n1083", "n1029", "n128", "n129", "I01DPHYPLLrelockxhand_lock_to_phy_sxU1xU_SYNCxsample_metax0x", "n131",
    "n132", "n186", "ack_clkin", "n133", "n134", "m_shadow_vcox6x", "n136", "n142",
    "n143", "n925", "n986", "n145", "n146", "n147", "n221", "n255",
    "n258", "n220", "n215", "n205", "cpbias_cntrl_shadowx1x", "vco_cntrl_shadowx1x", "cpbias_cntrl_shadowx3x", "cpbias_cntrl_shadowx4x",
    "cpbias_cntrl_shadowx5x", "vco_cntrl_shadowx5x", "int_cntrl_shadowx0x", "prop_cntrl_shadowx5x", "cpbias_cntrl_shadowx2x", "cpbias_cntrl_shadowx0x", "n1158", "n1157",
    "lock_feedback_mux", "n910", "n912", "n913", "n991", "n914", "n918", "m_shadow_vcox1x",
    "n183", "n184", "n187", "updatePLL_sync_clock_in", "n1135", "n919", "n1188", "n911",
    "n984", "n983", "n992", "n1002", "I01LOCKDETxN89", "m_shadow_vcox7x", "m_shadow_vcox8x", "n921",
    "I01LOCKDETxclklf", "n988", "n929", "m_shadow_vcox4x", "n927", "n1000", "n999", "n1001",
    "I01LOCKDETxnsclkcounterx8x", "n1200", "n248", "n993", "n923", "m_shadow_vcox9x", "m_shadow_vcox2x", "n922",
    "n998", "n997", "n989", "n987", "I01LOCKDETxregfbclk_dly", "I01LOCKDETxregrefclk_dly", "n1049", "I01LOCKDETxN81",
    "n864", "n1006", "n1008", "n190", "n191", "n192", "n193", "n198",
    "n200", "n201", "n202", "clkindiv_32x_s", "eco_net_213_0", "clkindiv_32x", "int_clkin_clean_s", "isoNet_in_clkin_G1B2I1",
    "gated_clock_in_with_updatePLL_sync_at_clock_in_s", "gated_clock_in_with_updatePLL_sync_at_clock_in", "x5_netx", "N7", "syncrstzvcoclk", "x0_netx", "n235", "x3_netx",
    "x10_netx", "N8", "x14_net_x0x", "N16", "x18_netx", "N18", "I01LOCKDETxhand_regrefclk_syncxU1xU_SYNCxsample_metax0x", "isoNet_in_vcoclk_G1B2I6",
    "eco_net_288_0", "clk_scanable", "n280", "int_clkin_G2IP", "isoNet_in_refclk", "n279", "isoNet_in_vcoclk_G1B2I3", "isoNet_in_updatepll_G1IP",
    "n217", "x9_net_x0x", "n219", "x13_net_x0x", "n207", "n232", "n250", "n236",
    "eco_net_115_0", "isoNet_in_vcoclk_G1B2I9", "SEM_TIE0_1xU46_net", "gated_buf_vcoclk_with_updatePLL_sync_at_vco_G2B1I2", "m_shadowx6x", "eco_net_237_0", "isoNet_in_vcoclk_G1B2I4", "eco_net_238_0",
    "isoNet_in_vcoclk_G1B2I1", "eco_net_303_0", "eco_net_257_0", "eco_net_300_0", "isoNet_in_vcoclk_G1B2I10", "isoNet_in_vcoclk_G1B2I5", "eco_net_111_0", "eco_net_109_0",
    "eco_net_107_0", "gated_buf_vcoclk_with_updatePLL_sync_at_vco_G2B1I1", "m_shadowx8x", "eco_net_297_0", "m_shadowx0x", "eco_net_291_0", "eco_net_251_0", "isoNet_in_vcoclk_G1B2I11",
    "eco_net_13_0", "eco_net_294_0", "isoNet_in_vcoclk_G1B2I7", "I01LOCKDETxnpclkcounterx2x", "eco_net_11_0", "eco_net_302_0", "eco_net_106_0", "eco_net_301_0",
    "eco_net_112_0", "isoNet_in_vcoclk_G1B2I2", "eco_net_113_0", "n1089", "n995", "n996", "n994", "n924",
    "m_shadow_vcox5x", "m_shadow_vcox3x", "eco_net_327_0", "int_clkin_G2B2I2", "eco_net_79_0", "eco_net_290_0", "updatePLL_sync_vco_clock_delay", "eco_net_125_0",
    "eco_net_287_0", "eco_net_120_0", "eco_net_322_0", "eco_net_29_0", "I01LOCKDETxhand_lockdet_syncxU1xU_SYNCxsample_metax0x", "eco_net_283_0", "update_pll_sync_into_buf_vco_clock_domainxU1xU_SYNCxsample_metax0x", "eco_net_211_0",
    "update_pll_sync_into_buf_vco_clock_domainxpulse_in_ff", "updatePLL_sync_vco_clock", "eco_net_212_0", "eco_net_176_0", "I01LOCKDETxhand_regfbclk_syncxU1xU_SYNCxsample_metax0x", "eco_net_285_0", "clkindiv_32x_s_G2B1I1", "eco_net_234_0",
    "eco_net_166_0", "n_shadowx0x", "updatePLL_s_G2B1I5", "eco_net_167_0", "eco_net_168_0", "n_shadowx2x", "updatePLL_s_G2B1I3", "eco_net_70_0",
    "n_shadowx3x", "eco_net_58_0", "eco_net_59_0", "m_shadowx1x", "eco_net_158_0", "m_shadowx2x", "eco_net_159_0", "m_shadowx3x",
    "eco_net_160_0", "eco_net_161_0", "m_shadowx5x", "eco_net_162_0", "eco_net_163_0", "m_shadowx7x", "eco_net_164_0", "eco_net_165_0",
    "m_shadowx9x", "int_clkin_G2B2I1", "eco_net_330_0", "eco_net_28_0", "hand_ack_clkinxU1xU_SYNCxsample_metax0x", "eco_net_27_0", "eco_net_335_0", "eco_net_334_0",
    "eco_net_169_0", "isoNet_in_clkin_G1B3I1", "eco_net_210_0", "eco_net_223_0", "eco_net_117_0", "eco_net_236_0", "eco_net_320_0", "eco_net_233_0",
    "eco_net_317_0", "clkindiv_32x_s_G2B1I2", "eco_net_275_0", "eco_net_267_0", "eco_net_83_0", "eco_net_82_0", "eco_net_1_0", "eco_net_278_0",
    "eco_net_333_0", "isoNet_in_vcoclk_G1B2I8", "int_clkin", "eco_net_138_0", "eco_net_289_0", "eco_net_121_0", "eco_net_286_0", "eco_net_119_0",
    "eco_net_42_0", "int_clkin_G2B2I4", "eco_net_123_0", "eco_net_299_0", "eco_net_308_0", "eco_net_284_0", "eco_net_118_0", "eco_net_49_0",
    "eco_net_48_0", "eco_net_280_0", "eco_net_316_0", "eco_net_325_0", "eco_net_81_0", "eco_net_306_0", "eco_net_305_0", "eco_net_311_0",
    "eco_net_321_0", "eco_net_328_0", "eco_net_307_0", "eco_net_276_0", "eco_net_217_0", "eco_net_266_0", "eco_net_274_0", "eco_net_214_0",
    "eco_net_108_0", "eco_net_110_0", "n78", "eco_net_15_0", "eco_net_293_0", "eco_net_116_0", "eco_net_281_0", "eco_net_2_0",
    "eco_net_279_0", "eco_net_312_0", "eco_net_114_0", "eco_net_105_0", "int_clkin_G2B2I3", "eco_net_104_0", "eco_net_269_0", "eco_net_277_0",
    "eco_net_268_0", "eco_net_304_0", "eco_net_292_0", "eco_net_103_0", "eco_net_296_0", "eco_net_298_0", "eco_net_12_0", "eco_net_263_0",
    "eco_net_309_0", "eco_net_122_0", "eco_net_101_0", "eco_net_102_0", "eco_net_310_0", "eco_net_282_0", "eco_net_295_0", "relock_delay_clock",
    "gated_buf_vcoclk_with_updatePLL_sync_at_vco", "eco_net_140_0", "eco_net_139_0", "eco_net_142_0", "eco_net_141_0", "eco_net_143_0", "eco_net_144_0", "eco_net_145_0",
    "eco_net_146_0", "eco_net_147_0", "eco_net_323_0", "eco_net_148_0", "eco_net_149_0", "eco_net_150_0", "eco_net_151_0", "eco_net_135_0",
    "eco_net_136_0", "eco_net_137_0", "UNCONNECTED_35", "eco_net_153_0", "updatePLL_s_G2B1I7", "eco_net_154_0", "updatePLL_s_G2B1I2", "eco_net_155_0",
    "eco_net_156_0", "eco_net_157_0", "eco_net_57_0", "eco_net_170_0", "updatePLL_s_G2B1I1", "eco_net_171_0", "eco_net_172_0", "eco_net_173_0",
    "eco_net_174_0", "eco_net_175_0", "eco_net_177_0", "updatePLL_s_G2B1I4", "eco_net_85_0", "eco_net_178_0", "eco_net_179_0", "eco_net_180_0",
    "eco_net_181_0", "updatePLL_s_G2B1I6", "eco_net_133_0", "eco_net_134_0", "eco_net_126_0", "eco_net_127_0", "eco_net_128_0", "eco_net_129_0",
    "eco_net_130_0", "eco_net_131_0", "eco_net_132_0", "eco_net_51_0", "eco_net_152_0", "isoNet_in_vcoclk_G1B1I2", "isoNet_in_vcoclk_G1B1I1", "int_clkin_G2B1I1",
    "isoNet_in_clkin_G1B1I1", "n208", "n209", "n212", "n213", "n214", "n216", "n233",
    "n251", "n252", "n253", "n257", "n73", "n77", "n79", "n80",
    "n81", "n82", "eco_net_198_0", "eco_net_197_0", "eco_net_196_0", "eco_net_195_0", "eco_net_194_0", "eco_net_193_0",
    "eco_net_192_0", "eco_net_100_0", "eco_net_191_0", "eco_net_99_0", "eco_net_190_0", "eco_net_98_0", "eco_net_189_0", "eco_net_97_0",
    "eco_net_188_0", "eco_net_96_0", "eco_net_187_0", "eco_net_95_0", "eco_net_186_0", "eco_net_94_0", "eco_net_185_0", "eco_net_93_0",
    "eco_net_184_0", "eco_net_92_0", "eco_net_183_0", "eco_net_91_0", "eco_net_182_0", "eco_net_90_0", "eco_net_89_0", "eco_net_88_0",
    "eco_net_87_0", "eco_net_86_0", "eco_net_84_0", "eco_net_78_0", "eco_net_77_0", "eco_net_76_0", "eco_net_75_0", "eco_net_74_0",
    "eco_net_73_0", "eco_net_72_0", "eco_net_71_0", "eco_net_69_0", "eco_net_68_0", "eco_net_67_0", "eco_net_66_0", "eco_net_65_0",
    "eco_net_64_0", "eco_net_63_0", "eco_net_62_0", "eco_net_61_0", "eco_net_60_0", "eco_net_56_0", "eco_net_55_0", "eco_net_54_0",
    "eco_net_53_0", "eco_net_52_0", "eco_net_50_0", "eco_net_47_0", "eco_net_46_0", "eco_net_45_0", "eco_net_44_0", "eco_net_43_0",
    "eco_net_41_0", "eco_net_40_0", "eco_net_39_0", "eco_net_38_0", "eco_net_37_0", "eco_net_36_0", "eco_net_35_0", "eco_net_34_0",
    "eco_net_33_0", "eco_net_32_0", "eco_net_31_0", "eco_net_30_0", "eco_net_26_0", "eco_net_25_0", "eco_net_24_0", "eco_net_23_0",
    "eco_net_22_0", "eco_net_21_0", "eco_net_20_0", "eco_net_19_0", "eco_net_18_0", "eco_net_17_0", "eco_net_16_0", "eco_net_14_0",
    "eco_net_10_0", "eco_net_9_0", "eco_net_8_0", "eco_net_7_0", "eco_net_6_0", "eco_net", "eco_net_80_0", "eco_net_5_0",
    "eco_net_4_0", "eco_net_3_0", "eco_net_0", "eco_net_215_0", "eco_net_216_0", "eco_net_218_0", "eco_net_219_0", "eco_net_220_0",
    "eco_net_221_0", "eco_net_222_0", "eco_net_224_0", "eco_net_225_0", "eco_net_226_0", "eco_net_227_0", "eco_net_228_0", "eco_net_229_0",
    "eco_net_230_0", "eco_net_231_0", "eco_net_232_0", "eco_net_235_0", "eco_net_239_0", "eco_net_240_0", "eco_net_241_0", "eco_net_242_0",
    "eco_net_243_0", "eco_net_244_0", "eco_net_245_0", "eco_net_246_0", "eco_net_247_0", "eco_net_248_0", "eco_net_249_0", "eco_net_250_0",
    "eco_net_252_0", "eco_net_253_0", "eco_net_254_0", "eco_net_255_0", "eco_net_256_0", "eco_net_258_0", "eco_net_259_0", "eco_net_260_0",
    "eco_net_261_0", "eco_net_262_0", "eco_net_264_0", "eco_net_265_0", "eco_net_270_0", "eco_net_271_0", "eco_net_272_0", "eco_net_273_0",
    "eco_net_313_0", "eco_net_314_0", "eco_net_315_0", "eco_net_318_0", "eco_net_319_0", "eco_net_329_0", "eco_net_331_0", "eco_net_332_0",
    "eco_net_324_0", "eco_net_326_0",
};

mipi_pll_dphy_dphyplldig::mipi_pll_dphy_dphyplldig(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    n_.reserve(1618);
    for (int i = 0; i < 1618; i++)
        n_.emplace_back(new sc_core::sc_signal_resolved(k_nets[i], sc_dt::SC_LOGIC_Z));
    SC_METHOD(split_n);
    sensitive << n;
    SC_METHOD(split_m);
    sensitive << m;
    SC_METHOD(split_th1);
    sensitive << th1;
    SC_METHOD(split_th2);
    sensitive << th2;
    SC_METHOD(split_th3);
    sensitive << th3;
    SC_METHOD(split_tstplldig);
    sensitive << tstplldig;
    SC_METHOD(split_clksel);
    sensitive << clksel;
    SC_METHOD(split_th_delay);
    sensitive << th_delay;
    SC_METHOD(split_phy_prop_cntrl);
    sensitive << phy_prop_cntrl;
    SC_METHOD(split_phy_int_cntrl);
    sensitive << phy_int_cntrl;
    SC_METHOD(split_phy_vco_cntrl);
    sensitive << phy_vco_cntrl;
    SC_METHOD(split_phy_cpbias_cntrl);
    sensitive << phy_cpbias_cntrl;
    SC_METHOD(split_phy_gmp_cntrl);
    sensitive << phy_gmp_cntrl;
    SC_METHOD(split_phy_n);
    sensitive << phy_n;
    SC_METHOD(split_phy_m);
    sensitive << phy_m;
    SC_METHOD(merge_int_cntrl_int);
    sensitive << (*n_[92]) << (*n_[93]) << (*n_[94]) << (*n_[95]) << (*n_[96]) << (*n_[97]);
    SC_METHOD(merge_phy_in_clksel);
    sensitive << (*n_[98]) << (*n_[99]);
    SC_METHOD(merge_prop_cntrl_int);
    sensitive << (*n_[100]) << (*n_[101]) << (*n_[102]) << (*n_[103]) << (*n_[104]) << (*n_[105]);
    SC_METHOD(merge_gmp_cntrl_int);
    sensitive << (*n_[106]) << (*n_[107]);
    SC_METHOD(merge_vco_cntrl_int);
    sensitive << (*n_[108]) << (*n_[109]) << (*n_[110]) << (*n_[111]) << (*n_[112]) << (*n_[113]);
    SC_METHOD(merge_clksel_int);
    sensitive << (*n_[114]) << (*n_[115]);
    SC_METHOD(merge_cpbias_cntrl_int);
    sensitive << (*n_[116]) << (*n_[117]) << (*n_[118]) << (*n_[119]) << (*n_[120]) << (*n_[121]) << (*n_[122]);
    SC_METHOD(merge_m_obs);
    sensitive << (*n_[123]) << (*n_[124]) << (*n_[125]) << (*n_[126]) << (*n_[127]) << (*n_[128]) << (*n_[129]) << (*n_[130]) << (*n_[131]) << (*n_[132]);
    SC_METHOD(merge_n_obs);
    sensitive << (*n_[133]) << (*n_[134]) << (*n_[135]) << (*n_[136]);
    SC_METHOD(merge_int_cntrl_obs);
    sensitive << (*n_[137]) << (*n_[138]) << (*n_[139]) << (*n_[140]) << (*n_[141]) << (*n_[142]);
    SC_METHOD(merge_prop_cntrl_obs);
    sensitive << (*n_[143]) << (*n_[144]) << (*n_[145]) << (*n_[146]) << (*n_[147]) << (*n_[148]);
    SC_METHOD(merge_vco_cntrl_obs);
    sensitive << (*n_[149]) << (*n_[150]) << (*n_[151]) << (*n_[152]) << (*n_[153]) << (*n_[154]);
    SC_METHOD(merge_cpbias_cntrl_obs);
    sensitive << (*n_[155]) << (*n_[156]) << (*n_[157]) << (*n_[158]) << (*n_[159]) << (*n_[160]) << (*n_[161]);
    SC_METHOD(merge_gmp_cntrl_obs);
    sensitive << (*n_[162]) << (*n_[163]);
    SC_METHOD(split_int_cntrl);
    sensitive << int_cntrl;
    SC_METHOD(split_vco_cntrl);
    sensitive << vco_cntrl;
    SC_METHOD(split_prop_cntrl);
    sensitive << prop_cntrl;
    SC_METHOD(split_cpbias_cntrl);
    sensitive << cpbias_cntrl;
    SC_METHOD(split_gmp_cntrl);
    sensitive << gmp_cntrl;
    SC_METHOD(assign_VSS);
    SC_METHOD(assign_VDD);
    build_0();
    build_1();
    build_2();
}

void mipi_pll_dphy_dphyplldig::split_n() {
    const vl::logic<4> v = vl::rd(n);
    (*n_[0]).write(v.bit(0).sl());
    (*n_[1]).write(v.bit(1).sl());
    (*n_[2]).write(v.bit(2).sl());
    (*n_[3]).write(v.bit(3).sl());
}

void mipi_pll_dphy_dphyplldig::split_m() {
    const vl::logic<10> v = vl::rd(m);
    (*n_[4]).write(v.bit(0).sl());
    (*n_[5]).write(v.bit(1).sl());
    (*n_[6]).write(v.bit(2).sl());
    (*n_[7]).write(v.bit(3).sl());
    (*n_[8]).write(v.bit(4).sl());
    (*n_[9]).write(v.bit(5).sl());
    (*n_[10]).write(v.bit(6).sl());
    (*n_[11]).write(v.bit(7).sl());
    (*n_[12]).write(v.bit(8).sl());
    (*n_[13]).write(v.bit(9).sl());
}

void mipi_pll_dphy_dphyplldig::split_th1() {
    const vl::logic<10> v = vl::rd(th1);
    (*n_[14]).write(v.bit(0).sl());
    (*n_[15]).write(v.bit(1).sl());
    (*n_[16]).write(v.bit(2).sl());
    (*n_[17]).write(v.bit(3).sl());
    (*n_[18]).write(v.bit(4).sl());
    (*n_[19]).write(v.bit(5).sl());
    (*n_[20]).write(v.bit(6).sl());
    (*n_[21]).write(v.bit(7).sl());
    (*n_[22]).write(v.bit(8).sl());
    (*n_[23]).write(v.bit(9).sl());
}

void mipi_pll_dphy_dphyplldig::split_th2() {
    const vl::logic<8> v = vl::rd(th2);
    (*n_[24]).write(v.bit(0).sl());
    (*n_[25]).write(v.bit(1).sl());
    (*n_[26]).write(v.bit(2).sl());
    (*n_[27]).write(v.bit(3).sl());
    (*n_[28]).write(v.bit(4).sl());
    (*n_[29]).write(v.bit(5).sl());
    (*n_[30]).write(v.bit(6).sl());
    (*n_[31]).write(v.bit(7).sl());
}

void mipi_pll_dphy_dphyplldig::split_th3() {
    const vl::logic<8> v = vl::rd(th3);
    (*n_[32]).write(v.bit(0).sl());
    (*n_[33]).write(v.bit(1).sl());
    (*n_[34]).write(v.bit(2).sl());
    (*n_[35]).write(v.bit(3).sl());
    (*n_[36]).write(v.bit(4).sl());
    (*n_[37]).write(v.bit(5).sl());
    (*n_[38]).write(v.bit(6).sl());
    (*n_[39]).write(v.bit(7).sl());
}

void mipi_pll_dphy_dphyplldig::split_tstplldig() {
    const vl::logic<3> v = vl::rd(tstplldig);
    (*n_[40]).write(v.bit(0).sl());
    (*n_[41]).write(v.bit(1).sl());
    (*n_[42]).write(v.bit(2).sl());
}

void mipi_pll_dphy_dphyplldig::split_clksel() {
    const vl::logic<2> v = vl::rd(clksel);
    (*n_[43]).write(v.bit(0).sl());
    (*n_[44]).write(v.bit(1).sl());
}

void mipi_pll_dphy_dphyplldig::split_th_delay() {
    const vl::logic<6> v = vl::rd(th_delay);
    (*n_[45]).write(v.bit(0).sl());
    (*n_[46]).write(v.bit(1).sl());
    (*n_[47]).write(v.bit(2).sl());
    (*n_[48]).write(v.bit(3).sl());
    (*n_[49]).write(v.bit(4).sl());
    (*n_[50]).write(v.bit(5).sl());
}

void mipi_pll_dphy_dphyplldig::split_phy_prop_cntrl() {
    const vl::logic<6> v = vl::rd(phy_prop_cntrl);
    (*n_[51]).write(v.bit(0).sl());
    (*n_[52]).write(v.bit(1).sl());
    (*n_[53]).write(v.bit(2).sl());
    (*n_[54]).write(v.bit(3).sl());
    (*n_[55]).write(v.bit(4).sl());
    (*n_[56]).write(v.bit(5).sl());
}

void mipi_pll_dphy_dphyplldig::split_phy_int_cntrl() {
    const vl::logic<6> v = vl::rd(phy_int_cntrl);
    (*n_[57]).write(v.bit(0).sl());
    (*n_[58]).write(v.bit(1).sl());
    (*n_[59]).write(v.bit(2).sl());
    (*n_[60]).write(v.bit(3).sl());
    (*n_[61]).write(v.bit(4).sl());
    (*n_[62]).write(v.bit(5).sl());
}

void mipi_pll_dphy_dphyplldig::split_phy_vco_cntrl() {
    const vl::logic<6> v = vl::rd(phy_vco_cntrl);
    (*n_[63]).write(v.bit(0).sl());
    (*n_[64]).write(v.bit(1).sl());
    (*n_[65]).write(v.bit(2).sl());
    (*n_[66]).write(v.bit(3).sl());
    (*n_[67]).write(v.bit(4).sl());
    (*n_[68]).write(v.bit(5).sl());
}

void mipi_pll_dphy_dphyplldig::split_phy_cpbias_cntrl() {
    const vl::logic<7> v = vl::rd(phy_cpbias_cntrl);
    (*n_[69]).write(v.bit(0).sl());
    (*n_[70]).write(v.bit(1).sl());
    (*n_[71]).write(v.bit(2).sl());
    (*n_[72]).write(v.bit(3).sl());
    (*n_[73]).write(v.bit(4).sl());
    (*n_[74]).write(v.bit(5).sl());
    (*n_[75]).write(v.bit(6).sl());
}

void mipi_pll_dphy_dphyplldig::split_phy_gmp_cntrl() {
    const vl::logic<2> v = vl::rd(phy_gmp_cntrl);
    (*n_[76]).write(v.bit(0).sl());
    (*n_[77]).write(v.bit(1).sl());
}

void mipi_pll_dphy_dphyplldig::split_phy_n() {
    const vl::logic<4> v = vl::rd(phy_n);
    (*n_[78]).write(v.bit(0).sl());
    (*n_[79]).write(v.bit(1).sl());
    (*n_[80]).write(v.bit(2).sl());
    (*n_[81]).write(v.bit(3).sl());
}

void mipi_pll_dphy_dphyplldig::split_phy_m() {
    const vl::logic<10> v = vl::rd(phy_m);
    (*n_[82]).write(v.bit(0).sl());
    (*n_[83]).write(v.bit(1).sl());
    (*n_[84]).write(v.bit(2).sl());
    (*n_[85]).write(v.bit(3).sl());
    (*n_[86]).write(v.bit(4).sl());
    (*n_[87]).write(v.bit(5).sl());
    (*n_[88]).write(v.bit(6).sl());
    (*n_[89]).write(v.bit(7).sl());
    (*n_[90]).write(v.bit(8).sl());
    (*n_[91]).write(v.bit(9).sl());
}

void mipi_pll_dphy_dphyplldig::merge_int_cntrl_int() {
    vl::logic<6> v(0);
    v.set_bit(0, vl::rd((*n_[92])));
    v.set_bit(1, vl::rd((*n_[93])));
    v.set_bit(2, vl::rd((*n_[94])));
    v.set_bit(3, vl::rd((*n_[95])));
    v.set_bit(4, vl::rd((*n_[96])));
    v.set_bit(5, vl::rd((*n_[97])));
    vl::wr(int_cntrl_int, v);
}

void mipi_pll_dphy_dphyplldig::merge_phy_in_clksel() {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd((*n_[98])));
    v.set_bit(1, vl::rd((*n_[99])));
    vl::wr(phy_in_clksel, v);
}

void mipi_pll_dphy_dphyplldig::merge_prop_cntrl_int() {
    vl::logic<6> v(0);
    v.set_bit(0, vl::rd((*n_[100])));
    v.set_bit(1, vl::rd((*n_[101])));
    v.set_bit(2, vl::rd((*n_[102])));
    v.set_bit(3, vl::rd((*n_[103])));
    v.set_bit(4, vl::rd((*n_[104])));
    v.set_bit(5, vl::rd((*n_[105])));
    vl::wr(prop_cntrl_int, v);
}

void mipi_pll_dphy_dphyplldig::merge_gmp_cntrl_int() {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd((*n_[106])));
    v.set_bit(1, vl::rd((*n_[107])));
    vl::wr(gmp_cntrl_int, v);
}

void mipi_pll_dphy_dphyplldig::merge_vco_cntrl_int() {
    vl::logic<6> v(0);
    v.set_bit(0, vl::rd((*n_[108])));
    v.set_bit(1, vl::rd((*n_[109])));
    v.set_bit(2, vl::rd((*n_[110])));
    v.set_bit(3, vl::rd((*n_[111])));
    v.set_bit(4, vl::rd((*n_[112])));
    v.set_bit(5, vl::rd((*n_[113])));
    vl::wr(vco_cntrl_int, v);
}

void mipi_pll_dphy_dphyplldig::merge_clksel_int() {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd((*n_[114])));
    v.set_bit(1, vl::rd((*n_[115])));
    vl::wr(clksel_int, v);
}

void mipi_pll_dphy_dphyplldig::merge_cpbias_cntrl_int() {
    vl::logic<7> v(0);
    v.set_bit(0, vl::rd((*n_[116])));
    v.set_bit(1, vl::rd((*n_[117])));
    v.set_bit(2, vl::rd((*n_[118])));
    v.set_bit(3, vl::rd((*n_[119])));
    v.set_bit(4, vl::rd((*n_[120])));
    v.set_bit(5, vl::rd((*n_[121])));
    v.set_bit(6, vl::rd((*n_[122])));
    vl::wr(cpbias_cntrl_int, v);
}

void mipi_pll_dphy_dphyplldig::merge_m_obs() {
    vl::logic<10> v(0);
    v.set_bit(0, vl::rd((*n_[123])));
    v.set_bit(1, vl::rd((*n_[124])));
    v.set_bit(2, vl::rd((*n_[125])));
    v.set_bit(3, vl::rd((*n_[126])));
    v.set_bit(4, vl::rd((*n_[127])));
    v.set_bit(5, vl::rd((*n_[128])));
    v.set_bit(6, vl::rd((*n_[129])));
    v.set_bit(7, vl::rd((*n_[130])));
    v.set_bit(8, vl::rd((*n_[131])));
    v.set_bit(9, vl::rd((*n_[132])));
    vl::wr(m_obs, v);
}

void mipi_pll_dphy_dphyplldig::merge_n_obs() {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd((*n_[133])));
    v.set_bit(1, vl::rd((*n_[134])));
    v.set_bit(2, vl::rd((*n_[135])));
    v.set_bit(3, vl::rd((*n_[136])));
    vl::wr(n_obs, v);
}

void mipi_pll_dphy_dphyplldig::merge_int_cntrl_obs() {
    vl::logic<6> v(0);
    v.set_bit(0, vl::rd((*n_[137])));
    v.set_bit(1, vl::rd((*n_[138])));
    v.set_bit(2, vl::rd((*n_[139])));
    v.set_bit(3, vl::rd((*n_[140])));
    v.set_bit(4, vl::rd((*n_[141])));
    v.set_bit(5, vl::rd((*n_[142])));
    vl::wr(int_cntrl_obs, v);
}

void mipi_pll_dphy_dphyplldig::merge_prop_cntrl_obs() {
    vl::logic<6> v(0);
    v.set_bit(0, vl::rd((*n_[143])));
    v.set_bit(1, vl::rd((*n_[144])));
    v.set_bit(2, vl::rd((*n_[145])));
    v.set_bit(3, vl::rd((*n_[146])));
    v.set_bit(4, vl::rd((*n_[147])));
    v.set_bit(5, vl::rd((*n_[148])));
    vl::wr(prop_cntrl_obs, v);
}

void mipi_pll_dphy_dphyplldig::merge_vco_cntrl_obs() {
    vl::logic<6> v(0);
    v.set_bit(0, vl::rd((*n_[149])));
    v.set_bit(1, vl::rd((*n_[150])));
    v.set_bit(2, vl::rd((*n_[151])));
    v.set_bit(3, vl::rd((*n_[152])));
    v.set_bit(4, vl::rd((*n_[153])));
    v.set_bit(5, vl::rd((*n_[154])));
    vl::wr(vco_cntrl_obs, v);
}

void mipi_pll_dphy_dphyplldig::merge_cpbias_cntrl_obs() {
    vl::logic<7> v(0);
    v.set_bit(0, vl::rd((*n_[155])));
    v.set_bit(1, vl::rd((*n_[156])));
    v.set_bit(2, vl::rd((*n_[157])));
    v.set_bit(3, vl::rd((*n_[158])));
    v.set_bit(4, vl::rd((*n_[159])));
    v.set_bit(5, vl::rd((*n_[160])));
    v.set_bit(6, vl::rd((*n_[161])));
    vl::wr(cpbias_cntrl_obs, v);
}

void mipi_pll_dphy_dphyplldig::merge_gmp_cntrl_obs() {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd((*n_[162])));
    v.set_bit(1, vl::rd((*n_[163])));
    vl::wr(gmp_cntrl_obs, v);
}

void mipi_pll_dphy_dphyplldig::split_int_cntrl() {
    const vl::logic<6> v = vl::rd(int_cntrl);
    (*n_[164]).write(v.bit(0).sl());
    (*n_[165]).write(v.bit(1).sl());
    (*n_[166]).write(v.bit(2).sl());
    (*n_[167]).write(v.bit(3).sl());
    (*n_[168]).write(v.bit(4).sl());
    (*n_[169]).write(v.bit(5).sl());
}

void mipi_pll_dphy_dphyplldig::split_vco_cntrl() {
    const vl::logic<6> v = vl::rd(vco_cntrl);
    (*n_[170]).write(v.bit(0).sl());
    (*n_[171]).write(v.bit(1).sl());
    (*n_[172]).write(v.bit(2).sl());
    (*n_[173]).write(v.bit(3).sl());
    (*n_[174]).write(v.bit(4).sl());
    (*n_[175]).write(v.bit(5).sl());
}

void mipi_pll_dphy_dphyplldig::split_prop_cntrl() {
    const vl::logic<6> v = vl::rd(prop_cntrl);
    (*n_[176]).write(v.bit(0).sl());
    (*n_[177]).write(v.bit(1).sl());
    (*n_[178]).write(v.bit(2).sl());
    (*n_[179]).write(v.bit(3).sl());
    (*n_[180]).write(v.bit(4).sl());
    (*n_[181]).write(v.bit(5).sl());
}

void mipi_pll_dphy_dphyplldig::split_cpbias_cntrl() {
    const vl::logic<7> v = vl::rd(cpbias_cntrl);
    (*n_[182]).write(v.bit(0).sl());
    (*n_[183]).write(v.bit(1).sl());
    (*n_[184]).write(v.bit(2).sl());
    (*n_[185]).write(v.bit(3).sl());
    (*n_[186]).write(v.bit(4).sl());
    (*n_[187]).write(v.bit(5).sl());
    (*n_[188]).write(v.bit(6).sl());
}

void mipi_pll_dphy_dphyplldig::split_gmp_cntrl() {
    const vl::logic<2> v = vl::rd(gmp_cntrl);
    (*n_[189]).write(v.bit(0).sl());
    (*n_[190]).write(v.bit(1).sl());
}

vl::logic<2> mipi_pll_dphy_dphyplldig::rd_clksel() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd((*n_[43])));
    v.set_bit(1, vl::rd((*n_[44])));
    return v;
}

vl::logic<2> mipi_pll_dphy_dphyplldig::rd_clksel_int() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd((*n_[114])));
    v.set_bit(1, vl::rd((*n_[115])));
    return v;
}

vl::logic<7> mipi_pll_dphy_dphyplldig::rd_cpbias_cntrl() const {
    vl::logic<7> v(0);
    v.set_bit(0, vl::rd((*n_[182])));
    v.set_bit(1, vl::rd((*n_[183])));
    v.set_bit(2, vl::rd((*n_[184])));
    v.set_bit(3, vl::rd((*n_[185])));
    v.set_bit(4, vl::rd((*n_[186])));
    v.set_bit(5, vl::rd((*n_[187])));
    v.set_bit(6, vl::rd((*n_[188])));
    return v;
}

vl::logic<7> mipi_pll_dphy_dphyplldig::rd_cpbias_cntrl_int() const {
    vl::logic<7> v(0);
    v.set_bit(0, vl::rd((*n_[116])));
    v.set_bit(1, vl::rd((*n_[117])));
    v.set_bit(2, vl::rd((*n_[118])));
    v.set_bit(3, vl::rd((*n_[119])));
    v.set_bit(4, vl::rd((*n_[120])));
    v.set_bit(5, vl::rd((*n_[121])));
    v.set_bit(6, vl::rd((*n_[122])));
    return v;
}

vl::logic<7> mipi_pll_dphy_dphyplldig::rd_cpbias_cntrl_obs() const {
    vl::logic<7> v(0);
    v.set_bit(0, vl::rd((*n_[155])));
    v.set_bit(1, vl::rd((*n_[156])));
    v.set_bit(2, vl::rd((*n_[157])));
    v.set_bit(3, vl::rd((*n_[158])));
    v.set_bit(4, vl::rd((*n_[159])));
    v.set_bit(5, vl::rd((*n_[160])));
    v.set_bit(6, vl::rd((*n_[161])));
    return v;
}

vl::logic<2> mipi_pll_dphy_dphyplldig::rd_gmp_cntrl() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd((*n_[189])));
    v.set_bit(1, vl::rd((*n_[190])));
    return v;
}

vl::logic<2> mipi_pll_dphy_dphyplldig::rd_gmp_cntrl_int() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd((*n_[106])));
    v.set_bit(1, vl::rd((*n_[107])));
    return v;
}

vl::logic<2> mipi_pll_dphy_dphyplldig::rd_gmp_cntrl_obs() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd((*n_[162])));
    v.set_bit(1, vl::rd((*n_[163])));
    return v;
}

vl::logic<6> mipi_pll_dphy_dphyplldig::rd_int_cntrl() const {
    vl::logic<6> v(0);
    v.set_bit(0, vl::rd((*n_[164])));
    v.set_bit(1, vl::rd((*n_[165])));
    v.set_bit(2, vl::rd((*n_[166])));
    v.set_bit(3, vl::rd((*n_[167])));
    v.set_bit(4, vl::rd((*n_[168])));
    v.set_bit(5, vl::rd((*n_[169])));
    return v;
}

vl::logic<6> mipi_pll_dphy_dphyplldig::rd_int_cntrl_int() const {
    vl::logic<6> v(0);
    v.set_bit(0, vl::rd((*n_[92])));
    v.set_bit(1, vl::rd((*n_[93])));
    v.set_bit(2, vl::rd((*n_[94])));
    v.set_bit(3, vl::rd((*n_[95])));
    v.set_bit(4, vl::rd((*n_[96])));
    v.set_bit(5, vl::rd((*n_[97])));
    return v;
}

vl::logic<6> mipi_pll_dphy_dphyplldig::rd_int_cntrl_obs() const {
    vl::logic<6> v(0);
    v.set_bit(0, vl::rd((*n_[137])));
    v.set_bit(1, vl::rd((*n_[138])));
    v.set_bit(2, vl::rd((*n_[139])));
    v.set_bit(3, vl::rd((*n_[140])));
    v.set_bit(4, vl::rd((*n_[141])));
    v.set_bit(5, vl::rd((*n_[142])));
    return v;
}

vl::logic<10> mipi_pll_dphy_dphyplldig::rd_m() const {
    vl::logic<10> v(0);
    static const int idx[10] = {4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
    for (int i = 0; i < 10; i++) v.set_bit(i, vl::rd(*n_[idx[i]]));
    return v;
}

vl::logic<10> mipi_pll_dphy_dphyplldig::rd_m_obs() const {
    vl::logic<10> v(0);
    static const int idx[10] = {123, 124, 125, 126, 127, 128, 129, 130, 131, 132};
    for (int i = 0; i < 10; i++) v.set_bit(i, vl::rd(*n_[idx[i]]));
    return v;
}

vl::logic<4> mipi_pll_dphy_dphyplldig::rd_n() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd((*n_[0])));
    v.set_bit(1, vl::rd((*n_[1])));
    v.set_bit(2, vl::rd((*n_[2])));
    v.set_bit(3, vl::rd((*n_[3])));
    return v;
}

vl::logic<4> mipi_pll_dphy_dphyplldig::rd_n_obs() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd((*n_[133])));
    v.set_bit(1, vl::rd((*n_[134])));
    v.set_bit(2, vl::rd((*n_[135])));
    v.set_bit(3, vl::rd((*n_[136])));
    return v;
}

vl::logic<7> mipi_pll_dphy_dphyplldig::rd_phy_cpbias_cntrl() const {
    vl::logic<7> v(0);
    v.set_bit(0, vl::rd((*n_[69])));
    v.set_bit(1, vl::rd((*n_[70])));
    v.set_bit(2, vl::rd((*n_[71])));
    v.set_bit(3, vl::rd((*n_[72])));
    v.set_bit(4, vl::rd((*n_[73])));
    v.set_bit(5, vl::rd((*n_[74])));
    v.set_bit(6, vl::rd((*n_[75])));
    return v;
}

vl::logic<2> mipi_pll_dphy_dphyplldig::rd_phy_gmp_cntrl() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd((*n_[76])));
    v.set_bit(1, vl::rd((*n_[77])));
    return v;
}

vl::logic<2> mipi_pll_dphy_dphyplldig::rd_phy_in_clksel() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd((*n_[98])));
    v.set_bit(1, vl::rd((*n_[99])));
    return v;
}

vl::logic<6> mipi_pll_dphy_dphyplldig::rd_phy_int_cntrl() const {
    vl::logic<6> v(0);
    v.set_bit(0, vl::rd((*n_[57])));
    v.set_bit(1, vl::rd((*n_[58])));
    v.set_bit(2, vl::rd((*n_[59])));
    v.set_bit(3, vl::rd((*n_[60])));
    v.set_bit(4, vl::rd((*n_[61])));
    v.set_bit(5, vl::rd((*n_[62])));
    return v;
}

vl::logic<10> mipi_pll_dphy_dphyplldig::rd_phy_m() const {
    vl::logic<10> v(0);
    static const int idx[10] = {82, 83, 84, 85, 86, 87, 88, 89, 90, 91};
    for (int i = 0; i < 10; i++) v.set_bit(i, vl::rd(*n_[idx[i]]));
    return v;
}

vl::logic<4> mipi_pll_dphy_dphyplldig::rd_phy_n() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd((*n_[78])));
    v.set_bit(1, vl::rd((*n_[79])));
    v.set_bit(2, vl::rd((*n_[80])));
    v.set_bit(3, vl::rd((*n_[81])));
    return v;
}

vl::logic<6> mipi_pll_dphy_dphyplldig::rd_phy_prop_cntrl() const {
    vl::logic<6> v(0);
    v.set_bit(0, vl::rd((*n_[51])));
    v.set_bit(1, vl::rd((*n_[52])));
    v.set_bit(2, vl::rd((*n_[53])));
    v.set_bit(3, vl::rd((*n_[54])));
    v.set_bit(4, vl::rd((*n_[55])));
    v.set_bit(5, vl::rd((*n_[56])));
    return v;
}

vl::logic<6> mipi_pll_dphy_dphyplldig::rd_phy_vco_cntrl() const {
    vl::logic<6> v(0);
    v.set_bit(0, vl::rd((*n_[63])));
    v.set_bit(1, vl::rd((*n_[64])));
    v.set_bit(2, vl::rd((*n_[65])));
    v.set_bit(3, vl::rd((*n_[66])));
    v.set_bit(4, vl::rd((*n_[67])));
    v.set_bit(5, vl::rd((*n_[68])));
    return v;
}

vl::logic<6> mipi_pll_dphy_dphyplldig::rd_prop_cntrl() const {
    vl::logic<6> v(0);
    v.set_bit(0, vl::rd((*n_[176])));
    v.set_bit(1, vl::rd((*n_[177])));
    v.set_bit(2, vl::rd((*n_[178])));
    v.set_bit(3, vl::rd((*n_[179])));
    v.set_bit(4, vl::rd((*n_[180])));
    v.set_bit(5, vl::rd((*n_[181])));
    return v;
}

vl::logic<6> mipi_pll_dphy_dphyplldig::rd_prop_cntrl_int() const {
    vl::logic<6> v(0);
    v.set_bit(0, vl::rd((*n_[100])));
    v.set_bit(1, vl::rd((*n_[101])));
    v.set_bit(2, vl::rd((*n_[102])));
    v.set_bit(3, vl::rd((*n_[103])));
    v.set_bit(4, vl::rd((*n_[104])));
    v.set_bit(5, vl::rd((*n_[105])));
    return v;
}

vl::logic<6> mipi_pll_dphy_dphyplldig::rd_prop_cntrl_obs() const {
    vl::logic<6> v(0);
    v.set_bit(0, vl::rd((*n_[143])));
    v.set_bit(1, vl::rd((*n_[144])));
    v.set_bit(2, vl::rd((*n_[145])));
    v.set_bit(3, vl::rd((*n_[146])));
    v.set_bit(4, vl::rd((*n_[147])));
    v.set_bit(5, vl::rd((*n_[148])));
    return v;
}

vl::logic<10> mipi_pll_dphy_dphyplldig::rd_th1() const {
    vl::logic<10> v(0);
    static const int idx[10] = {14, 15, 16, 17, 18, 19, 20, 21, 22, 23};
    for (int i = 0; i < 10; i++) v.set_bit(i, vl::rd(*n_[idx[i]]));
    return v;
}

vl::logic<8> mipi_pll_dphy_dphyplldig::rd_th2() const {
    vl::logic<8> v(0);
    v.set_bit(0, vl::rd((*n_[24])));
    v.set_bit(1, vl::rd((*n_[25])));
    v.set_bit(2, vl::rd((*n_[26])));
    v.set_bit(3, vl::rd((*n_[27])));
    v.set_bit(4, vl::rd((*n_[28])));
    v.set_bit(5, vl::rd((*n_[29])));
    v.set_bit(6, vl::rd((*n_[30])));
    v.set_bit(7, vl::rd((*n_[31])));
    return v;
}

vl::logic<8> mipi_pll_dphy_dphyplldig::rd_th3() const {
    vl::logic<8> v(0);
    v.set_bit(0, vl::rd((*n_[32])));
    v.set_bit(1, vl::rd((*n_[33])));
    v.set_bit(2, vl::rd((*n_[34])));
    v.set_bit(3, vl::rd((*n_[35])));
    v.set_bit(4, vl::rd((*n_[36])));
    v.set_bit(5, vl::rd((*n_[37])));
    v.set_bit(6, vl::rd((*n_[38])));
    v.set_bit(7, vl::rd((*n_[39])));
    return v;
}

vl::logic<6> mipi_pll_dphy_dphyplldig::rd_th_delay() const {
    vl::logic<6> v(0);
    v.set_bit(0, vl::rd((*n_[45])));
    v.set_bit(1, vl::rd((*n_[46])));
    v.set_bit(2, vl::rd((*n_[47])));
    v.set_bit(3, vl::rd((*n_[48])));
    v.set_bit(4, vl::rd((*n_[49])));
    v.set_bit(5, vl::rd((*n_[50])));
    return v;
}

vl::logic<3> mipi_pll_dphy_dphyplldig::rd_tstplldig() const {
    vl::logic<3> v(0);
    v.set_bit(0, vl::rd((*n_[40])));
    v.set_bit(1, vl::rd((*n_[41])));
    v.set_bit(2, vl::rd((*n_[42])));
    return v;
}

vl::logic<6> mipi_pll_dphy_dphyplldig::rd_vco_cntrl() const {
    vl::logic<6> v(0);
    v.set_bit(0, vl::rd((*n_[170])));
    v.set_bit(1, vl::rd((*n_[171])));
    v.set_bit(2, vl::rd((*n_[172])));
    v.set_bit(3, vl::rd((*n_[173])));
    v.set_bit(4, vl::rd((*n_[174])));
    v.set_bit(5, vl::rd((*n_[175])));
    return v;
}

vl::logic<6> mipi_pll_dphy_dphyplldig::rd_vco_cntrl_int() const {
    vl::logic<6> v(0);
    v.set_bit(0, vl::rd((*n_[108])));
    v.set_bit(1, vl::rd((*n_[109])));
    v.set_bit(2, vl::rd((*n_[110])));
    v.set_bit(3, vl::rd((*n_[111])));
    v.set_bit(4, vl::rd((*n_[112])));
    v.set_bit(5, vl::rd((*n_[113])));
    return v;
}

vl::logic<6> mipi_pll_dphy_dphyplldig::rd_vco_cntrl_obs() const {
    vl::logic<6> v(0);
    v.set_bit(0, vl::rd((*n_[149])));
    v.set_bit(1, vl::rd((*n_[150])));
    v.set_bit(2, vl::rd((*n_[151])));
    v.set_bit(3, vl::rd((*n_[152])));
    v.set_bit(4, vl::rd((*n_[153])));
    v.set_bit(5, vl::rd((*n_[154])));
    return v;
}

void mipi_pll_dphy_dphyplldig::assign_VSS() {
    vl::wr(VSS, vl::logic<1>(0x0ULL));
}

void mipi_pll_dphy_dphyplldig::assign_VDD() {
    vl::wr(VDD, vl::logic<1>(0x1ULL));
}
