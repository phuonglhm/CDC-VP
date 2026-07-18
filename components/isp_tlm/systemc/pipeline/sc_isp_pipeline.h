#ifndef SC_ISP_PIPELINE_H
#define SC_ISP_PIPELINE_H

#include <systemc>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <map>
#include <vector>

#include "../../pipeline/include/isp_pipeline.h"
#include "../hw/frame_feedback.h"
#include "../hw/isp_arch_config.h"
#include "../hw/line_channel.h"
#include "../hw/line_stage.h"
#include "../hw/metrics.h"
#include "../tb_utils/hardware_params.h"

/**
 * Line-granular SystemC architecture model.
 *
 * The public FIFO pointers are retained as an ideal testbench boundary for
 * existing callers.  The pipeline immediately converts those pixel streams to
 * bounded line channels, and every internal stage exchanges line tokens.
 */
SC_MODULE(sc_isp_pipeline) {
public:
    using u16_stage = isp_tlm::sc_line_frame_stage<std::uint16_t, std::uint16_t>;
    using csc_stage = isp_tlm::sc_line_frame_stage<std::uint16_t, std::uint8_t>;
    using u8_stage = isp_tlm::sc_line_frame_stage<std::uint8_t, std::uint8_t>;

    sc_core::sc_fifo<std::uint16_t>* raw_in = nullptr;
    sc_core::sc_fifo<std::uint8_t>* yuv_out = nullptr;

    SC_HAS_PROCESS(sc_isp_pipeline);

    struct MetricsRequest {
        bool enable = false;
        std::string output_dir = "output/metrics";
        const std::vector<std::string>* skip = nullptr;
    };

    static void set_metrics_request(bool enable,
                                    const std::string& output_dir = "output/metrics",
                                    const std::vector<std::string>* skip = nullptr);
    static void clear_metrics_request();
    static MetricsRequest consume_metrics_request();

    sc_isp_pipeline(sc_core::sc_module_name name,
                    const isp_config& cfg,
                    const std::vector<float>& lsc_lut,
                    sc_core::sc_fifo<std::uint16_t>* raw_in_fifo,
                    sc_core::sc_fifo<std::uint8_t>* yuv_out_fifo,
                    std::uint8_t input_bit_depth = 12,
                    cfa_types bayer_pattern = cfa_types::RGGB,
                    const hw_params* hw = nullptr,
                    const isp_arch_config* arch = nullptr);

    ~sc_isp_pipeline();

    void bind_clock(sc_core::sc_clock* clk);

    float get_awb_r_gain() const noexcept { return m_last_awb_r_gain; }
    float get_awb_b_gain() const noexcept { return m_last_awb_b_gain; }
    std::int32_t get_aec_feedback() const noexcept { return m_last_aec_feedback; }

    void precompute_awb_gains(const std::uint16_t* rgb12);
    void precompute_awb_gains_from_bayer(const std::uint16_t* bayer16);
    void prime_awb_gains(float r_gain, float b_gain);

    std::size_t awb_input_pixels() const noexcept {
        return static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height);
    }

    const hw_params& get_hw_params() const noexcept { return m_hw_params; }
    void set_hw_params(const hw_params& hw) noexcept { m_hw_params = hw; }

    void record_frame_start_time() noexcept {
        if (m_frame_start_time == sc_core::SC_ZERO_TIME) {
            m_frame_start_time = sc_core::sc_time_stamp();
        }
    }
    void record_frame_end_time() noexcept { m_frame_end_time = sc_core::sc_time_stamp(); }

    sc_core::sc_time get_frame_start_time() const noexcept { return m_frame_start_time; }
    sc_core::sc_time get_frame_end_time() const noexcept { return m_frame_end_time; }
    sc_core::sc_time get_frame_time() const noexcept {
        return m_frame_end_time > m_frame_start_time
                   ? m_frame_end_time - m_frame_start_time
                   : sc_core::SC_ZERO_TIME;
    }
    double get_frame_time_ns() const noexcept {
        return get_frame_time().to_seconds() / 1e-9;
    }
    double get_frame_time_us() const noexcept {
        return get_frame_time().to_seconds() / 1e-6;
    }
    double estimate_frame_time_us() const;
    double estimate_fps() const;

    bool enable_metrics = false;
    std::string metrics_output_dir = "output/metrics";
    std::vector<std::string> metrics_skip_blocks;

    bool dump_pipeline_metrics() const;
    bool dump_all_block_metrics() const;
    std::size_t metrics_sample_count() const noexcept;
    std::size_t block_metrics_sample_count() const noexcept;
    void print_metrics_summary() const;

    void enable_arch_metrics(const std::string& output_dir = "output/arch_metrics");
    arch_metrics_collector* get_arch_metrics() noexcept { return &m_arch_metrics; }
    const arch_metrics_collector* get_arch_metrics() const noexcept { return &m_arch_metrics; }
    void collect_block_metrics();
    void update_arch_block_metrics(const std::string& block_name,
                                   std::uint64_t active_cycles,
                                   std::uint64_t starved_cycles,
                                   std::uint64_t blocked_cycles);
    void update_arch_frame_timing(std::uint64_t frame_id);
    void dump_arch_metrics() const;
    void dump_arch_summary() const;

