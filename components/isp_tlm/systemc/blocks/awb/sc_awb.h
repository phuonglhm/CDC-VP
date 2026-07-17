/**
 * @file sc_awb.h
 * @brief SystemC Streaming Module for Auto White Balance (AWB)
 *
 * Computes white balance statistics over the frame and outputs R/B gains.
 * Acts as a pass-through for RGB data while computing statistics.
 */
#ifndef SC_AWB_H
#define SC_AWB_H

#include <systemc>
using namespace sc_core;

#include <systemc>
#include <cstdint>
#include <vector>

#include "../../../blocks/awb/include/awb.h"
#include "../../../core/include/isp_types.h"
#include "../../tb_utils/sc_block_metrics.h"
#include "../../hw/isp_arch_config.h"

SC_MODULE(sc_awb) {
public:
    sc_core::sc_port<sc_fifo_in_if<std::uint16_t>> fifo_in;
    sc_core::sc_port<sc_fifo_out_if<std::uint16_t>> fifo_out;

    // Optional clock port for timed mode (pointer for optional binding)
    sc_in<bool>* clk = nullptr;

    SC_HAS_PROCESS(sc_awb);

    sc_awb(sc_core::sc_module_name name,
            const awb_config& cfg,
            std::uint32_t width,
            std::uint32_t height,
            std::uint8_t bit_depth,
            const hw_params* hw = nullptr)
        : sc_module(name), m_cfg(cfg), m_width(width), m_height(height),
          m_bit_depth(bit_depth), m_r_gain(1.0f), m_b_gain(1.0f),
          m_bayer_pattern(cfa_types::RGGB), m_input_bit_depth(bit_depth),
          m_hw(hw), m_metrics("awb") {
        SC_THREAD(process_stream);
    }

    sc_block_metrics<std::uint16_t> m_metrics;

    // Hardware configuration
    void set_hw_params(const hw_params* hw) { m_hw = hw; }

    float get_r_gain() const { return m_r_gain; }
    float get_b_gain() const { return m_b_gain; }

    // Latched gains: updated at end-of-frame, safe for downstream blocks to read
    float get_latched_r_gain() const { return m_latched_r_gain; }
    float get_latched_b_gain() const { return m_latched_b_gain; }

    // Pre-compute the R/B gains from a raw Bayer buffer (single-channel
    // 16-bit values). The buffer is interpreted as a mosaic in the CFA
    // pattern given by `m_bayer_pattern` (set externally via
    // `set_bayer_pattern`). This is a fast approximation sufficient for
    // estimating the white balance of the scene and lets the testbench
    // prime the AWB without running the full demosaic pipeline.
    //
    // Returns true if gains were computed, false on bad input.
    bool precompute_gains_from_bayer(const std::uint16_t* bayer,
                                     std::size_t pixels);

    // Pre-compute the R/B gains from a raw RGB buffer (RGB interleaved,
    // 12-bit values). This allows the testbench to prime the AWB before
    // the streaming pipeline starts so the first frame already has
    // proper colour balance. Returns the computed (r_gain, b_gain).
    void precompute_gains(const std::uint16_t* rgb, std::size_t pixels);

    // Set the Bayer pattern used by `precompute_gains_from_bayer`. Must
    // be called before that function.
    void set_bayer_pattern(cfa_types p) { m_bayer_pattern = p; }

    // Set the input bit-depth (typically 16) used by
    // `precompute_gains_from_bayer`. The Bayer input is rescaled to the
    // pipeline's working bit-depth (12) so the over/under exposure
    // thresholds are in the right range.
    void set_input_bit_depth(std::uint8_t bd) { m_input_bit_depth = bd; }

    // Directly set the AWB-computed R/B gains. Use this when the
    // reference pipeline has already produced the gains and you want
    // the SystemC pipeline to apply them on the very first frame
    // (the streaming AWB would otherwise need a whole frame's worth
    // of input before its gains become available).
    void prime_gains(float r_gain, float b_gain) {
        m_r_gain = r_gain;
        m_b_gain = b_gain;
    }

    // Architecture metrics (for collect_block_metrics)
    std::uint64_t active_cycles() const { return m_active_cycles; }
    std::uint64_t starved_cycles() const { return m_starved_cycles; }
    std::uint64_t total_cycles() const { return m_cycle_count; }
    double utilization() const {
        return (m_cycle_count > 0) ?
            static_cast<double>(m_active_cycles) / m_cycle_count : 0.0;
    }

    void reset_counters() {
        m_active_cycles = 0;
        m_starved_cycles = 0;
        m_cycle_count = 0;
    }

private:
    void process_stream();

    const awb_config& m_cfg;
    std::uint32_t m_width;
    std::uint32_t m_height;
    std::uint8_t m_bit_depth;

    float m_r_gain;
    float m_b_gain;
    float m_latched_r_gain = 1.0f;  // Latched at end-of-frame, safe to read
    float m_latched_b_gain = 1.0f;   // Latched at end-of-frame, safe to read
    cfa_types m_bayer_pattern;
    std::uint8_t m_input_bit_depth;

    // Hardware parameters
    const hw_params* m_hw;

    // Architecture metrics
    std::uint64_t m_active_cycles = 0;
    std::uint64_t m_starved_cycles = 0;
    std::uint64_t m_cycle_count = 0;
};

#endif  // SC_AWB_H
