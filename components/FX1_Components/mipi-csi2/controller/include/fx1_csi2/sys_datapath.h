// The sys_clk domain of the receiver as one cycle model: the asynchronous FIFO
// after Lane Management, Packet Analyzer (Block No. 3) and Blocks No. 4 to 12
// (spec_decisions C24-C26). Plain C++: the controller calls cycle() on every
// sys_clk edge; unit tests drive it directly with merged words.
#pragma once

#include <cstdint>
#include <deque>

#include "fx1_csi2/dma_engine.h"
#include "fx1_csi2/frame_control.h"
#include "fx1_csi2/lane_management.h"
#include "fx1_csi2/packet_analyzer.h"
#include "fx1_csi2/pixel_path.h"
#include "fx1_csi2/stream_out.h"

namespace fx1::csi2 {

// Consumer of the AXI4-Stream master: called once per cycle with the values the
// master drives; returns TREADY for that cycle. A transfer happens when
// tvalid && TREADY.
class AxisSink {
public:
    virtual ~AxisSink() = default;
    virtual bool axis_cycle(bool tvalid, const AxisBeat& beat) = 0;
};

// Events of the sys_clk domain towards the register file (Interrupt & Error
// Aggregator side). Packet Analyzer events are forwarded through PaEvents,
// payload beats after the discard gate.
class SysEvents {
public:
    virtual ~SysEvents() = default;
    virtual void sys_line_buffer_full() = 0;          // rise(linebuffer_full && !active_sel): IRQ 12
    virtual void sys_line_buffer_overflow() = 0;      // rise(linebuf_overflow): IRQ 13, sticky
    virtual void sys_cdc_overflow() = 0;              // VP: CDC FIFO overflow (C25): IRQ 13
    // Capture ledger (M19): every capture that opens ends once, as dropped or
    // delivered (D06, C34).
    virtual void sys_frame_dropped() = 0;             // IRQ 14, FRAME_DROP_COUNT
    virtual void sys_capture_start(uint16_t frame_id, uint64_t timestamp_ns) = 0;  // TIMESTAMP (C30)
    virtual void sys_capture_done() = 0;              // delivered: IRQ 15, FRAME_COUNT
    virtual void sys_published(unsigned vc, const FrameInfo& info) = 0;
    virtual void sys_resolution_mismatch(unsigned vc) = 0;  // IRQ 20
    virtual void sys_err_wc_align() = 0;                    // IRQ 21
    virtual void sys_fmtbridge_overflow() = 0;              // IRQ 22
    virtual void sys_frame_num_err(unsigned vc) = 0;        // IRQ 23
};

struct SysConfig {
    PaConfig pa;
    FcConfig fc;
    FbConfig fb;
    TpgConfig tpg;
    DmaConfig dma;
    bool core_enable = true;
};

struct SysParams {
    unsigned cdc_depth = 16;          // asynchronous FIFO after Lane Management (C25)
    unsigned cdc_latency_cycles = 2;  // SYNC_STAGES: a word is readable 2 sys_clk edges after its write
    unsigned line_buffer_depth = 2048, full_watermark = 1740;
    unsigned stream_fifo_depth = 16, dma_fifo_depth = 8;
    DmaParams dma;
};

class SysDatapath : private PaEvents {
public:
    explicit SysDatapath(SysParams p = {});
    void set_pa_events(PaEvents* e) { pa_ev_ = e; }
    void set_sys_events(SysEvents* e) { sys_ev_ = e; }
    void set_axis_sink(AxisSink* s) { axis_sink_ = s; }

    // rxbyteclkhs side: one word written at time t_ps. A full FIFO loses words (C25).
    void cdc_push(const MergedWord& w, uint64_t t_ps);
    void cdc_flush();
    // core_enable 1 -> 0: Packet FSM to ST_IDLE, per-VC contexts cleared, FIFO
    // flushed, and a stop latched for the rest of the sys_clk domain (C37): it
    // sees core_enable = 0, and the FIFO stays flushed, until the open captures
    // have ended as dropped, Control FSM has reached ST_IDLE (through
    // HOLD/FLUSH when needed) and the stream master has completed the abort
    // (last abort beat accepted), even if core_enable is set again meanwhile.
    void core_disable();
    bool stop_pending() const { return stop_hold_; }

