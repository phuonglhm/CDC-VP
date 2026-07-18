#include "sc_isp_pipeline.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>

namespace {

sc_isp_pipeline::MetricsRequest& pending_metrics_request() {
    static sc_isp_pipeline::MetricsRequest request;
    return request;
}

std::uint32_t safe_dimension(std::uint32_t value) {
    return value == 0 ? 1u : value;
}

std::uint32_t safe_samples(std::uint64_t value) {
    if (value == 0) {
        return 1u;
    }
    if (value > std::numeric_limits<std::uint32_t>::max()) {
        throw std::invalid_argument("line sample count exceeds uint32_t");
    }
    return static_cast<std::uint32_t>(value);
}

std::uint32_t cycles_from_time(sc_core::sc_time duration,
                               sc_core::sc_time cycle) {
    if (duration <= sc_core::SC_ZERO_TIME || cycle <= sc_core::SC_ZERO_TIME) {
        return 0;
    }
    const double value = duration.to_seconds() / cycle.to_seconds();
    return static_cast<std::uint32_t>(std::ceil(value));
}

}  // namespace

void sc_isp_pipeline::set_metrics_request(
    bool enable, const std::string& output_dir,
    const std::vector<std::string>* skip) {
    MetricsRequest& request = pending_metrics_request();
    request.enable = enable;
    request.output_dir = output_dir;
    request.skip = skip;
}

void sc_isp_pipeline::clear_metrics_request() {
    pending_metrics_request() = MetricsRequest{};
}

sc_isp_pipeline::MetricsRequest sc_isp_pipeline::consume_metrics_request() {
    MetricsRequest request = pending_metrics_request();
    pending_metrics_request() = MetricsRequest{};
    return request;
}

sc_isp_pipeline::sc_isp_pipeline(
    sc_core::sc_module_name name, const isp_config& cfg,
    const std::vector<float>& lsc_lut,
    sc_core::sc_fifo<std::uint16_t>* raw_in_fifo,
    sc_core::sc_fifo<std::uint8_t>* yuv_out_fifo,
    std::uint8_t input_bit_depth, cfa_types bayer_pattern,
    const hw_params* hw, const isp_arch_config* arch)
    : sc_core::sc_module(name),
      raw_in(raw_in_fifo),
      yuv_out(yuv_out_fifo),
      m_cfg(cfg),
      m_lsc_lut(lsc_lut),
      m_arch_config(),
      m_hw_params(hw != nullptr ? *hw : hw_params{}),
      m_input_bit_depth(input_bit_depth == 0 ? 12 : input_bit_depth),
      m_bayer_pattern(bayer_pattern),
      m_feedback(cfg.awb.is_enable, cfg.aec.is_enable,
                 cfg.awb.is_enable && cfg.wb.is_enable,
                 cfg.aec.is_enable && cfg.dg.is_auto),
      m_dg_gain_state(cfg.dg.current_gain),
      m_arch_metrics("output/arch_metrics") {
    if (raw_in == nullptr || yuv_out == nullptr) {
        throw std::invalid_argument("sc_isp_pipeline requires raw and YUV FIFOs");
    }

    m_arch_config.init_defaults();
    if (arch != nullptr) {
        m_arch_config = *arch;
    }

    m_width = safe_dimension(m_cfg.scale.in_width);
    m_height = safe_dimension(m_cfg.scale.in_height);
    m_cfg.scale.in_width = static_cast<std::uint16_t>(m_width);
    m_cfg.scale.in_height = static_cast<std::uint16_t>(m_height);

    m_scaled_width = m_cfg.scale.is_enable
                         ? safe_dimension(m_cfg.scale.out_width)
                         : m_width;
    m_scaled_height = m_cfg.scale.is_enable
                          ? safe_dimension(m_cfg.scale.out_height)
                          : m_height;
    m_cfg.scale.out_width = static_cast<std::uint16_t>(m_scaled_width);
    m_cfg.scale.out_height = static_cast<std::uint16_t>(m_scaled_height);
    m_bit_depth = m_cfg.ccm.bit_depth == 0 ? 12 : m_cfg.ccm.bit_depth;

    const std::uint32_t half_width = ceil_half(m_scaled_width);
    const std::uint32_t half_height = ceil_half(m_scaled_height);
    if (m_cfg.yuv420.is_enable) {
        m_final_rows = m_scaled_height + half_height;
        m_final_line_samples = std::max(m_scaled_width, 2u * half_width);
        m_final_frame_elements = static_cast<std::size_t>(m_scaled_width) * m_scaled_height +
                                 2u * static_cast<std::size_t>(half_width) * half_height;
    } else {
        m_final_rows = m_scaled_height;
        m_final_line_samples = safe_samples(3ull * m_scaled_width);
        m_final_frame_elements = static_cast<std::size_t>(m_scaled_width) * m_scaled_height * 3u;
    }

    m_sink_frame.resize(m_final_frame_elements);

    if (m_lsc_lut.empty()) {
        m_lsc_lut.assign(8192, 1.0f);
    }

    const MetricsRequest request = consume_metrics_request();
    enable_metrics = request.enable;
    metrics_output_dir = request.output_dir;
    if (request.skip != nullptr) {
        metrics_skip_blocks = *request.skip;
    }

    init_channels();
    init_stages();
    SC_THREAD(ingress_loop);
    SC_THREAD(egress_loop);
}

sc_isp_pipeline::~sc_isp_pipeline() = default;

void sc_isp_pipeline::bind_clock(sc_core::sc_clock* clk) {
    (void)clk;
}

