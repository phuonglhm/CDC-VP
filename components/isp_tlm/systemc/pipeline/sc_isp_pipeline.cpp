/**
 * @file sc_isp_pipeline.cpp
 * @brief Implementation of sc_isp_pipeline SystemC module
 *
 * Metrics layers:
 *   - Block-level: each sc_* block measures internal processing latency
 *     (from last read to last write per processing unit).
 *   - Boundary-level: sc_metrics_wrapper<T> inserted between every block
 *     boundary measures inter-block throughput.
 *   - Frame-level: sc_isp_pipeline records first-pixel-in and last-pixel-out
 *     timestamps for total frame processing time.
 *
 * Hardware parameters (hw_params) flow from the pipeline down to every
 * block and wrapper, enabling real-time conversion of cycle-based metrics.
 */
#include "sc_isp_pipeline.h"
#include <iostream>
#include <iomanip>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <algorithm>

namespace {

// Per-call dispatch helpers — typed lambdas won't compile as function
// pointers directly, so we provide these free functions.
inline void dump_wrapper_u16(void* p, const std::string& path) {
    static_cast<sc_metrics_wrapper<std::uint16_t>*>(p)->dump_metrics(path);
}
inline std::size_t count_wrapper_u16(void* p) {
    return static_cast<sc_metrics_wrapper<std::uint16_t>*>(p)->sample_count();
}
inline void dump_wrapper_u8(void* p, const std::string& path) {
    static_cast<sc_metrics_wrapper<std::uint8_t>*>(p)->dump_metrics(path);
}
inline std::size_t count_wrapper_u8(void* p) {
    return static_cast<sc_metrics_wrapper<std::uint8_t>*>(p)->sample_count();
}

// Block metrics dispatch helpers
inline void dump_block_u16(void* p, const std::string& path) {
    static_cast<sc_block_metrics<std::uint16_t>*>(p)->dump_metrics(path);
}
inline std::size_t count_block_u16(void* p) {
    return static_cast<sc_block_metrics<std::uint16_t>*>(p)->sample_count();
}
inline sc_core::sc_time mean_latency_block_u16(void* p) {
    return static_cast<sc_block_metrics<std::uint16_t>*>(p)->mean_latency();
}
inline sc_core::sc_time min_latency_block_u16(void* p) {
    return static_cast<sc_block_metrics<std::uint16_t>*>(p)->min_latency();
}
inline sc_core::sc_time max_latency_block_u16(void* p) {
    return static_cast<sc_block_metrics<std::uint16_t>*>(p)->max_latency();
}
inline double stddev_block_u16(void* p) {
    return static_cast<sc_block_metrics<std::uint16_t>*>(p)->stddev_latency();
}
inline float throughput_block_u16(void* p) {
    return static_cast<sc_block_metrics<std::uint16_t>*>(p)->throughput_tokens_s();
}

inline void dump_block_u8(void* p, const std::string& path) {
    static_cast<sc_block_metrics<std::uint8_t>*>(p)->dump_metrics(path);
}
inline std::size_t count_block_u8(void* p) {
    return static_cast<sc_block_metrics<std::uint8_t>*>(p)->sample_count();
}
inline sc_core::sc_time mean_latency_block_u8(void* p) {
    return static_cast<sc_block_metrics<std::uint8_t>*>(p)->mean_latency();
}
inline sc_core::sc_time min_latency_block_u8(void* p) {
    return static_cast<sc_block_metrics<std::uint8_t>*>(p)->min_latency();
}
inline sc_core::sc_time max_latency_block_u8(void* p) {
    return static_cast<sc_block_metrics<std::uint8_t>*>(p)->max_latency();
}
inline double stddev_block_u8(void* p) {
    return static_cast<sc_block_metrics<std::uint8_t>*>(p)->stddev_latency();
}
inline float throughput_block_u8(void* p) {
    return static_cast<sc_block_metrics<std::uint8_t>*>(p)->throughput_tokens_s();
}

// Heuristic helper: mkdir -p for relative output dirs.
inline void ensure_dir(const std::string& path) {
    if (path.empty()) return;
    std::string p = path;
    if (p.front() != '/') {
        char cwd[1024] = {0};
        if (getcwd(cwd, sizeof(cwd)) != nullptr) {
            p = std::string(cwd) + "/" + p;
        }
    }
    for (std::size_t i = 1; i < p.size(); ++i) {
        if (p[i] == '/') {
            std::string sub = p.substr(0, i);
            mkdir(sub.c_str(), 0755);
        }
    }
    mkdir(p.c_str(), 0755);
}

// Global one-shot request consumed by the next sc_isp_pipeline ctor.
sc_isp_pipeline::MetricsRequest& pending_request() {
    static sc_isp_pipeline::MetricsRequest req;
    return req;
}
}  // namespace

void sc_isp_pipeline::set_metrics_request(bool enable,
                                          const std::string& output_dir,
                                          const std::vector<std::string>* skip) {
    auto& r = pending_request();
    r.enable = enable;
    r.output_dir = output_dir;
    r.skip = skip;
}

void sc_isp_pipeline::clear_metrics_request() {
    pending_request() = MetricsRequest{};
}

sc_isp_pipeline::MetricsRequest sc_isp_pipeline::consume_metrics_request() {
    auto r = pending_request();
    pending_request() = MetricsRequest{};
    return r;
}

