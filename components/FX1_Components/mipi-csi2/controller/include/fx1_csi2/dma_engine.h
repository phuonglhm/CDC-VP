// DMA (MAS v1.3 Block No. 13 / HAS v1.2 Table 4-27), RAW path only (D04): the
// sys_clk side of the write engine. Claim at capture start, beat intake from
// the fbd_* group, burst assembly, bounds and layout checks, ownership,
// abort / drain / finalisation (spec_decisions M01-M17, M18-M22).
//
// Plain C++. The AXI write itself is issued by a worker thread of the
// controller (one TLM transaction per burst, P03): it takes bursts with
// take_burst() and reports each response with burst_done().
#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <utility>
#include <vector>

#include "fx1_csi2/stream_out.h"

namespace fx1::csi2 {

enum class BufState : uint8_t { HwOwned = 0, SwOwnedValid = 1, SwOwnedError = 2 };

// DMA configuration from the CSR (DMA_CONTROL, DMA_BUFFERn_ADDR, DMA_BUFFER_SIZE,
// LINE_PITCH, DMA_ATTR). Latched at the claim (M09). dma_format is not part of
// it: Format Bridge applies it live and software changes it only while the DMA
// path is disabled and idle (MAS Block 8, M09).
struct DmaConfig {
    bool enable = false, start = false, abort = false, err_policy_release = true;
    std::array<uint32_t, 2> base{};
    uint32_t size = 0;
    uint16_t bytes_per_line = 0;
    uint8_t line_padding = 0;
    uint8_t max_burst_len = 0;  // beats - 1 (D03)
    uint8_t qos = 0, prot = 0, max_outstanding = 0;
};

// One AXI4 INCR burst of 8-byte beats; the last beat may be partial (WSTRB
// from the line tail, data length = bytes actually written).
struct DmaBurst {
    uint64_t addr = 0;
    std::vector<uint8_t> data;
    unsigned beats = 0;
    unsigned side = 0;      // AWID (M14): RAW buffer 0 -> 0, 1 -> 1
    uint16_t capture = 0;   // frame id of the capture
    uint32_t epoch = 0;     // reset epoch (M13)
    uint16_t line = 0;      // line index in the buffer
    bool line_end = false;  // last burst of the line
    uint8_t qos = 0, prot = 0;
};

// Register-side services and events of the DMA (implemented by the controller).
class DmaEvents {
public:
    virtual ~DmaEvents() = default;
    virtual BufState dma_buf_state(unsigned side) = 0;
    // Ownership of a side and its FRAME_ID half (frame_id 0: FRAME_ID unchanged).
    virtual void dma_set_buffer(unsigned side, BufState s, uint16_t frame_id) = 0;
    virtual void dma_set_last_valid_line(unsigned side, uint16_t line) = 0;         // BUFFER_ERROR_INFO
    virtual void dma_set_active_buffer(unsigned side) = 0;                           // DMA_STATUS.active_buffer
    virtual void dma_busy(bool busy) = 0;          // claim .. finalisation, and an old write in flight (M17)
    virtual bool dma_claim_blocked() = 0;          // reset_in_progress: no claim (M17)
    virtual void dma_buf_done(bool ok, uint32_t epoch) = 0;     // IRQ 16 / 17, after the ownership update (M12)
    virtual void dma_overflow(uint32_t epoch) = 0;              // IRQ 13: beat lost, layout or bounds fault
    virtual void dma_axi_error(uint8_t awid, uint8_t resp, uint32_t addr, uint32_t epoch) = 0;  // IRQ 18, M4b
};

// Final outcome of a capture on the DMA path, for the capture ledger (M19).
enum class DmaOutcome : uint8_t { None, Valid, ErrorReleased, Rewound, Refused };

struct DmaParams {
    unsigned raw_fifo_beats = 512;   // RAW_FIFO_DEPTH
    unsigned cmd_fifo = 16;          // CMD_FIFO_DEPTH
    unsigned max_burst_beats = 256;  // MAX_BURST_BEATS
};

class DmaEngine {
public:
    explicit DmaEngine(DmaParams p = {}) : p_(p) {}
    void set_events(DmaEvents* e) { ev_ = e; }