std::size_t sc_isp_pipeline::link_depth(std::size_t link_id) const noexcept {
    if (link_id >= m_arch_config.links.size()) {
        return std::max<std::size_t>(1, m_hw_params.fifo_depth);
    }
    const std::size_t configured = m_arch_config.links[link_id].depth;
    return std::max<std::size_t>(1, configured == 0 ? m_hw_params.fifo_depth : configured);
}

isp_tlm::stage_timing sc_isp_pipeline::stage_timing(std::size_t block_id) const {
    isp_tlm::stage_timing timing;
    if (block_id < m_arch_config.blocks.size()) {
        const block_arch_config& block = m_arch_config.blocks[block_id];
        timing.compute_latency_cycles = std::max<std::uint32_t>(1, block.pipeline_latency);
        timing.pixel_ii = std::max<std::uint32_t>(1, block.initiation_interval);
        timing.pixels_per_cycle = std::max<std::uint32_t>(1, block.pixel_per_cycle);
        timing.max_in_flight_lines = std::max<std::uint32_t>(
            2, block.line_buffer_rows + block.line_buffer_banks + 1);
        timing.source = isp_tlm::timing_source::assumed;
    }
    const float cycle_ns = m_hw_params.cycle_ns() > 0.0f
                               ? m_hw_params.cycle_ns()
                               : 1.0f;
    timing.cycle_period = sc_core::sc_time(cycle_ns, sc_core::SC_NS);
    return timing;
}

void sc_isp_pipeline::init_channels() {
    const std::uint32_t raw_samples = m_width;
    const std::uint32_t rgb_samples = safe_samples(3ull * m_width);
    const std::uint32_t scaled_rgb_samples = safe_samples(3ull * m_scaled_width);
    const std::uint32_t csc_samples = rgb_samples;
    const std::uint32_t final_samples = m_final_line_samples;

    auto u16 = [](const char* name, std::size_t depth, std::size_t samples) {
        return std::make_unique<isp_tlm::line_channel<std::uint16_t>>(
            name, depth, samples);
    };
    auto u8 = [](const char* name, std::size_t depth, std::size_t samples) {
        return std::make_unique<isp_tlm::line_channel<std::uint8_t>>(
            name, depth, samples);
    };

    m_line_ingress = u16("line_ingress", link_depth(0), raw_samples);
    m_norm_blc = u16("line_norm_blc", link_depth(0), raw_samples);
    m_blc_dpc = u16("line_blc_dpc", link_depth(1), raw_samples);
    m_dpc_lsc = u16("line_dpc_lsc", link_depth(2), raw_samples);
    m_lsc_dg = u16("line_lsc_dg", link_depth(3), raw_samples);
    m_dg_bnr = u16("line_dg_bnr", link_depth(4), raw_samples);
    m_bnr_demosaic = u16("line_bnr_demosaic", link_depth(5), raw_samples);
    m_demosaic_awb = u16("line_demosaic_awb", link_depth(6), rgb_samples);
    m_awb_wb = u16("line_awb_wb", link_depth(7), rgb_samples);
    m_wb_ccm = u16("line_wb_ccm", link_depth(8), rgb_samples);
    m_ccm_gc = u16("line_ccm_gc", link_depth(9), rgb_samples);
    m_gc_aec = u16("line_gc_aec", link_depth(10), rgb_samples);
    m_aec_csc = u16("line_aec_csc", link_depth(11), rgb_samples);
    m_csc_cse = u8("line_csc_cse", link_depth(12), csc_samples);
    m_cse_sharpen = u8("line_cse_sharpen", link_depth(13), csc_samples);
    m_sharpen_2dnr = u8("line_sharpen_2dnr", link_depth(14), csc_samples);
    m_2dnr_scale = u8("line_2dnr_scale", link_depth(15), csc_samples);
    m_scale_yuv420 = u8("line_scale_yuv420", link_depth(16), scaled_rgb_samples);
    m_line_egress = u8("line_egress", link_depth(17), final_samples);
}

