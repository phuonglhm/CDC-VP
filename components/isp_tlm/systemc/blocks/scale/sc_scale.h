/**

 * @file sc_scale.h

 * @brief SystemC Streaming Module for Scaling

 *

 * Implements bilinear interpolation scaling.

 */

#ifndef SC_SCALE_H

#define SC_SCALE_H



#include <systemc>
using namespace sc_core;


#include <systemc>
#include <cstdint>
#include <vector>

#include "../../../blocks/scale/include/scale.h"

SC_MODULE(sc_scale) {
public:
    sc_core::sc_port<sc_fifo_in_if<std::uint8_t>> fifo_in;
    sc_core::sc_port<sc_fifo_out_if<std::uint8_t>> fifo_out;

    SC_HAS_PROCESS(sc_scale);

    sc_scale(sc_core::sc_module_name name,
             const scale_config& cfg,
             std::uint32_t in_width,
             std::uint32_t in_height)
        : sc_module(name), m_cfg(cfg), m_in_width(in_width), m_in_height(in_height) {
        SC_THREAD(process_stream);
    }

private:
    void process_stream();
    std::uint8_t interpolate_bilinear_yuv(const std::vector<std::uint8_t>& data,
                                          std::uint32_t channel, float x, float y);

    const scale_config& m_cfg;
    std::uint32_t m_in_width;
    std::uint32_t m_in_height;
};

#endif  // SC_SCALE_H
