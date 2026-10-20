#pragma once
#include <systemc>
#include <map>
#include <memory>
#include <limits>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>
#include "fu3_bus/config.h"
#include "fu3_bus/access.h"
#include "fu3_bus/transaction.h"
#include "fu3_bus/admission.h"
#include "fu3_bus/fifo.h"

namespace fu3_bus {
class InitiatorPort : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket_optional<InitiatorPort> socket{"socket"};
    tlm_utils::simple_initiator_socket<InitiatorPort> out{"out"};
    explicit InitiatorPort(sc_core::sc_module_name name);
    void configure(const InitiatorConfig&, const BusConfig&, const bool& staging,
                   const bool& accepting, sc_core::sc_event& activity);
    unsigned in_flight() const { return in_flight_; }
    unsigned peak(bool write=false) const { return write && descriptor_.separate_read_write ? writes_.peak() : reads_.peak(); }
private:
    InitiatorConfig descriptor_;
    std::vector<TargetConfig> regions_;
    const bool* staging_ = nullptr;
    const bool* accepting_ = nullptr;
    sc_core::sc_event* activity_ = nullptr;
    unsigned rejection_latency_ns_=2, in_flight_=0;
    Admission reads_, writes_;
    std::map<std::pair<bool,unsigned>, std::unique_ptr<Fifo>> ordering_;
    void b_transport(tlm::tlm_generic_payload&, sc_core::sc_time&);
    unsigned transport_dbg(tlm::tlm_generic_payload& tx) {
        auto n=out->transport_dbg(tx); tx.set_dmi_allowed(false); return n;
    }
};

// One-stage dispatch pipeline plus bounded downstream occupancy.
// Stage ownership ends before target completion; this is an LT, not RTL, model.
class TargetPort : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<TargetPort> input{"input"};
    tlm_utils::simple_initiator_socket<TargetPort> socket{"socket"};
    explicit TargetPort(sc_core::sc_module_name name, unsigned cycle_ns=0) : sc_module(name), cycle_ns_(cycle_ns) {
        input.register_b_transport(this, &TargetPort::b_transport);
        input.register_transport_dbg(this, &TargetPort::transport_dbg);
    }
    void cycle(unsigned ns) { cycle_ns_=ns; }
    void capacity(unsigned n) { pending_.capacity(n); }
private:
    unsigned cycle_ns_;
    Fifo queue_;
    Admission pending_;
    unsigned transport_dbg(tlm::tlm_generic_payload& tx) {
        const auto n = socket->transport_dbg(tx);
        tx.set_dmi_allowed(false);
        return n;
    }
    void b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay) {
        consume_delay(delay);
        Admission::Lease pending(pending_);
        { Fifo::Lease stage(queue_); sc_core::wait(cycle_ns_,sc_core::SC_NS); }
        socket->b_transport(tx, delay);
        consume_delay(delay);
        // This bus deliberately does not expose DMI, including from external targets.
        tx.set_dmi_allowed(false);
    }
};
} // namespace fu3_bus
