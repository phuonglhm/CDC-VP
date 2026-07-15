/**

 * @file sc_yuv420.h

 * @brief SystemC Streaming Module for YUV420 Conversion

 *

 * Converts YUV444 to YUV420 planar format.

 */

#ifndef SC_YUV420_H

#define SC_YUV420_H



#include <systemc>
using namespace sc_core;


#include <systemc>
#include <cstdint>
#include <vector>

#include "../../../blocks/yuv420/include/yuv420.h"

SC_MODULE(sc_yuv420) {
public:
    sc_core::sc_port<sc_fifo_in_if<std::uint8_t>> fifo_in;
    sc_core::sc_port<sc_fifo_out_if<std::uint8_t>> fifo_out;

    SC_HAS_PROCESS(sc_yuv420);

    sc_yuv420(sc_core::sc_module_name name,
              const yuv420_config& cfg,
              std::uint32_t width,
              std::uint32_t height)
        : sc_module(name), m_cfg(cfg), m_width(width), m_height(height) {
        SC_THREAD(process_stream);
    }

private:
    void process_stream();

    const yuv420_config& m_cfg;
    std::uint32_t m_width;
    std::uint32_t m_height;
};

#endif  // SC_YUV420_H
