/**

 * @file sc_csc.h

 * @brief SystemC Streaming Module for RGB to YUV Color Space Conversion (CSC)

 *

 * Implements RGB to YUV conversion using BT.601 or BT.709 coefficients.

 * Also handles bit depth reduction from 12-bit to 8-bit.

 */

#ifndef SC_CSC_H

#define SC_CSC_H



#include <systemc>
using namespace sc_core;


#include <systemc>
#include <cstdint>

#include "../../../blocks/rgb_to_yuv/include/rgb_to_yuv.h"

SC_MODULE(sc_csc) {
public:
    sc_core::sc_port<sc_fifo_in_if<std::uint16_t>> fifo_in;
    sc_core::sc_port<sc_fifo_out_if<std::uint8_t>> fifo_out;

    SC_HAS_PROCESS(sc_csc);

    sc_csc(sc_core::sc_module_name name, const csc_config& cfg)
        : sc_module(name), m_cfg(cfg) {
        SC_THREAD(process_stream);
    }

private:
    void process_stream();

    std::uint8_t clip_to_uint8(std::int32_t value);

    const csc_config& m_cfg;
};

#endif  // SC_CSC_H
