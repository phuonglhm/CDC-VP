#pragma once

#include <cstdint>
#include <string>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

namespace cdc::platforms::fu3_soc {

// Clock gate and reset of one bus target, between the fabric and the block.
//
// While the block's clock is gated or its reset is held (CRM CLK_GATE_x,
// SW_RST_N_x, system or DBG reset), a functional access answers
// TLM_GENERIC_ERROR_RESPONSE without reaching the block and leaves the payload
// data untouched (decision Q02-i: the VP's representation of an access HAS
// calls prohibited; not a confirmed silicon response). Debug accesses (image
// loader, host inspection) always pass through.
//
// Why the block is unavailable decides how a refusal is reported (plan T21):
//   - clock gated or its SW_RST_N bit low while its domain runs: a software
//     error; the first refusals are warnings, later ones only count (refused);
//   - its domain (system or DBG) in reset, synchronizer included: an access
//     that was on its way when the reset came (its master is being reset as
//     well), aborted by the reset; counted (aborted_by_reset), info only.
class gated_target : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<gated_target> socket{"socket"};
    tlm_utils::simple_initiator_socket<gated_target> out{"out"};

    gated_target(sc_core::sc_module_name name, std::string block);

    enum class state { available, clock_gated, software_reset, domain_reset };

    const std::string& block() const { return block_; }
    void set_state(state s) { state_ = s; }
    state current_state() const { return state_; }
    bool available() const { return state_ == state::available; }
    // Functional accesses refused while gated or held by its SW_RST_N bit.
    std::uint64_t refused() const { return refused_; }
    // Functional accesses that reached the gate while its domain was in reset.
    std::uint64_t aborted_by_reset() const { return aborted_; }

private:
    void b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay);
    unsigned transport_dbg(tlm::tlm_generic_payload& tx);

    static constexpr std::uint64_t kReported = 3;
    std::string block_;
    state state_ = state::domain_reset;
    std::uint64_t refused_ = 0, aborted_ = 0;
};

} // namespace cdc::platforms::fu3_soc