    // soft_reset (full = false) or resetn / full_reset.
    void reset(bool full);
    // One sys_clk edge at time now_ps; now_ns is the timestamp counter.
    void cycle(const SysConfig& cfg, uint64_t now_ps, uint64_t now_ns);
    // No cycle would change any state unless an input changes (C26).
    bool quiescent(const SysConfig& cfg) const;

    // Observation.
    const PacketAnalyzer& packet_analyzer() const { return pa_; }
    const FrameContext& frame_context() const { return fc_; }
    const ControlFsm& control_fsm() const { return ctl_; }
    const LineBuffer& line_buffer() const { return lb_; }
    const FormatBridge& format_bridge() const { return fb_; }
    const AxisMaster& axis_master() const { return axis_; }
    const TpgMux& tpg_mux() const { return mux_; }
    const TestPatternGenerator& tpg() const { return tpg_; }
    DmaEngine& dma() { return dma_; }
    const DmaEngine& dma() const { return dma_; }
    // Resolve the capture ledger with the DMA outcomes available now (also
    // called by the DMA worker right after a response, M12 ordering).
    void resolve_ledger();
    unsigned cdc_fill() const { return static_cast<unsigned>(cdc_.size()); }
    unsigned cdc_max_fill() const { return cdc_max_; }
    uint64_t cdc_lost_words() const { return cdc_lost_; }
    uint64_t cycles() const { return cycles_; }

private:
    // PaEvents, called by pa_.push() inside cycle().
    void pa_beat(const PayloadBeat& b) override;
    void pa_error(PaError e, int vc, unsigned line) override;
    void pa_packet_done() override;
    void pa_frame_start(unsigned vc, uint16_t frame_number) override;
    void pa_frame_end(unsigned vc, uint16_t frame_number) override;
    void pa_gsp(uint8_t vc, uint8_t dt, uint16_t data) override;
    void pa_null_blank() override;
    void pa_vc_unsupported() override;

    struct CdcEntry {
        MergedWord w;
        uint64_t t_ps;      // write time (rxbyteclkhs edge)
        unsigned edges = 0;  // sys_clk edges seen after the write
    };

    SysParams p_;
    PaEvents* pa_ev_ = nullptr;
    SysEvents* sys_ev_ = nullptr;
    AxisSink* axis_sink_ = nullptr;

    std::deque<CdcEntry> cdc_;
    bool cdc_dropping_ = false, cdc_marker_pending_ = false;
    unsigned cdc_max_ = 0;
    uint64_t cdc_lost_ = 0;

    PacketAnalyzer pa_;
    ByteToPixel b2p_;
    TestPatternGenerator tpg_;
    TpgMux mux_;
    LineBuffer lb_;
    FormatBridge fb_;
    AxisMaster axis_;
    FrameContext fc_;
    ControlFsm ctl_;
    DmaEngine dma_;
    struct LedgerEntry {
        uint16_t id = 0;
        bool dma = false;  // the DMA path took part (dma_enable at capture start)
        DmaOutcome out = DmaOutcome::None;
        bool ended = false, ctl_dropped = false;
    };
    std::deque<LedgerEntry> ledger_;

    // Per-cycle scratch filled by the PaEvents callbacks.
    const SysConfig* cfg_ = nullptr;
    uint8_t discard_ = 3;
    bool bp_valid_ = false;
    PayloadBeat bp_;
    bool fc_fs_ = false, fc_fe_ = false;
    uint8_t fc_vc_ = 0;
    CtlIn ctl_in_;

    bool stop_hold_ = false;  // C37: latched core_enable 1 -> 0 not yet processed
    bool lb_full_q_ = false;  // edge detection of IRQ 12
    bool lb_ovf_q_ = false;   // edge detection of IRQ 13
    uint64_t cycles_ = 0;
};

}  // namespace fx1::csi2
