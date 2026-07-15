/**

 * @file sc_demosaic.h

 * @brief SystemC Streaming Module for Demosaic

 *

 * Implements CFA (Bayer) to RGB conversion using 5x5 window interpolation.

 * Uses internal line buffers for streaming operation with boundary padding.

 */

#ifndef SC_DEMOSAIC_H

#define SC_DEMOSAIC_H



#include <systemc>
using namespace sc_core;


#include <systemc>
#include <cstdint>
#include <vector>
#include <deque>

#include "../../../blocks/demosaic/include/demosaic.h"
#include "../../../blocks/blc/include/blc.h"

SC_MODULE(sc_demosaic) {
public:
    sc_core::sc_port<sc_fifo_in_if<std::uint16_t>> fifo_in;
    sc_core::sc_port<sc_fifo_out_if<std::uint16_t>> fifo_out;

    SC_HAS_PROCESS(sc_demosaic);

    sc_demosaic(sc_core::sc_module_name name,
                const demosaic_config& cfg,
                cfa_types bayer,
                std::uint8_t bit_depth,
                std::uint32_t width,
                std::uint32_t height)
        : sc_module(name), m_cfg(cfg), m_bayer(bayer),
          m_bit_depth(bit_depth), m_width(width), m_height(height) {
        SC_THREAD(process_stream);
    }

private:
    void process_stream();

    const demosaic_config& m_cfg;
    cfa_types m_bayer;
    std::uint8_t m_bit_depth;
    std::uint32_t m_width;
    std::uint32_t m_height;
};

#endif  // SC_DEMOSAIC_H
