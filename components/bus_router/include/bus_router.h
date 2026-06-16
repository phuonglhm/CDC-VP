#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include "common.h"

namespace cdc::components {

// Generic TLM-2.0 address-decoding bus router.
//
// Upstream side: one or more initiators (e.g. a CPU with separate instruction
// and data buses) connect to the router's target sockets — see cpu_port().
// Downstream side: peripheral targets are attached at construction time via
// add_target(), each of which creates a fresh initiator socket the caller binds
// to the peripheral's target socket.
//
// On each transaction the router finds the region containing the address,
// translates the address to region-local (addr - base), forwards it, then
// restores the original address. No matching region -> TLM_ADDRESS_ERROR_RESPONSE.
// All upstream ports share the same downstream address map.
//
// b_transport and transport_dbg are forwarded. DMI forwarding is not yet
// implemented (see README).
class bus_router : public sc_core::sc_module {
public:
    using initiator_socket = tlm_utils::simple_initiator_socket<bus_router>;
    using cpu_socket_t = tlm_utils::simple_target_socket<bus_router>;

    // Primary upstream port (initiator #0), e.g. a single CPU bus.
    cpu_socket_t target_socket;

    // num_targets:     number of downstream peripheral sockets to create.
    // num_initiators:  number of upstream ports (default 1). Extra ports beyond
    //                  the primary are reachable via cpu_port(i). SystemC requires
    //                  all sockets be created during construction, so both pools
    //                  are sized up front here.
    bus_router(sc_core::sc_module_name name, unsigned num_targets,
               unsigned num_initiators = 1);

    // Map the next peripheral region and return its initiator socket to bind.
    // Must be called at most num_targets times. The returned reference is stable.
    initiator_socket& add_target(std::uint64_t base, std::uint64_t size);

    // Upstream port by index: 0 = target_socket, 1.. = extra initiator ports.
    cpu_socket_t& cpu_port(unsigned index);

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    unsigned int transport_dbg(tlm::tlm_generic_payload& trans);

    // Returns index of the region containing addr, or -1 if none.
    int decode(std::uint64_t addr) const;

    std::vector<common::address_range> ranges_;
    std::vector<std::unique_ptr<initiator_socket>> sockets_;
    std::vector<std::unique_ptr<cpu_socket_t>> extra_ports_;
};

} // namespace cdc::components
