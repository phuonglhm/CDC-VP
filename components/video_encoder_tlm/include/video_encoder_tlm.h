#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include "block.h"
#include "encoder_defs.h"
#include "fme.h"
#include "fme_result.h"
#include "frame.h"
#include "ime.h"
#include "ime_result.h"
#include "mode_decision.h"
#include "mode_decision_result.h"
#include "posi.h"
#include "prediction_result.h"
#include "prei.h"
#include "prei_result.h"

namespace cdc::components {

// -----------------------------------------------------------------------------
// video_encoder_tlm
//
// TLM equivalent of xk265 rtl/top:
//
// rtl/top/enc_top.v
// rtl/top/enc_core.v
// rtl/top/enc_ctrl.v
// rtl/top/enc_data_pipeline.v
// rtl/top/prei_top_buf.v
// rtl/top/posi_top_buf.v
// rtl/top/ime_top_buf.v
// rtl/top/fme_top_buf.v
//
// Current scope:
// - TOP control
// - PREI
// - POSI
// - IME
// - FME
// - mode decision helper
//
// Not implemented here:
// - REC
// - DB
// - CABAC
//
// The output of this TOP is ctu_predictions_.
// Later, REC/DB/CABAC can consume these final prediction results.
// -----------------------------------------------------------------------------

class video_encoder_tlm : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<video_encoder_tlm> socket;

    explicit video_encoder_tlm(
        sc_core::sc_module_name name,
        sc_core::sc_time latency = sc_core::sc_time(10, sc_core::SC_NS)
    );

    // Direct software/testbench APIs
    void load_input_frame(const frame& input);
    void load_reference_frame(const frame& reference);

    void set_frame_type(frame_type type);
    void set_qp(std::uint32_t qp);

    std::vector<std::uint8_t> encode_frame();

    const frame& input_frame() const;
    const frame& reference_frame() const;
    const frame& reconstructed_frame() const;

    const std::vector<prediction_result>& ctu_predictions() const;
    const std::vector<mode_decision_result>& ctu_decisions() const;

    const std::vector<std::uint8_t>& last_bitstream() const;
    std::size_t last_bitstream_size() const;
    std::uint32_t bit_count() const;

private:
    // TLM target callbacks
    void b_transport(tlm::tlm_generic_payload& trans,
                     sc_core::sc_time& delay);

    unsigned int transport_dbg(tlm::tlm_generic_payload& trans);

    // Register access
    std::uint32_t read_reg(std::uint64_t addr, bool& ok) const;
    void write_reg(std::uint64_t addr, std::uint32_t value);

    // TOP pipeline
    void run_pipeline();
    prediction_result encode_ctu(const block& ctu);

    prei_rate_control_config make_prei_rc_config() const;
    mode_decision_config make_mode_decision_config(std::uint32_t qp) const;

    void append_prediction_summary(const prediction_result& result);

    bool input_valid() const;

    void clear_status();
    void set_busy();
    void set_done();
    void set_error();

    std::uint32_t clamp_qp(std::uint32_t qp) const;

private:
    // Sub-blocks owned by TOP
    prei prei_;
    posi posi_;
    ime ime_;
    fme fme_;
    mode_decision mode_decision_;

    // Frame storage
    frame input_frame_;
    frame reference_frame_;

    // Placeholder until REC block is connected.
    // REC owner can later replace/update this frame.
    frame reconstructed_frame_;

    // Output of this TOP scope:
    // one final prediction result per CTU.
    std::vector<prediction_result> ctu_predictions_;
    std::vector<mode_decision_result> ctu_decisions_;

    // Placeholder bitstream summary.
    // Actual CABAC bitstream is outside this scope.
    std::vector<std::uint8_t> last_bitstream_;
    std::uint32_t bit_count_ = 0;

    sc_core::sc_time access_latency_;

    frame_type frame_type_ = frame_type::inter;
    std::uint32_t qp_ = INIT_QP;

    std::uint32_t control_ = 0;
    std::uint32_t status_ = 0;

    std::uint32_t configured_width_ = 0;
    std::uint32_t configured_height_ = 0;
};

} // namespace cdc::components

