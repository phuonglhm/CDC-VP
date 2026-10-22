// Lane Management (MAS v1.3 Block No. 2): per-lane PHY delay FIFOs, Burst FSM,
// common read for lane alignment, merging into 32-bit words, abort marker.
//
// Cycle model of the rxbyteclkhs domain: clock() is one rising edge. Plain C++
// (no SystemC) so that it can be unit-tested with synthetic PPI traces; the
// controller samples the PHY and calls it on every edge.
#pragma once

#include <array>
#include <cstdint>
#include <deque>

namespace fx1::csi2 {

// PPI inputs of one rxbyteclkhs edge (values before the edge).
struct PpiSample {
    std::array<uint8_t, 4> data{};
    std::array<bool, 4> valid{}, active{}, sync{}, errsync{};
    bool skewcal = false;  // common rxskewcalhs of this PHY, applied to every lane
};

// One entry of the asynchronous FIFO towards Packet Analyzer.
struct MergedWord {
    uint32_t data = 0;        // byte 0 in bits 7:0
    uint8_t byte_valid = 0;   // 1111, 0111, 0011, 0001; 0000 only in the abort marker
    bool sob = false, eob = false;
    bool is_abort_marker() const { return byte_valid == 0 && sob && eob; }
};

enum class LmState : uint8_t { Idle = 0, Arming = 1, Aligned = 2, Drain = 3, Abort = 4 };

class LmEvents {
public:
    virtual ~LmEvents() = default;
    virtual void lm_word(const MergedWord& w) = 0;
    virtual void lm_abort(bool sync_err, bool fifo_full, bool unarmed, uint8_t lane_sync_seen) = 0;
    virtual void lm_deskew_burst() = 0;
    virtual void lm_state(LmState s) = 0;
    virtual void lm_lanes_applied(unsigned enc) = 0;  // PROTOCOL_CONFIG encoding
};

class LaneManagement {
public:
    explicit LaneManagement(unsigned max_lanes = 4, unsigned skew_depth = 8, unsigned af_margin = 2);

    // resetn, full_reset, or soft_reset seen on an edge: FSM to ST_IDLE,
    // flags and FIFOs cleared, lane count not yet adopted (applied = 00).
    void reset();
    // One rxbyteclkhs edge. active_lanes_enc: PROTOCOL_CONFIG.active_lanes.
    void clock(const PpiSample& in, unsigned active_lanes_enc, LmEvents& ev);

    LmState state() const { return st_; }
    unsigned lanes() const { return n_; }
    // Diagnostics (not in the CSR map): bytes lost on a full FIFO after
    // alignment, which the MAS sizing rules exclude.
    uint64_t dropped_bytes() const { return dropped_; }
    uint64_t resets() const { return resets_; }  // diagnostic: reset() calls

private:
    void merge(const uint8_t* b, unsigned k, bool flush, LmEvents& ev);
    void emit(uint32_t data, uint8_t bv, bool eob, LmEvents& ev);
    bool packer_empty() const { return pos_ == 0 && !have_pending_; }
    bool lane_on(unsigned i) const { return i < n_ && i < max_; }
    void clear_burst();

    const unsigned max_, depth_, margin_;
    LmState st_ = LmState::Idle;
    unsigned n_ = 1, n_enc_ = 0;  // adopted lane count and its encoding
    std::array<std::deque<uint8_t>, 4> fifo_;
    std::array<bool, 4> sync_seen_{}, deskew_seen_{}, eot_{};
    // Lane Merging
    std::array<uint8_t, 4> word_{};
    unsigned pos_ = 0;
    uint32_t pending_ = 0;
    bool have_pending_ = false, first_ = true;
    uint64_t dropped_ = 0, resets_ = 0;
};

}  // namespace fx1::csi2
