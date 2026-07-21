/**
 * @file sc_ccm.h
 * @brief SystemC Streaming Module for Color Correction Matrix (CCM)
 *
 * Implements 3x3 color correction matrix multiplication.
 *
 * Hardware shell features (Phase 3):
 *   - Optional hw_params for timing
 *   - Configurable cycles per pixel
 *   - Architecture metrics
 */
#ifndef SC_CCM_H
#define SC_CCM_H

#include <systemc>
using namespace sc_core;

#include <systemc>
#include <cstdint>

#include "../../../blocks/ccm/include/ccm.h"
#include "../../tb_utils/hardware_params.h"

SC_MODULE(sc_ccm) {
public:
    sc_core::sc_port<sc_fifo_in_if<std::uint16_t>> fifo_in;
    sc_core::sc_port<sc_fifo_out_if<std::uint16_t>> fifo_out;

    // Optional clock port for timed mode (pointer for optional binding)
    sc_in<bool>* clk = nullptr;

    SC_HAS_PROCESS(sc_ccm);

    sc_ccm(sc_core::sc_module_name name,
           const ccm_config& cfg,
           const hw_params* hw = nullptr)
        : sc_module(name)
        , m_cfg(cfg)
        , m_hw(hw)
        , m_cycles_per_pixel(2) {
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

    // Pure functional methods
    std::uint16_t max_for_bit_depth(std::uint8_t bit_depth) const;
    float clamp01(float value) const;
    std::uint16_t quantize(float value, std::uint16_t max_value) const;
    void process_rgb_triplet(std::uint16_t r_in, std::uint16_t g_in, std::uint16_t b_in,
                           std::uint16_t& r_out, std::uint16_t& g_out, std::uint16_t& b_out);

    const ccm_config& m_cfg;

    // Hardware parameters
    const hw_params* m_hw;
    int m_cycles_per_pixel;

    // Architecture metrics
    std::uint64_t m_active_cycles = 0;
    std::uint64_t m_starved_cycles = 0;
    std::uint64_t m_cycle_count = 0;
};

#endif  // SC_CCM_H