// Wrap a u16 boundary. PROD writes to OUT_FIFO, wrapper reads from OUT_FIFO
// and writes to out_f, CONS reads from out_f. When metrics are disabled,
// fall back to direct binding PROD -> OUT_FIFO -> CONS.
#define WRAP_U16(NAME, PROD, CONS, OUT_FIFO, METRICS_ON)                       \
    do {                                                                       \
        PROD->fifo_out(*(OUT_FIFO));                                          \
        if ((METRICS_ON) &&                                                    \
            std::find(skip.begin(), skip.end(),                                \
                      std::string(NAME)) == skip.end()) {                      \
            auto* w = new sc_metrics_wrapper<std::uint16_t>(                    \
                sc_core::sc_gen_unique_name(NAME "_wrap"), NAME);               \
            w->fifo_in(*(OUT_FIFO));                                           \
            auto* out_f = new sc_core::sc_fifo<std::uint16_t>(FIFO_DEPTH);     \
            w->fifo_out(*out_f);                                               \
            CONS->fifo_in(*out_f);                                             \
            WrapperNode node{NAME, w, &dump_wrapper_u16, &count_wrapper_u16};   \
            m_wrapper_nodes.push_back(node);                                   \
        } else {                                                              \
            CONS->fifo_in(*(OUT_FIFO));                                        \
        }                                                                      \
    } while (0)

// Wrap a u8 boundary. PROD writes to OUT_FIFO, wrapper reads from OUT_FIFO
// and writes to out_f, CONS reads from out_f. When metrics are disabled,
// fall back to direct binding PROD -> OUT_FIFO -> CONS.
#define WRAP_U8(NAME, PROD, CONS, OUT_FIFO, METRICS_ON)                        \
    do {                                                                       \
        PROD->fifo_out(*(OUT_FIFO));                                          \
        if ((METRICS_ON) &&                                                    \
            std::find(skip.begin(), skip.end(),                                \
                      std::string(NAME)) == skip.end()) {                      \
            auto* w = new sc_metrics_wrapper<std::uint8_t>(                    \
                sc_core::sc_gen_unique_name(NAME "_wrap"), NAME);               \
            w->fifo_in(*(OUT_FIFO));                                           \
            auto* out_f = new sc_core::sc_fifo<std::uint8_t>(FIFO_DEPTH);      \
            w->fifo_out(*out_f);                                               \
            CONS->fifo_in(*out_f);                                             \
            WrapperNode node{NAME, w, &dump_wrapper_u8, &count_wrapper_u8};     \
            m_wrapper_nodes.push_back(node);                                    \
        } else {                                                               \
            CONS->fifo_in(*(OUT_FIFO));                                        \
        }                                                                      \
    } while (0)

sc_isp_pipeline::sc_isp_pipeline(sc_core::sc_module_name name,
                               const isp_config& cfg,
                               const std::vector<float>& lsc_lut,
                               sc_core::sc_fifo<std::uint16_t>* raw_in_fifo,
                               sc_core::sc_fifo<std::uint8_t>* yuv_out_fifo,
                               std::uint8_t input_bit_depth,
                               cfa_types bayer_pattern,
                               const hw_params* hw)
    : sc_module(name)
    , m_cfg(cfg)
    , m_lsc_lut(lsc_lut)
    , m_width(cfg.scale.in_width)
    , m_height(cfg.scale.in_height)
    , m_bit_depth(12)
    , m_input_bit_depth(input_bit_depth)
    , m_bayer_pattern(bayer_pattern)
    , raw_in(raw_in_fifo)
    , yuv_out(yuv_out_fifo)
    , m_hw_params(hw ? *hw : hw_params{})
    , m_frame_start_time(SC_ZERO_TIME)
    , m_frame_end_time(SC_ZERO_TIME) {
    // Consume any pre-construction request set via set_metrics_request().
    {
        MetricsRequest req = consume_metrics_request();
        if (req.enable) {
            enable_metrics    = true;
            metrics_output_dir = req.output_dir;
            if (req.skip != nullptr) {
                metrics_skip_blocks = *req.skip;
            }
        }
    }

    std::cout << "[sc_isp_pipeline] Initializing ISP pipeline..." << std::endl;
    std::cout << "[sc_isp_pipeline] Image size: " << m_width << "x" << m_height << std::endl;
    std::cout << "[sc_isp_pipeline] Hardware params:\n"
              << m_hw_params.to_string();
    std::cout << "[sc_isp_pipeline] Metrics collection: "
              << (enable_metrics ? "ENABLED" : "disabled") << std::endl;

    init_modules();
    bind_channels();

    std::cout << "[sc_isp_pipeline] Pipeline initialization complete" << std::endl;
}

sc_isp_pipeline::~sc_isp_pipeline() {
    for (auto& node : m_wrapper_nodes) {
        node.wrapper_ptr = nullptr;
    }
    for (auto& node : m_block_nodes) {
        node.block_metrics_ptr = nullptr;
    }
}

