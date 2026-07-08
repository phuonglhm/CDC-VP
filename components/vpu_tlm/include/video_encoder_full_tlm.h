#pragma once

#include <cstdint>
#include <vector>

#include <systemc>

#include "debug_config.h"
#include "rec_backend_bridges.h"
#include "video_encoder_tlm.h"
#include "video_encoder_to_rec.h"
#include "vpu_rec_top.h"
#include "vpu_db_top.h"
#include "vpu_cabac_top.h"
#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"

namespace cdc::components {

// Result bundle for a single region through the current end-to-end TLM path:
// prediction -> rec -> db/cabac.
struct video_encoder_full_tlm_trace {
    video_encoder_tlm_result encoder_result {};
};

struct video_encoder_full_tlm_result {
    bool valid = false;
    std::vector<std::uint8_t> db_output;
    std::vector<std::uint8_t> cabac_output;
    video_encoder_full_tlm_trace trace;
};

struct video_encoder_frame_tlm_result {
    bool valid = false;
    std::uint32_t ctu_size = 0;
    std::uint32_t total_regions = 0;
    std::uint32_t completed_regions = 0;
    frame reconstructed_frame {};
    std::vector<block> regions;
    std::vector<video_encoder_full_tlm_result> region_results;
};

class full_top_packet_sink : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<full_top_packet_sink> socket;
    std::vector<std::uint8_t> last_data;

    explicit full_top_packet_sink(sc_core::sc_module_name name);

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
};

class full_top_rec_driver : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<full_top_rec_driver> intra_socket;
    tlm_utils::simple_initiator_socket<full_top_rec_driver> mc_socket;

    explicit full_top_rec_driver(sc_core::sc_module_name name);
};

class video_encoder_full_tlm {
public:
    video_encoder_full_tlm();

    void set_verbose(bool enabled)
    {
        set_verbose_enabled(enabled);
    }

    // Run one region through the integrated encoder path and capture the
    // backend outputs currently exposed at DB and CABAC sinks.
    video_encoder_full_tlm_result run_prediction(
        const frame& input,
        const frame& reconstructed,
        const frame& reference,
        const block& region,
        std::uint32_t qp = INIT_QP);

    // Traverse a full frame in CTU order and run the current integrated path
    // once per region. Edge CTUs are clipped to the remaining frame area.
    video_encoder_frame_tlm_result run_frame(
        const frame& input,
        const frame& reconstructed,
        const frame& reference,
        std::uint32_t ctu_size = 16,
        std::uint32_t qp = INIT_QP);

    video_encoder_tlm encoder {};
    RecTop rec {"rec"};
    DbTop db {"db"};
    CabacTop cabac {"cabac"};
    rec_to_db_bridge rec_to_db {"rec_to_db"};
    rec_to_cabac_bridge rec_to_cabac {"rec_to_cabac"};
    full_top_rec_driver rec_driver {"rec_driver"};
    full_top_packet_sink frame_sink {"frame_sink"};
    full_top_packet_sink db_sink {"db_sink"};
    full_top_packet_sink cabac_sink {"cabac_sink"};

private:
    video_encoder_to_rec encoder_to_rec_ {};
};

} // namespace cdc::components
