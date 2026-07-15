/**
 * @file sc_input_normalizer.h
 * @brief SystemC wrapper for the C++ pipeline's input bit-depth normalization
 *
 * The original `isp_pipeline::run()` scales every input pixel from the sensor's
 * `input_bit_depth_` to the pipeline's 12-bit working range before BLC.
 * This module replicates that step in streaming form so the SystemC pipeline
 * can accept the same 12- or 16-bit RAW input as the reference model.
 *
 *   out = clamp( (in * work_max) / src_max, 0, work_max )
 *
 * If `src_max == work_max` (i.e. input is already 12-bit) this is a no-op.
 */
#ifndef SC_INPUT_NORMALIZER_H
#define SC_INPUT_NORMALIZER_H

#include <systemc>
using namespace sc_core;

#include <cstdint>
#include <cstdint>

SC_MODULE(sc_input_normalizer) {
public:
    sc_core::sc_port<sc_fifo_in_if<std::uint16_t>>  fifo_in;
    sc_core::sc_port<sc_fifo_out_if<std::uint16_t>> fifo_out;

    SC_HAS_PROCESS(sc_input_normalizer);

    /**
     * @param name           Module name
     * @param src_bit_depth  Bit depth of incoming sensor samples (e.g. 10, 12, 14, 16)
     * @param work_bit_depth Pipeline working bit depth (typically 12)
     */
    sc_input_normalizer(sc_core::sc_module_name name,
                        std::uint8_t src_bit_depth  = 12,
                        std::uint8_t work_bit_depth = 12)
        : sc_module(name)
        , m_src_bit_depth(src_bit_depth)
        , m_work_bit_depth(work_bit_depth) {
        SC_THREAD(process_stream);
    }

private:
    void process_stream() {
        const std::uint32_t work_max = (1u << m_work_bit_depth) - 1u;
        const std::uint32_t src_max  = (1u << m_src_bit_depth)  - 1u;
        if (src_max == 0) {
            // Bad config: pass through unchanged.
            while (true) fifo_out->write(fifo_in->read());
        }
        if (work_max == src_max) {
            // Identity: pass through unchanged.
            while (true) fifo_out->write(fifo_in->read());
        }
        const std::uint64_t num = work_max;
        const std::uint64_t den = src_max;
        while (true) {
            const std::uint16_t v = fifo_in->read();
            const std::uint64_t scaled = (static_cast<std::uint64_t>(v) * num) / den;
            const std::uint16_t out = static_cast<std::uint16_t>(
                scaled > work_max ? work_max : scaled);
            fifo_out->write(out);
        }
    }

    std::uint8_t m_src_bit_depth;
    std::uint8_t m_work_bit_depth;
};

#endif // SC_INPUT_NORMALIZER_H
