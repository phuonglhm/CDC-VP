#pragma once
#include <systemc>
#include <tlm>
#include <array>
#include <cstdint>
#include <vector>
#include <stdexcept>

static_assert(SC_VERSION_MAJOR == 2 && SC_VERSION_MINOR == 3 && SC_VERSION_PATCH == 4,
              "This VP is qualified with SystemC 2.3.4 only");

namespace h264 {
// Decoded sequence fields (PDF table 20-3); only nal_capacity_bytes is a VP allocation setting.
struct SequenceParameters {
    unsigned frame_count = 1;
    unsigned qp = 26;
    uint32_t nal_capacity_bytes = 65536; // VP allocation contract, not a hardware register.
    unsigned gop_m = 1, gop_n = 1;
    unsigned cmb_frames = 0; // Host convenience: 0 means frame_count when encoding registers.
    bool frame_address_mode = false;
    unsigned log2_fn = 0, log2_poc = 0;
    bool force_log = false;
};
enum class PictureType { I, P, B };
struct PictureTask {
    unsigned coding_index = 0, display_index = 0, source_slot = 0;
    PictureType type = PictureType::I;
};
struct FrameConfig {
    unsigned width = 0, height = 0;
    uint32_t cmb = 0, refm = 0, nal = 0;
    uint32_t dfcon = 0, spara0 = 0, spara1 = 0, spara2 = 0; // Raw register snapshot for adapters.
    SequenceParameters sequence;
    unsigned completed_frames = 0, activation_frames = 0;
    bool filter_enabled = false;
    int alpha = 0, beta = 0, slice_qp_delta = 0;
    unsigned crop_bottom = 0;
    uint64_t frame_bytes() const { return uint64_t(width) * height * 3 / 2; }
};
enum class Client { Cmb, SearchWindow, Reference, Nal };
struct Macroblock {
    unsigned frame = 0, x = 0, y = 0;
    std::array<unsigned char, 384> pixels{};
};
struct ProcessedBlock {
    Macroblock reconstructed;
    std::vector<unsigned char> output;
};
struct ProcessingIf {
    virtual ~ProcessingIf() = default;
    // Blocking: callable only from an SC_THREAD; completion means output is ready.
    virtual ProcessedBlock process(const Macroblock& input) = 0;
};
inline uint32_t load_le(const unsigned char* p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
inline void store_le(unsigned char* p, uint32_t v) {
    for (unsigned i = 0; i < 4; ++i) p[i] = static_cast<unsigned char>(v >> (8 * i));
}
inline void consume_delay(sc_core::sc_time& delay) {
    if (delay != sc_core::SC_ZERO_TIME) sc_core::wait(delay);
    delay = sc_core::SC_ZERO_TIME;
}
}