void sc_isp_pipeline::init_stages() {
    using isp_tlm::line_meta;
    using isp_tlm::line_plane;

    const std::size_t raw_frame_elements =
        static_cast<std::size_t>(m_width) * m_height;
    const std::size_t rgb_frame_elements = raw_frame_elements * 3u;
    const std::uint32_t rgb_samples = safe_samples(3ull * m_width);
    const std::uint32_t csc_samples = rgb_samples;
    const std::uint32_t scaled_rgb_samples = safe_samples(3ull * m_scaled_width);
    const std::size_t scaled_rgb_frame_elements =
        static_cast<std::size_t>(m_scaled_width) * m_scaled_height * 3u;
    const std::size_t final_frame_elements = m_final_frame_elements;

    const auto raw_meta = [](line_meta meta) {
        meta.plane = line_plane::raw;
        return meta;
    };
    const auto rgb_meta = [](line_meta meta) {
        meta.plane = line_plane::rgb;
        return meta;
    };
    const auto yuv_meta = [](line_meta meta) {
        meta.plane = line_plane::yuv444;
        return meta;
    };
    const auto final_meta = [this](line_meta meta) {
        const std::uint32_t half_width = ceil_half(m_scaled_width);
        const std::uint32_t half_height = ceil_half(m_scaled_height);
        if (!m_cfg.yuv420.is_enable) {
            meta.plane = line_plane::yuv444;
            meta.width_pixels = m_scaled_width;
            meta.valid_samples = 3u * m_scaled_width;
            meta.dst_offset_bytes = meta.row * meta.valid_samples;
            return meta;
        }
        if (meta.row < m_scaled_height) {
            meta.plane = line_plane::y;
            meta.width_pixels = m_scaled_width;
            meta.valid_samples = m_scaled_width;
            meta.dst_offset_bytes = meta.row * m_scaled_width;
        } else {
            const std::uint32_t uv_row = meta.row - m_scaled_height;
            meta.plane = line_plane::uv;
            meta.width_pixels = half_width;
            meta.valid_samples = 2u * half_width;
            meta.dst_offset_bytes = static_cast<std::uint32_t>(
                static_cast<std::size_t>(m_scaled_width) * m_scaled_height +
                static_cast<std::size_t>(uv_row) * 2u * half_width);
        }
        (void)half_height;
        return meta;
    };

    auto normalize = [this](const std::uint16_t* in, std::uint16_t* out,
                            std::uint32_t width, std::uint32_t height,
                            std::uint32_t, std::uint32_t,
                            std::uint64_t) {
        const std::uint32_t source_max =
            m_input_bit_depth >= 16 ? std::numeric_limits<std::uint16_t>::max()
                                    : ((1u << m_input_bit_depth) - 1u);
        const std::uint32_t work_max = (1u << m_bit_depth) - 1u;
        const std::size_t count = static_cast<std::size_t>(width) * height;
        for (std::size_t i = 0; i < count; ++i) {
            const std::uint32_t value = in[i];
            out[i] = static_cast<std::uint16_t>(
                source_max == work_max
                    ? value
                    : (value * work_max) / source_max);
        }
    };

    auto normalize_line = [this](const std::uint16_t* in, std::uint16_t* out,
                                 std::uint32_t, std::uint32_t input_samples,
                                 std::uint32_t output_samples, std::uint64_t) {
        const std::uint32_t source_max =
            m_input_bit_depth >= 16 ? std::numeric_limits<std::uint16_t>::max()
                                    : ((1u << m_input_bit_depth) - 1u);
        const std::uint32_t work_max = (1u << m_bit_depth) - 1u;
        const std::size_t count =
            std::min<std::size_t>(input_samples, output_samples);
        for (std::size_t i = 0; i < count; ++i) {
            const std::uint32_t value = in[i];
            out[i] = static_cast<std::uint16_t>(
                source_max == work_max
                    ? value
                    : (value * work_max) / source_max);
        }
    };

    auto raw_kernel = [this](auto* kernel, auto config, const std::uint16_t* in,
                             std::uint16_t* out, std::uint32_t width,
                             std::uint32_t height) {
        kernel->process(in, out, width, height, config);
    };

    m_input_norm = std::make_unique<u16_stage>(
        "stage_input_norm", m_line_ingress.get(), m_norm_blc.get(),
        m_height, m_width, m_width, m_height, m_width, m_width, raw_frame_elements,
        normalize, raw_meta, stage_timing(isp_blocks::INPUT_NORMALIZER),
        normalize_line);

    m_blc = std::make_unique<u16_stage>(
        "stage_blc", m_norm_blc.get(), m_blc_dpc.get(), m_height, m_width,
        m_width, m_height, m_width, m_width, raw_frame_elements,
        [this](const std::uint16_t* in, std::uint16_t* out, std::uint32_t w,
               std::uint32_t h, std::uint32_t, std::uint32_t,
               std::uint64_t) {
            m_blc_kernel.process(in, out, w, h, m_cfg.blc, m_bayer_pattern, m_bit_depth);
        }, raw_meta, stage_timing(isp_blocks::BLC));

    m_dpc = std::make_unique<u16_stage>(
        "stage_dpc", m_blc_dpc.get(), m_dpc_lsc.get(), m_height, m_width,
        m_width, m_height, m_width, m_width, raw_frame_elements,
        [this](const std::uint16_t* in, std::uint16_t* out, std::uint32_t w,
               std::uint32_t h, std::uint32_t, std::uint32_t,
               std::uint64_t) { m_dpc_kernel.process(in, out, w, h, m_cfg.dpc); },
        raw_meta, stage_timing(isp_blocks::DPC));

    m_lsc = std::make_unique<u16_stage>(
        "stage_lsc", m_dpc_lsc.get(), m_lsc_dg.get(), m_height, m_width,
        m_width, m_height, m_width, m_width, raw_frame_elements,
        [this](const std::uint16_t* in, std::uint16_t* out, std::uint32_t w,
               std::uint32_t h, std::uint32_t, std::uint32_t,
               std::uint64_t) {
            m_lsc_kernel.process(in, out, w, h, m_cfg.lsc, m_lsc_lut.data(),
                                 m_bayer_pattern, m_bit_depth);
        }, raw_meta, stage_timing(isp_blocks::LSC));

    m_dg = std::make_unique<u16_stage>(
        "stage_dg", m_lsc_dg.get(), m_dg_bnr.get(), m_height, m_width,
        m_width, m_height, m_width, m_width, raw_frame_elements,
        [this](const std::uint16_t* in, std::uint16_t* out, std::uint32_t w,
               std::uint32_t h, std::uint32_t, std::uint32_t,
               std::uint64_t frame_id) {
            dg_config config = m_cfg.dg;
            if (config.is_auto) {
                if (m_cfg.aec.is_enable && frame_id != 0) {
                    while (!m_feedback.ready(frame_id)) {
                        sc_core::wait(m_feedback.ready_event());
                    }
                    const isp_tlm::frame_feedback feedback =
                        m_feedback.snapshot(frame_id);
                    config.current_gain =
                        static_cast<std::uint16_t>(feedback.dg_gain);
                    m_feedback.consume_aec(frame_id);
                } else {
                    config.current_gain = m_dg_gain_state;
                }
            }
            m_dg_kernel.process(in, out, w, h, config, m_bit_depth);
        }, raw_meta, stage_timing(isp_blocks::DG));

    m_bnr = std::make_unique<u16_stage>(
        "stage_bnr", m_dg_bnr.get(), m_bnr_demosaic.get(), m_height, m_width,
        m_width, m_height, m_width, m_width, raw_frame_elements,
        [this](const std::uint16_t* in, std::uint16_t* out, std::uint32_t w,
               std::uint32_t h, std::uint32_t, std::uint32_t,
               std::uint64_t) {
            m_bnr_kernel.process(in, out, w, h, m_cfg.bnr, m_bayer_pattern, m_bit_depth);
        }, raw_meta, stage_timing(isp_blocks::BNR));

    m_demosaic = std::make_unique<u16_stage>(
        "stage_demosaic", m_bnr_demosaic.get(), m_demosaic_awb.get(), m_height,
        m_width, m_width, m_height, rgb_samples, m_width, rgb_frame_elements,
        [this](const std::uint16_t* in, std::uint16_t* out, std::uint32_t w,
               std::uint32_t h, std::uint32_t, std::uint32_t,
               std::uint64_t) {
            m_demosaic_kernel.process(in, out, w, h, m_cfg.demosaic,
                                      m_bayer_pattern, m_bit_depth);
        }, rgb_meta, stage_timing(isp_blocks::DEMOSAIC));

    m_awb = std::make_unique<u16_stage>(
        "stage_awb", m_demosaic_awb.get(), m_awb_wb.get(), m_height, rgb_samples,
        m_width, m_height, rgb_samples, m_width, rgb_frame_elements,
        [this](const std::uint16_t* in, std::uint16_t* out, std::uint32_t w,
               std::uint32_t h, std::uint32_t, std::uint32_t,
               std::uint64_t frame_id) {
            const std::size_t count = static_cast<std::size_t>(w) * h * 3u;
            std::copy_n(in, count, out);
            awb_config config = m_cfg.awb;
            if (config.is_enable) {
                m_awb_kernel.process(in, w, h, config, m_bit_depth);
                m_last_awb_r_gain = config.r_gain_out;
                m_last_awb_b_gain = config.b_gain_out;
                const std::uint32_t dg_gain = m_dg_gain_state;
                m_feedback.commit_awb(frame_id, config.r_gain_out,
                                      config.b_gain_out, dg_gain);
            }
        }, rgb_meta, stage_timing(isp_blocks::AWB));

    m_wb = std::make_unique<u16_stage>(
        "stage_wb", m_awb_wb.get(), m_wb_ccm.get(), m_height, rgb_samples,
        m_width, m_height, rgb_samples, m_width, rgb_frame_elements,
        [this](const std::uint16_t* in, std::uint16_t* out, std::uint32_t w,
               std::uint32_t h, std::uint32_t, std::uint32_t,
               std::uint64_t frame_id) {
            wb_config config = m_cfg.wb;
            if (m_cfg.awb.is_enable && config.is_enable) {
                if (frame_id == 0 && m_awb_override_valid) {
                    config.r_gain = m_awb_override_r;
                    config.b_gain = m_awb_override_b;
                } else {
                    while (!m_feedback.ready(frame_id)) {
                        sc_core::wait(m_feedback.ready_event());
                    }
                    const isp_tlm::frame_feedback feedback = m_feedback.snapshot(frame_id);
                    config.r_gain *= feedback.awb_r_gain;
                    config.b_gain *= feedback.awb_b_gain;
                    m_feedback.consume_awb(frame_id);
                }
                m_last_awb_r_gain = config.r_gain;
                m_last_awb_b_gain = config.b_gain;
            }
            m_wb_kernel.process(in, out, w, h, config);
        }, rgb_meta, stage_timing(isp_blocks::WB));

    m_ccm = std::make_unique<u16_stage>(
        "stage_ccm", m_wb_ccm.get(), m_ccm_gc.get(), m_height, rgb_samples,
        m_width, m_height, rgb_samples, m_width, rgb_frame_elements,
        [this](const std::uint16_t* in, std::uint16_t* out, std::uint32_t w,
               std::uint32_t h, std::uint32_t, std::uint32_t,
               std::uint64_t) { m_ccm_kernel.process(in, out, w, h, m_cfg.ccm); },
        rgb_meta, stage_timing(isp_blocks::CCM),
        [this](const std::uint16_t* in, std::uint16_t* out, std::uint32_t,
               std::uint32_t input_samples, std::uint32_t,
               std::uint64_t) {
            m_ccm_kernel.process(in, out, input_samples / 3u, 1, m_cfg.ccm);
        });

    m_gc = std::make_unique<u16_stage>(
        "stage_gc", m_ccm_gc.get(), m_gc_aec.get(), m_height, rgb_samples,
        m_width, m_height, rgb_samples, m_width, rgb_frame_elements,
        [this](const std::uint16_t* in, std::uint16_t* out, std::uint32_t w,
               std::uint32_t h, std::uint32_t, std::uint32_t,
               std::uint64_t) { m_gc_kernel.process(in, out, w, h, m_cfg.gc); },
        rgb_meta, stage_timing(isp_blocks::GC),
        [this](const std::uint16_t* in, std::uint16_t* out, std::uint32_t,
               std::uint32_t input_samples, std::uint32_t,
               std::uint64_t) {
            m_gc_kernel.process(in, out, input_samples / 3u, 1, m_cfg.gc);
        });

    m_aec = std::make_unique<u16_stage>(
        "stage_aec", m_gc_aec.get(), m_aec_csc.get(), m_height, rgb_samples,
        m_width, m_height, rgb_samples, m_width, rgb_frame_elements,
        [this](const std::uint16_t* in, std::uint16_t* out, std::uint32_t w,
               std::uint32_t h, std::uint32_t, std::uint32_t,
               std::uint64_t frame_id) {
            std::copy_n(in, static_cast<std::size_t>(w) * h * 3u, out);
            if (m_cfg.aec.is_enable) {
                aec_config config = m_cfg.aec;
                m_aec_kernel.process(in, w, h, config, m_bit_depth);
                m_last_aec_feedback = config.ae_feedback;
                if (m_cfg.dg.is_auto) {
                    if (config.ae_feedback < 0 &&
                        m_dg_gain_state < kGainArraySize) {
                        ++m_dg_gain_state;
                    } else if (config.ae_feedback > 0 &&
                               m_dg_gain_state > 0) {
                        --m_dg_gain_state;
                    }
                }
                m_feedback.commit_aec(frame_id, config.ae_feedback,
                                       m_dg_gain_state);
            }
        }, rgb_meta, stage_timing(isp_blocks::AEC));

    m_csc = std::make_unique<csc_stage>(
        "stage_csc", m_aec_csc.get(), m_csc_cse.get(), m_height, rgb_samples,
        m_width, m_height, csc_samples, m_width, rgb_frame_elements,
        [this](const std::uint16_t* in, std::uint8_t* out, std::uint32_t w,
               std::uint32_t h, std::uint32_t, std::uint32_t,
               std::uint64_t) { m_csc_kernel.process(in, out, w, h, m_cfg.csc); },
        yuv_meta, stage_timing(isp_blocks::CSC),
        [this](const std::uint16_t* in, std::uint8_t* out, std::uint32_t,
               std::uint32_t input_samples, std::uint32_t,
               std::uint64_t) {
            m_csc_kernel.process(in, out, input_samples / 3u, 1, m_cfg.csc);
        });

    m_cse = std::make_unique<u8_stage>(
        "stage_cse", m_csc_cse.get(), m_cse_sharpen.get(), m_height,
        csc_samples, m_width, m_height, csc_samples, m_width, rgb_frame_elements,
        [this](const std::uint8_t* in, std::uint8_t* out, std::uint32_t w,
               std::uint32_t h, std::uint32_t, std::uint32_t,
               std::uint64_t) { m_cse_kernel.process(in, out, w, h, m_cfg.cse); },
        yuv_meta, stage_timing(isp_blocks::CSE),
        [this](const std::uint8_t* in, std::uint8_t* out, std::uint32_t,
               std::uint32_t input_samples, std::uint32_t,
               std::uint64_t) {
            m_cse_kernel.process(in, out, input_samples / 3u, 1, m_cfg.cse);
        });

    m_sharpen = std::make_unique<u8_stage>(
        "stage_sharpen", m_cse_sharpen.get(), m_sharpen_2dnr.get(), m_height,
        csc_samples, m_width, m_height, csc_samples, m_width, rgb_frame_elements,
        [this](const std::uint8_t* in, std::uint8_t* out, std::uint32_t w,
               std::uint32_t h, std::uint32_t, std::uint32_t,
               std::uint64_t) {
            m_sharpen_kernel.process(in, out, w, h, m_cfg.sharpen);
        }, yuv_meta, stage_timing(isp_blocks::SHARPEN));

    m_2dnr = std::make_unique<u8_stage>(
        "stage_2dnr", m_sharpen_2dnr.get(), m_2dnr_scale.get(), m_height,
        csc_samples, m_width, m_height, csc_samples, m_width, rgb_frame_elements,
        [this](const std::uint8_t* in, std::uint8_t* out, std::uint32_t w,
               std::uint32_t h, std::uint32_t, std::uint32_t,
               std::uint64_t) { m_2dnr_kernel.process(in, out, w, h, m_cfg.twodnr); },
        yuv_meta, stage_timing(isp_blocks::TWO_DNR));

    m_scale = std::make_unique<u8_stage>(
        "stage_scale", m_2dnr_scale.get(), m_scale_yuv420.get(), m_height,
        csc_samples, m_width, m_scaled_height, scaled_rgb_samples, m_scaled_width,
        scaled_rgb_frame_elements,
        [this](const std::uint8_t* in, std::uint8_t* out, std::uint32_t w,
               std::uint32_t h, std::uint32_t out_w, std::uint32_t out_h,
               std::uint64_t) {
            m_scale_kernel.process(in, out, w, h, out_w, out_h, m_cfg.scale);
        }, yuv_meta, stage_timing(isp_blocks::SCALE));

    m_yuv420 = std::make_unique<u8_stage>(
        "stage_yuv420", m_scale_yuv420.get(), m_line_egress.get(),
        m_scaled_height, scaled_rgb_samples, m_scaled_width, m_final_rows,
        m_final_line_samples, m_scaled_width, final_frame_elements,
        [this](const std::uint8_t* in, std::uint8_t* out, std::uint32_t w,
               std::uint32_t h, std::uint32_t, std::uint32_t,
               std::uint64_t) {
            m_yuv420_kernel.process(in, out, w, h, m_cfg.yuv420);
        }, final_meta, stage_timing(isp_blocks::YUV420));
}

