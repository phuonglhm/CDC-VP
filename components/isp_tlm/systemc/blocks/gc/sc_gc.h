/**

 * @file sc_gc.h

 * @brief SystemC Streaming Module for Gamma Correction (GC)

 *

 * Implements gamma correction using a 12-bit LUT lookup.

 */

#ifndef SC_GC_H

#define SC_GC_H



#include <systemc>
using namespace sc_core;


#include <systemc>
#include <cstdint>
#include <vector>

#include "../../../blocks/gc/include/gc.h"
#include "../../../blocks/gc/gc_lut/lut.h"

SC_MODULE(sc_gc) {
public:
    sc_core::sc_port<sc_fifo_in_if<std::uint16_t>> fifo_in;
    sc_core::sc_port<sc_fifo_out_if<std::uint16_t>> fifo_out;

    SC_HAS_PROCESS(sc_gc);

    sc_gc(sc_core::sc_module_name name, const gc_config& cfg)
        : sc_module(name), m_cfg(cfg) {
        SC_THREAD(process_stream);
    }

private:
    void process_stream();

    const gc_config& m_cfg;
    const std::uint16_t* m_lut;
    std::size_t m_lut_size;
};

#endif  // SC_GC_H