void sc_isp_pipeline::bind_clock(sc_core::sc_clock* clk) {
    if (!clk) return;

    // Bind clock to all blocks that have a clock port
    if (m_input_norm && m_input_norm->clk) {
        m_input_norm->clk->bind(*clk);
    }
    if (m_blc && m_blc->clk) {
        m_blc->clk->bind(*clk);
    }
    if (m_dpc && m_dpc->clk) {
        m_dpc->clk->bind(*clk);
    }
    if (m_lsc && m_lsc->clk) {
        m_lsc->clk->bind(*clk);
    }
    if (m_dg && m_dg->clk) {
        m_dg->clk->bind(*clk);
    }
    if (m_bnr && m_bnr->clk) {
        m_bnr->clk->bind(*clk);
    }
    if (m_demosaic && m_demosaic->clk) {
        m_demosaic->clk->bind(*clk);
    }
    if (m_awb && m_awb->clk) {
        m_awb->clk->bind(*clk);
    }
    if (m_wb && m_wb->clk) {
        m_wb->clk->bind(*clk);
    }
    if (m_ccm && m_ccm->clk) {
        m_ccm->clk->bind(*clk);
    }
    if (m_gc && m_gc->clk) {
        m_gc->clk->bind(*clk);
    }
    if (m_aec && m_aec->clk) {
        m_aec->clk->bind(*clk);
    }
    if (m_csc && m_csc->clk) {
        m_csc->clk->bind(*clk);
    }
    if (m_cse && m_cse->clk) {
        m_cse->clk->bind(*clk);
    }
    if (m_sharpen && m_sharpen->clk) {
        m_sharpen->clk->bind(*clk);
    }
    if (m_2dnr && m_2dnr->clk) {
        m_2dnr->clk->bind(*clk);
    }
    if (m_scale && m_scale->clk) {
        m_scale->clk->bind(*clk);
    }
    if (m_yuv420 && m_yuv420->clk) {
        m_yuv420->clk->bind(*clk);
    }

    std::cout << "[sc_isp_pipeline] Bound clock to all blocks" << std::endl;
}

void sc_isp_pipeline::init_modules() {
    // Allocate FIFOs
    fifo_in_norm = new sc_core::sc_fifo<std::uint16_t>(FIFO_DEPTH);
    fifo_blc_dpc = new sc_core::sc_fifo<std::uint16_t>(FIFO_DEPTH);
    fifo_dpc_lsc = new sc_core::sc_fifo<std::uint16_t>(FIFO_DEPTH);
    fifo_lsc_dg = new sc_core::sc_fifo<std::uint16_t>(FIFO_DEPTH);
    fifo_dg_bnr = new sc_core::sc_fifo<std::uint16_t>(FIFO_DEPTH);
    fifo_bnr_demosaic = new sc_core::sc_fifo<std::uint16_t>(FIFO_DEPTH);
    fifo_demosaic_awb = new sc_core::sc_fifo<std::uint16_t>(FIFO_DEPTH);
    fifo_demosaic_wb = new sc_core::sc_fifo<std::uint16_t>(FIFO_DEPTH);
    fifo_wb_ccm = new sc_core::sc_fifo<std::uint16_t>(FIFO_DEPTH);
    fifo_ccm_gc = new sc_core::sc_fifo<std::uint16_t>(FIFO_DEPTH);
    fifo_gc_aec = new sc_core::sc_fifo<std::uint16_t>(FIFO_DEPTH);
    fifo_aec_csc = new sc_core::sc_fifo<std::uint16_t>(FIFO_DEPTH);
    fifo_csc_cse = new sc_core::sc_fifo<std::uint8_t>(FIFO_DEPTH);
    fifo_cse_sharpen = new sc_core::sc_fifo<std::uint8_t>(FIFO_DEPTH);
    fifo_sharpen_2dnr = new sc_core::sc_fifo<std::uint8_t>(FIFO_DEPTH);
    fifo_2dnr_scale = new sc_core::sc_fifo<std::uint8_t>(FIFO_DEPTH);
    fifo_scale_yuv420 = new sc_core::sc_fifo<std::uint8_t>(FIFO_DEPTH);
    fifo_2dnr_yuv420 = new sc_core::sc_fifo<std::uint8_t>(FIFO_DEPTH);

    // RAW domain processing
    m_input_norm = new sc_input_normalizer("input_norm", m_input_bit_depth, m_bit_depth, &m_hw_params);
    m_blc = new sc_blc("blc", m_cfg.blc, m_bayer_pattern, m_bit_depth, m_width, m_height, &m_hw_params);
    m_dpc = new sc_dpc("dpc", m_cfg.dpc, m_width, m_height, &m_hw_params);
    m_lsc = new sc_lsc("lsc", m_cfg.lsc, m_lsc_lut, m_bayer_pattern, m_bit_depth, m_width, m_height, &m_hw_params);
    m_dg = new sc_dg("dg", m_cfg.dg, m_bit_depth, &m_hw_params);
    m_bnr = new sc_bnr("bnr", m_cfg.bnr, m_bayer_pattern, m_bit_depth, m_width, m_height, &m_hw_params);

    // RGB domain processing
    m_demosaic = new sc_demosaic("demosaic", m_cfg.demosaic, m_bayer_pattern, m_bit_depth, m_width, m_height, &m_hw_params);
    m_awb = new sc_awb("awb", m_cfg.awb, m_width, m_height, m_bit_depth);
    m_wb = new sc_wb("wb", m_cfg.wb, &m_hw_params);
    m_ccm = new sc_ccm("ccm", m_cfg.ccm, &m_hw_params);
    m_gc = new sc_gc("gc", m_cfg.gc, &m_hw_params);
    m_aec = new sc_aec("aec", m_cfg.aec, m_width, m_height, m_bit_depth);
    m_csc = new sc_csc("csc", m_cfg.csc, &m_hw_params);

    // YUV domain processing
    m_cse = new sc_cse("cse", m_cfg.cse, &m_hw_params);
    m_sharpen = new sc_sharpen("sharpen", m_cfg.sharpen, m_width, m_height, &m_hw_params);
    m_2dnr = new sc_2dnr("2dnr", m_cfg.twodnr, m_width, m_height, &m_hw_params);
    if (m_cfg.scale.is_enable) {
        m_scale = new sc_scale("scale", m_cfg.scale, m_width, m_height, &m_hw_params);
        m_yuv420 = new sc_yuv420("yuv420", m_cfg.yuv420,
                                 m_cfg.scale.out_width, m_cfg.scale.out_height, &m_hw_params);
    } else {
        m_scale = nullptr;
        m_yuv420 = new sc_yuv420("yuv420", m_cfg.yuv420, m_width, m_height, &m_hw_params);
    }

    // Inject hardware params into every block that supports it
    // (each block exposes m_metrics which exposes set_hw)
    if (enable_metrics) {
        m_input_norm->m_metrics.set_hw(&m_hw_params);
        m_blc->m_metrics.set_hw(&m_hw_params);
        m_dpc->m_metrics.set_hw(&m_hw_params);
        m_lsc->m_metrics.set_hw(&m_hw_params);
        m_dg->m_metrics.set_hw(&m_hw_params);
        m_bnr->m_metrics.set_hw(&m_hw_params);
        m_demosaic->m_metrics.set_hw(&m_hw_params);
        m_awb->m_metrics.set_hw(&m_hw_params);
        m_wb->m_metrics.set_hw(&m_hw_params);
        m_ccm->m_metrics.set_hw(&m_hw_params);
        m_gc->m_metrics.set_hw(&m_hw_params);
        m_aec->m_metrics.set_hw(&m_hw_params);
        m_csc->m_metrics.set_hw(&m_hw_params);
        m_cse->m_metrics.set_hw(&m_hw_params);
        m_sharpen->m_metrics.set_hw(&m_hw_params);
        m_2dnr->m_metrics.set_hw(&m_hw_params);
        if (m_scale) m_scale->m_metrics.set_hw(&m_hw_params);
        m_yuv420->m_metrics.set_hw(&m_hw_params);

        // Register block nodes for aggregation
        setup_block_metrics();
    }
}

