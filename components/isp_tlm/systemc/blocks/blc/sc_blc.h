/**

 * @file sc_blc.h

 * @brief SystemC Streaming Module for Black Level Correction (BLC)

 *

 * Implements per-channel offset subtraction and optional linear/non-linear

 * saturation for RAW Bayer data. Streaming architecture with pixel-by-pixel

 * processing using sc_fifo synchronization.

 */

#ifndef SC_BLC_H

#define SC_BLC_H



#include <systemc>
using namespace sc_core;


#include <systemc>
#include <cstdint>
#include <algorithm>

#include "../../../blocks/blc/include/blc.h"

SC_MODULE(sc_blc) {
public:
    sc_core::sc_port<sc_fifo_in_if<std::uint16_t>> fifo_in;
    sc_core::sc_port<sc_fifo_out_if<std::uint16_t>> fifo_out;

    SC_HAS_PROCESS(sc_blc);

    /**
     * @brief Constructor
     * @param name Module name
     * @param cfg BLC configuration
     * @param bayer Bayer CFA pattern
     * @param bit_depth Working bit depth
     */
    sc_blc(sc_core::sc_module_name name,
           const blc_config& cfg,
           cfa_types bayer,
           std::uint8_t bit_depth = 12)
        : sc_module(name), m_cfg(cfg), m_bayer(bayer), m_bit_depth(bit_depth) {
        SC_THREAD(process_stream);
    }

private:
    void process_stream();

    /**
     * @brief Compute per-pixel BLC offset based on Bayer position
     */
    std::uint16_t get_channel_offset(std::uint32_t row, std::uint32_t col) const;

    /**
     * @brief Compute per-pixel saturation range
     */
    std::uint16_t get_saturation_range(std::uint32_t row, std::uint32_t col) const;

    const blc_config& m_cfg;
    cfa_types m_bayer;
    std::uint8_t m_bit_depth;
    std::uint32_t m_row = 0;
    std::uint32_t m_col = 0;
};

#endif  // SC_BLC_H
