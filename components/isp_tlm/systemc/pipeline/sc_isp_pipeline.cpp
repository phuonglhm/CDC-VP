#include "sc_isp_pipeline.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>

namespace {

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

}  // namespace

sc_isp_pipeline::sc_isp_pipeline(
    sc_core::sc_module_name name, const isp_config& cfg,
    const std::vector<float>& lsc_lut,
    sc_core::sc_fifo<std::uint16_t>* raw_in_fifo,
    sc_core::sc_fifo<std::uint8_t>* yuv_out_fifo,
    std::uint8_t input_bit_depth, cfa_types bayer_pattern,
    const isp_arch_config* arch)
    : sc_core::sc_module(name),
      raw_in(raw_in_fifo),
      yuv_out(yuv_out_fifo),
      m_cfg(cfg),
      m_lsc_lut(lsc_lut),
      m_arch_config(),
      m_input_bit_depth(input_bit_depth == 0 ? 12 : input_bit_depth),
      m_bayer_pattern(bayer_pattern),
      m_feedback(cfg.awb.is_enable, cfg.aec.is_enable,
                 cfg.awb.is_enable && cfg.wb.is_enable,
                 cfg.aec.is_enable && cfg.dg.is_auto),
      m_dg_gain_state(cfg.dg.current_gain) {
    if (raw_in == nullptr || yuv_out == nullptr) {
        throw std::invalid_argument("sc_isp_pipeline requires raw and YUV FIFOs");
    }

    m_arch_config.init_defaults();
    if (arch != nullptr) {
        m_arch_config = *arch;
    }
    if (!m_arch_config.is_valid()) {
        throw std::invalid_argument("invalid ISP architecture configuration");
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
    m_logical_input_pixels =
        isp_tlm::checked_mul(static_cast<std::uint64_t>(m_width), m_height);

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

    init_channels();
    init_stages();
    SC_THREAD(ingress_loop);
    SC_THREAD(egress_loop);
}

sc_isp_pipeline::~sc_isp_pipeline() = default;

std::size_t sc_isp_pipeline::link_depth(std::size_t link_id) const noexcept {
    return m_arch_config.links[link_id].depth;
}

isp_tlm::stage_timing sc_isp_pipeline::stage_timing(std::size_t block_id) const {
    const block_arch_config& block = m_arch_config.blocks[block_id];
    isp_tlm::stage_timing timing;
    timing.pipeline_latency_cycles = block.pipeline_latency_cycles;
    timing.pixel_initiation_interval_cycles =
        block.pixel_initiation_interval_cycles;
    timing.pixels_per_cycle = block.pixels_per_cycle;
    timing.max_in_flight_lines = block.max_in_flight_lines;
    timing.cycle_period = sc_core::sc_time(
        1000.0 / static_cast<double>(m_arch_config.clock_freq_mhz),
        sc_core::SC_NS);
    if (block.workload.available) {
        timing.workload = block.workload;
    }
    if (block.local_memory.available) {
        timing.local_memory = block.local_memory;
    }
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

    m_line_ingress = u16("line_ingress", m_height, raw_samples);
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
    m_input_norm->set_enabled(true);
    m_blc->set_enabled(m_cfg.blc.is_enable);
    m_dpc->set_enabled(m_cfg.dpc.is_enable);
    m_lsc->set_enabled(m_cfg.lsc.is_enable);
    m_dg->set_enabled(m_cfg.dg.is_enable);
    m_bnr->set_enabled(m_cfg.bnr.is_enable);
    m_demosaic->set_enabled(m_cfg.demosaic.is_enable);
    m_awb->set_enabled(m_cfg.awb.is_enable);
    m_wb->set_enabled(m_cfg.wb.is_enable);
    m_ccm->set_enabled(m_cfg.ccm.is_enable);
    m_gc->set_enabled(m_cfg.gc.is_enable);
    m_aec->set_enabled(m_cfg.aec.is_enable);
    m_csc->set_enabled(true);
    m_cse->set_enabled(m_cfg.cse.is_enable);
    m_sharpen->set_enabled(m_cfg.sharpen.is_enable);
    m_2dnr->set_enabled(m_cfg.twodnr.is_enable);
    m_scale->set_enabled(m_cfg.scale.is_enable);
    m_yuv420->set_enabled(m_cfg.yuv420.is_enable);
}

void sc_isp_pipeline::ingress_loop() {
    while (true) {
        const std::uint64_t frame_id = m_next_input_frame++;
        if (frame_id != 0) {
            while (m_completed_frame < frame_id) {
                sc_core::wait(m_completed_frame_event);
            }
            if (m_feedback.enabled_producers() != 0) {
                while (!m_feedback.ready(frame_id)) {
                    sc_core::wait(m_feedback.ready_event());
                }
            }
        }

        m_frame_input_id = frame_id;
        m_frame_has_input = false;
        m_frame_input_bytes = 0;
        for (std::uint32_t row = 0; row < m_height; ++row) {
            auto token = m_line_ingress->reserve_result();
            auto view = token.view();
            for (std::uint32_t sample = 0; sample < m_width; ++sample) {
                const std::uint16_t value = raw_in->read();
                if (!m_frame_has_input) {
                    m_frame_first_input_time = sc_core::sc_time_stamp();
                    m_frame_input_bytes = isp_tlm::checked_mul(
                        m_logical_input_pixels, sizeof(std::uint16_t));
                    m_frame_has_input = true;
                }
                view[sample] = value;
            }
            isp_tlm::line_meta meta{};
            meta.frame_id = frame_id;
            meta.row = row;
            meta.width_pixels = m_width;
            meta.valid_samples = m_width;
            meta.dst_offset_bytes = static_cast<std::uint32_t>(
                static_cast<std::size_t>(row) * m_width * sizeof(std::uint16_t));
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
        bool metadata_valid = true;
        m_frame_has_output = false;
        m_frame_output_bytes = 0;
        std::fill(m_sink_frame.begin(), m_sink_frame.end(), 0u);

        for (std::uint32_t row = 0; row < m_final_rows; ++row) {
            auto token = m_line_egress->read();
            const isp_tlm::line_meta& meta = token.meta();
            const sc_core::sc_time line_time = sc_core::sc_time_stamp();
            if (!have_frame) {
                frame_id = meta.frame_id;
                have_frame = true;
            }
            if (meta.frame_id != frame_id || meta.frame_id != m_frame_input_id ||
                meta.row != row ||
                meta.start_of_frame != (row == 0) ||
                meta.end_of_frame != (row + 1 == m_final_rows) ||
                meta.valid_samples == 0 || meta.valid_samples > token.view().size()) {
                metadata_valid = false;
                SC_REPORT_ERROR("sc_isp_pipeline",
                                "invalid egress frame/line metadata");
            }

            const auto view = token.view();
            const std::size_t offset = meta.dst_offset_bytes;
            const std::size_t valid = meta.valid_samples;
            if (offset > m_sink_frame.size() ||
                valid > m_sink_frame.size() - offset) {
                metadata_valid = false;
                SC_REPORT_ERROR("sc_isp_pipeline",
                                "egress line exceeds sink frame storage");
                continue;
            }
            std::copy_n(view.data(), valid, m_sink_frame.data() + offset);
            if (!m_frame_has_output) {
                m_frame_first_output_time = line_time;
                m_frame_has_output = true;
            }
            m_frame_last_output_time = line_time;
            m_frame_output_bytes =
                isp_tlm::checked_add(m_frame_output_bytes, valid);
        }

        for (const std::uint8_t value : m_sink_frame) {
            yuv_out->write(value);
        }

        isp_tlm::raw_pipeline_metrics raw;
        raw.frame.frame_id = frame_id;
        raw.frame.width = m_width;
        raw.frame.height = m_height;
        raw.frame.logical_pixels = m_logical_input_pixels;
        raw.frame.input_bytes = m_frame_input_bytes;
        raw.frame.output_bytes = m_frame_output_bytes;
        raw.frame.input_bytes_available = m_frame_has_input;
        raw.frame.output_bytes_available = m_frame_has_output && metadata_valid;
        raw.frame.cycle_period = stage_timing(isp_blocks::INPUT_NORMALIZER).cycle_period;
        raw.frame.first_input_time = m_frame_first_input_time;
        raw.frame.first_output_time = m_frame_first_output_time;
        raw.frame.last_output_time = m_frame_last_output_time;
        raw.frame.has_first_input = m_frame_has_input;
        raw.frame.has_first_output = m_frame_has_output && metadata_valid;
        raw.frame.has_last_output = m_frame_has_output && metadata_valid;
        const bool valid_frame_times =
            raw.frame.has_first_input && raw.frame.has_last_output &&
            raw.frame.last_output_time >= raw.frame.first_input_time;
        const std::uint64_t frame_cycles =
            valid_frame_times
                ? isp_tlm::time_to_cycles(
                      raw.frame.last_output_time - raw.frame.first_input_time,
                      raw.frame.cycle_period)
                : 0;


        const auto add_block = [this, &raw, frame_cycles](const char* name,
                                                           const auto& stage,
                                                           std::size_t block_id) {
            const auto& metric = stage->metrics();
            const auto timing = stage_timing(block_id);
            isp_tlm::raw_block_metrics block;
            block.name = name;
            block.enabled = stage->enabled();
            block.input_lines = metric.input_lines;
            block.input_logical_pixels = metric.input_logical_pixels;
            block.output_logical_pixels = metric.output_logical_pixels;
            block.accepted_input_beats = metric.accepted_input_beats;
            block.produced_output_beats = metric.produced_output_beats;
            block.output_lines = metric.output_lines;
            block.logical_pixels = metric.logical_pixels;
            block.processing_beats = metric.processing_beats;
            block.active_cycles = metric.active_cycles;
            block.bypass_cycles = metric.bypass_cycles;
            block.input_starved_cycles =
                isp_tlm::time_to_cycles(metric.input_starved_time, timing.cycle_period);
            block.output_blocked_cycles =
                isp_tlm::time_to_cycles(metric.output_blocked_time, timing.cycle_period);
            block.completion_wait_cycles =
                isp_tlm::time_to_cycles(metric.completion_wait_time, timing.cycle_period);
            block.memory_wait_cycles = metric.memory_wait_cycles;
            block.memory_wait_available = metric.memory_wait_available;
            block.memory_wait_provenance = metric.memory_wait_provenance;
            block.issue_window_cycles = metric.issue_window_cycles;
            block.observation_cycles = frame_cycles;
            block.operations = metric.operations;
            raw.blocks.push_back(std::move(block));
        };
        add_block(isp_blocks::BLOCK_NAMES[isp_blocks::INPUT_NORMALIZER], m_input_norm,
                  isp_blocks::INPUT_NORMALIZER);
        add_block(isp_blocks::BLOCK_NAMES[isp_blocks::BLC], m_blc, isp_blocks::BLC);
        add_block(isp_blocks::BLOCK_NAMES[isp_blocks::DPC], m_dpc, isp_blocks::DPC);
        add_block(isp_blocks::BLOCK_NAMES[isp_blocks::LSC], m_lsc, isp_blocks::LSC);
        add_block(isp_blocks::BLOCK_NAMES[isp_blocks::DG], m_dg, isp_blocks::DG);
        add_block(isp_blocks::BLOCK_NAMES[isp_blocks::BNR], m_bnr, isp_blocks::BNR);
        add_block(isp_blocks::BLOCK_NAMES[isp_blocks::DEMOSAIC], m_demosaic, isp_blocks::DEMOSAIC);
        add_block(isp_blocks::BLOCK_NAMES[isp_blocks::AWB], m_awb, isp_blocks::AWB);
        add_block(isp_blocks::BLOCK_NAMES[isp_blocks::WB], m_wb, isp_blocks::WB);
        add_block(isp_blocks::BLOCK_NAMES[isp_blocks::CCM], m_ccm, isp_blocks::CCM);
        add_block(isp_blocks::BLOCK_NAMES[isp_blocks::GC], m_gc, isp_blocks::GC);
        add_block(isp_blocks::BLOCK_NAMES[isp_blocks::AEC], m_aec, isp_blocks::AEC);
        add_block(isp_blocks::BLOCK_NAMES[isp_blocks::CSC], m_csc, isp_blocks::CSC);
        add_block(isp_blocks::BLOCK_NAMES[isp_blocks::CSE], m_cse, isp_blocks::CSE);
        add_block(isp_blocks::BLOCK_NAMES[isp_blocks::SHARPEN], m_sharpen, isp_blocks::SHARPEN);
        add_block(isp_blocks::BLOCK_NAMES[isp_blocks::TWO_DNR], m_2dnr, isp_blocks::TWO_DNR);
        add_block(isp_blocks::BLOCK_NAMES[isp_blocks::SCALE], m_scale, isp_blocks::SCALE);
        add_block(isp_blocks::BLOCK_NAMES[isp_blocks::YUV420], m_yuv420, isp_blocks::YUV420);

        const auto add_link = [&raw](const char* name, const auto& link) {
            const auto metric = link->snapshot();
            isp_tlm::raw_link_metrics result;
            result.name = name;
            result.published_lines = metric.published;
            result.read_lines = metric.read;
            result.released_lines = metric.released;
            result.logical_bytes = metric.logical_bytes;
            result.occupancy_high_water = metric.occupancy_high_water;
            result.producer_wait_events = metric.producer_wait_events;
            result.consumer_wait_events = metric.consumer_wait_events;
            result.producer_wait = metric.producer_wait;
            result.consumer_wait = metric.consumer_wait;
            result.producer_wait_cycles =
                isp_tlm::time_to_cycles(metric.producer_wait, raw.frame.cycle_period);
            result.consumer_wait_cycles =
                isp_tlm::time_to_cycles(metric.consumer_wait, raw.frame.cycle_period);
            raw.links.push_back(std::move(result));
        };
        add_link(isp_links::LINK_NAMES[0], m_norm_blc);
        add_link(isp_links::LINK_NAMES[1], m_blc_dpc);
        add_link(isp_links::LINK_NAMES[2], m_dpc_lsc);
        add_link(isp_links::LINK_NAMES[3], m_lsc_dg);
        add_link(isp_links::LINK_NAMES[4], m_dg_bnr);
        add_link(isp_links::LINK_NAMES[5], m_bnr_demosaic);
        add_link(isp_links::LINK_NAMES[6], m_demosaic_awb);
        add_link(isp_links::LINK_NAMES[7], m_awb_wb);
        add_link(isp_links::LINK_NAMES[8], m_wb_ccm);
        add_link(isp_links::LINK_NAMES[9], m_ccm_gc);
        add_link(isp_links::LINK_NAMES[10], m_gc_aec);
        add_link(isp_links::LINK_NAMES[11], m_aec_csc);
        add_link(isp_links::LINK_NAMES[12], m_csc_cse);
        add_link(isp_links::LINK_NAMES[13], m_cse_sharpen);
        add_link(isp_links::LINK_NAMES[14], m_sharpen_2dnr);
        add_link(isp_links::LINK_NAMES[15], m_2dnr_scale);
        add_link(isp_links::LINK_NAMES[16], m_scale_yuv420);
        add_link(isp_links::LINK_NAMES[17], m_line_egress);
        m_latest_metrics = isp_tlm::derive_metrics(raw);
        m_has_latest_metrics = true;
        m_input_norm->reset_metrics();
        m_blc->reset_metrics(); m_dpc->reset_metrics(); m_lsc->reset_metrics();
        m_dg->reset_metrics(); m_bnr->reset_metrics(); m_demosaic->reset_metrics();
        m_awb->reset_metrics(); m_wb->reset_metrics(); m_ccm->reset_metrics();
        m_gc->reset_metrics(); m_aec->reset_metrics(); m_csc->reset_metrics();
        m_cse->reset_metrics(); m_sharpen->reset_metrics(); m_2dnr->reset_metrics();
        m_scale->reset_metrics(); m_yuv420->reset_metrics();
        m_line_ingress->reset_metrics(); m_norm_blc->reset_metrics();
        m_blc_dpc->reset_metrics(); m_dpc_lsc->reset_metrics(); m_lsc_dg->reset_metrics();
        m_dg_bnr->reset_metrics(); m_bnr_demosaic->reset_metrics(); m_demosaic_awb->reset_metrics();
        m_awb_wb->reset_metrics(); m_wb_ccm->reset_metrics(); m_ccm_gc->reset_metrics();
        m_gc_aec->reset_metrics(); m_aec_csc->reset_metrics(); m_csc_cse->reset_metrics();
        m_cse_sharpen->reset_metrics(); m_sharpen_2dnr->reset_metrics(); m_2dnr_scale->reset_metrics();
        m_scale_yuv420->reset_metrics(); m_line_egress->reset_metrics();
        m_completed_frame = frame_id + 1;
        m_completed_frame_event.notify(sc_core::SC_ZERO_TIME);
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

isp_tlm::pipeline_metrics sc_isp_pipeline::metrics() const {
    return m_has_latest_metrics ? m_latest_metrics : isp_tlm::pipeline_metrics{};
}

bool sc_isp_pipeline::write_metrics(const std::string& path) const {
    return isp_tlm::write_metrics(metrics(), path);
}
