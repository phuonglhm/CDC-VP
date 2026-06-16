#include "bus_router.h"

#include <stdexcept>
#include <string>

namespace cdc::components {

bus_router::bus_router(sc_core::sc_module_name name, unsigned num_targets,
                       unsigned num_initiators)
    : sc_core::sc_module(name)
    , target_socket("target_socket")
{
    target_socket.register_b_transport(this, &bus_router::b_transport);
    target_socket.register_transport_dbg(this, &bus_router::transport_dbg);

    // Extra upstream ports (port 0 is target_socket). Created here, inside the
    // constructor, sharing the same decode handlers.
    for (unsigned i = 1; i < num_initiators; ++i) {
        const std::string port_name = "cpu_port_" + std::to_string(i);
        auto port = std::make_unique<cpu_socket_t>(port_name.c_str());
        port->register_b_transport(this, &bus_router::b_transport);
        port->register_transport_dbg(this, &bus_router::transport_dbg);
        extra_ports_.push_back(std::move(port));
    }

    // Downstream initiator sockets — also created during construction so they are
    // parented to this router (SystemC requires it).
    sockets_.reserve(num_targets);
    ranges_.reserve(num_targets);
    for (unsigned i = 0; i < num_targets; ++i) {
        const std::string sock_name = "init_" + std::to_string(i);
        sockets_.push_back(std::make_unique<initiator_socket>(sock_name.c_str()));
    }
}

bus_router::cpu_socket_t& bus_router::cpu_port(unsigned index)
{
    if (index == 0) {
        return target_socket;
    }
    if (index - 1 >= extra_ports_.size()) {
        throw std::out_of_range("bus_router::cpu_port index exceeds num_initiators");
    }
    return *extra_ports_[index - 1];
}

bus_router::initiator_socket& bus_router::add_target(std::uint64_t base, std::uint64_t size)
{
    if (ranges_.size() >= sockets_.size()) {
        throw std::out_of_range("bus_router::add_target exceeds num_targets");
    }
    initiator_socket& sock = *sockets_[ranges_.size()];
    ranges_.push_back(common::address_range{base, size});
    return sock;
}

int bus_router::decode(std::uint64_t addr) const
{
    for (std::size_t i = 0; i < ranges_.size(); ++i) {
        if (ranges_[i].contains(addr)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void bus_router::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
{
    const std::uint64_t addr = trans.get_address();
    const int idx = decode(addr);
    if (idx < 0) {
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    trans.set_address(addr - ranges_[idx].base);
    (*sockets_[idx])->b_transport(trans, delay);
    trans.set_address(addr);
}

unsigned int bus_router::transport_dbg(tlm::tlm_generic_payload& trans)
{
    const std::uint64_t addr = trans.get_address();
    const int idx = decode(addr);
    if (idx < 0) {
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return 0;
    }

    trans.set_address(addr - ranges_[idx].base);
    const unsigned int n = (*sockets_[idx])->transport_dbg(trans);
    trans.set_address(addr);
    return n;
}

} // namespace cdc::components
