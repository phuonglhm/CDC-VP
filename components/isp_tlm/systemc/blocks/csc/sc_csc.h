/**
 * @file sc_csc.h
 * @brief SystemC Streaming Module for RGB to YUV Color Space Conversion (CSC)
 *
 * Implements RGB to YUV conversion using BT.601 or BT.709 coefficients.
 * Also handles bit depth reduction from 12-bit to 8-bit.
 * Streaming architecture with pixel-by-pixel processing.
 *
 * Hardware shell features (Phase 3):
 *   - Optional hw_params for timing
 *   - Configurable cycles per pixel
 *   - Architecture metrics
 */
#ifndef SC_CSC_H
#define SC_CSC_H

#include <systemc>
using namespace sc_core;

#include <systemc>
#include <cstdint>

#include "../../../blocks/rgb_to_yuv/include/rgb_to_yuv.h"
#include "../../tb_utils/hardware_params.h"

SC_MODULE(sc_csc) {
public:
    sc_core::sc_port<sc_fifo_in_if<std::uint16_t>> fifo_in;
    sc_core::sc_port<sc_fifo_out_if<std::uint8_t>> fifo_out;

    // Optional clock port for timed mode (pointer for optional binding)
    sc_in<bool>* clk = nullptr;

    SC_HAS_PROCESS(sc_csc);

    sc_csc(sc_core::sc_module_name name,
          const csc_config& cfg,
          const hw_params* hw = nullptr)
        : sc_module(name), m_cfg(cfg), m_hw(hw),
          m_cycles_per_pixel(2) {
        SC_THREAD(process_stream);
    }


    // Hardware configuration
    void set_hw_params(const hw_params* hw) { m_hw = hw; }
    void set_cycles_per_pixel(int cycles) { m_cycles_per_pixel = cycles; }

    // Architecture metrics
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

    std::uint8_t clip_to_uint8(std::int32_t value);

    // Process RGB triplet to YUV - pure functional
    void process_rgb_triplet(std::uint16_t r, std::uint16_t g, std::uint16_t b,
                           std::uint8_t& y, std::uint8_t& u, std::uint8_t& v);

    const csc_config& m_cfg;

    // Hardware parameters
    const hw_params* m_hw;
    int m_cycles_per_pixel;

    // Architecture metrics
    std::uint64_t m_active_cycles = 0;
    std::uint64_t m_starved_cycles = 0;
    std::uint64_t m_cycle_count = 0;
};

#endif  // SC_CSC_H
