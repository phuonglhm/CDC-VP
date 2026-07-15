/**

 * @file sc_wb.h

 * @brief SystemC Streaming Module for White Balance (WB)

 *

 * Implements channel-specific gain multiplication (R and B channels).

 * Uses PixelRGB struct for RGB interleaved data streaming.

 */

#ifndef SC_WB_H

#define SC_WB_H



#include <systemc>
using namespace sc_core;


#include <systemc>
#include <cstdint>
#include <algorithm>

#include "../../../blocks/wb/include/wb.h"

SC_MODULE(sc_wb) {
public:
    sc_core::sc_port<sc_fifo_in_if<std::uint16_t>> fifo_in;
    sc_core::sc_port<sc_fifo_out_if<std::uint16_t>> fifo_out;

    SC_HAS_PROCESS(sc_wb);

    sc_wb(sc_core::sc_module_name name, const wb_config& cfg)
        : sc_module(name), m_cfg(cfg), m_awb(nullptr) {
        SC_THREAD(process_stream);
    }

    // Allow `sc_isp_pipeline` glue to wire up the upstream AWB so this
    // module can read the latest computed R/B gains (the AWB finishes its
    // statistics after reading the entire frame; the WB then applies
    // those gains, with a one-frame latency on the first frame).
    void bind_awb(class sc_awb* awb) { m_awb = awb; }

private:
    void process_stream();

    const wb_config& m_cfg;
    class sc_awb* m_awb;
};

#endif  // SC_WB_H
