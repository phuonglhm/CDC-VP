/**
 * @file isp_arch_config.h
 * @brief Hardware architecture configuration for ISP pipeline
 *
 * Provides configuration groups for:
 *   - Stream link (FIFO) parameters
 *   - Block-level hardware parameters (latency, parallelism, memory)
 *   - Bus architecture
 *   - Global ISP architecture settings
 *
 * Every architecture parameter must change an observable metric or trace.
 * Architecture-only parameters must NOT change output pixels.
 */

#ifndef ISP_ARCH_CONFIG_H
#define ISP_ARCH_CONFIG_H

#include <cstdint>
#include <array>
#include <string>

// ============================================================================
// Block count (must match actual pipeline)
// ============================================================================
namespace isp_blocks {
    constexpr std::size_t COUNT = 18;

    // Block indices - must match sc_isp_pipeline instantiation order
    enum BlockId : std::size_t {
        INPUT_NORMALIZER = 0,
        BLC = 1,
        DPC = 2,
        LSC = 3,
        DG = 4,
        BNR = 5,
        DEMOSAIC = 6,
        AWB = 7,
        WB = 8,
        CCM = 9,
        GC = 10,
        AEC = 11,
        CSC = 12,
        CSE = 13,
        SHARPEN = 14,
        TWO_DNR = 15,
        SCALE = 16,
        YUV420 = 17,
        // Count excluding normalizer and yuv420 (not in pipeline path for some configs)
        PROCESSING_BLOCKS = 17
    };

    constexpr const char* BLOCK_NAMES[] = {
        "normalizer", "blc", "dpc", "lsc", "dg", "bnr", "demosaic",
        "awb", "wb", "ccm", "gc", "aec", "csc", "cse", "sharpen",
        "2dnr", "scale", "yuv420"
    };
}

// Link count between blocks (PROCESSING_BLOCKS + 1 for input, + 1 for output)
namespace isp_links {
    constexpr std::size_t COUNT = isp_blocks::PROCESSING_BLOCKS + 1;  // 18 links

    enum LinkId : std::size_t {
        INPUT_NORM_TO_BLC = 0,
        BLC_TO_DPC = 1,
        DPC_TO_LSC = 2,
        LSC_TO_DG = 3,
        DG_TO_BNR = 4,
        BNR_TO_DEMOSAIC = 5,
        DEMOSAIC_TO_AWB = 6,
        AWB_TO_WB = 7,
        WB_TO_CCM = 8,
        CCM_TO_GC = 9,
        GC_TO_AEC = 10,
        AEC_TO_CSC = 11,
        CSC_TO_CSE = 12,
        CSE_TO_SHARPEN = 13,
        SHARPEN_TO_2DNR = 14,
        _2DNR_TO_SCALE_OR_YUV420 = 15,
        SCALE_TO_YUV420 = 16,
        YUV420_TO_OUTPUT = 17
    };

    constexpr const char* LINK_NAMES[] = {
        "input_norm_to_blc", "blc_to_dpc", "dpc_to_lsc", "lsc_to_dg",
        "dg_to_bnr", "bnr_to_demosaic", "demosaic_to_awb", "awb_to_wb",
        "wb_to_ccm", "ccm_to_gc", "gc_to_aec", "aec_to_csc",
        "csc_to_cse", "cse_to_sharpen", "sharpen_to_2dnr",
        "2dnr_to_scale_or_yuv420", "scale_to_yuv420", "yuv420_to_output"
    };
}

// ============================================================================
// Stream Link Configuration (FIFO between blocks)
// ============================================================================
struct stream_link_config {
    std::uint32_t depth = 1024;           // FIFO depth in entries
    std::uint32_t width_bits = 64;         // Physical data width (bits/cycle)
    std::uint32_t latency_cycles = 0;      // Combinational delay (cycles)
    std::uint32_t beats_per_cycle = 1;     // Number of tokens processed per cycle

    // Validation
    bool is_valid() const {
        return depth > 0 && width_bits > 0 && beats_per_cycle > 0;
    }
};

// ============================================================================
// Block Architecture Configuration
// ============================================================================
struct block_arch_config {
    // Parallelism
    std::uint32_t input_lanes = 1;         // Parallel input channels
    std::uint32_t output_lanes = 1;        // Parallel output channels
    std::uint32_t pixel_per_cycle = 1;      // Pixels processed per clock cycle

    // Timing
    std::uint32_t pipeline_latency = 1;    // Cycles from first input to first output
    std::uint32_t initiation_interval = 1;  // Cycles between successive outputs (II)

    // Line buffer (for spatial blocks)
    std::uint32_t line_buffer_rows = 0;    // Number of rows to buffer (0 = no buffering)
    std::uint32_t line_buffer_banks = 1;   // Memory banks for ping-pong

