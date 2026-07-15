/**

 * @file sc_cse.h

 * @brief SystemC Streaming Module for Color Saturation Enhancement (CSE)

 *

 * Implements chroma saturation enhancement on U and V channels.

 */

#ifndef SC_CSE_H

#define SC_CSE_H



#include <systemc>
using namespace sc_core;


#include <systemc>
#include <cstdint>

#include "../../../blocks/cse/include/cse.h"

SC_MODULE(sc_cse) {
public:
    sc_core::sc_port<sc_fifo_in_if<std::uint8_t>> fifo_in;
    sc_core::sc_port<sc_fifo_out_if<std::uint8_t>> fifo_out;

    SC_HAS_PROCESS(sc_cse);

    sc_cse(sc_core::sc_module_name name, const cse_config& cfg)
        : sc_module(name), m_cfg(cfg) {
        SC_THREAD(process_stream);
    }

private:
    void process_stream();

    std::uint8_t clip_to_uint8(std::int32_t value);

    const cse_config& m_cfg;
};

#endif  // SC_CSE_H
