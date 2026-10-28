#pragma once
#include <systemc>
#include <map>
#include <set>
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
    // Requests accepted so far (a sequence number per request) and whether every
    // request numbered below `mark` has completed.
    std::uint64_t issued() const { return issued_; }
    bool completed_before(std::uint64_t mark) const { return live_.empty() || *live_.begin() >= mark; }
    // Latency of a rejected or malformed request; changeable while running.
    void rejection_latency(sc_core::sc_time t) { rejection_latency_=t; }
    unsigned peak(bool write=false) const { return write && descriptor_.separate_read_write ? writes_.peak() : reads_.peak(); }
private:
    InitiatorConfig descriptor_;
    std::vector<TargetConfig> regions_;
    const bool* staging_ = nullptr;
    const bool* accepting_ = nullptr;
    sc_core::sc_event* activity_ = nullptr;
    sc_core::sc_time rejection_latency_{2,sc_core::SC_NS};
    unsigned in_flight_=0;
    std::uint64_t issued_=0;
    std::set<std::uint64_t> live_;
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
    explicit TargetPort(sc_core::sc_module_name name, unsigned cycle_ns=0) : sc_module(name), cycle_(cycle_ns,sc_core::SC_NS) {
        input.register_b_transport(this, &TargetPort::b_transport);
        input.register_transport_dbg(this, &TargetPort::transport_dbg);
    }
    void cycle(unsigned ns) { cycle_=sc_core::sc_time(ns,sc_core::SC_NS); }
    void cycle(sc_core::sc_time t) { cycle_=t; }
    void capacity(unsigned n) { pending_.capacity(n); }
private:
    sc_core::sc_time cycle_;
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
        { Fifo::Lease stage(queue_); sc_core::wait(cycle_); }
        socket->b_transport(tx, delay);
        consume_delay(delay);
        // This bus deliberately does not expose DMI, including from external targets.
        tx.set_dmi_allowed(false);
    }
};
} // namespace fu3_bus
