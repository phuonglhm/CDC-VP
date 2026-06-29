#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include "encoder_defs.h"
#include "frame.h"
#include "block.h"
#include "prediction_result.h"

#include "cabac.h"
#include "fme.h"
#include "ime.h"
#include "mode_decision.h"
#include "posi.h"
#include "prei.h"
#include "rec.h"

// DB block is part of H.265 reconstruction loop.
// If db.h is still empty, keep the include here and add the db class later.
#include "db.h"

namespace cdc::components {

class video_encoder_tlm : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<video_encoder_tlm> socket;

    explicit video_encoder_tlm(
        sc_core::sc_module_name name,
        sc_core::sc_time access_latency = sc_core::sc_time(10, sc_core::SC_NS)
    );

    // -------------------------------------------------------------------------
    // Frame input API
    // -------------------------------------------------------------------------

    void load_input_frame(const frame& input);
    void load_reference_frame(const frame& reference);

    void set_frame_type(frame_type type);
    void set_qp(std::uint32_t qp);

    // -------------------------------------------------------------------------
    // Main encode API
    // -------------------------------------------------------------------------

    std::vector<std::uint8_t> encode_frame();

    const frame& input_frame() const;
    const frame& reference_frame() const;
    const frame& reconstructed_frame() const;

    const std::vector<std::uint8_t>& last_bitstream() const;

    std::size_t last_bitstream_size() const;
    std::uint32_t bit_count() const;

private:
    // -------------------------------------------------------------------------
    // TLM register access
    // -------------------------------------------------------------------------

    void b_transport(tlm::tlm_generic_payload& trans,
                     sc_core::sc_time& delay);

    unsigned int transport_dbg(tlm::tlm_generic_payload& trans);

    std::uint32_t read_reg(std::uint64_t addr, bool& ok) const;
    bool write_reg(std::uint64_t addr, std::uint32_t value);

    // -------------------------------------------------------------------------
    // TOP control / pipeline
    // -------------------------------------------------------------------------

    void run_pipeline();
    void encode_ctu(const block& ctu);

    bool input_valid() const;
    void clear_status();
    void set_busy();
    void set_done();
    void set_error();

private:
    // -------------------------------------------------------------------------
    // Sub-blocks equivalent to xk265 rtl folders
    //
    // rtl/prei  -> prei_
    // rtl/posi  -> posi_
    // rtl/ime   -> ime_
    // rtl/fme   -> fme_
    // rtl/rec   -> rec_
    // rtl/db    -> db_
    // rtl/cabac -> cabac_
    // -------------------------------------------------------------------------

    prei prei_;
    posi posi_;
    ime ime_;
    fme fme_;
    mode_decision mode_decision_;
    rec rec_;

    // NOTE:
    // If db.h has not defined class db yet, temporarily comment this member.
    // Uncomment after DB/SAO block owner implements db.h/.cpp.
    //
    // db db_;

    cabac cabac_;

    // -------------------------------------------------------------------------
    // Frame buffers
    // -------------------------------------------------------------------------

    frame input_frame_;
    frame reference_frame_;
    frame reconstructed_frame_;

    // -------------------------------------------------------------------------
    // Output bitstream
    // -------------------------------------------------------------------------

    std::vector<std::uint8_t> last_bitstream_;
    std::uint32_t bit_count_ = 0;

    // -------------------------------------------------------------------------
    // TOP control state
    // -------------------------------------------------------------------------

    sc_core::sc_time access_latency_;

    frame_type frame_type_ = frame_type::intra;
    std::uint32_t qp_ = INIT_QP;

    std::uint32_t control_ = 0;
    std::uint32_t status_ = 0;

    std::uint32_t configured_width_ = 0;
    std::uint32_t configured_height_ = 0;
};

} // namespace cdc::components