void sc_isp_pipeline::setup_block_metrics() {
    // RAW domain blocks (uint16_t)
    m_block_nodes.push_back(BlockNode(
        "normalizer",
        &m_input_norm->m_metrics,
        &dump_block_u16, &count_block_u16,
        &mean_latency_block_u16, &min_latency_block_u16, &max_latency_block_u16,
        &stddev_block_u16, &throughput_block_u16
    ));
    m_block_nodes.push_back(BlockNode(
        "blc",
        &m_blc->m_metrics,
        &dump_block_u16, &count_block_u16,
        &mean_latency_block_u16, &min_latency_block_u16, &max_latency_block_u16,
        &stddev_block_u16, &throughput_block_u16
    ));
    m_block_nodes.push_back(BlockNode(
        "dpc",
        &m_dpc->m_metrics,
        &dump_block_u16, &count_block_u16,
        &mean_latency_block_u16, &min_latency_block_u16, &max_latency_block_u16,
        &stddev_block_u16, &throughput_block_u16
    ));
    m_block_nodes.push_back(BlockNode(
        "lsc",
        &m_lsc->m_metrics,
        &dump_block_u16, &count_block_u16,
        &mean_latency_block_u16, &min_latency_block_u16, &max_latency_block_u16,
        &stddev_block_u16, &throughput_block_u16
    ));
    m_block_nodes.push_back(BlockNode(
        "dg",
        &m_dg->m_metrics,
        &dump_block_u16, &count_block_u16,
        &mean_latency_block_u16, &min_latency_block_u16, &max_latency_block_u16,
        &stddev_block_u16, &throughput_block_u16
    ));
    m_block_nodes.push_back(BlockNode(
        "bnr",
        &m_bnr->m_metrics,
        &dump_block_u16, &count_block_u16,
        &mean_latency_block_u16, &min_latency_block_u16, &max_latency_block_u16,
        &stddev_block_u16, &throughput_block_u16
    ));
    m_block_nodes.push_back(BlockNode(
        "demosaic",
        &m_demosaic->m_metrics,
        &dump_block_u16, &count_block_u16,
        &mean_latency_block_u16, &min_latency_block_u16, &max_latency_block_u16,
        &stddev_block_u16, &throughput_block_u16
    ));
    m_block_nodes.push_back(BlockNode(
        "awb",
        &m_awb->m_metrics,
        &dump_block_u16, &count_block_u16,
        &mean_latency_block_u16, &min_latency_block_u16, &max_latency_block_u16,
        &stddev_block_u16, &throughput_block_u16
    ));
    m_block_nodes.push_back(BlockNode(
        "wb",
        &m_wb->m_metrics,
        &dump_block_u16, &count_block_u16,
        &mean_latency_block_u16, &min_latency_block_u16, &max_latency_block_u16,
        &stddev_block_u16, &throughput_block_u16
    ));
    m_block_nodes.push_back(BlockNode(
        "ccm",
        &m_ccm->m_metrics,
        &dump_block_u16, &count_block_u16,
        &mean_latency_block_u16, &min_latency_block_u16, &max_latency_block_u16,
        &stddev_block_u16, &throughput_block_u16
    ));
    m_block_nodes.push_back(BlockNode(
        "gc",
        &m_gc->m_metrics,
        &dump_block_u16, &count_block_u16,
        &mean_latency_block_u16, &min_latency_block_u16, &max_latency_block_u16,
        &stddev_block_u16, &throughput_block_u16
    ));
    m_block_nodes.push_back(BlockNode(
        "aec",
        &m_aec->m_metrics,
        &dump_block_u16, &count_block_u16,
        &mean_latency_block_u16, &min_latency_block_u16, &max_latency_block_u16,
        &stddev_block_u16, &throughput_block_u16
    ));
    // YUV domain blocks (uint8_t)
    m_block_nodes.push_back(BlockNode(
        "csc",
        &m_csc->m_metrics,
        &dump_block_u8, &count_block_u8,
        &mean_latency_block_u8, &min_latency_block_u8, &max_latency_block_u8,
        &stddev_block_u8, &throughput_block_u8
    ));
    m_block_nodes.push_back(BlockNode(
        "cse",
        &m_cse->m_metrics,
        &dump_block_u8, &count_block_u8,
        &mean_latency_block_u8, &min_latency_block_u8, &max_latency_block_u8,
        &stddev_block_u8, &throughput_block_u8
    ));
    m_block_nodes.push_back(BlockNode(
        "sharpen",
        &m_sharpen->m_metrics,
        &dump_block_u8, &count_block_u8,
        &mean_latency_block_u8, &min_latency_block_u8, &max_latency_block_u8,
        &stddev_block_u8, &throughput_block_u8
    ));
    m_block_nodes.push_back(BlockNode(
        "2dnr",
        &m_2dnr->m_metrics,
        &dump_block_u8, &count_block_u8,
        &mean_latency_block_u8, &min_latency_block_u8, &max_latency_block_u8,
        &stddev_block_u8, &throughput_block_u8
    ));
    if (m_scale) {
        m_block_nodes.push_back(BlockNode(
            "scale",
            &m_scale->m_metrics,
            &dump_block_u8, &count_block_u8,
            &mean_latency_block_u8, &min_latency_block_u8, &max_latency_block_u8,
            &stddev_block_u8, &throughput_block_u8
        ));
    }
    m_block_nodes.push_back(BlockNode(
        "yuv420",
        &m_yuv420->m_metrics,
        &dump_block_u8, &count_block_u8,
        &mean_latency_block_u8, &min_latency_block_u8, &max_latency_block_u8,
        &stddev_block_u8, &throughput_block_u8
    ));
}

