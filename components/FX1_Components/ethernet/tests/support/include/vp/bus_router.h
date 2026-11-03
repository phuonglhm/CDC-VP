// =============================================================================
// Integrated Circuits and Systems laboratory
// Author: Hung Nguyen <ngthung@hcmut.edu.vn>
// Module: vp.common.bus_router
// Description: LT address router: N initiators, M targets, local offsets.
// Changelog:
//   2026-09-28: Initial version.
// =============================================================================
//
// Decodes the absolute address against non-overlapping (base, size) windows,
// hands the target its *local* offset (a peripheral never knows where the
// platform put it), restores the address on the way back, and propagates
// every response.  An unmapped access answers TLM_ADDRESS_ERROR_RESPONSE --
// it is never silently read as zero.  The debug path is routed the same way.
// Arbitration between initiators is not modelled (LT, blocking transport).
#pragma once

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

namespace vp {

class BusRouter : public sc_core::sc_module {
public:
    using TargetSocket = tlm_utils::simple_target_socket_tagged<BusRouter>;
    using InitSocket = tlm_utils::simple_initiator_socket_tagged<BusRouter>;

    struct Window {
        std::string name;
        uint64_t base, size;
    };

    // The memory map is fixed at construction (SystemC creates ports during
    // elaboration only).  Windows must not overlap.
    BusRouter(sc_core::sc_module_name name, unsigned n_initiators, std::vector<Window> map,
              sc_core::sc_time hop = sc_core::SC_ZERO_TIME)
        : sc_module(name), hop_(hop), map_(std::move(map)) {
        for (unsigned i = 0; i < n_initiators; ++i) {
            auto s = std::make_unique<TargetSocket>(("in" + std::to_string(i)).c_str());
            s->register_b_transport(this, &BusRouter::b_transport, int(i));
            s->register_transport_dbg(this, &BusRouter::transport_dbg, int(i));
            in_.push_back(std::move(s));
        }
        for (size_t i = 0; i < map_.size(); ++i) {
            for (size_t j = 0; j < i; ++j)
                if (map_[i].base < map_[j].base + map_[j].size &&
                    map_[j].base < map_[i].base + map_[i].size)
                    throw std::runtime_error("bus_router: " + map_[i].name + " overlaps " +
                                             map_[j].name);
            out_.push_back(std::make_unique<InitSocket>(("out_" + map_[i].name).c_str()));
        }
    }

    TargetSocket& initiator_port(unsigned i) { return *in_.at(i); }
    // the socket to bind to the target of window `name`
    InitSocket& target_port(const std::string& name) {
        for (size_t i = 0; i < map_.size(); ++i)
            if (map_[i].name == name) return *out_[i];
        throw std::runtime_error("bus_router: no window " + name);
    }

private:

    int decode(uint64_t a) const {
        for (size_t i = 0; i < map_.size(); ++i)
            if (a >= map_[i].base && a - map_[i].base < map_[i].size) return int(i);
        return -1;
    }

    void b_transport(int, tlm::tlm_generic_payload& tx, sc_core::sc_time& delay) {
        const uint64_t a = tx.get_address();
        const int t = decode(a);
        if (t < 0) {
            tx.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
        delay += hop_;
        tx.set_address(a - map_[t].base);
        (*out_[t])->b_transport(tx, delay);
        tx.set_address(a);
    }

    unsigned transport_dbg(int, tlm::tlm_generic_payload& tx) {
        const uint64_t a = tx.get_address();
        const int t = decode(a);
        if (t < 0) return 0;
        tx.set_address(a - map_[t].base);
        const unsigned n = (*out_[t])->transport_dbg(tx);
        tx.set_address(a);
        return n;
    }

    sc_core::sc_time hop_;
    std::vector<std::unique_ptr<TargetSocket>> in_;
    std::vector<std::unique_ptr<InitSocket>> out_;
    std::vector<Window> map_;
};

}  // namespace vp
