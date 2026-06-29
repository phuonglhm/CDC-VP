#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include "cabac.h"
#include "fme.h"
#include "frame.h"
#include "ime.h"
#include "mode_decision.h"
#include "posi.h"
#include "prei.h"
#include "rec.h"

namespace cdc::components {

class video_encoder_tlm : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<video_encoder_tlm> socket;

    explicit video_encoder_tlm(sc_core::sc_module_name name,
                               sc_core::sc_time access_latency = sc_core::sc_time(10, sc_core::SC_NS));

    void load_input_frame(const frame& input);
    std::vector<std::uint8_t> encode_frame();
    std::size_t last_bitstream_size() const;

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    unsigned int transport_dbg(tlm::tlm_generic_payload& trans);
    std::uint32_t read_reg(std::uint64_t addr, bool& ok) const;
    bool write_reg(std::uint64_t addr, std::uint32_t value);
    void run_pipeline();

    prei prei_;
    posi posi_;
    ime ime_;
    fme fme_;
    mode_decision mode_decision_;
    rec rec_;
    cabac cabac_;

    frame input_frame_;
    frame reconstructed_frame_;
    std::vector<std::uint8_t> last_bitstream_;
    sc_core::sc_time access_latency_;
    std::uint32_t control_ = 0;
    std::uint32_t status_ = 0;
};

} // namespace cdc::components