void sc_isp_pipeline::bind_channels() {
    const bool m_on     = enable_metrics;
    const auto& skip    = metrics_skip_blocks;

    // ------------------------------------------------------------------------
    // Each boundary: connect PROD's fifo_out and CONS's fifo_in. When
    // metrics are enabled the macros inject wrappers; when disabled
    // they connect PROD -> OUT_FIFO -> CONS directly.
    // ------------------------------------------------------------------------
    m_input_norm->fifo_in(*raw_in);
    if (m_on) {
        WRAP_U16("input_norm_to_blc", m_input_norm, m_blc, fifo_in_norm, true);
    } else {
        m_input_norm->fifo_out(*fifo_in_norm);
        m_blc->fifo_in(*fifo_in_norm);
    }

    if (m_on) {
        WRAP_U16("blc_to_dpc", m_blc, m_dpc, fifo_blc_dpc, true);
    } else {
        m_blc->fifo_out(*fifo_blc_dpc);
        m_dpc->fifo_in(*fifo_blc_dpc);
    }

    if (m_on) {
        WRAP_U16("dpc_to_lsc", m_dpc, m_lsc, fifo_dpc_lsc, true);
    } else {
        m_dpc->fifo_out(*fifo_dpc_lsc);
        m_lsc->fifo_in(*fifo_dpc_lsc);
    }

    if (m_on) {
        WRAP_U16("lsc_to_dg", m_lsc, m_dg, fifo_lsc_dg, true);
    } else {
        m_lsc->fifo_out(*fifo_lsc_dg);
        m_dg->fifo_in(*fifo_lsc_dg);
    }

    if (m_on) {
        WRAP_U16("dg_to_bnr", m_dg, m_bnr, fifo_dg_bnr, true);
    } else {
        m_dg->fifo_out(*fifo_dg_bnr);
        m_bnr->fifo_in(*fifo_dg_bnr);
    }

    if (m_on) {
        WRAP_U16("bnr_to_demosaic", m_bnr, m_demosaic, fifo_bnr_demosaic, true);
    } else {
        m_bnr->fifo_out(*fifo_bnr_demosaic);
        m_demosaic->fifo_in(*fifo_bnr_demosaic);
    }

    // ------------------------------------------------------------------------
    // RGB domain — demosaic outputs to fifo_demosaic_awb; AWB pass-through.
    // ------------------------------------------------------------------------
    if (m_on) {
        WRAP_U16("demosaic_to_awb", m_demosaic, m_awb, fifo_demosaic_awb, true);
    } else {
        m_demosaic->fifo_out(*fifo_demosaic_awb);
        m_awb->fifo_in(*fifo_demosaic_awb);
    }

    if (m_on) {
        WRAP_U16("awb_to_wb", m_awb, m_wb, fifo_demosaic_wb, true);
    } else {
        m_awb->fifo_out(*fifo_demosaic_wb);  // AWB is pass-through
        m_wb->fifo_in(*fifo_demosaic_wb);
    }

    m_wb->bind_awb(m_awb);

    if (m_on) {
        WRAP_U16("wb_to_ccm", m_wb, m_ccm, fifo_wb_ccm, true);
    } else {
        m_wb->fifo_out(*fifo_wb_ccm);
        m_ccm->fifo_in(*fifo_wb_ccm);
    }

    if (m_on) {
        WRAP_U16("ccm_to_gc", m_ccm, m_gc, fifo_ccm_gc, true);
    } else {
        m_ccm->fifo_out(*fifo_ccm_gc);
        m_gc->fifo_in(*fifo_ccm_gc);
    }

    if (m_on) {
        WRAP_U16("gc_to_aec", m_gc, m_aec, fifo_gc_aec, true);
    } else {
        m_gc->fifo_out(*fifo_gc_aec);
        m_aec->fifo_in(*fifo_gc_aec);
    }

    if (m_on) {
        WRAP_U16("aec_to_csc", m_aec, m_csc, fifo_aec_csc, true);
    } else {
        m_aec->fifo_out(*fifo_aec_csc);
        m_csc->fifo_in(*fifo_aec_csc);
    }

    // ------------------------------------------------------------------------
    // YUV444 domain (uint8_t)
    // ------------------------------------------------------------------------
    if (m_on) {
        WRAP_U8("csc_to_cse", m_csc, m_cse, fifo_csc_cse, true);
    } else {
        m_csc->fifo_out(*fifo_csc_cse);
        m_cse->fifo_in(*fifo_csc_cse);
    }

    if (m_on) {
        WRAP_U8("cse_to_sharpen", m_cse, m_sharpen, fifo_cse_sharpen, true);
    } else {
        m_cse->fifo_out(*fifo_cse_sharpen);
        m_sharpen->fifo_in(*fifo_cse_sharpen);
    }

    if (m_on) {
        WRAP_U8("sharpen_to_2dnr", m_sharpen, m_2dnr, fifo_sharpen_2dnr, true);
    } else {
        m_sharpen->fifo_out(*fifo_sharpen_2dnr);
        m_2dnr->fifo_in(*fifo_sharpen_2dnr);
    }

    if (m_cfg.scale.is_enable) {
        if (m_on) {
            WRAP_U8("2dnr_to_scale", m_2dnr, m_scale, fifo_2dnr_scale, true);
            WRAP_U8("scale_to_yuv420", m_scale, m_yuv420, fifo_scale_yuv420, true);
        } else {
            m_2dnr->fifo_out(*fifo_2dnr_scale);
            m_scale->fifo_in(*fifo_2dnr_scale);
            m_scale->fifo_out(*fifo_scale_yuv420);
            m_yuv420->fifo_in(*fifo_scale_yuv420);
        }
    } else {
        if (m_on) {
            WRAP_U8("2dnr_to_yuv420", m_2dnr, m_yuv420, fifo_2dnr_yuv420, true);
        } else {
            m_2dnr->fifo_out(*fifo_2dnr_yuv420);
            m_yuv420->fifo_in(*fifo_2dnr_yuv420);
        }
    }

    // Final output
    m_yuv420->fifo_out(*yuv_out);

    if (m_on) {
        std::cout << "[sc_isp_pipeline] Inserted " << m_wrapper_nodes.size()
                  << " boundary wrappers, registered " << m_block_nodes.size()
                  << " block metrics" << std::endl;
    }
}

