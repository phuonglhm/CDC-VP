#pragma once
#include <h264/intra/intra_core.h>
#include <h264/sync/reset_domain.h>
#include <tlm_utils/simple_target_socket.h>
#include <string>

namespace h264::intra {
// Simulation protocol addresses, NOT encoder MMIO registers.
enum class Operation : uint64_t { Evaluate=0, Replay=1, Reconstruct=2, ImportReconstructed=3 };
struct Extension : tlm::tlm_extension<Extension> {
    Block block;
    Metric metric = Metric::Sad;
    std::array<uint32_t,9> mode_penalty{};
    Token token;
    std::optional<Decision> decision;
    bool block_done = false;
    tlm_extension_base* clone() const override { return new Extension(*this); }
    void copy_from(const tlm_extension_base& rhs) override {
        *this=static_cast<const Extension&>(rhs);
    }
};
struct Options {
    unsigned coded_width=16, coded_height=16;
    sc_core::sc_time reference_latency{10,sc_core::SC_NS};
    sc_core::sc_time candidate_latency{4,sc_core::SC_NS};
    sc_core::sc_time compare_latency{1,sc_core::SC_NS};
    sc_core::sc_time replay_latency{4,sc_core::SC_NS};
    sc_core::sc_time feedback_latency{10,sc_core::SC_NS};
};
class IntraTlm : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<IntraTlm> target_socket{"target_socket"};
    sc_core::sc_in<bool> reset_n{"reset_n"};
    sc_core::sc_in<bool> frame_enable{"frame_enable"};
    sc_core::sc_event predictor_ready, block_completed;
    SC_HAS_PROCESS(IntraTlm);
    explicit IntraTlm(sc_core::sc_module_name, Options = {});
    const Core& core() const { return core_; }
    bool ready() const { return ready_; }
    bool block_done() const { return done_; } // Sticky until next evaluate/frame/reset.
    bool transport_active() const { return in_transport_; }
    const std::string& last_error() const { return last_error_; }
private:
    struct Cancelled {};
    void signals_changed();
    void transport(tlm::tlm_generic_payload&, sc_core::sc_time&);
    void pause(sc_core::sc_time, uint64_t, tlm::tlm_generic_payload&);
    void check(uint64_t, tlm::tlm_generic_payload&) const;
    Options options_;
    Core core_;
    bool previous_enable_=false, in_transport_=false;
    bool ready_=false, done_=false, replay_accepted_=false;
    sc_core::sc_event generation_changed_;
    std::string last_error_;
};
}
