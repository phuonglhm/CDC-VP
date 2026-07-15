/**

 * @file sc_sharpen.h

 * @brief SystemC Streaming Module for Sharpening

 *

 * Implements Gaussian high-pass sharpening on Y channel.

 * Uses internal line buffers for streaming Gaussian filtering.

 */

#ifndef SC_SHARPEN_H

#define SC_SHARPEN_H



#include <systemc>
using namespace sc_core;


#include <systemc>
#include <cstdint>
#include <vector>
#include <deque>

#include "../../../blocks/sharpen/include/sharpen.h"

SC_MODULE(sc_sharpen) {
public:
    sc_core::sc_port<sc_fifo_in_if<std::uint8_t>> fifo_in;
    sc_core::sc_port<sc_fifo_out_if<std::uint8_t>> fifo_out;

    SC_HAS_PROCESS(sc_sharpen);

    sc_sharpen(sc_core::sc_module_name name,
               const sharpen_config& cfg,
               std::uint32_t width,
               std::uint32_t height)
        : sc_module(name), m_cfg(cfg), m_width(width), m_height(height) {
        SC_THREAD(process_stream);
    }

private:
    void process_stream();
    std::vector<float> create_gaussian_kernel(std::uint8_t sigma);

    const sharpen_config& m_cfg;
    std::uint32_t m_width;
    std::uint32_t m_height;

    static constexpr int MAX_KERNEL_SIZE = 25;
    int m_radius = 1;
    std::vector<float> m_kernel;
};

#endif  // SC_SHARPEN_H
