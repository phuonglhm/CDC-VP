/**

 * @file sc_lsc.h

 * @brief SystemC Streaming Module for Lens Shading Correction (LSC)

 *

 * Implements lens shading correction using bilinear interpolation over a gain grid.

 * Uses internal line buffers for streaming operation.

 */

#ifndef SC_LSC_H

#define SC_LSC_H



#include <systemc>
using namespace sc_core;


#include <systemc>
#include <cstdint>
#include <vector>
#include <deque>

#include "../../../blocks/lsc/include/lsc.h"
#include "../../../blocks/blc/include/blc.h"

SC_MODULE(sc_lsc) {
public:
    sc_core::sc_port<sc_fifo_in_if<std::uint16_t>> fifo_in;
    sc_core::sc_port<sc_fifo_out_if<std::uint16_t>> fifo_out;

    SC_HAS_PROCESS(sc_lsc);

    /**
     * @brief Constructor
     * @param name Module name
     * @param cfg LSC configuration
     * @param lsc_lut Pointer to LSC LUT data
     * @param bayer Bayer CFA pattern
     * @param bit_depth Working bit depth
     * @param width Image width
     * @param height Image height
     */
    sc_lsc(sc_core::sc_module_name name,
           const lsc_config& cfg,
           const std::vector<float>& lsc_lut,
           cfa_types bayer,
           std::uint8_t bit_depth,
           std::uint32_t width,
           std::uint32_t height)
        : sc_module(name), m_cfg(cfg), m_lsc_lut(lsc_lut), m_bayer(bayer),
          m_bit_depth(bit_depth), m_width(width), m_height(height) {
        SC_THREAD(process_stream);
    }

private:
    void process_stream();

    /**
     * @brief Get LSC gain for a specific pixel position and channel
     */
    float get_lsc_gain(std::uint32_t row, std::uint32_t col, bayer_channel channel);

    /**
     * @brief Determine Bayer channel at given position
     */
    bayer_channel get_bayer_channel(std::uint32_t row, std::uint32_t col);

    const lsc_config& m_cfg;
    const std::vector<float>& m_lsc_lut;
    cfa_types m_bayer;
    std::uint8_t m_bit_depth;
    std::uint32_t m_width;
    std::uint32_t m_height;

    std::uint32_t m_row = 0;
    std::uint32_t m_col = 0;
};

#endif  // SC_LSC_H