void sc_isp_pipeline::ingress_loop() {
    while (true) {
        const std::uint64_t frame_id = m_next_input_frame++;
        if (frame_id != 0 && m_feedback.enabled_producers() != 0) {
            while (!m_feedback.ready(frame_id)) {
                sc_core::wait(m_feedback.ready_event());
            }
        }

        bool frame_started = false;
        for (std::uint32_t row = 0; row < m_height; ++row) {
            auto token = m_line_ingress->reserve_result();
            auto view = token.view();
            for (std::uint32_t sample = 0; sample < m_width; ++sample) {
                const std::uint16_t value = raw_in->read();
                if (!frame_started) {
                    const sc_core::sc_time start = sc_core::sc_time_stamp();
                    m_frame_start_times[frame_id] = start;
                    if (frame_id == 0) {
                        m_frame_start_time = start;
                    }
                    frame_started = true;
                }
                view[sample] = value;
            }
            isp_tlm::line_meta meta{};
            meta.frame_id = frame_id;
            meta.row = row;
            meta.width_pixels = m_width;
            meta.valid_samples = m_width;
            meta.dst_offset_bytes = row * m_width * sizeof(std::uint16_t);
            meta.plane = isp_tlm::line_plane::raw;
            meta.start_of_frame = row == 0;
            meta.end_of_frame = row + 1 == m_height;
            m_line_ingress->publish(std::move(token), meta);
        }
    }
}