void sc_isp_pipeline::setup_metrics() {
    // Kept for future per-test rerun support.
}

bool sc_isp_pipeline::dump_pipeline_metrics() const {
    if (m_wrapper_nodes.empty()) {
        std::cout << "[sc_isp_pipeline] No boundary wrappers were inserted "
                  << "(enable_metrics is false or set after construction)"
                  << std::endl;
        return false;
    }
    ensure_dir(metrics_output_dir);

    bool any = false;
    std::size_t total = 0;
    for (const auto& node : m_wrapper_nodes) {
        const std::string out = metrics_output_dir + "/" + node.label;
        node.dump_fn(node.wrapper_ptr, out);
        const std::size_t c = node.count_fn(node.wrapper_ptr);
        total += c;
        if (c > 0) any = true;
    }
    std::cout << "[sc_isp_pipeline] Dumped " << m_wrapper_nodes.size()
              << " boundary wrapper(s), total samples: " << total << std::endl;
    return any;
}

std::size_t sc_isp_pipeline::metrics_sample_count() const {
    std::size_t total = 0;
    for (const auto& node : m_wrapper_nodes) {
        total += node.count_fn(node.wrapper_ptr);
    }
    return total;
}

bool sc_isp_pipeline::dump_all_block_metrics() const {
    if (m_block_nodes.empty()) {
        std::cout << "[sc_isp_pipeline] No block metrics registered "
                  << "(enable_metrics is false or set after construction)"
                  << std::endl;
        return false;
    }
    ensure_dir(metrics_output_dir);

    bool any = false;
    std::size_t total = 0;
    for (const auto& node : m_block_nodes) {
        const std::string out = metrics_output_dir + "/block_" + node.label;
        node.dump_fn(node.block_metrics_ptr, out);
        const std::size_t c = node.count_fn(node.block_metrics_ptr);
        total += c;
        if (c > 0) any = true;
    }
    std::cout << "[sc_isp_pipeline] Dumped " << m_block_nodes.size()
              << " block metric(s), total samples: " << total << std::endl;
    return any;
}

std::size_t sc_isp_pipeline::block_metrics_sample_count() const {
    std::size_t total = 0;
    for (const auto& node : m_block_nodes) {
        total += node.count_fn(node.block_metrics_ptr);
    }
    return total;
}

