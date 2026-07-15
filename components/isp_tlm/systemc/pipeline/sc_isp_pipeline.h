/**

 * @file sc_isp_pipeline.h

 * @brief Top-Level SystemC ISP Pipeline Integration

 *

 * Instantiates all 17 ISP processing blocks and connects them with sc_fifo channels.

 * Provides a complete streaming ISP pipeline from RAW input to YUV420 output.

 */

#ifndef SC_ISP_PIPELINE_H

#define SC_ISP_PIPELINE_H



#include <systemc>
using namespace sc_core;


#include <systemc>
#include <cstdint>
#include <vector>

#include "../../pipeline/include/isp_pipeline.h"

#include "sc_input_normalizer.h"

// Point operations
#include "../blocks/blc/sc_blc.h"
#include "../blocks/dpc/sc_dpc.h"
#include "../blocks/dg/sc_dg.h"
#include "../blocks/wb/sc_wb.h"
#include "../blocks/ccm/sc_ccm.h"
#include "../blocks/gc/sc_gc.h"
#include "../blocks/csc/sc_csc.h"
#include "../blocks/cse/sc_cse.h"

// Spatial operations
#include "../blocks/lsc/sc_lsc.h"
#include "../blocks/bnr/sc_bnr.h"
#include "../blocks/demosaic/sc_demosaic.h"
#include "../blocks/sharpen/sc_sharpen.h"
#include "../blocks/2dnr/sc_2dnr.h"

// Statistical and resizing
#include "../blocks/awb/sc_awb.h"
#include "../blocks/aec/sc_aec.h"
#include "../blocks/scale/sc_scale.h"
#include "../blocks/yuv420/sc_yuv420.h"

SC_MODULE(sc_isp_pipeline) {
public:
    // Input/Output ports (external FIFOs are wired in by the testbench)
    sc_core::sc_fifo<std::uint16_t>* raw_in;
    sc_core::sc_fifo<std::uint8_t>* yuv_out;

    SC_HAS_PROCESS(sc_isp_pipeline);

    /**
     * @brief Constructor
     * @param name Module name
     * @param cfg Full ISP configuration
     * @param lsc_lut LSC lookup table data
     */
    sc_isp_pipeline(sc_core::sc_module_name name,
                 const isp_config& cfg,
                 const std::vector<float>& lsc_lut,
                 sc_core::sc_fifo<std::uint16_t>* raw_in_fifo,
                 sc_core::sc_fifo<std::uint8_t>* yuv_out_fifo,
                 std::uint8_t input_bit_depth = 12,
                 cfa_types bayer_pattern = cfa_types::RGGB);

    ~sc_isp_pipeline();

    float get_awb_r_gain() const;
    float get_awb_b_gain() const;
    std::int32_t get_aec_feedback() const;

    // Prime the AWB statistics by running the AWB algorithm over a raw
    // 12-bit RGB buffer (RGB interleaved, width*height pixels). Useful
    // before `sc_start()` so the first frame already uses the computed
    // R/B gains (otherwise streaming would need at least two frames to
    // converge). The buffer must already be in the pipeline's working
    // (12-bit) bit-depth.
    void precompute_awb_gains(const std::uint16_t* rgb12);

    // Prime the AWB from a raw 16-bit Bayer buffer (single channel,
    // width*height values). The pipeline's configured Bayer pattern is
    // used. Faster than the RGB variant because it avoids running the
    // full demosaic.
    void precompute_awb_gains_from_bayer(const std::uint16_t* bayer16);

    // Directly prime the AWB with already-computed R/B gains. This is
    // the most accurate option when the reference pipeline has already
    // computed the gains (e.g. when both pipelines share the same
    // tuning and we want bit-exact parity on the first frame).
    void prime_awb_gains(float r_gain, float b_gain);

    std::size_t awb_input_pixels() const {
        return static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height);
    }

private:
    void init_modules();
    void bind_channels();

    // Configuration
    const isp_config& m_cfg;
    const std::vector<float>& m_lsc_lut;

    // FIFO depths
    static constexpr std::size_t FIFO_DEPTH = 1024;

    // RAW domain FIFOs
    sc_core::sc_fifo<std::uint16_t>* fifo_in_norm;
    sc_core::sc_fifo<std::uint16_t>* fifo_blc_dpc;
    sc_core::sc_fifo<std::uint16_t>* fifo_dpc_lsc;
    sc_core::sc_fifo<std::uint16_t>* fifo_lsc_dg;
    sc_core::sc_fifo<std::uint16_t>* fifo_dg_bnr;
    sc_core::sc_fifo<std::uint16_t>* fifo_bnr_demosaic;

    // RGB domain FIFOs
    sc_core::sc_fifo<std::uint16_t>* fifo_demosaic_awb;
    sc_core::sc_fifo<std::uint16_t>* fifo_demosaic_wb;
    sc_core::sc_fifo<std::uint16_t>* fifo_wb_ccm;
    sc_core::sc_fifo<std::uint16_t>* fifo_ccm_gc;
    sc_core::sc_fifo<std::uint16_t>* fifo_gc_aec;
    sc_core::sc_fifo<std::uint16_t>* fifo_aec_csc;

    // YUV444 domain FIFOs
    sc_core::sc_fifo<std::uint8_t>* fifo_csc_cse;
    sc_core::sc_fifo<std::uint8_t>* fifo_cse_sharpen;
    sc_core::sc_fifo<std::uint8_t>* fifo_sharpen_2dnr;
    sc_core::sc_fifo<std::uint8_t>* fifo_2dnr_scale;
    sc_core::sc_fifo<std::uint8_t>* fifo_scale_yuv420;
    sc_core::sc_fifo<std::uint8_t>* fifo_2dnr_yuv420;  // Direct path when scale disabled

    // Module instances
    sc_input_normalizer* m_input_norm;
    sc_blc* m_blc;
    sc_dpc* m_dpc;
    sc_dg* m_dg;
    sc_lsc* m_lsc;
    sc_bnr* m_bnr;
    sc_demosaic* m_demosaic;
    sc_awb* m_awb;
    sc_wb* m_wb;
    sc_ccm* m_ccm;
    sc_gc* m_gc;
    sc_aec* m_aec;
    sc_csc* m_csc;
    sc_cse* m_cse;
    sc_sharpen* m_sharpen;
    sc_2dnr* m_2dnr;
    sc_scale* m_scale;
    sc_yuv420* m_yuv420;

    // Image dimensions
    std::uint32_t m_width;
    std::uint32_t m_height;
    std::uint8_t m_bit_depth;          // pipeline working bit depth (12)
    std::uint8_t m_input_bit_depth;    // sensor input bit depth (12, 14, 16)
    cfa_types m_bayer_pattern;
};

#endif  // SC_ISP_PIPELINE_H