void sc_isp_pipeline::egress_loop() {
    while (true) {
        std::uint64_t frame_id = 0;
        bool have_frame = false;
        std::fill(m_sink_frame.begin(), m_sink_frame.end(), 0u);

        for (std::uint32_t row = 0; row < m_final_rows; ++row) {
            auto token = m_line_egress->read();
            const isp_tlm::line_meta& meta = token.meta();
            if (!have_frame) {
                frame_id = meta.frame_id;
                have_frame = true;
                const auto start = m_frame_start_times.find(frame_id);
                if (m_arch_metrics_enabled) {
                    m_arch_metrics.start_frame(frame_id, m_width, m_height);
                    if (start != m_frame_start_times.end()) {
                        m_arch_metrics.record_first_input(start->second);
                    }
                    m_arch_metrics.record_first_output(sc_core::sc_time_stamp());
                }
            }

            const auto view = token.view();
            const std::size_t offset = meta.dst_offset_bytes;
            const std::size_t valid =
                std::min<std::size_t>(meta.valid_samples, view.size());
            if (offset > m_sink_frame.size() ||
                valid > m_sink_frame.size() - offset) {
                SC_REPORT_ERROR("sc_isp_pipeline",
                                "egress line exceeds sink frame storage");
                continue;
            }
            std::copy_n(view.data(), valid, m_sink_frame.data() + offset);
        }

        for (const std::uint8_t value : m_sink_frame) {
            yuv_out->write(value);
        }

        const sc_core::sc_time end = sc_core::sc_time_stamp();
        const auto start = m_frame_start_times.find(frame_id);
        if (start != m_frame_start_times.end()) {
            m_frame_start_time = start->second;
            m_frame_start_times.erase(start);
        }
        m_frame_end_time = end;
        if (m_arch_metrics_enabled) {
            m_arch_metrics.record_last_output(end);
            m_arch_metrics.end_frame();
        }
    }
}