void sc_isp_pipeline::print_metrics_summary() const {
    std::cout << "\n============================================================\n";
    std::cout << "         ISP PIPELINE METRICS SUMMARY\n";
    std::cout << "============================================================\n";

    // Frame timing
    const sc_time frame_time = get_frame_time();
    std::cout << "\n--- Frame Timing ---\n";
    std::cout << "  Frame size         : " << m_width << "x" << m_height << " ("
              << (m_width * m_height) << " RAW pixels)\n";
    std::cout << "  Frame time (sim)  : " << std::fixed << std::setprecision(3)
              << get_frame_time_us() << " us\n";
    std::cout << "  Sim timestamp     : " << sc_time_stamp().to_double() / 1e-9
              << " ns\n";

    // Hardware params
    std::cout << "\n--- Hardware Parameters ---\n";
    std::cout << m_hw_params.to_string();

    // Per-block latency
    if (!m_block_nodes.empty()) {
        std::cout << "\n--- Block Processing Latency (per processing unit) ---\n";
        std::cout << std::setw(12) << "block"
                  << std::setw(10) << "samples"
                  << std::setw(14) << "mean_ns"
                  << std::setw(14) << "min_ns"
                  << std::setw(14) << "max_ns"
                  << std::setw(14) << "stddev_ns"
                  << std::setw(14) << "tok/s"
                  << "\n";
        std::cout << std::string(92, '-') << "\n";

        for (const auto& node : m_block_nodes) {
            const sc_time mean = node.mean_latency_fn(node.block_metrics_ptr);
            const sc_time min  = node.min_latency_fn(node.block_metrics_ptr);
            const sc_time max  = node.max_latency_fn(node.block_metrics_ptr);
            const double stddev = node.stddev_fn(node.block_metrics_ptr);
            const float  tp    = node.throughput_fn(node.block_metrics_ptr);
            const std::size_t n = node.count_fn(node.block_metrics_ptr);

            std::cout << std::setw(12) << node.label
                      << std::setw(10) << n
                      << std::setw(14) << std::setprecision(3) << mean.to_double() / 1e-9
                      << std::setw(14) << min.to_double() / 1e-9
                      << std::setw(14) << max.to_double() / 1e-9
                      << std::setw(14) << std::setprecision(3) << stddev
                      << std::setw(14) << (tp > 0 ? std::to_string(tp) : "inf")
                      << "\n";
        }
    }

    // Per-boundary throughput
    if (!m_wrapper_nodes.empty()) {
        std::cout << "\n--- Boundary Throughput (inter-block tokens/s) ---\n";
        std::cout << std::setw(30) << "boundary"
                  << std::setw(12) << "samples"
                  << std::setw(16) << "tok/s"
                  << "\n";
        std::cout << std::string(60, '-') << "\n";

        for (const auto& node : m_wrapper_nodes) {
            const std::size_t n = node.count_fn(node.wrapper_ptr);
            (void)node;  // silence unused warning
            // Throughput is computed per-boundary in its own CSV;
            // we only print the label and count here.
            std::cout << std::setw(30) << node.label
                      << std::setw(12) << n
                      << std::setw(16) << "(see CSV)"
                      << "\n";
        }
    }

    std::cout << "\n============================================================\n";
    std::cout << "Metrics files: " << metrics_output_dir << "/\n";
    std::cout << "  block_*.csv         — per-block latency CSV\n";
    std::cout << "  block_*_summary.txt — per-block text summary\n";
    std::cout << "  *_*_to_*.csv        — boundary throughput CSV\n";
    std::cout << "  *_to_*_summary.txt  — boundary text summary\n";
    std::cout << "============================================================\n\n";
}

double sc_isp_pipeline::estimate_frame_time_us() const {
    // Estimate based on measured block throughput.
    // Sum the inverse of each block's throughput (time per pixel).
    double total_time_us = 0.0;
    for (const auto& node : m_block_nodes) {
        const float tp = node.throughput_fn(node.block_metrics_ptr);
        if (tp > 0.0f) {
            total_time_us += 1.0 / static_cast<double>(tp) * 1e6;
        }
    }
    return total_time_us;
}

double sc_isp_pipeline::estimate_fps() const {
    const double frame_us = estimate_frame_time_us();
    if (frame_us <= 0.0) return 0.0;
    return 1e6 / frame_us;
}

float sc_isp_pipeline::get_awb_r_gain() const {
    return m_awb ? m_awb->get_r_gain() : 1.0f;
}

float sc_isp_pipeline::get_awb_b_gain() const {
    return m_awb ? m_awb->get_b_gain() : 1.0f;
}

std::int32_t sc_isp_pipeline::get_aec_feedback() const {
    return m_aec ? m_aec->get_ae_feedback() : 0;
}

void sc_isp_pipeline::precompute_awb_gains(const std::uint16_t* rgb12) {
    if (m_awb == nullptr) return;
    m_awb->precompute_gains(rgb12, awb_input_pixels());
}

void sc_isp_pipeline::precompute_awb_gains_from_bayer(const std::uint16_t* bayer16) {
    if (m_awb == nullptr) return;
    m_awb->set_bayer_pattern(m_bayer_pattern);
    m_awb->set_input_bit_depth(m_input_bit_depth);
    m_awb->precompute_gains_from_bayer(bayer16, awb_input_pixels());
}

void sc_isp_pipeline::prime_awb_gains(float r_gain, float b_gain) {
    if (m_awb == nullptr) return;
    m_awb->prime_gains(r_gain, b_gain);
}

// ============================================================================
// Phase 5: Architecture Metrics Collector
// ============================================================================
void sc_isp_pipeline::enable_arch_metrics(const std::string& output_dir) {
    m_arch_metrics_enabled = true;
    m_arch_metrics = arch_metrics_collector(output_dir);

    // Start first frame tracking
    m_arch_metrics.start_frame(0, m_width, m_height);
    m_arch_metrics.record_first_input(sc_time_stamp());

    std::cout << "[sc_isp_pipeline] Architecture metrics enabled, output: "
              << output_dir << std::endl;
}

