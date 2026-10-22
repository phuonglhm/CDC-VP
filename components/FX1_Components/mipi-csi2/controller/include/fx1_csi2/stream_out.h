// Output side of the sys_clk domain: Format Bridge (MAS v1.3 Block No. 8) and
// AXI4-Stream Master (Block No. 12). Same cycle model as pixel_path.h (C24).
#pragma once

#include <array>
#include <cstdint>
#include <deque>

#include "fx1_csi2/pixel_path.h"

namespace fx1::csi2 {

// One 64-bit beat of the fbs_* / fbd_* groups (byte 0 in bits 7:0).
struct OutBeat {
    uint64_t data = 0;
    uint8_t keep = 0;  // fbs_byte_en / fbd_byte_en
    bool sof = false, sol = false, eol = false, err = false;
    uint8_t vc = 0;
    uint16_t tag = 0;  // VP sideband (M18), carried like vc_tag; not compared
    bool operator==(const OutBeat& o) const {
        return data == o.data && keep == o.keep && sof == o.sof && sol == o.sol && eol == o.eol && err == o.err &&
               vc == o.vc;
    }
};

// Static configuration of the two output paths, from the CSR (C27).
struct FbConfig {
    bool axis_enable = false, stream_raw16 = false;  // STREAM_CONFIG
    bool dma_enable = false, dma_raw16 = false;      // DMA_CONTROL
};

// ---------------------------------------------------------------- Block 8
class FormatBridge {
public:
    explicit FormatBridge(unsigned stream_depth = 16, unsigned dma_depth = 8);
    void reset();  // resetn / soft_reset / fb_flush
    bool ready(const FbConfig& cfg) const;  // fb_ready
    bool stream_valid() const { return !sfifo_.empty(); }
    const OutBeat& stream_head() const { return sfifo_.front(); }
    bool dma_valid() const { return !dfifo_.empty(); }
    const OutBeat& dma_head() const { return dfifo_.front(); }
    unsigned stream_count() const { return static_cast<unsigned>(sfifo_.size()); }
    unsigned dma_count() const { return static_cast<unsigned>(dfifo_.size()); }
    // One edge. take: the Line Buffer entry transferred this cycle (fb_px_valid
    // && fb_ready), nullptr otherwise; pops of the two FIFOs by their consumers.
    // Returns fb_overflow.
    bool clock(const FbConfig& cfg, const PxBeat* take, bool stream_pop, bool dma_pop);
    bool quiescent() const;
    // No beat on the DMA side: FIFO empty and no line-end flush pending.
    bool dma_side_empty() const { return dfifo_.empty() && !rep_.flush_req() && !r16_.flush_req(); }

private:
    // One accumulate-and-drain formatter (bypass re-packer or RAW16).
    struct Formatter {
        std::array<uint8_t, 16> buf{};
        unsigned cnt = 0;
        bool sof_pend = false, sol_pend = false, eol_pend = false;
        bool flush_req() const { return eol_pend && cnt != 0; }
        // Returns true and fills `beat` when this formatter emits a beat this cycle.
        bool step(const uint8_t* bytes, unsigned n, bool in_sof, bool in_sol, bool in_eol, bool err_hold, uint8_t vc,
                  uint16_t tag, OutBeat& beat);
    };
    unsigned sdepth_, ddepth_;
    Formatter rep_, r16_;
    bool dt_hold_raw10_ = false, err_hold_ = false;
    uint8_t vc_hold_ = 0;
    uint16_t tag_hold_ = 0;
    std::deque<OutBeat> sfifo_, dfifo_;
};

// AXI4-Stream transfer (IHI0051B). tuser bit 0 = start_of_frame, bit 1 = packet_error.
struct AxisBeat {
    uint64_t tdata = 0;
    uint8_t tkeep = 0;
    bool tlast = false;
    uint8_t tuser = 0;
    uint8_t tid = 0;
    bool operator==(const AxisBeat& o) const {
        return tdata == o.tdata && tkeep == o.tkeep && tlast == o.tlast && tuser == o.tuser && tid == o.tid;
    }
};

// ---------------------------------------------------------------- Block 12
class AxisMaster {
public:
    enum class State : uint8_t { Passthrough, Drain, Inject };
    void reset();
    bool tvalid() const { return out_valid_; }
    const AxisBeat& beat() const { return out_; }
    bool fbs_ready() const { return st_ != State::Passthrough || !skid_valid_; }
    // One edge. tready: TREADY sampled this cycle; fbs: head of the stream FIFO
    // (valid with fbs_valid). Returns true when the fbs beat was taken.
    bool clock(bool tready, bool fbs_valid, const OutBeat& fbs, bool abort_req, uint8_t abort_mask, bool axis_enable);
    bool abort_done() const { return abort_done_; }  // pulse of the last edge (observation)
    State state() const { return st_; }
    bool line_open() const { return line_open_; }
    bool quiescent() const { return st_ == State::Passthrough && !out_valid_ && !skid_valid_ && abort_mask_ == 0; }

private:
    State st_ = State::Passthrough;
    bool out_valid_ = false, skid_valid_ = false;
    AxisBeat out_, skid_;
    bool line_open_ = false;
    uint8_t line_tid_ = 0, abort_mask_ = 0;
    std::array<bool, 2> pass_{true, true};
    bool abort_done_ = false;
};

}  // namespace fx1::csi2
