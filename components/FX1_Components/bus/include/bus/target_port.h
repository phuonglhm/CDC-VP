#pragma once
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

namespace bus {
class InitiatorPort : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket_optional<InitiatorPort> socket{"socket"};
    tlm_utils::simple_initiator_socket<InitiatorPort> out{"out"};
    explicit InitiatorPort(sc_core::sc_module_name name) : sc_module(name) {
        socket.register_b_transport(this,&InitiatorPort::b_transport);
        socket.register_transport_dbg(this,&InitiatorPort::transport_dbg);
    }
private:
    void b_transport(tlm::tlm_generic_payload& tx,sc_core::sc_time& d){out->b_transport(tx,d);tx.set_dmi_allowed(false);}
    unsigned transport_dbg(tlm::tlm_generic_payload& tx){auto n=out->transport_dbg(tx);tx.set_dmi_allowed(false);return n;}
};

// Zero-latency boundary between an internal router and an external VP IP.
// No storage or device behavior belongs here. External IPs may wait or annotate delay.
class TargetPort : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<TargetPort> input{"input"};
    tlm_utils::simple_initiator_socket<TargetPort> socket{"socket"};
    explicit TargetPort(sc_core::sc_module_name name) : sc_module(name) {
        input.register_b_transport(this, &TargetPort::b_transport);
        input.register_transport_dbg(this, &TargetPort::transport_dbg);
    }
private:
    unsigned transport_dbg(tlm::tlm_generic_payload& tx) {
        const auto n = socket->transport_dbg(tx);
        tx.set_dmi_allowed(false);
        return n;
    }
    void b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay) {
        socket->b_transport(tx, delay);
        // This bus deliberately does not expose DMI, including from external targets.
        tx.set_dmi_allowed(false);
    }
};
} // namespace bus