void sc_isp_pipeline::dump_arch_metrics() const {
    if (!m_arch_metrics_enabled) {
        std::cout << "[sc_isp_pipeline] Architecture metrics not enabled" << std::endl;
        return;
    }

    std::cout << "\n============================================================\n";
    std::cout << "         ARCHITECTURE METRICS REPORT\n";
    std::cout << "============================================================\n";

    // Frame summary
    const auto& frames = m_arch_metrics.frames();
    if (!frames.empty()) {
        const auto& f = frames.back();
        std::cout << "\n--- Frame Summary ---\n";
        std::cout << "  Frame ID          : " << f.frame_id << "\n";
        std::cout << "  Resolution        : " << f.width << "x" << f.height << "\n";
        std::cout << "  Total pixels      : " << f.total_pixels << "\n";
        std::cout << "  Frame time (ns)   : " << std::fixed << std::setprecision(0)
                  << f.frame_time().to_double() / 1.0 << "\n";
        std::cout << "  FPS (est)         : " << std::fixed << std::setprecision(2)
                  << f.fps(200.0) << "\n";
    }

    // Bottleneck analysis
    bottleneck_report bottleneck = m_arch_metrics.analyze_bottleneck();
    std::cout << "\n--- Bottleneck Analysis ---\n";
    if (bottleneck.type == bottleneck_type::NONE) {
        std::cout << "  No significant bottleneck detected\n";
    } else {
        std::cout << "  Location           : " << bottleneck.location << "\n";
        std::cout << "  Type              : ";
        switch (bottleneck.type) {
            case bottleneck_type::INPUT_BANDWIDTH: std::cout << "Input Bandwidth Limited\n"; break;
            case bottleneck_type::COMPUTE_II: std::cout << "Compute II Limited\n"; break;
            case bottleneck_type::MEMORY_PORT: std::cout << "Memory Port Limited\n"; break;
            case bottleneck_type::DOWNSTREAM_BACKPRESSURE: std::cout << "Downstream Backpressure\n"; break;
            case bottleneck_type::OUTPUT_BANDWIDTH: std::cout << "Output Bandwidth Limited\n"; break;
            case bottleneck_type::FRAME_BARRIER: std::cout << "Frame Barrier Limited\n"; break;
            default: std::cout << "Unknown\n";
        }
        std::cout << "  Severity          : " << std::fixed << std::setprecision(1)
                  << (bottleneck.severity * 100.0) << "%\n";
    }

    // Block utilization summary
    std::cout << "\n--- Block Utilization ---\n";
    std::cout << std::setw(16) << "Block"
              << std::setw(12) << "Util %"
              << std::setw(12) << "Active"
              << std::setw(12) << "Starved"
              << std::setw(12) << "Blocked"
              << "\n";
    std::cout << std::string(64, '-') << "\n";

    const auto& blocks = m_arch_metrics.blocks();
    for (const auto& bm : blocks) {
        std::cout << std::setw(16) << bm.block_name
                  << std::setw(11) << std::fixed << std::setprecision(1)
                  << (bm.block_utilization * 100.0) << "%"
                  << std::setw(12) << bm.cycles.active_cycles
                  << std::setw(12) << bm.cycles.starved_cycles
                  << std::setw(12) << bm.cycles.blocked_cycles
                  << "\n";
    }

    std::cout << "\n============================================================\n";
    std::cout << "Detailed metrics written to: " << (m_arch_metrics_enabled ? "arch_metrics/" : "N/A") << "\n";
    std::cout << "============================================================\n";
}

void sc_isp_pipeline::dump_arch_summary() const {
    if (!m_arch_metrics_enabled) return;

    // Dump CSV files
    m_arch_metrics.dump_all();

    std::cout << "[sc_isp_pipeline] Architecture metrics dumped to arch_metrics/" << std::endl;
}

void sc_isp_pipeline::collect_block_metrics() {
    if (!m_arch_metrics_enabled) return;

    // Collect metrics from each block that has architecture metrics
    // This should be called at the end of each frame

    auto update_block = [&](const std::string& name, auto* block_ptr) {
        if (block_ptr) {
            block_cycle_counters cycles;
            cycles.active_cycles = block_ptr->active_cycles();
            cycles.starved_cycles = block_ptr->starved_cycles();
            cycles.blocked_cycles = 0;  // Not tracked in current blocks
            m_arch_metrics.update_block_cycles(name, cycles);
        }
    };

    // RAW domain
    update_block("normalizer", m_input_norm);
    update_block("blc", m_blc);
    update_block("dpc", m_dpc);
    update_block("lsc", m_lsc);
    update_block("dg", m_dg);
    update_block("bnr", m_bnr);

    // RGB domain
    update_block("demosaic", m_demosaic);
    update_block("awb", m_awb);
    update_block("wb", m_wb);
    update_block("ccm", m_ccm);
    update_block("gc", m_gc);
    update_block("aec", m_aec);

    // YUV domain
    update_block("csc", m_csc);
    update_block("cse", m_cse);
    update_block("sharpen", m_sharpen);
    update_block("2dnr", m_2dnr);
    if (m_scale) update_block("scale", m_scale);
    update_block("yuv420", m_yuv420);
}

void sc_isp_pipeline::update_arch_block_metrics(const std::string& block_name,
                                               std::uint64_t active_cycles,
                                               std::uint64_t starved_cycles,
                                               std::uint64_t blocked_cycles) {
    if (!m_arch_metrics_enabled) return;

    block_cycle_counters cycles;
    cycles.active_cycles = active_cycles;
    cycles.starved_cycles = starved_cycles;
    cycles.blocked_cycles = blocked_cycles;

    m_arch_metrics.update_block_cycles(block_name, cycles);
}

void sc_isp_pipeline::update_arch_frame_timing(std::uint64_t frame_id) {
    if (!m_arch_metrics_enabled) return;

    m_arch_metrics.record_last_output(sc_time_stamp());
    m_arch_metrics.end_frame();

    // Start next frame tracking
    m_arch_metrics.start_frame(frame_id + 1, m_width, m_height);
    m_arch_metrics.record_first_input(sc_time_stamp());
}
