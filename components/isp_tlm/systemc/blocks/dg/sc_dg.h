/**

 * @file sc_dg.h

 * @brief SystemC Streaming Module for Digital Gain (DG)

 *

 * Implements digital gain multiplication for RAW Bayer data.

 * Streaming architecture with pixel-by-pixel processing.

 */

#ifndef SC_DG_H

#define SC_DG_H



#include <systemc>
using namespace sc_core;


#include <systemc>
#include <cstdint>
#include <algorithm>

#include "../../../blocks/dg/include/dg.h"

SC_MODULE(sc_dg) {
public:
    sc_core::sc_port<sc_fifo_in_if<std::uint16_t>> fifo_in;
    sc_core::sc_port<sc_fifo_out_if<std::uint16_t>> fifo_out;

    SC_HAS_PROCESS(sc_dg);

    sc_dg(sc_core::sc_module_name name,
          const dg_config& cfg,
          std::uint8_t bit_depth = 12)
        : sc_module(name), m_cfg(cfg), m_bit_depth(bit_depth) {
        SC_THREAD(process_stream);
    }

private:
    void process_stream();

    const dg_config& m_cfg;
    std::uint8_t m_bit_depth;
};

#endif  // SC_DG_H