private:
    void init_channels();
    void init_stages();
    void ingress_loop();
    void egress_loop();

    std::size_t link_depth(std::size_t link_id) const noexcept;
    isp_tlm::stage_timing stage_timing(std::size_t block_id) const;
    static std::uint32_t ceil_half(std::uint32_t value) noexcept {
        return (value + 1u) / 2u;
    }

    isp_config m_cfg;
    std::vector<float> m_lsc_lut;
    isp_arch_config m_arch_config;
    hw_params m_hw_params;
    std::uint8_t m_bit_depth = 12;
    std::uint8_t m_input_bit_depth = 12;
    cfa_types m_bayer_pattern = cfa_types::RGGB;
    std::uint32_t m_width = 0;
    std::uint32_t m_height = 0;
    std::uint32_t m_scaled_width = 0;
    std::uint32_t m_scaled_height = 0;
    std::uint32_t m_final_rows = 0;
    std::uint32_t m_final_line_samples = 0;
    std::size_t m_final_frame_elements = 0;

    // The ideal sink places rate-changing output by byte offset before
    // exposing the canonical frame order at the public FIFO boundary.
    std::vector<std::uint8_t> m_sink_frame;

    std::unique_ptr<isp_tlm::line_channel<std::uint16_t>> m_line_ingress;
    std::unique_ptr<isp_tlm::line_channel<std::uint16_t>> m_norm_blc;
    std::unique_ptr<isp_tlm::line_channel<std::uint16_t>> m_blc_dpc;
    std::unique_ptr<isp_tlm::line_channel<std::uint16_t>> m_dpc_lsc;
    std::unique_ptr<isp_tlm::line_channel<std::uint16_t>> m_lsc_dg;
    std::unique_ptr<isp_tlm::line_channel<std::uint16_t>> m_dg_bnr;
    std::unique_ptr<isp_tlm::line_channel<std::uint16_t>> m_bnr_demosaic;
    std::unique_ptr<isp_tlm::line_channel<std::uint16_t>> m_demosaic_awb;
    std::unique_ptr<isp_tlm::line_channel<std::uint16_t>> m_awb_wb;
    std::unique_ptr<isp_tlm::line_channel<std::uint16_t>> m_wb_ccm;
    std::unique_ptr<isp_tlm::line_channel<std::uint16_t>> m_ccm_gc;
    std::unique_ptr<isp_tlm::line_channel<std::uint16_t>> m_gc_aec;
    std::unique_ptr<isp_tlm::line_channel<std::uint16_t>> m_aec_csc;
    std::unique_ptr<isp_tlm::line_channel<std::uint8_t>> m_csc_cse;
    std::unique_ptr<isp_tlm::line_channel<std::uint8_t>> m_cse_sharpen;
    std::unique_ptr<isp_tlm::line_channel<std::uint8_t>> m_sharpen_2dnr;
    std::unique_ptr<isp_tlm::line_channel<std::uint8_t>> m_2dnr_scale;
    std::unique_ptr<isp_tlm::line_channel<std::uint8_t>> m_scale_yuv420;
    std::unique_ptr<isp_tlm::line_channel<std::uint8_t>> m_line_egress;

    std::unique_ptr<u16_stage> m_input_norm;
    std::unique_ptr<u16_stage> m_blc;
    std::unique_ptr<u16_stage> m_dpc;
    std::unique_ptr<u16_stage> m_lsc;
    std::unique_ptr<u16_stage> m_dg;
    std::unique_ptr<u16_stage> m_bnr;
    std::unique_ptr<u16_stage> m_demosaic;
    std::unique_ptr<u16_stage> m_awb;
    std::unique_ptr<u16_stage> m_wb;
    std::unique_ptr<u16_stage> m_ccm;
    std::unique_ptr<u16_stage> m_gc;
    std::unique_ptr<u16_stage> m_aec;
    std::unique_ptr<csc_stage> m_csc;
    std::unique_ptr<u8_stage> m_cse;
    std::unique_ptr<u8_stage> m_sharpen;
    std::unique_ptr<u8_stage> m_2dnr;
    std::unique_ptr<u8_stage> m_scale;
    std::unique_ptr<u8_stage> m_yuv420;

    // Untimed functional kernels are reused as the byte-exact stage oracle.
    blc_block m_blc_kernel;
    dpc_block m_dpc_kernel;
    lsc_block m_lsc_kernel;
    dg_block m_dg_kernel;
    bnr_block m_bnr_kernel;
    demosaic_block m_demosaic_kernel;
    awb_block m_awb_kernel;
    wb_block m_wb_kernel;
    ccm_block m_ccm_kernel;
    gc_block m_gc_kernel;
    aec_block m_aec_kernel;
    csc_block m_csc_kernel;
    cse_block m_cse_kernel;
    sharpen_block m_sharpen_kernel;
    twodnr_block m_2dnr_kernel;
    scale_block m_scale_kernel;
    yuv420_block m_yuv420_kernel;

    isp_tlm::frame_feedback_latch m_feedback;
    std::uint16_t m_dg_gain_state = 0;
    float m_last_awb_r_gain = 1.0f;
    float m_last_awb_b_gain = 1.0f;
    std::int32_t m_last_aec_feedback = 0;
    bool m_awb_override_valid = false;
    float m_awb_override_r = 1.0f;
    float m_awb_override_b = 1.0f;

    sc_core::sc_time m_frame_start_time = sc_core::SC_ZERO_TIME;
    std::map<std::uint64_t, sc_core::sc_time> m_frame_start_times;
    sc_core::sc_time m_frame_end_time = sc_core::SC_ZERO_TIME;
    std::uint64_t m_next_input_frame = 0;
    bool m_arch_metrics_enabled = false;
    arch_metrics_collector m_arch_metrics;
};

#endif  // SC_ISP_PIPELINE_H
