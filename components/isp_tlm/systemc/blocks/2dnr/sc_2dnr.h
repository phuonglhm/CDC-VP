/**

 * @file sc_2dnr.h

 * @brief SystemC Streaming Module for 2D Noise Reduction (2DNR)

 *

 * Implements patch-based denoising on Y channel.

 */

#ifndef SC_2DNR_H

#define SC_2DNR_H



#include <systemc>
using namespace sc_core;


#include <systemc>
#include <cstdint>
#include <vector>

#include "../../../blocks/2dnr/include/2dnr.h"

SC_MODULE(sc_2dnr) {
public:
    sc_core::sc_port<sc_fifo_in_if<std::uint8_t>> fifo_in;
    sc_core::sc_port<sc_fifo_out_if<std::uint8_t>> fifo_out;

    SC_HAS_PROCESS(sc_2dnr);

    sc_2dnr(sc_core::sc_module_name name,
            const twodnr_config& cfg,
            std::uint32_t width,
            std::uint32_t height)
        : sc_module(name), m_cfg(cfg), m_width(width), m_height(height) {
        SC_THREAD(process_stream);
    }

private:
    void process_stream();

    const twodnr_config& m_cfg;
    std::uint32_t m_width;
    std::uint32_t m_height;
};

#endif  // SC_2DNR_H
