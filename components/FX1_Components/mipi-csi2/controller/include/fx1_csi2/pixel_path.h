// Pixel path of the sys_clk domain: Byte-to-Pixel Unpacking (MAS v1.3 Block
// No. 4), Test Pattern Generator (No. 5), TPG Mux (No. 6) and Line Buffer
// (No. 7).
//
// Cycle model (spec_decisions C24): every block is a plain C++ object whose
// clock() is one rising edge of sys_clk. Outputs that the MAS registers are
// returned by accessors and change only in clock(); combinational outputs are
// functions of the registered state and of the inputs of the same cycle.
#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "fx1_csi2/packet_analyzer.h"

namespace fx1::csi2 {

// 48-bit pixel bus of Blocks 4 -> 6 -> 7 -> 8: four 12-bit lanes, P0 in lane 0;
// RAW10 pixels in the 10 LSBs. count = px_valid_count (0, 2 or 4); lanes at
// and above count are 0.
struct PxBeat {
    std::array<uint16_t, 4> px{};
    uint8_t count = 0;
    bool sof = false, sol = false, eol = false, err_wc_align = false;
    uint8_t vc = 0;
    bool raw10 = false;  // px_dt_sel: 1 = RAW10, 0 = RAW12
    // VP sideband (M18): frame id of the capture the beat belongs to, 0 = none.
    // Set at the Line Buffer write port; not part of the MAS buses, not compared.
    uint16_t tag = 0;
    bool operator==(const PxBeat& o) const {
        return px == o.px && count == o.count && sof == o.sof && sol == o.sol && eol == o.eol &&
               err_wc_align == o.err_wc_align && vc == o.vc && raw10 == o.raw10;
    }
};

// ---------------------------------------------------------------- Block 4
class ByteToPixel {
public:
    void reset();
    // bp: the image beat of this cycle (bp_valid = 1), nullptr otherwise.
    void clock(const PayloadBeat* bp);
    // Output register: px_valid = out().count != 0; err_wc_align on the px_eol cycle.
    const PxBeat& out() const { return out_; }
    bool quiescent() const { return out_.count == 0; }

private:
    std::array<uint8_t, 4> carry_{};
    unsigned carry_cnt_ = 0;
    bool sof_pend_ = false, sol_pend_ = false;
    bool raw10_ = false;  // format of the line in progress (K of carry_cnt)
    uint8_t vc_ = 0;
    PxBeat out_;
};

// ---------------------------------------------------------------- Block 5
struct TpgConfig {
    bool enable = false;      // TPG_CONTROL.tpg_enable
    bool horizontal = false;  // TPG_CONTROL.pattern: 0 vertical, 1 horizontal bars
    bool raw10 = false;       // TPG_CONTROL.tpg_pixel_format: 1 = RAW10
    uint16_t width = 0, height = 0, hblank = 0, vblank = 0;
    uint8_t cfa = 0;          // no CSR field in workbook v1.4 (D11): RGGB
};

class TestPatternGenerator {
public:
    enum class State : uint8_t { Idle, Calc, Running, Stopping };
    struct Events {
        bool fs = false, fe = false, eof = false;  // tpg_fs / tpg_fe / tpg_eof (VC0 only, C28)
    };
    void reset();
    // Combinational outputs of the current state.
    bool px_valid() const;
    PxBeat px() const;  // count = 4
    // One edge. run = tpg_enable && core_enable (MAS Control FSM).
    Events clock(const TpgConfig& cfg, bool core_enable, bool px_ready, bool linebuf_empty);
    State state() const { return st_; }
    uint16_t height_shadow() const { return h_; }
    // Idle and not about to start (the start also needs an empty Line Buffer).
    bool quiescent(const TpgConfig& cfg, bool core_enable) const;

private:
    State st_ = State::Idle;
    // shadows (latched in CALC_THRESHOLDS)
    uint16_t w_ = 0, h_ = 0, hblank_ = 0, vblank_ = 0;
    bool horizontal_ = false, raw10_ = false;
    uint8_t cfa_ = 0;
    std::array<uint32_t, 7> thr_{};
    // datapath
    uint16_t x_ = 0, y_ = 0;
    uint32_t blank_cnt_ = 0, vblank_lines_ = 0;
    bool eof_pend_ = false;
};

// ---------------------------------------------------------------- Block 6
class TpgMux {
public:
    enum class State : uint8_t { Streaming, Draining, Acquiring };
    struct In {
        const PxBeat* rx = nullptr;  // Byte-to-Pixel output (valid when count != 0)
        bool tpg_valid = false;
        PxBeat tpg;
        bool fc_fs = false, fc_fe = false;  // Packet Analyzer frame events of this cycle
        uint8_t fc_vc = 0;
        bool rx_ctx_clear = false;
        bool tpg_fs = false, tpg_eof = false;
        bool lb_full = false, lb_empty = true;
        bool tpg_enable = false;  // pending_sel
    };
    void reset();
    bool tpg_ready(const In& in) const;
    // The beat presented to the Line Buffer write port this cycle (false: px_valid = 0).
    bool output(const In& in, PxBeat& out) const;
    void clock(const In& in);
    bool active_sel() const { return active_; }
    bool switch_pending() const { return st_ != State::Streaming; }
    State state() const { return st_; }
    bool rx_frame_active() const { return vc_active_[0] || vc_active_[1]; }
    // No transition is possible without a new input event.
    bool quiescent(bool tpg_enable, bool lb_empty) const;

private:
    bool incoming_sof(const In& in) const;
    State st_ = State::Streaming;
    bool active_ = false, armed_ = false, tpg_frame_active_ = false;
    std::array<bool, 2> vc_active_{};
};

// ---------------------------------------------------------------- Block 7
class LineBuffer {
public:
    explicit LineBuffer(unsigned depth = 2048, unsigned watermark = 1740);
    void reset();  // resetn / soft_reset / lb_flush: pointers and output stage
    unsigned depth() const { return depth_; }
    unsigned fill() const { return fill_; }
    bool full() const { return fill_ >= wm_; }  // linebuffer_full
    bool empty() const { return fill_ == 0; }   // linebuf_empty
    bool out_valid() const { return out_valid_; }
    const PxBeat& out() const { return out_; }
    // One edge: wr = Line Buffer write port (nullptr: px_valid = 0), fb_ready of
    // this cycle. Returns linebuf_overflow (a write refused this cycle).
    bool clock(const PxBeat* wr, bool fb_ready);
    unsigned max_fill() const { return max_fill_; }  // diagnostic

private:
    unsigned depth_, wm_;
    std::vector<PxBeat> mem_;
    unsigned wr_ = 0, rd_ = 0, fill_ = 0;
    bool inflight_ = false;
    unsigned inflight_addr_ = 0;
    bool out_valid_ = false, skid_valid_ = false;
    PxBeat out_, skid_;
    unsigned max_fill_ = 0;
};

}  // namespace fx1::csi2