    // Memory ports
    std::uint32_t memory_read_ports = 1;    // Concurrent read ports
    std::uint32_t memory_write_ports = 1;   // Concurrent write ports
    std::uint32_t memory_latency_cycles = 1; // Memory access latency

    // Power
    bool clock_gating = false;              // Enable clock gating when idle

    // Validation
    bool is_valid() const {
        return input_lanes > 0 && output_lanes > 0 &&
               pipeline_latency > 0 && initiation_interval > 0 &&
               memory_read_ports > 0 && memory_write_ports > 0;
    }
};

// ============================================================================
// Bus Architecture Configuration
// ============================================================================
struct bus_arch_config {
    std::uint32_t data_width_bits = 64;    // Data bus width
    std::uint32_t address_width_bits = 32;  // Address bus width
    std::uint32_t burst_length = 1;         // Maximum burst length
    std::uint32_t max_outstanding = 4;      // Maximum outstanding transactions
    std::uint32_t read_latency_cycles = 1;  // Read data latency
    std::uint32_t write_latency_cycles = 1; // Write acknowledge latency
    std::uint32_t arbitration_cycles = 0;   // Cycles for arbitration decision

    // Validation
    bool is_valid() const {
        return data_width_bits > 0 && address_width_bits > 0 &&
               burst_length > 0 && max_outstanding > 0;
    }
};

// ============================================================================
// DMA Configuration
// ============================================================================
struct dma_arch_config {
    bool is_enable = true;                 // Enable DMA
    std::uint32_t bus_width_bits = 64;     // DMA bus width
    std::uint32_t max_burst_length = 16;   // Maximum burst
    std::uint32_t read_latency_cycles = 4; // Cycles for first data
    std::uint32_t throughput_tokens_per_cycle = 4; // Tokens per cycle sustained
    std::uint32_t max_outstanding = 4;      // Outstanding transactions
};

// ============================================================================
// Global ISP Architecture Configuration
// ============================================================================
struct isp_arch_config {
    // Clock
    float clock_freq_mhz = 200.0f;         // Operating frequency
    std::uint32_t clock_phase_ns = 0;       // Clock phase offset

    // Global settings
    bool enable_metrics = true;             // Collect architecture metrics
    bool enable_tracing = false;            // Enable VCD tracing
    std::string trace_file = "isp_trace.vcd"; // Trace output file

    // Block configurations
    std::array<block_arch_config, isp_blocks::COUNT> blocks;
    std::array<stream_link_config, isp_links::COUNT> links;

    // Bus configurations
    bus_arch_config input_bus;
    bus_arch_config output_bus;

    // DMA configurations
    dma_arch_config input_dma;
    dma_arch_config output_dma;

    // Default block configurations (per block type)
    static block_arch_config default_point_op() {
        return {1, 1, 1, 1, 1, 0, 1, 1, 1, 1, false};
    }

    static block_arch_config default_spatial() {
        return {1, 1, 1, 1, 1, 3, 2, 2, 2, 2, false};
    }

    static block_arch_config default_frame() {
        return {1, 1, 1, 1, 1, 0, 1, 2, 2, 4, false};
    }

    static block_arch_config default_rate_change() {
        return {1, 1, 1, 2, 1, 0, 1, 1, 1, 2, false};
    }

    // Initialize with sensible defaults
    void init_defaults() {
        // Point operations: normalizer, BLC, DG, LSC, WB, CCM, GC, CSC, CSE
        for (std::size_t i = 0; i < isp_blocks::COUNT; ++i) {
            blocks[i] = default_point_op();
        }

        // Override specific blocks
        blocks[isp_blocks::DPC] = default_spatial();
        blocks[isp_blocks::BNR] = default_spatial();
        blocks[isp_blocks::DEMOSAIC] = default_spatial();
        blocks[isp_blocks::SHARPEN] = default_spatial();
        blocks[isp_blocks::TWO_DNR] = default_spatial();

        // Statistical blocks
        blocks[isp_blocks::AWB] = default_frame();
        blocks[isp_blocks::AEC] = default_frame();

        // Rate-changing
        blocks[isp_blocks::SCALE] = default_rate_change();
        blocks[isp_blocks::YUV420] = default_rate_change();

        // Default link configs
        for (auto& link : links) {
            link = {1024, 64, 0, 1};
        }
    }

    // Compute cycle time from clock frequency
    float cycle_ns() const {
        return (clock_freq_mhz > 0.0f) ? (1000.0f / clock_freq_mhz) : 0.0f;
    }

    // Validation
    bool is_valid() const {
        if (clock_freq_mhz <= 0.0f) return false;
        for (const auto& blk : blocks) {
            if (!blk.is_valid()) return false;
        }
        for (const auto& lnk : links) {
            if (!lnk.is_valid()) return false;
        }
        return input_bus.is_valid() && output_bus.is_valid();
    }
};

#endif  // ISP_ARCH_CONFIG_H
