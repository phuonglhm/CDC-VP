/*
 * ISP Parameters - Matched with Infinite-ISP RTL
 */

#ifndef ISP_PARAMS_H
#define ISP_PARAMS_H

#include "isp_types.h"

//=============================================================================
// Default ISP Parameters (matching RTL defaults)
//=============================================================================
namespace isp_params {

// Image sensor dimensions
constexpr int DEFAULT_SNS_WIDTH = 2048;
constexpr int DEFAULT_SNS_HEIGHT = 1536;

// Crop dimensions
constexpr int DEFAULT_CROP_WIDTH = 1920;
constexpr int DEFAULT_CROP_HEIGHT = 1080;

// Default Bayer pattern
constexpr BayerPattern DEFAULT_BAYER = BayerPattern::RGGB;

// Bit width
constexpr int DEFAULT_BITS = 10;

// BNR weight bits
constexpr int BNR_WEIGHT_BITS = 8;

// Digital gain array
constexpr int DGAIN_ARRAY_SIZE = 100;
constexpr int DGAIN_ARRAY_BITS = 7;  // clog2(100) = 7

// Sharpening weight bits
constexpr int SHARP_WEIGHT_BITS = 20;

// 2DNR weight bits
constexpr int NR2D_WEIGHT_BITS = 5;

// Statistics output bits
constexpr int STAT_OUT_BITS = 32;
constexpr int STAT_HIST_BITS = DEFAULT_BITS;

// AWB crop margins
constexpr int AWB_CROP_LEFT = 8;
constexpr int AWB_CROP_RIGHT = 8;
constexpr int AWB_CROP_TOP = 16;
constexpr int AWB_CROP_BOTTOM = 0;

// Pipeline delay estimates (matching RTL)
constexpr int DPC_DELAY = 10;
constexpr int BNR_DELAY = 50;   // Complex filter
constexpr int DEMOSAIC_DELAY = 20;
constexpr int CCM_DELAY = 5;
constexpr int GAMMA_DELAY = 5;
constexpr int CSC_DELAY = 5;

// Memory initialization file paths (for LUTs)
constexpr const char* OECF_R_LUT_INIT = "OECF_R_LUT_INIT.mem";
constexpr const char* OECF_GR_LUT_INIT = "OECF_GR_LUT_INIT.mem";
constexpr const char* OECF_GB_LUT_INIT = "OECF_GB_LUT_INIT.mem";
constexpr const char* OECF_B_LUT_INIT = "OECF_B_LUT_INIT.mem";

constexpr const char* GAMMA_R_LUT_INIT = "GAMMA_R_LUT_INIT.mem";
constexpr const char* GAMMA_G_LUT_INIT = "GAMMA_G_LUT_INIT.mem";
constexpr const char* GAMMA_B_LUT_INIT = "GAMMA_B_LUT_INIT.mem";

} // namespace isp_params

//=============================================================================
// Module Enable Masks
//=============================================================================
struct IspEnables {
    bool crop_en;
    bool dpc_en;
    bool blc_en;
    bool oecf_en;
    bool dgain_en;
    bool lsc_en;
    bool bnr_en;
    bool wb_en;
    bool demosic_en;
    bool ccm_en;
    bool gamma_en;
    bool csc_en;
    bool sharpen_en;
    bool ldci_en;
    bool nr2d_en;
    bool stat_ae_en;
    bool awb_en;
    bool ae_en;

    IspEnables() {
        crop_en = true;
        dpc_en = true;
        blc_en = true;
        oecf_en = true;
        dgain_en = true;
        lsc_en = false;   // LSC not implemented in RTL yet
        bnr_en = true;
        wb_en = true;
        demosic_en = true;
        ccm_en = true;
        gamma_en = true;
        csc_en = true;
        sharpen_en = true;
        ldci_en = false;   // LDCI not implemented in RTL yet
        nr2d_en = true;
        stat_ae_en = false;
        awb_en = true;
        ae_en = true;
    }
};

#endif // ISP_PARAMS_H
