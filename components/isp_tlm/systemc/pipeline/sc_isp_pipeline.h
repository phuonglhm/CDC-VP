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
#include <string>

#include "../../pipeline/include/isp_pipeline.h"
#include "../tb_utils/hardware_params.h"
#include "../hw/metrics.h"

#include "sc_input_normalizer.h"
#include "../tb_utils/sc_metrics_wrapper.h"
#include "../tb_utils/sc_block_metrics.h"

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

    // ------------------------------------------------------------------------
    // Hardware parameters — shared across all blocks and wrappers for
    // throughput and real-time calculations.
    // ------------------------------------------------------------------------
    hw_params m_hw_params;

    // ------------------------------------------------------------------------
    // Phase 7 metrics pre-construction request
    // ------------------------------------------------------------------------
    // Testbenches typically construct sc_isp_pipeline after populating its
    // `enable_metrics` / `metrics_output_dir` fields. But the *first*
    // `bind_channels()` call happens inside the pipeline's constructor
    // body — too late to read post-construction field writes. To allow
    // a clean "set then construct" pattern, callers may push a one-shot
    // request via these statics; the constructor reads it via
    // `sc_isp_pipeline::consume_metrics_request()` and copies the values
    // onto the public fields before bind_channels runs.
    static void set_metrics_request(bool enable,
                                    const std::string& output_dir =
                                        "output/metrics",
                                    const std::vector<std::string>* skip =
                                        nullptr);
    static void clear_metrics_request();
    // Internal — used by the constructor only.
    struct MetricsRequest {
        bool enable = false;
        std::string output_dir = "output/metrics";
        const std::vector<std::string>* skip = nullptr;
    };
    static MetricsRequest consume_metrics_request();

    /**
     * @brief Constructor
     * @param name Module name
     * @param cfg Full ISP configuration
     * @param lsc_lut LSC lookup table data
     * @param raw_in_fifo Pointer to external raw input FIFO
     * @param yuv_out_fifo Pointer to external YUV420 output FIFO
     * @param input_bit_depth Sensor input bit depth (12, 14, 16)
     * @param bayer_pattern Bayer CFA pattern
     * @param hw Hardware parameters (passed by reference; defaults used if null)
     */
    sc_isp_pipeline(sc_core::sc_module_name name,
                 const isp_config& cfg,
                 const std::vector<float>& lsc_lut,
                 sc_core::sc_fifo<std::uint16_t>* raw_in_fifo,
                 sc_core::sc_fifo<std::uint8_t>* yuv_out_fifo,
                 std::uint8_t input_bit_depth = 12,
                 cfa_types bayer_pattern = cfa_types::RGGB,
                 const hw_params* hw = nullptr);

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

    // ------------------------------------------------------------------------
    // Hardware parameters API
    // ------------------------------------------------------------------------
    const hw_params& get_hw_params() const { return m_hw_params; }
    void set_hw_params(const hw_params& hw) { m_hw_params = hw; }

    // ------------------------------------------------------------------------
    // Frame timing API
    // ------------------------------------------------------------------------
    // Record timestamps of first pixel in and last pixel out.
    // Called by sc_input_normalizer and sc_yuv420 internally.
    void record_frame_start_time() {
        if (m_frame_start_time == SC_ZERO_TIME) {
            m_frame_start_time = sc_time_stamp();
        }
    }
    void record_frame_end_time() {
        m_frame_end_time = sc_time_stamp();
    }

    sc_time get_frame_start_time() const { return m_frame_start_time; }
    sc_time get_frame_end_time() const { return m_frame_end_time; }

    // Total frame processing time in simulation time units.
    sc_time get_frame_time() const {
        if (m_frame_end_time > m_frame_start_time) {
            return m_frame_end_time - m_frame_start_time;
        }
        return SC_ZERO_TIME;
    }

    double get_frame_time_ns() const {
        return get_frame_time().to_double() / 1e-9;
    }

    double get_frame_time_us() const {
        return get_frame_time().to_double() / 1e-3;
    }

    // Estimated frame time in microseconds based on hardware parameters
    // (only meaningful when timed_mode=true or using measured throughput).
    double estimate_frame_time_us() const;
    double estimate_fps() const;

    // ------------------------------------------------------------------------
    // Metrics collection
    // ------------------------------------------------------------------------
    // When `enable_metrics == true`, bind_channels() will interpose a
    // sc_metrics_wrapper<T> between every block's output and the next
    // block's input (single-FIFO spacing per wrapper).
    bool enable_metrics = false;
    // Directory (relative or absolute) where CSVs and summaries are written.
    std::string metrics_output_dir = "output/metrics";
    // Per-boundary toggle: names listed here are wrapped WITHOUT a
    // sc_metrics_wrapper (i.e. behavior identical to enable_metrics=false
    // for that one boundary). Empty = wrap everything.
    std::vector<std::string> metrics_skip_blocks;

    // Dump all wrapper CSVs and per-wrapper summary .txt files into
    // metrics_output_dir. Returns true if at least one wrapper was inserted
    // AND had collected samples, false otherwise.
    bool dump_pipeline_metrics() const;

    // Dump all block-level CSVs and per-block summary .txt files into
    // metrics_output_dir. Returns true if at least one block had samples.
    bool dump_all_block_metrics() const;

    // Total number of samples across every boundary wrapper.
    std::size_t metrics_sample_count() const;

    // Total number of samples across every block.
    std::size_t block_metrics_sample_count() const;

    // Print a combined frame timing + per-block latency + per-boundary
    // throughput summary to stdout.
    void print_metrics_summary() const;

    // ------------------------------------------------------------------------
    // Phase 5: Architecture metrics collector
    // ------------------------------------------------------------------------
    // Enable architecture-level metrics collection (frame, block, link metrics)
    void enable_arch_metrics(const std::string& output_dir = "output/arch_metrics");

    // Get the architecture metrics collector
    arch_metrics_collector* get_arch_metrics() { return &m_arch_metrics; }
    const arch_metrics_collector* get_arch_metrics() const { return &m_arch_metrics; }

    // Collect block cycle metrics from all blocks into arch_metrics
    void collect_block_metrics();

    // Update arch_metrics with a specific block's cycle counters
    void update_arch_block_metrics(const std::string& block_name,
                                   std::uint64_t active_cycles,
                                   std::uint64_t starved_cycles,
                                   std::uint64_t blocked_cycles);

    // Update arch_metrics with frame timing
    void update_arch_frame_timing(std::uint64_t frame_id);

    // Dump architecture metrics to files
    void dump_arch_metrics() const;
    void dump_arch_summary() const;

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

    // ------------------------------------------------------------------------
    // Frame timing
    // ------------------------------------------------------------------------
    sc_time m_frame_start_time;
    sc_time m_frame_end_time;

    // ------------------------------------------------------------------------
    // Metrics: type-erased wrapper registry (boundary throughput)
    // ------------------------------------------------------------------------
    struct WrapperNode {
        std::string label;
        void* wrapper_ptr;
        void (*dump_fn)(void*, const std::string&);
        std::size_t (*count_fn)(void*);
    };
    std::vector<WrapperNode> m_wrapper_nodes;

    // ------------------------------------------------------------------------
    // Metrics: type-erased block metrics registry (block latency)
    // ------------------------------------------------------------------------
    struct BlockNode {
        std::string label;
        void* block_metrics_ptr;
        void (*dump_fn)(void*, const std::string&);
        std::size_t (*count_fn)(void*);
        sc_core::sc_time (*mean_latency_fn)(void*);
        sc_core::sc_time (*min_latency_fn)(void*);
        sc_core::sc_time (*max_latency_fn)(void*);
        double (*stddev_fn)(void*);
        float (*throughput_fn)(void*);

        BlockNode() = default;
        BlockNode(const std::string& l, void* p,
                  void (*df)(void*, const std::string&),
                  std::size_t (*cf)(void*),
                  sc_core::sc_time (*mlf)(void*),
                  sc_core::sc_time (*mnf)(void*),
                  sc_core::sc_time (*mxf)(void*),
                  double (*sf)(void*),
                  float (*tf)(void*))
            : label(l), block_metrics_ptr(p), dump_fn(df), count_fn(cf),
              mean_latency_fn(mlf), min_latency_fn(mnf), max_latency_fn(mxf),
              stddev_fn(sf), throughput_fn(tf) {}
    };
    std::vector<BlockNode> m_block_nodes;

    void setup_metrics();
    void setup_block_metrics();

    // ------------------------------------------------------------------------
    // Phase 5: Architecture metrics collector
    // ------------------------------------------------------------------------
    arch_metrics_collector m_arch_metrics;
    bool m_arch_metrics_enabled = false;
};

#endif  // SC_ISP_PIPELINE_H