void sc_isp_pipeline::precompute_awb_gains(const std::uint16_t* rgb12) {
    if (rgb12 == nullptr || !m_cfg.awb.is_enable) {
        return;
    }
    awb_config config = m_cfg.awb;
    m_awb_kernel.process(rgb12, m_width, m_height, config, m_bit_depth);
    prime_awb_gains(config.r_gain_out, config.b_gain_out);
}

void sc_isp_pipeline::precompute_awb_gains_from_bayer(const std::uint16_t* bayer16) {
    if (bayer16 == nullptr || !m_cfg.awb.is_enable) {
        return;
    }
    std::vector<std::uint16_t> rgb(static_cast<std::size_t>(m_width) * m_height * 3u);
    m_demosaic_kernel.process(bayer16, rgb.data(), m_width, m_height,
                              m_cfg.demosaic, m_bayer_pattern, m_bit_depth);
    precompute_awb_gains(rgb.data());
}

void sc_isp_pipeline::prime_awb_gains(float r_gain, float b_gain) {
    m_awb_override_valid = true;
    m_awb_override_r = r_gain;
    m_awb_override_b = b_gain;
    m_last_awb_r_gain = r_gain;
    m_last_awb_b_gain = b_gain;
}

double sc_isp_pipeline::estimate_frame_time_us() const {
    if (get_frame_time() > sc_core::SC_ZERO_TIME) {
        return get_frame_time_us();
    }
    double cycles = 0.0;
    for (std::size_t id = 0; id < m_arch_config.blocks.size(); ++id) {
        const isp_tlm::stage_timing timing = stage_timing(id);
        const std::uint32_t lines =
            id == isp_blocks::YUV420 ? m_final_rows : m_height;
        const std::uint32_t width =
            (id == isp_blocks::SCALE || id == isp_blocks::YUV420)
                ? m_scaled_width
                : m_width;
        const std::uint32_t ppc = std::max<std::uint32_t>(1, timing.pixels_per_cycle);
        const std::uint64_t transfer_cycles =
            (static_cast<std::uint64_t>(width) + ppc - 1u) / ppc;
        const std::uint64_t issue_cycles =
            std::max<std::uint64_t>(transfer_cycles, timing.pixel_ii);
        cycles += static_cast<double>(lines) * issue_cycles;
    }
    const double cycle_ns = m_hw_params.cycle_ns() > 0.0f ? m_hw_params.cycle_ns() : 1.0;
    return cycles * cycle_ns / 1000.0;
}

