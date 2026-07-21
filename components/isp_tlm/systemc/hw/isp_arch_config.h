#ifndef ISP_ARCH_CONFIG_H
#define ISP_ARCH_CONFIG_H

#include <array>
#include <cstddef>
#include <cstdint>

// Configuration for the internal, line-granular timing model.  These values
// describe the ISP itself; external buses, DMA, and tracing are intentionally
// not represented here.
namespace isp_blocks {
constexpr std::size_t COUNT = 18;

enum BlockId : std::size_t {
    INPUT_NORMALIZER = 0, BLC, DPC, LSC, DG, BNR, DEMOSAIC, AWB, WB, CCM,
    GC, AEC, CSC, CSE, SHARPEN, TWO_DNR, SCALE, YUV420,
    PROCESSING_BLOCKS = 17
};

constexpr const char* BLOCK_NAMES[] = {
    "normalizer", "blc", "dpc", "lsc", "dg", "bnr", "demosaic",
    "awb", "wb", "ccm", "gc", "aec", "csc", "cse", "sharpen",
    "2dnr", "scale", "yuv420"
};
} // namespace isp_blocks

namespace isp_links {
constexpr std::size_t COUNT = isp_blocks::PROCESSING_BLOCKS + 1;

enum LinkId : std::size_t {
    INPUT_NORM_TO_BLC = 0, BLC_TO_DPC, DPC_TO_LSC, LSC_TO_DG, DG_TO_BNR,
    BNR_TO_DEMOSAIC, DEMOSAIC_TO_AWB, AWB_TO_WB, WB_TO_CCM, CCM_TO_GC,
    GC_TO_AEC, AEC_TO_CSC, CSC_TO_CSE, CSE_TO_SHARPEN, SHARPEN_TO_2DNR,
    _2DNR_TO_SCALE_OR_YUV420, SCALE_TO_YUV420, YUV420_TO_OUTPUT
};

constexpr const char* LINK_NAMES[] = {
    "input_norm_to_blc", "blc_to_dpc", "dpc_to_lsc", "lsc_to_dg",
    "dg_to_bnr", "bnr_to_demosaic", "demosaic_to_awb", "awb_to_wb",
    "wb_to_ccm", "ccm_to_gc", "gc_to_aec", "aec_to_csc",
    "csc_to_cse", "cse_to_sharpen", "sharpen_to_2dnr",
    "2dnr_to_scale_or_yuv420", "scale_to_yuv420", "yuv420_to_output"
};
} // namespace isp_links

enum class metric_provenance : std::uint8_t { unavailable, modeled, measured };

struct workload_profile {
    bool available = false;
    metric_provenance provenance = metric_provenance::unavailable;
    std::uint32_t additions_per_pixel = 0;
    std::uint32_t multiplications_per_pixel = 0;
    std::uint32_t comparisons_per_pixel = 0;
    std::uint32_t reads_per_pixel = 0;
    std::uint32_t writes_per_pixel = 0;

    bool is_valid() const noexcept {
        return !available || provenance == metric_provenance::modeled;
    }
};

struct local_memory_service_profile {
    bool available = false;
    metric_provenance provenance = metric_provenance::unavailable;
    std::uint32_t read_ports = 0;
    std::uint32_t write_ports = 0;
    std::uint32_t access_cycles = 0;

    bool is_valid() const noexcept {
        return !available ||
               (provenance == metric_provenance::modeled && access_cycles > 0);
    }
};

struct stream_link_config {
    std::uint32_t depth = 1024;
    bool is_valid() const noexcept { return depth > 0; }
};

struct block_arch_config {
    std::uint32_t pixels_per_cycle = 1;
    std::uint32_t pixel_initiation_interval_cycles = 1;
    std::uint32_t pipeline_latency_cycles = 1;
    std::uint32_t max_in_flight_lines = 1;
    workload_profile workload{};
    local_memory_service_profile local_memory{};

    bool is_valid() const noexcept {
        return pixels_per_cycle > 0 && pixel_initiation_interval_cycles > 0 &&
               pipeline_latency_cycles > 0 && max_in_flight_lines > 0 &&
               workload.is_valid() && local_memory.is_valid();
    }
};

struct isp_arch_config {
    // The model has one authoritative clock source for every block and link.
    float clock_freq_mhz = 200.0f;
    std::array<block_arch_config, isp_blocks::COUNT> blocks{};
    std::array<stream_link_config, isp_links::COUNT> links{};
    static block_arch_config default_point_op() {
        block_arch_config result;
        result.max_in_flight_lines = 2;
        return result;
    }

    static block_arch_config default_spatial() {
        block_arch_config result = default_point_op();
        result.max_in_flight_lines = 6;
        return result;
    }

    static block_arch_config default_frame() {
        block_arch_config result = default_point_op();
        result.max_in_flight_lines = 2;
        return result;
    }

    static block_arch_config default_rate_change() {
        block_arch_config result = default_point_op();
        result.pipeline_latency_cycles = 2;
        result.max_in_flight_lines = 2;
        return result;
    }
    void init_defaults() {
        for (auto& block : blocks) {
            block = default_point_op();
        }
        blocks[isp_blocks::DPC] = default_spatial();
        blocks[isp_blocks::BNR] = default_spatial();
        blocks[isp_blocks::DEMOSAIC] = default_spatial();
        blocks[isp_blocks::SHARPEN] = default_spatial();
        blocks[isp_blocks::TWO_DNR] = default_spatial();
        blocks[isp_blocks::AWB] = default_frame();
        blocks[isp_blocks::AEC] = default_frame();
        blocks[isp_blocks::SCALE] = default_rate_change();
        blocks[isp_blocks::YUV420] = default_rate_change();
        for (auto& link : links) {
            link = stream_link_config{};
        }
    }

    float cycle_ns() const noexcept {
        return clock_freq_mhz > 0.0f ? 1000.0f / clock_freq_mhz : 0.0f;
    }

    bool is_valid() const noexcept {
        if (clock_freq_mhz <= 0.0f) {
            return false;
        }
        for (const auto& block : blocks) {
            if (!block.is_valid()) {
                return false;
            }
        }
        for (const auto& link : links) {
            if (!link.is_valid()) {
                return false;
            }
        }
        return true;
    }
};

#endif // ISP_ARCH_CONFIG_H
