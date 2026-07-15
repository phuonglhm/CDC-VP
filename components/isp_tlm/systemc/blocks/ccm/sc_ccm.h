/**

 * @file sc_ccm.h

 * @brief SystemC Streaming Module for Color Correction Matrix (CCM)

 *

 * Implements 3x3 color correction matrix multiplication.

 */

#ifndef SC_CCM_H

#define SC_CCM_H



#include <systemc>
using namespace sc_core;


#include <systemc>
#include <cstdint>
#include <algorithm>

#include "../../../blocks/ccm/include/ccm.h"

SC_MODULE(sc_ccm) {
public:
    sc_core::sc_port<sc_fifo_in_if<std::uint16_t>> fifo_in;
    sc_core::sc_port<sc_fifo_out_if<std::uint16_t>> fifo_out;

    SC_HAS_PROCESS(sc_ccm);

    sc_ccm(sc_core::sc_module_name name, const ccm_config& cfg)
        : sc_module(name), m_cfg(cfg) {
        SC_THREAD(process_stream);
    }

private:
    void process_stream();

    std::uint16_t max_for_bit_depth(std::uint8_t bit_depth) const;
    float clamp01(float value) const;
    std::uint16_t quantize(float value, std::uint16_t max_value) const;

    const ccm_config& m_cfg;
};

#endif  // SC_CCM_H
