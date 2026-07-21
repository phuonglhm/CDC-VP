/**
 * @file sc_input_normalizer.h
 * @brief SystemC wrapper for the C++ pipeline's input bit-depth normalization
 *
 * The original `isp_pipeline::run()` scales every input pixel from the sensor's
 * `input_bit_depth_` to the pipeline's 12-bit working range before BLC.
 * This module replicates that step in streaming form so the SystemC pipeline
 * can accept the same 12- or 16-bit RAW input as the reference model.
 *
 *   out = clamp( (in * work_max) / src_max, 0, work_max )
 *
 * If `src_max == work_max` (i.e. input is already 12-bit) this is a no-op.
 *
 * Hardware shell features (Phase 3):
 *   - Optional hw_params for timing
 *   - Configurable cycles per pixel
 *   - Architecture metrics
 */
#ifndef SC_INPUT_NORMALIZER_H
#define SC_INPUT_NORMALIZER_H

#include <systemc>
using namespace sc_core;

#include <cstdint>

#include "../tb_utils/hardware_params.h"

SC_MODULE(sc_input_normalizer) {
public:
    sc_core::sc_port<sc_fifo_in_if<std::uint16_t>>  fifo_in;
    sc_core::sc_port<sc_fifo_out_if<std::uint16_t>> fifo_out;

    // Optional clock port for timed mode
    sc_in<bool>* clk = nullptr;

    SC_HAS_PROCESS(sc_input_normalizer);

    /**
     * @param name           Module name
     * @param src_bit_depth  Bit depth of incoming sensor samples (e.g. 10, 12, 14, 16)
     * @param work_bit_depth Pipeline working bit depth (typically 12)
     * @param hw             Optional hardware parameters for timed mode
     */
    sc_input_normalizer(sc_core::sc_module_name name,
                        std::uint8_t src_bit_depth  = 12,
                        std::uint8_t work_bit_depth = 12,
                        const hw_params* hw = nullptr)
        : sc_module(name)
        , m_src_bit_depth(src_bit_depth)
        , m_work_bit_depth(work_bit_depth)
        , m_hw(hw)
        , m_cycles_per_pixel(1)
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

    // Process single pixel - pure functional
    std::uint16_t process_pixel(std::uint16_t pixel);

    std::uint8_t m_src_bit_depth;
    std::uint8_t m_work_bit_depth;

    // Hardware parameters
    const hw_params* m_hw;
    int m_cycles_per_pixel;

    // Architecture metrics
    std::uint64_t m_active_cycles = 0;
    std::uint64_t m_starved_cycles = 0;
    std::uint64_t m_cycle_count = 0;
};

// ============================================================================
// Inline Implementation
// ============================================================================
inline std::uint16_t sc_input_normalizer::process_pixel(std::uint16_t pixel) {
    const std::uint32_t work_max = (1u << m_work_bit_depth) - 1u;
    const std::uint32_t src_max  = (1u << m_src_bit_depth)  - 1u;

    if (src_max == 0 || work_max == src_max) {
        return pixel;
    }

    const std::uint64_t num = work_max;
    const std::uint64_t den = src_max;
    const std::uint64_t scaled = (static_cast<std::uint64_t>(pixel) * num) / den;
    return static_cast<std::uint16_t>(scaled > work_max ? work_max : scaled);
}

inline void sc_input_normalizer::process_stream() {
    // Check if timed mode is enabled
    bool timed_mode = (m_hw != nullptr) && m_hw->timed_mode && (clk != nullptr);
    if (timed_mode) {
        m_cycles_per_pixel = m_hw->default_cycles_per_pixel;
    }

    const std::uint32_t work_max = (1u << m_work_bit_depth) - 1u;
    const std::uint32_t src_max  = (1u << m_src_bit_depth)  - 1u;
    const bool need_conversion = (src_max != 0) && (work_max != src_max);

    const std::uint64_t scale_num = work_max;
    const std::uint64_t scale_den = src_max;

    while (true) {
        const std::uint16_t v = fifo_in->read();

        std::uint16_t out;
        if (!need_conversion) {
            out = v;
        } else {
            const std::uint64_t scaled = (static_cast<std::uint64_t>(v) * scale_num) / scale_den;
            out = static_cast<std::uint16_t>(scaled > work_max ? work_max : scaled);
        }

        // Hardware shell: timing
        if (timed_mode) {
            for (int i = 0; i < m_cycles_per_pixel; ++i) {
                wait();
                ++m_cycle_count;
                ++m_active_cycles;
            }
        } else {
            ++m_cycle_count;
            ++m_active_cycles;
        }

        fifo_out->write(out);
    }
}

#endif // SC_INPUT_NORMALIZER_H
