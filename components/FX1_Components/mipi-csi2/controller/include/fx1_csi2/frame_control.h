// Frame Context & Metadata Capture (MAS v1.3 Block No. 9, without Record
// Builder: metadata DMA deferred, D04) and Control FSM (Block No. 10).
// Same cycle model as pixel_path.h (C24).
#pragma once

#include <array>
#include <cstdint>

#include "fx1_csi2/packet_analyzer.h"

namespace fx1::csi2 {

// ---------------------------------------------------------------- Block 9

struct FcConfig {
    bool core_enable = true;
    bool fnum_check_en = false, metadata_capture_en = false, emb_extract_en = false;  // METADATA_DT_CONFIG
    uint16_t expected_width = 0, expected_height = 0;                                 // EXPECTED_RESOLUTION
    uint32_t record_id = 0;                                                           // METADATA_RECORD_ID
    std::array<uint32_t, 2> emb_cfg{};                                                // EMB_EXTRACT_CFG0/1
    bool active_sel = false;  // event source: 0 = Packet Analyzer, 1 = Test Pattern Generator
    bool dual_vc = false;     // pairing of the selected source (linear profile: false, C28)
    uint8_t lead_vc = 0;      // linear profile: VC0 (D09)
};

// Published channel registers: VCn_IMAGE_INFO1/2, EMBEDDED_DATA_INFO_VCn,
// EMB_EXTRACT_VCn_VALm.
struct FrameInfo {
    uint16_t line_count = 0, byte_count = 0;
    uint8_t data_type = 0;
    bool width_mismatch = false, height_mismatch = false;
    uint16_t emb_words = 0;
    uint8_t emb_lines = 0;
    std::array<uint32_t, 2> extract{};
};

// Pulses and events of one sys_clk cycle.
struct FcCycleOut {
    std::array<bool, 2> capture_start{}, capture_end{}, capture_dropped{};  // per slot
    bool ev_fs = false;
    uint8_t ev_vc = 0;
    bool capture_done = false;               // a capture ended and was not dropped
    std::array<bool, 2> err_frame_num{};     // per channel
    std::array<bool, 2> err_resolution{};    // per channel
    std::array<bool, 2> published{};         // published registers of the channel reloaded
};

class FrameContext {
public:
    // resetn / full_reset (full) or soft_reset: contexts, slots, published
    // registers. The timestamp and frame_id counters survive soft_reset.
    void reset(bool full);
    // Start of a cycle: configuration, timestamp counter (ns) and the
    // capture_drop command of Control FSM (applied to the attachments held
    // before the frame events of the cycle).
    void begin_cycle(const FcConfig& cfg, uint64_t now_ns, bool capture_drop, uint8_t drop_vc_mask);
    // Frame events of the selected source (Event Source Select is done by the
    // caller from active_sel).
    void frame_start(unsigned vc, uint16_t frame_number);
    void frame_end(unsigned vc);
    // Payload after the discard gate of Packet Analyzer (sensor path only).
    void image_beat(const PayloadBeat& b);
    void emb_beat(const PayloadBeat& b);
    const FcCycleOut& end_cycle();

    // Live context (Interrupt & Error Aggregator).
    uint16_t cur_frame_id(unsigned vc) const;
    uint16_t cur_line(unsigned vc) const { return geo_[vc & 1].line_count; }
    uint16_t newest_frame_id() const { return any_alloc_ ? slot_[last_alloc_].frame_id : 0; }
    const FrameInfo& published(unsigned vc) const { return pub_[vc & 1]; }
    uint64_t slot_timestamp(unsigned s) const { return slot_[s & 1].timestamp; }
    uint16_t slot_frame_id(unsigned s) const { return slot_[s & 1].frame_id; }
    bool orphan(unsigned vc) const { return ch_[vc & 1].orphan; }
    bool channel_active(unsigned vc) const { return ch_[vc & 1].active; }
    bool slot_open(unsigned s) const { return slot_[s & 1].open; }
    bool quiescent() const;

private:
    struct Channel {
        bool active = false, orphan = true;
        uint8_t slot = 0;
        uint16_t last_fnum = 0;
        bool fnum_err = false;
    };
    struct Slot {
        bool open = false, wait_lag = false, dropped = false;
        uint16_t frame_id = 0;
        uint64_t timestamp = 0;
        uint32_t record_id = 0;
    };
    struct Geometry {
        uint16_t byte_acc = 0, byte_count_last = 0, line_count = 0;
        uint8_t dt = 0;
        bool width_mismatch = false;
        uint16_t exp_w = 0, exp_h = 0;
    };
    struct Embedded {
        uint16_t words = 0;
        uint8_t lines = 0;
        uint32_t byte_ptr = 0;
        std::array<uint32_t, 2> extract{};  // kept across frames (cleared by reset)
    };
    void open_channel(unsigned vc);
    void close_channel(unsigned vc, bool sync);
    bool attached_active(unsigned s) const;

    FcConfig cfg_;
    uint64_t now_ns_ = 0;
    std::array<Channel, 2> ch_{};
    std::array<Slot, 2> slot_{};
    std::array<Geometry, 2> geo_{};
    std::array<Embedded, 2> emb_{};
    std::array<FrameInfo, 2> pub_{};
    unsigned last_alloc_ = 0;
    bool any_alloc_ = false;
    uint16_t frame_id_cnt_ = 0;
    bool prev_active_sel_ = false;
    FcCycleOut out_;
};

// ---------------------------------------------------------------- Block 10

struct CtlIn {
    bool err_ecc2 = false, err_lane_abort = false;  // channel unknown
    uint8_t err_crc = 0, err_wc = 0;               // bit n: error on VC n
    bool err_wc_unknown = false;                   // truncated header (C21), channel unknown (C31)
    bool lb_full = false, lb_overflow = false, fb_overflow = false;
    bool core_enable = true, axis_enable = false, active_sel = false;
    std::array<bool, 2> capture_start{}, capture_end{}, capture_dropped{};
    bool ev_fs = false;
    uint8_t ev_vc = 0, lead_vc = 0;
    bool dma_drop = false;  // from the DMA (M4); 0 in M3b
};

// Commands of Control FSM, registered: valid for the whole cycle.
struct CtlOut {
    uint8_t force_discard = 3;
    bool lb_flush = false, fb_flush = false, rx_ctx_clear = false;
    bool asm_abort_req = false;
    uint8_t asm_abort_mask = 0;
    bool capture_drop = false;
    uint8_t drop_vc_mask = 0;
    bool dma_abort_req = false;
};

class ControlFsm {
public:
    enum class State : uint8_t { Idle, Run, Hold, Flush, Skip };
    static constexpr unsigned kHoldCycles = 2;  // HOLD_CYCLES = OUT_PIPE_STAGES + 1
    void reset();
    CtlOut outputs(bool active_sel) const;
    // One edge; returns frame_drop_pulse of this cycle.
    bool clock(const CtlIn& in);
    State state() const { return st_; }
    uint8_t discard() const { return discard_; }
    bool quiescent(bool core_enable, bool axis_enable) const;

private:
    State st_ = State::Idle;
    unsigned hold_cnt_ = 0;
    uint8_t discard_ = 3;
    bool lead_seen_ = false, drop_now_q_ = false, core_enable_q_ = false, axis_enable_q_ = false,
         stream_stop_q_ = false, drop_pend_ = false;
    std::array<bool, 2> cap_open_{}, dma_mark_{};
    unsigned last_slot_ = 0;
};

}  // namespace fx1::csi2
