#pragma once
#include "bus/config.h"
#include <systemc>
#include <tlm>
#include <tlm_utils/multi_passthrough_target_socket.h>
#include <tlm_utils/multi_passthrough_initiator_socket.h>
#include <memory>
#include <vector>

namespace bus {
// Behavioral replacement for address decode + DEMUX + MUX + DECERR.
class Router : public sc_core::sc_module {
public:
    tlm_utils::multi_passthrough_target_socket<Router> target{"target"};
    tlm_utils::multi_passthrough_initiator_socket<Router> out{"out"};
    Router(sc_core::sc_module_name name, std::vector<Region> regions,
           unsigned outputs, unsigned latency_ns = 2, bool trace = false);
    ~Router() override;
    std::uint64_t forwarded(unsigned port) const { return forwarded_.at(port); }
    std::uint64_t errors() const { return errors_; }
private:
    void b_transport(int source, tlm::tlm_generic_payload& tx, sc_core::sc_time& delay);
    unsigned transport_dbg(int source, tlm::tlm_generic_payload& tx);
    void end_of_elaboration() override;
    std::vector<Region> regions_;
    struct FifoArbiter;
    std::vector<std::unique_ptr<FifoArbiter>> arbiters_;
    std::vector<std::uint64_t> forwarded_;
    std::uint64_t errors_ = 0;
    bool trace_;
    unsigned latency_ns_;
};
} // namespace bus
