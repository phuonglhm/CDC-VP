#pragma once
#include <h264/inter/inter_core.h>
#include <h264/sync/reset_domain.h>
#include <tlm_utils/simple_target_socket.h>
#include <string>

namespace h264::inter {
// VP protocol addresses; these are not HAS MMIO registers.
enum class Operation : uint64_t { Evaluate = 0, Commit = 1, Peek = 2, Accept = 3 };
struct Token {
    uint64_t generation = 0, sequence = 0;
    bool operator==(const Token& b) const { return generation == b.generation && sequence == b.sequence; }
};
struct Extension : tlm::tlm_extension<Extension> {
    Request request;
    Token token;
    unsigned accept_sequence = 0;
    std::optional<Decision> decision;
    std::optional<MotionSyntax> syntax;
    std::optional<Sample> sample;
    bool done = false;
    tlm_extension_base* clone() const override { return new Extension(*this); }
    void copy_from(const tlm_extension_base& other) override { *this = static_cast<const Extension&>(other); }
};
struct Options {
    sc_core::sc_time candidate_latency{4, sc_core::SC_NS};
    sc_core::sc_time compare_latency{1, sc_core::SC_NS};
    sc_core::sc_time sample_latency{1, sc_core::SC_NS};
    sc_core::sc_time accept_latency{1, sc_core::SC_NS};
};
struct Progress {
    enum class Stage { Idle, Incoming, Integer, Fractional, Commit, Sample, Accept };
    Stage stage = Stage::Idle;
    unsigned candidate = 0, sample = 0;
    std::optional<Candidate> metadata;
};
class InterTlm : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<InterTlm> target_socket{"target_socket"};
    sc_core::sc_in<bool> reset_n{"reset_n"}, frame_enable{"frame_enable"};
    sc_core::sc_event predictor_ready, block_completed;
    SC_HAS_PROCESS(InterTlm);
    explicit InterTlm(sc_core::sc_module_name, Options = {});
    // Successful SW-DMA response adapter calls. Retag before filling a new
    // picture. Never publish failed/incomplete DMA reads as resident pixels.
    void retag(List, Tag, unsigned width, unsigned height);
    void refill(Reference, Plane, unsigned x, unsigned y, unsigned width,
                unsigned height, const std::vector<uint8_t>& decoded_pixels);
    const ReferenceCache& cache() const { return cache_; }
    const Progress& progress() const { return progress_; }
    const std::optional<Sample>& output() const { return held_; }
    bool done() const { return done_; }
    bool busy() const { return in_transport_ || decision_.has_value(); }
    uint64_t generation() const { return generation_; }
    const std::string& last_error() const { return last_error_; }
private:
    struct Cancelled {};
    using Guard = std::function<void()>;
    void signals_changed();
    void invalidate();
    void transport(tlm::tlm_generic_payload&, sc_core::sc_time&);
    void check(uint64_t, tlm::tlm_generic_payload&) const;
    void pause(sc_core::sc_time, tlm::tlm_generic_payload&, const Guard&);
    void wait_support(const std::function<void()>&, tlm::tlm_generic_payload&, const Guard&);
    void check_token(Token) const;
    Options options_;
    ReferenceCache cache_;
    uint64_t generation_ = 0, sequence_ = 0;
    bool active_ = false, previous_enable_ = false, in_transport_ = false;
    bool committed_ = false, done_ = false;
    Token token_;
    std::optional<Decision> decision_;
    std::vector<Sample> layout_;
    std::optional<Sample> held_;
    unsigned cursor_ = 0;
    Progress progress_;
    sc_core::sc_event changed_;
    std::string last_error_;
};
}
