/**

 * @file sc_aec.h

 * @brief SystemC Streaming Module for Auto Exposure Control (AEC)

 *

 * Computes histogram statistics over the frame for exposure feedback.

 * Acts as a pass-through for RGB data while computing statistics.

 */

#ifndef SC_AEC_H

#define SC_AEC_H



#include <systemc>
using namespace sc_core;


#include <systemc>
#include <cstdint>
#include <vector>

#include "../../../blocks/aec/include/aec.h"
#include "../../tb_utils/sc_block_metrics.h"

SC_MODULE(sc_aec) {
public:
    sc_core::sc_port<sc_fifo_in_if<std::uint16_t>> fifo_in;
    sc_core::sc_port<sc_fifo_out_if<std::uint16_t>> fifo_out;

    SC_HAS_PROCESS(sc_aec);

    sc_aec(sc_core::sc_module_name name,
            const aec_config& cfg,
            std::uint32_t width,
            std::uint32_t height,
            std::uint8_t bit_depth)
        : sc_module(name), m_cfg(cfg), m_width(width), m_height(height),
          m_bit_depth(bit_depth), m_ae_feedback(0), m_metrics("aec") {
        SC_THREAD(process_stream);
    }
    std::int32_t get_ae_feedback() const { return m_ae_feedback; }

    // Block-level metrics collector (public so sc_isp_pipeline can access)
    sc_block_metrics<std::uint16_t> m_metrics;

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

    const aec_config& m_cfg;
    std::uint32_t m_width;
    std::uint32_t m_height;
    std::uint8_t m_bit_depth;
    std::int32_t m_ae_feedback;

    // Architecture metrics
    std::uint64_t m_active_cycles = 0;
    std::uint64_t m_starved_cycles = 0;
    std::uint64_t m_cycle_count = 0;
};

#endif  // SC_AEC_H
