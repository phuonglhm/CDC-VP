#pragma once

#include <cstdint>
#include <string>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

namespace cdc::platforms::fu3_soc {

// Bus target for a block whose FU3 model is not integrated into the platform yet.
//
// Every functional access answers TLM_GENERIC_ERROR_RESPONSE (slave error) and
// leaves the payload data untouched; the fabric answers unmapped space with
// TLM_ADDRESS_ERROR_RESPONSE, so software can tell the two apart. Debug
// (loader) accesses transfer 0 bytes. It is not a model: it returns no register
// values, drives no interrupt and never counts as an integrated block.
class not_integrated_target : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<not_integrated_target> socket{"socket"};

    not_integrated_target(sc_core::sc_module_name name, std::string block);

    const std::string& block() const { return block_; }
    std::uint64_t accesses() const { return accesses_; }
    std::uint64_t debug_accesses() const { return debug_accesses_; }
    // Local offset of the most recent access of either kind.
    std::uint64_t last_offset() const { return last_offset_; }

private:
    void b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay);
    unsigned transport_dbg(tlm::tlm_generic_payload& tx);

    std::string block_;
    std::uint64_t accesses_ = 0;
    std::uint64_t debug_accesses_ = 0;
    std::uint64_t last_offset_ = 0;
};

} // namespace cdc::platforms::fu3_soc
