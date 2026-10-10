#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace h264::intra {
enum class Plane { Y, U, V };
enum class Kind { Luma4x4, Luma16x16, Chroma8x8 };
enum class Metric { Sad, Satd };
struct Block {
    Plane plane = Plane::Y;
    Kind kind = Kind::Luma4x4;
    unsigned x = 0, y = 0; // Plane sample coordinates, not MB coordinates.
    unsigned size() const;
};
struct References {
    std::array<uint8_t, 16> top{}, left{};
    uint8_t upper_left = 128;
    bool has_top = false, has_left = false;
    bool has_upper_left = false, has_upper_right = false;
};
struct Token {
    uint64_t generation = 0, sequence = 0;
    bool operator==(const Token& b) const {
        return generation == b.generation && sequence == b.sequence;
    }
};
struct Candidate { unsigned mode; uint64_t distortion, cost; };
struct Decision {
    Block block;
    Token token;
    References references; // Same snapshot for evaluation and replay.
    unsigned mode = 0;
    uint64_t cost = 0;
    std::vector<Candidate> candidates;
    std::vector<uint8_t> predictor;
    std::vector<int16_t> residual;
};
// H.264 mode numbers: luma4 0..8; luma16 V/H/DC/plane 0..3;
// chroma DC/H/V/plane 0..3. No HEVC mode numbering.
std::vector<unsigned> legal_modes(Kind, const References&);
std::vector<uint8_t> predict(Kind, unsigned mode, const References&);
uint64_t distortion(const std::vector<int16_t>&, unsigned size, Metric);

class Core {
public:
    void start_frame(unsigned coded_width, unsigned coded_height);
    void reset();
    uint64_t generation() const { return generation_; }
    bool frame_active() const { return active_; }
    bool busy() const { return pending_.has_value(); }
    References resolve(const Block&) const;
    const Decision& evaluate(const Block&, const std::vector<uint8_t>& source,
                             Metric = Metric::Sad,
                             const std::array<uint32_t, 9>& mode_penalty = {});
    const Decision& replay(Token) const;
    void reconstruct(Token, const std::vector<uint8_t>& decoded_pixels);
    // Adapter import of already reconstructed pixels only; never source/CMB.
    void import_reconstructed(const Block&, const std::vector<uint8_t>&);
    std::optional<unsigned> committed_mode(const Block&) const;
private:
    struct Memory {
        unsigned width = 0, height = 0;
        std::vector<uint8_t> pixels, valid;
        std::vector<int> modes;
    };
    const Memory& memory(const Block&) const;
    Memory& memory(const Block&);
    void validate(const Block&) const;
    void store(const Block&, const std::vector<uint8_t>&, int mode);
    std::array<Memory, 3> memory_;
    bool active_ = false;
    uint64_t generation_ = 0, sequence_ = 0;
    std::optional<Decision> pending_;
};
}