double sc_isp_pipeline::estimate_fps() const {
    const double frame_us = estimate_frame_time_us();
    return frame_us > 0.0 ? 1.0e6 / frame_us : 0.0;
}

std::size_t sc_isp_pipeline::metrics_sample_count() const noexcept {
    const std::array<const isp_tlm::line_channel<std::uint16_t>*, 13> u16_links = {
        m_line_ingress.get(), m_norm_blc.get(), m_blc_dpc.get(), m_dpc_lsc.get(),
        m_lsc_dg.get(), m_dg_bnr.get(), m_bnr_demosaic.get(), m_demosaic_awb.get(),
        m_awb_wb.get(), m_wb_ccm.get(), m_ccm_gc.get(), m_gc_aec.get(),
        m_aec_csc.get()};
    const std::array<const isp_tlm::line_channel<std::uint8_t>*, 7> u8_links = {
        m_csc_cse.get(), m_cse_sharpen.get(), m_sharpen_2dnr.get(), m_2dnr_scale.get(),
        m_scale_yuv420.get(), m_line_egress.get(), nullptr};
    std::size_t total = 0;
    for (const auto* link : u16_links) {
        if (link != nullptr) {
            total += link->snapshot().published;
        }
    }
    for (const auto* link : u8_links) {
        if (link != nullptr) {
            total += link->snapshot().published;
        }
    }
    return total;
}

std::size_t sc_isp_pipeline::block_metrics_sample_count() const noexcept {
    std::size_t total = 0;
    const auto add = [&total](const auto& stage) {
        if (stage != nullptr) {
            total += stage->metrics().output_lines;
        }
    };
    add(m_input_norm);
    add(m_blc);
    add(m_dpc);
    add(m_lsc);
    add(m_dg);
    add(m_bnr);
    add(m_demosaic);
    add(m_awb);
    add(m_wb);
    add(m_ccm);
    add(m_gc);
    add(m_aec);
    add(m_csc);
    add(m_cse);
    add(m_sharpen);
    add(m_2dnr);
    add(m_scale);
    add(m_yuv420);
    return total;
}

bool sc_isp_pipeline::dump_pipeline_metrics() const {
    std::error_code error;
    std::filesystem::create_directories(metrics_output_dir, error);
    std::ofstream output(metrics_output_dir + "/line_links.csv");
    if (!output) {
        return false;
    }
    output << "link,published,read,released,occupancy_high_water,blocked_producers,blocked_consumers\n";
    std::size_t index = 0;
    const auto dump = [&output, &index](const auto* link) {
        if (link == nullptr) {
            return;
        }
        const auto snapshot = link->snapshot();
        output << index++ << ',' << snapshot.published << ',' << snapshot.read << ','
               << snapshot.released << ',' << snapshot.occupancy_high_water << ','
               << snapshot.blocked_producers << ',' << snapshot.blocked_consumers << '\n';
    };
    dump(m_norm_blc.get());
    dump(m_blc_dpc.get());
    dump(m_dpc_lsc.get());
    dump(m_lsc_dg.get());
    dump(m_dg_bnr.get());
    dump(m_bnr_demosaic.get());
    dump(m_demosaic_awb.get());
    dump(m_awb_wb.get());
    dump(m_wb_ccm.get());
    dump(m_ccm_gc.get());
    dump(m_gc_aec.get());
    dump(m_aec_csc.get());
    dump(m_csc_cse.get());
    dump(m_cse_sharpen.get());
    dump(m_sharpen_2dnr.get());
    dump(m_2dnr_scale.get());
    dump(m_scale_yuv420.get());
    dump(m_line_egress.get());
    return true;
}

bool sc_isp_pipeline::dump_all_block_metrics() const {
    std::error_code error;
    std::filesystem::create_directories(metrics_output_dir, error);
    std::ofstream output(metrics_output_dir + "/line_stages.csv");
    if (!output) {
        return false;
    }
    output << "stage,input_lines,output_lines,issued_lines,retired_lines,"
              "in_flight_high_water,issue_stalls,input_wait_s,issue_wait_s,"
              "retire_wait_s,output_wait_s,frames\n";
    const auto dump = [&output](const char* name, const auto* stage) {
        if (stage == nullptr) {
            return;
        }
        const auto metrics = stage->metrics();
        output << name << ',' << metrics.input_lines << ','
               << metrics.output_lines << ',' << metrics.issued_lines << ','
               << metrics.retired_lines << ','
               << metrics.in_flight_high_water << ',' << metrics.issue_stalls << ','
               << metrics.input_wait.to_seconds() << ','
               << metrics.issue_wait.to_seconds() << ','
               << metrics.retire_wait.to_seconds() << ','
               << metrics.output_wait.to_seconds() << ',' << metrics.frames << '\n';
    };
    dump("input_norm", m_input_norm.get());
    dump("blc", m_blc.get());
    dump("dpc", m_dpc.get());
    dump("lsc", m_lsc.get());
    dump("dg", m_dg.get());
    dump("bnr", m_bnr.get());
    dump("demosaic", m_demosaic.get());
    dump("awb", m_awb.get());
    dump("wb", m_wb.get());
    dump("ccm", m_ccm.get());
    dump("gc", m_gc.get());
    dump("aec", m_aec.get());
    dump("csc", m_csc.get());
    dump("cse", m_cse.get());
    dump("sharpen", m_sharpen.get());
    dump("2dnr", m_2dnr.get());
    dump("scale", m_scale.get());
    dump("yuv420", m_yuv420.get());
    return true;
}

