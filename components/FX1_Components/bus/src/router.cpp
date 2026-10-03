#include "bus/router.h"
#include "bus/transaction.h"
#include <iostream>
#include <stdexcept>

namespace bus {
Router::Router(sc_core::sc_module_name name, std::vector<Region> regions,
               unsigned outputs, bool trace)
    : sc_module(name), regions_(std::move(regions)), forwarded_(outputs, 0), trace_(trace) {
    if (!outputs) throw std::invalid_argument("Router requires outputs");
    for (std::size_t i = 0; i < regions_.size(); ++i) {
        const auto& r = regions_[i];
        if (r.begin >= r.end || r.port >= outputs)
            throw std::invalid_argument("Invalid address region");
        for (std::size_t j = 0; j < i; ++j)
            if (r.begin < regions_[j].end && regions_[j].begin < r.end)
                throw std::invalid_argument("Overlapping address regions");
    }
    for (unsigned i = 0; i < outputs; ++i)
        locks_.push_back(std::make_unique<sc_core::sc_mutex>(sc_core::sc_gen_unique_name("port_lock")));
    target.register_b_transport(this, &Router::b_transport);
    target.register_transport_dbg(this, &Router::transport_dbg);
}
void Router::end_of_elaboration() {
    if (out.size() != locks_.size())
        SC_REPORT_FATAL(name(), "Output socket bindings do not match map configuration");
}
void Router::b_transport(int source, tlm::tlm_generic_payload& tx, sc_core::sc_time& delay) {
    consume_delay(delay);
    if (!validate(tx)) { ++errors_; return; }
    const auto address = tx.get_address();
    const Region* selected = nullptr;
    for (const auto& r : regions_)
        if (address >= r.begin && address < r.end) { selected = &r; break; }
    // Reject the whole transfer before writing any byte if it crosses a region.
    if (!selected || tx.get_data_length() > selected->end - address) {
        sc_core::wait(config::ROUTER_NS, sc_core::SC_NS);
        tx.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        ++errors_;
        return;
    }
    Lock lock(*locks_[selected->port]);
    sc_core::wait(config::ROUTER_NS, sc_core::SC_NS);
    RestoreAddress restore(tx);
    if (selected->translate) tx.set_address(address - selected->begin);
    if (trace_)
        std::cout << sc_core::sc_time_stamp() << " " << name() << " source=" << source
                  << (tx.is_write() ? " WRITE " : " READ ") << "0x" << std::hex << address
                  << std::dec << " bytes=" << tx.get_data_length() << " -> " << selected->name << '\n';
    ++forwarded_[selected->port];
    out[selected->port]->b_transport(tx, delay);
    consume_delay(delay);
    if (tx.is_response_error()) ++errors_;
}
unsigned Router::transport_dbg(int, tlm::tlm_generic_payload& tx) {
    tx.set_dmi_allowed(false);
    if (!tx.is_read() && !tx.is_write()) {
        tx.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return 0;
    }
    if (!tx.get_data_ptr() || !tx.get_data_length()) {
        tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return 0;
    }
    const auto address = tx.get_address();
    for (const auto& r : regions_) {
        if (address < r.begin || address >= r.end) continue;
        if (tx.get_data_length() > r.end - address) break;
        RestoreAddress restore(tx);
        if (r.translate) tx.set_address(address - r.begin);
        // Debug never waits, arbitrates, adds timing or modifies functional counters.
        const auto n = out[r.port]->transport_dbg(tx);
        tx.set_dmi_allowed(false);
        return n;
    }
    tx.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
    return 0;
}
} // namespace bus
