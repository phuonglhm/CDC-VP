/**
 * @file sc_scale.h
 * @brief SystemC Streaming Module for Scaling
 *
 * Implements bilinear interpolation scaling.
 *
 * Hardware shell features (Phase 3):
 *   - Optional hw_params for timing
 *   - Architecture metrics
 */
#ifndef SC_SCALE_H
#define SC_SCALE_H

#include <systemc>
using namespace sc_core;

#include <systemc>
#include <cstdint>
#include <vector>

#include "../../../blocks/scale/include/scale.h"
#include "../../tb_utils/hardware_params.h"

SC_MODULE(sc_scale) {
public:
    sc_core::sc_port<sc_fifo_in_if<std::uint8_t>> fifo_in;
    sc_core::sc_port<sc_fifo_out_if<std::uint8_t>> fifo_out;

    // Optional clock port for timed mode (pointer for optional binding)
    sc_in<bool>* clk = nullptr;

    SC_HAS_PROCESS(sc_scale);

    sc_scale(sc_core::sc_module_name name,
             const scale_config& cfg,
             std::uint32_t in_width,
             std::uint32_t in_height,
             const hw_params* hw = nullptr)
        : sc_module(name)
        , m_cfg(cfg)
        , m_in_width(in_width)
        , m_in_height(in_height)
        , m_hw(hw) {
        SC_THREAD(process_stream);
    }


    // Hardware configuration
    void set_hw_params(const hw_params* hw) { m_hw = hw; }

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
    std::uint8_t interpolate_bilinear_yuv(const std::vector<std::uint8_t>& data,
                                          std::uint32_t channel, float x, float y);

    const scale_config& m_cfg;
    std::uint32_t m_in_width;
    std::uint32_t m_in_height;

    // Hardware parameters
    const hw_params* m_hw;

    // Architecture metrics
    std::uint64_t m_active_cycles = 0;
    std::uint64_t m_starved_cycles = 0;
    std::uint64_t m_cycle_count = 0;
};

#endif  // SC_SCALE_H
