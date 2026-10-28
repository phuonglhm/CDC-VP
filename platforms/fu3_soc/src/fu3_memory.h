#pragma once

#include <cstdint>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

namespace cdc::platforms::fu3_soc {

// Byte-addressed on-chip memory (ROM, ISRAM, DSRAM) behind the FU3 bus.
//
// Addresses are local offsets (the leaf router translates). Functional
// accesses honour byte enables, so the word lanes the bus produces for subword
// requests touch only the enabled bytes. A read-only memory answers functional
// writes with TLM_GENERIC_ERROR_RESPONSE; the debug path (image loader) may
// write it. Access policy (who may read, write or fetch what) is the bus's job,
// not the memory's.
class fu3_memory : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<fu3_memory> socket{"socket"};

    fu3_memory(sc_core::sc_module_name name, std::uint64_t size, bool read_only);

    std::uint64_t size() const { return bytes_.size(); }
    bool read_only() const { return read_only_; }
    // Host-side inspection and stimulus for tests; no simulated time, no bus policy.
    std::uint8_t peek(std::uint64_t offset) const { return bytes_.at(offset); }
    void poke(std::uint64_t offset, std::uint8_t value) { bytes_.at(offset) = value; }

private:
    void b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay);
    unsigned transport_dbg(tlm::tlm_generic_payload& tx);
    bool in_range(const tlm::tlm_generic_payload& tx) const;

    std::vector<std::uint8_t> bytes_;
    bool read_only_;
};

} // namespace cdc::platforms::fu3_soc
