#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

namespace cdc::components {

// Flat memory model usable as ROM or RAM.
//
// Addresses are region-local (0 .. size-1); the bus_router translates global
// addresses before forwarding. Supports b_transport, transport_dbg (backdoor,
// untimed) and DMI.
class memory_tlm : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<memory_tlm> socket;

    // size_bytes:     storage size.
    // read_only:      true => ROM (writes via b_transport return COMMAND_ERROR).
    // access_latency: delay added per b_transport.
    explicit memory_tlm(sc_core::sc_module_name name,
                        std::size_t size_bytes,
                        bool read_only = false,
                        sc_core::sc_time access_latency = sc_core::sc_time(10, sc_core::SC_NS));

    // Backdoor load (e.g. firmware image) at a region-local offset.
    void load(const std::uint8_t* data, std::size_t len, std::uint64_t offset = 0);

    std::size_t size() const { return data_.size(); }

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    unsigned int transport_dbg(tlm::tlm_generic_payload& trans);
    bool get_direct_mem_ptr(tlm::tlm_generic_payload& trans, tlm::tlm_dmi& dmi);

    std::vector<std::uint8_t> data_;
    bool read_only_;
    sc_core::sc_time access_latency_;
};

} // namespace cdc::components
