#pragma once
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <algorithm>
#include <functional>
#include <map>
#include <stdexcept>
#include <vector>

namespace bus::test {
// Sparse storage avoids allocating the entire DDR aperture. Records what actually
// reaches an IP, independently of the router's counters or generated route table.
class ProbeTarget : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<ProbeTarget> socket{"socket"};
    struct Visit { std::uint64_t address; unsigned length; unsigned value; };
    std::vector<Visit> visits;
    std::function<void(const Visit&)> on_visit;
    std::uint64_t size;
    bool readonly = false, annotate = false, throw_once = false;
    unsigned latency_ns = 7, debug_calls = 0;
    tlm::tlm_response_status forced_status = tlm::TLM_OK_RESPONSE;

    ProbeTarget(sc_core::sc_module_name name, std::uint64_t bytes) : sc_module(name), size(bytes) {
        socket.register_b_transport(this, &ProbeTarget::transport);
        socket.register_transport_dbg(this, &ProbeTarget::debug);
    }
private:
    std::map<std::uint64_t, unsigned char> bytes_;
    unsigned copy(tlm::tlm_generic_payload& tx, bool debug) {
        const auto a = tx.get_address();
        if (!tx.get_data_ptr() || a >= size || !tx.get_data_length()) {
            tx.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE); return 0;
        }
        if (!tx.is_read() && !tx.is_write()) {
            tx.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE); return 0;
        }
        const auto n = static_cast<unsigned>(std::min<std::uint64_t>(size-a, tx.get_data_length()));
        if (!debug && n != tx.get_data_length()) {
            tx.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE); return 0;
        }
        if (!debug && readonly && tx.is_write()) {
            tx.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE); return 0;
        }
        for (unsigned i=0; i<n; ++i) {
            if (!debug && tx.get_byte_enable_ptr() &&
                tx.get_byte_enable_ptr()[i % tx.get_byte_enable_length()] == 0) continue;
            if (tx.is_write()) bytes_[a+i] = tx.get_data_ptr()[i];
            else tx.get_data_ptr()[i] = bytes_[a+i];
        }
        tx.set_response_status(tlm::TLM_OK_RESPONSE);
        return n;
    }
    void transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay) {
        visits.push_back({tx.get_address(), tx.get_data_length(), tx.get_data_ptr() ? tx.get_data_ptr()[0] : 0U});
        if (on_visit) on_visit(visits.back());
        if (annotate) delay += sc_core::sc_time(latency_ns, sc_core::SC_NS);
        else sc_core::wait(latency_ns, sc_core::SC_NS);
        if (throw_once) { throw_once=false; throw std::runtime_error("injected downstream failure"); }
        tx.set_dmi_allowed(true); // Verify that the fabric suppresses this.
        if (forced_status != tlm::TLM_OK_RESPONSE) { tx.set_response_status(forced_status); return; }
        copy(tx, false);
    }
    unsigned debug(tlm::tlm_generic_payload& tx) {
        ++debug_calls;
        tx.set_dmi_allowed(true);
        return copy(tx, true);
    }
};
} // namespace bus::test