    // resetn / full_reset (full) or soft_reset: queued bursts and captures are
    // discarded, the epoch advances; a write already issued is drained without
    // event (M13).
    void reset(bool full);

    // sys_clk side -----------------------------------------------------------
    // A capture opened (lead Frame Start), frame id `capture`; tpg: its source
    // is the Test Pattern Generator (active_sel at its start). dma_enable = 0:
    // the DMA path does not take part (returns false). Otherwise the claim is
    // made or refused (M10, M04, M11, M17).
    bool capture_start(uint16_t capture, const DmaConfig& cfg, bool core_enable, bool tpg);
    void capture_end(uint16_t capture);
    // Control FSM dma_abort_req: abort every capture holding a buffer (M16).
    void abort_all_ctl();
    // DMA_CONTROL.dma_enable 1 -> 0 / dma_abort 0 -> 1: abort by the DMA (M16).
    void abort_all_dma();
    // One fbd_* beat (valid with the beat); returns fbd_ready. Held low only for
    // a capture of the Test Pattern Generator, which can be stalled (MAS Block 8
    // notes); the source is the one of the beat's capture, not the current
    // active_sel (the tail of a TPG frame can follow the switch back to RX).
    bool beat(bool valid, const OutBeat& b);
    // End of a sys_clk cycle; pipe_empty: no fbd beat of an ended capture can
    // still come (Byte-to-Pixel, Line Buffer, Format Bridge DMA side empty).
    void cycle_end(bool pipe_empty);

    // worker side -------------------------------------------------------------
    bool burst_ready() const { return !queue_.empty(); }
    bool take_burst(DmaBurst& b);  // false: nothing to issue
    // Response of the burst issued last; ok = OKAY.
    void burst_done(const DmaBurst& b, bool ok, uint8_t resp);

    // observation --------------------------------------------------------------
    bool busy() const;
    bool quiescent() const;  // nothing for the sys_clk side to do without input
    unsigned fifo_beats() const { return fifo_beats_; }
    unsigned max_fifo_beats() const { return max_fifo_beats_; }
    uint64_t bursts_issued() const { return issued_; }
    uint32_t epoch() const { return epoch_; }
    // Outcome of captures resolved since the last call (frame id, outcome).
    std::vector<std::pair<uint16_t, DmaOutcome>> take_outcomes();

private:
    enum class Cause : uint8_t { None, Layout, Bounds, Overflow, Axi, Ctl, Dma };
    struct Capture {
        uint16_t id = 0;
        bool claimed = false, tpg = false;
        unsigned side = 0;
        DmaConfig cfg;
        uint32_t stride = 0;
        uint16_t line = 0;         // current line index
        uint32_t line_bytes = 0;   // bytes of the current line received
        bool any_line = false;     // a line completed with every write OKAY so far
        uint16_t last_valid = 0xFFFF;
        bool started = false;      // a beat of the capture has arrived
        bool ended = false;        // Frame End (capture_end) seen
        bool intake_done = false;  // no further beat can come
        Cause abort = Cause::None;
        bool finalised = false;
        unsigned queued = 0, inflight = 0;  // bursts not issued / issued without response
        // open burst
        DmaBurst open;
        bool has_open = false;
        uint32_t open_line_off = 0;
    };
    Capture* find(uint16_t id);
    void close_open(Capture& c);
    void start_abort(Capture& c, Cause cause);
    void try_finalise(Capture& c);
    void update_busy();

    DmaParams p_;
    DmaEvents* ev_ = nullptr;
    uint32_t epoch_ = 0;
    std::deque<Capture> caps_;     // in Frame Start order
    std::deque<DmaBurst> queue_;   // closed bursts not yet issued
    unsigned fifo_beats_ = 0, max_fifo_beats_ = 0;
    unsigned inflight_ = 0;        // issued, response pending (any epoch)
    std::vector<unsigned> stale_rewind_;  // sides to rewind when an old write drains (soft reset)
    uint16_t intake_ = 0;          // capture of the last beat taken
    unsigned last_done_side_ = 1;  // M11: try the side after the last completed one
    uint64_t issued_ = 0;
    bool busy_ = false;
    std::vector<std::pair<uint16_t, DmaOutcome>> outcomes_;
};

}  // namespace fx1::csi2
