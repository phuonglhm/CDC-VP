#pragma once
#include <cstdint>
#include <vector>
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

namespace fx1 {
// Platform-level interrupt controller, SiFive register layout, level-triggered
// sources, one interrupt-enable output per context:
//   0x000000 + 4*s        priority[s]       WARL 0..max_priority, s >= 1
//   0x001000              pending           bit s, read only
//   0x002000 + 0x80*c     enable[c]         bit s
//   0x200000 + 0x1000*c   threshold[c]      WARL 0..max_priority
//   0x200004 + 0x1000*c   claim / complete
// irq_in[i] is source i+1 (source 0 means "none"). Gateway: a high source line
// becomes pending unless the source is in flight (claimed, not completed); a
// complete re-arms it, so a line still high is pending again at once. A claim
// returns the pending, enabled source with priority > 0 and the highest
// priority (lowest ID on ties), independent of the threshold, and clears its
// pending bit, so two contexts never receive the same request. The threshold
// only gates notification: eip[c] is high while that claim candidate's
// priority exceeds threshold[c]. Priority 0 neither notifies nor can be
// claimed. Registers take aligned 32-bit accesses; anything else, or an
// unmapped offset, is a slave error.
class Plic : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<Plic> socket{"socket"};
    sc_core::sc_vector<sc_core::sc_in<bool>> irq_in{"irq_in"};
    sc_core::sc_vector<sc_core::sc_out<bool>> eip{"eip"};

    SC_HAS_PROCESS(Plic);
    // `sources` counts IDs including the reserved 0, so irq_in has sources-1 lines.
    Plic(sc_core::sc_module_name name, unsigned sources, unsigned contexts,
         unsigned max_priority = 7,
         sc_core::sc_time access_latency = sc_core::sc_time(10, sc_core::SC_NS));

    unsigned sources() const noexcept { return sources_; }
    unsigned contexts() const noexcept { return contexts_; }
    bool pending(unsigned source) const { return pending_.at(source); }
    std::uint64_t claims() const noexcept { return claims_; }

private:
    void b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay);
    unsigned transport_dbg(tlm::tlm_generic_payload& tx);
    bool read(std::uint64_t offset, std::uint32_t& value) const;
    bool write(std::uint64_t offset, std::uint32_t value);
    std::uint32_t claim(unsigned context);
    void complete(unsigned context, std::uint32_t source);
    unsigned best(unsigned context) const;    // claim candidate
    bool notify(unsigned context) const;      // eip: candidate above threshold
    void sample();   // gateway: source lines -> pending
    void drive();    // pending/enable/threshold -> eip

    unsigned sources_, contexts_, max_priority_;
    sc_core::sc_time latency_;
    std::vector<std::uint32_t> priority_;
    std::vector<bool> pending_, in_flight_;
    std::vector<std::vector<bool>> enable_;
    std::vector<std::uint32_t> threshold_;
    std::uint64_t claims_ = 0;
    sc_core::sc_event changed_;
};
} // namespace fx1
