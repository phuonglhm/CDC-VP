/**

 * @file sc_dpc.h

 * @brief SystemC Streaming Module for Defective Pixel Correction (DPC)

 *

 * Implements defect pixel detection using gradient analysis on a 5x5 window.

 * Uses internal line buffers to handle boundary conditions without global lookups.

 * Streaming architecture with sc_fifo synchronization.

 */

#ifndef SC_DPC_H

#define SC_DPC_H



#include <systemc>
using namespace sc_core;


#include <systemc>
#include <cstdint>
#include <algorithm>
#include <deque>

#include "../../../blocks/dpc/include/dpc.h"

SC_MODULE(sc_dpc) {
public:
    sc_core::sc_port<sc_fifo_in_if<std::uint16_t>> fifo_in;
    sc_core::sc_port<sc_fifo_out_if<std::uint16_t>> fifo_out;

    SC_HAS_PROCESS(sc_dpc);

    /**
     * @brief Constructor
     * @param name Module name
     * @param cfg DPC configuration
     * @param width Image width
     * @param height Image height
     */
    sc_dpc(sc_core::sc_module_name name,
           const dpc_config& cfg,
           std::uint32_t width,
           std::uint32_t height)
        : sc_module(name), m_cfg(cfg), m_width(width), m_height(height) {
        SC_THREAD(process_stream);
    }

private:
    void process_stream();

    const dpc_config& m_cfg;
    std::uint32_t m_width;
    std::uint32_t m_height;
};

#endif  // SC_DPC_H