void sc_isp_pipeline::print_metrics_summary() const {
    std::cout << "[sc_isp_pipeline] line tokens=" << metrics_sample_count()
              << ", stage outputs=" << block_metrics_sample_count()
              << ", frame_us=" << std::fixed << std::setprecision(3)
              << estimate_frame_time_us() << ", fps=" << estimate_fps() << '\n';
}

void sc_isp_pipeline::enable_arch_metrics(const std::string& output_dir) {
    m_arch_metrics_enabled = true;
    m_arch_metrics = arch_metrics_collector(output_dir);
}

void sc_isp_pipeline::collect_block_metrics() {
    if (!m_arch_metrics_enabled) {
        return;
    }
    const sc_core::sc_time cycle = sc_core::sc_time(
        m_hw_params.cycle_ns() > 0.0f ? m_hw_params.cycle_ns() : 1.0f,
        sc_core::SC_NS);
    const auto collect = [this, cycle](const char* name, const auto* stage,
                                       std::size_t block_id) {
        if (stage == nullptr) {
            return;
        }
        const auto metrics = stage->metrics();
        const auto timing = stage_timing(block_id);
        block_perf_metrics block;
        block.block_name = name;
        block.frame_id = m_next_input_frame == 0 ? 0 : m_next_input_frame - 1;
        block.input_beats = metrics.input_lines;
        block.output_beats = metrics.output_lines;
        block.effective_ii = static_cast<double>(timing.pixel_ii);
        block.cycles.active_cycles =
            metrics.retired_lines * timing.compute_latency_cycles;
        block.cycles.starved_cycles = cycles_from_time(metrics.input_wait, cycle);
        block.cycles.blocked_cycles = cycles_from_time(metrics.retire_wait, cycle);
        block.cycles.stall_cycles =
            block.cycles.starved_cycles + block.cycles.blocked_cycles +
            cycles_from_time(metrics.issue_wait, cycle);
        block.block_utilization = block.cycles.utilization();
        m_arch_metrics.record_block_metrics(block);
    };
    collect("input_norm", m_input_norm.get(), isp_blocks::INPUT_NORMALIZER);
    collect("blc", m_blc.get(), isp_blocks::BLC);
    collect("dpc", m_dpc.get(), isp_blocks::DPC);
    collect("lsc", m_lsc.get(), isp_blocks::LSC);
    collect("dg", m_dg.get(), isp_blocks::DG);
    collect("bnr", m_bnr.get(), isp_blocks::BNR);
    collect("demosaic", m_demosaic.get(), isp_blocks::DEMOSAIC);
    collect("awb", m_awb.get(), isp_blocks::AWB);
    collect("wb", m_wb.get(), isp_blocks::WB);
    collect("ccm", m_ccm.get(), isp_blocks::CCM);
    collect("gc", m_gc.get(), isp_blocks::GC);
    collect("aec", m_aec.get(), isp_blocks::AEC);
    collect("csc", m_csc.get(), isp_blocks::CSC);
    collect("cse", m_cse.get(), isp_blocks::CSE);
    collect("sharpen", m_sharpen.get(), isp_blocks::SHARPEN);
    collect("2dnr", m_2dnr.get(), isp_blocks::TWO_DNR);
    collect("scale", m_scale.get(), isp_blocks::SCALE);
    collect("yuv420", m_yuv420.get(), isp_blocks::YUV420);
}

void sc_isp_pipeline::update_arch_block_metrics(
    const std::string& block_name, std::uint64_t active_cycles,
    std::uint64_t starved_cycles, std::uint64_t blocked_cycles) {
    if (!m_arch_metrics_enabled) {
        return;
    }
    block_perf_metrics metrics;
    metrics.block_name = block_name;
    metrics.frame_id = m_next_input_frame == 0 ? 0 : m_next_input_frame - 1;
    metrics.cycles.active_cycles = active_cycles;
    metrics.cycles.starved_cycles = starved_cycles;
    metrics.cycles.blocked_cycles = blocked_cycles;
    metrics.cycles.stall_cycles = starved_cycles + blocked_cycles;
    metrics.block_utilization = metrics.cycles.utilization();
    m_arch_metrics.record_block_metrics(metrics);
}

void sc_isp_pipeline::update_arch_frame_timing(std::uint64_t) {
    if (!m_arch_metrics_enabled || m_frame_end_time <= m_frame_start_time) {
        return;
    }
    // Frame timing is recorded by egress_loop as each frame closes.
}

void sc_isp_pipeline::dump_arch_metrics() const {
    if (m_arch_metrics_enabled) {
        m_arch_metrics.dump_all();
    }
}

void sc_isp_pipeline::dump_arch_summary() const {
    if (m_arch_metrics_enabled) {
        const bottleneck_report report = m_arch_metrics.analyze_bottleneck();
        std::cout << "[sc_isp_pipeline] bottleneck=" << report.location
                  << ", severity=" << report.severity << '\n';
    }
}
