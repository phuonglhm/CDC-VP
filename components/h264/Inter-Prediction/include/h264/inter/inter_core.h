#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <vector>

namespace h264::inter {
enum class Plane { Y, U, V };
enum class List { L0, L1 };
enum class Picture { P, B };
struct MotionVector { int x = 0, y = 0; }; // Signed quarter-luma-sample units.
struct Tag {
    uint64_t picture = 0;
    unsigned slot = 0; // One of the encoder's three REFM slots.
    bool operator==(const Tag& b) const { return picture == b.picture && slot == b.slot; }
};
struct Reference { List list = List::L0; Tag tag; };
struct Partition { unsigned x = 0, y = 0, width = 16, height = 16; };
void validate_partition(const Partition&);
void validate_mv(MotionVector);
struct NotResident : std::runtime_error { using std::runtime_error::runtime_error; };

// A sparse view of a reconstructed reference picture, not a source-picture cache.
// Both lists have independent tags and per-pixel support residency. Filling an
// already resident sample with different data requires a retag first.
class ReferenceCache {
public:
    void invalidate();
    void retag(List, Tag, unsigned width, unsigned height);
    void fill(Reference, Plane, unsigned x, unsigned y, unsigned width,
              unsigned height, const std::vector<uint8_t>& decoded_pixels);
    bool matches(Reference) const;
    uint64_t incarnation(Reference) const;
    unsigned width(Reference, Plane = Plane::Y) const;
    unsigned height(Reference, Plane = Plane::Y) const;
    uint8_t sample(Reference, Plane, int x, int y) const; // AVC edge extension.
private:
    struct Image { std::vector<uint8_t> pixels, resident; };
    struct Entry {
        std::optional<Tag> tag;
        unsigned width = 0, height = 0;
        uint64_t incarnation = 0;
        std::array<Image, 3> planes;
    };
    const Entry& entry(Reference) const;
    std::array<Entry, 2> entries_;
    uint64_t sequence_ = 0;
};

using SampleReader = std::function<uint8_t(int, int)>;
uint8_t luma_qpel(const SampleReader&, int x_q4, int y_q4);
uint8_t chroma_eighth(const SampleReader&, int x_q8, int y_q8);
struct SadPartition { Partition partition; uint64_t sad = 0; };
std::vector<SadPartition> sad_tree(const std::array<uint8_t, 256>& current,
                                 const std::array<uint8_t, 256>& reference);
uint64_t checked_cost(uint64_t sad, uint64_t rate);
struct Candidate { Reference reference; MotionVector mv; uint64_t rate = 0; };
struct Refinement { MotionVector offset; uint64_t rate = 0; }; // Relative to IME winner.
struct Request {
    unsigned mb_x = 0, mb_y = 0; // Absolute, MB-aligned luma sample coordinates.
    Partition partition;
    Picture picture = Picture::P;
    std::vector<Candidate> integer_candidates; // Caller-defined stable order.
    std::vector<Refinement> fractional_candidates;
    MotionVector predicted_mv; // EEI input from caller's neighbour/MVP policy.
};
struct CandidateResult { Candidate candidate; uint64_t sad = 0, cost = 0; };
struct Mode {
    unsigned mb_x = 0, mb_y = 0;
    Partition partition;
    CandidateResult winner;
    uint64_t reference_incarnation = 0;
    MotionVector predicted_mv;
};
struct Decision {
    Mode mode;
    std::vector<CandidateResult> integer_results, fractional_results;
    std::vector<uint8_t> predictor;
    std::vector<int16_t> residual;
};
void validate_request(const ReferenceCache&, const Request&);
std::vector<uint8_t> predict_luma(const ReferenceCache&, unsigned mb_x,
                                unsigned mb_y, Partition, const Candidate&);
// Hook runs before each candidate. A blocking VP adapter may wait for support
// residency/service here without advancing its candidate/phase metadata.
using CandidateHook = std::function<void(const Candidate&, bool integer, unsigned index)>;
Decision evaluate(const ReferenceCache&, const Request&, const std::array<uint8_t, 256>&,
                  const CandidateHook& = {});
void validate_mode(const ReferenceCache&, const Mode&);
struct MotionSyntax {
    Reference reference;
    Partition partition;
    MotionVector mv, mvd;
};
MotionSyntax motion_syntax(const Mode&);
struct Sample {
    Plane plane = Plane::Y;
    unsigned block_index = 0; // AVC 4x4 index in the containing MB.
    unsigned x = 0, y = 0;  // Absolute plane sample coordinates.
    unsigned phase_x = 0, phase_y = 0;
    unsigned sequence = 0;
    uint8_t value = 0;
    bool last = false;
};
std::vector<Sample> sample_layout(const Mode&); // Y, U, V; transform-block order.
uint8_t compensate_sample(const ReferenceCache&, const Mode&, const Sample&);
}
