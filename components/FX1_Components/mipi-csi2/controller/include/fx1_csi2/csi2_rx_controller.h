// MIPI CSI-2 RX controller model: CSR & AXI4-Lite subordinate (workbook v1.4),
// PHY reset sequencer, interrupt & error aggregator and the control side of
// the PHY adapter, connected to the detailed D-PHY model.
//
// Contracts: docs/REGISTER_CONTRACT.md, docs/spec_decisions.md (IDs below).
// M2 scope: every register location with its access semantics and side
// effects; PHY_CONFIG drives the real PHY through DphyAdapter and
// PHY_STATUS/LANE_STATUS and IRQ bits 9–11 come from the PHY. The datapath
// blocks of M3/M4 report through the hw_* interface; in M2 unit tests use it
// to inject events (it is a C++ interface, never part of the CSR map).
// M3b: the sys_clk domain (Packet Analyzer to AXI4-Stream Master) runs as a
// cycle model (SysDatapath) clocked by an SC_THREAD at sys_clk (C24-C26).
#pragma once

#include <array>
#include <cstdint>
#include <deque>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include "fx1_csi2/fx1_csi2_regs.h"
#include "fx1_csi2/lane_management.h"
#include "fx1_csi2/packet_analyzer.h"
#include "fx1_csi2/sys_datapath.h"
#include "fx1_csi2_phy/dphy_adapter.h"
#include "fx1_csi2_phy/dphy_rx_wrapper.h"

namespace fx1::csi2 {

struct Csi2ControllerConfig {
    unsigned max_lanes = 4;                    // MAX_LANES build parameter: 1, 2 or 4
    uint32_t ip_version = 0x00010100u;         // D01
    unsigned gsp_fifo_depth = 8;               // SP_FIFO_DEPTH
    unsigned exposure_count = 1;               // D09: linear build profile (only 1 supported)
    sc_core::sc_time csr_latency = sc_core::SC_ZERO_TIME;
    sc_core::sc_time sys_clk_period = sc_core::sc_time(4, sc_core::SC_NS);  // sys_clk 250 MHz (C24)
    sc_core::sc_time mm_clk_period = sc_core::sc_time(5, sc_core::SC_NS);   // mm_aclk 200 MHz (M21)
    SysParams sys;                             // datapath capacities (C25, C27)
    DphyAdapterConfig phy;                     // P08: build-time PHY parameters
};

// Bit numbers of IRQ_STATUS (workbook rows 106–129).
enum IrqBit : unsigned {
    IRQ_FRAME_LEVEL_ERR_VC0 = 0, IRQ_FRAME_SYNC_ERR_VC0 = 1, IRQ_FRAME_LEVEL_ERR_VC1 = 2,
    IRQ_FRAME_SYNC_ERR_VC1 = 3, IRQ_CRC_ERROR = 4, IRQ_ECC_1BIT = 5, IRQ_ECC_2BIT = 6,
    IRQ_UNSUPPORTED_DT = 7, IRQ_WC_CORRUPTION = 8, IRQ_SOT_ERROR = 9, IRQ_SOT_SYNC_ERROR = 10,
    IRQ_STOP_STATE = 11, IRQ_LINE_BUFFER_FULL = 12, IRQ_BUFFER_OVERFLOW = 13, IRQ_FRAME_DROPPED = 14,
    IRQ_FRAME_RECEIVED = 15, IRQ_BUF_DONE_OK = 16, IRQ_BUF_DONE_ERR = 17, IRQ_AXI_WRITE_ERROR = 18,
    IRQ_LANE_ALIGN_ABORT = 19, IRQ_RESOLUTION_MISMATCH = 20, IRQ_ERR_WC_ALIGN = 21,
    IRQ_FMTBRIDGE_OVERFLOW = 22, IRQ_FRAME_NUM_SEQ_ERR = 23, IRQ_COUNT = 24,
};

enum class Counter { Packet, Frame, Ecc1, Ecc2, Crc, FrameDrop, LaneAbort, AxiSlverr, AxiDecerr };

// AXI4 write attributes of a DMA burst (M14, M21), attached to every TLM write
// of the DMA. The FX1 bus ignores extensions: these values are not observable
// on the platform (M14).
struct Csi2AxiExtension : tlm::tlm_extension<Csi2AxiExtension> {
    uint8_t awid = 0;
    uint8_t awlen = 0;    // beats - 1
    uint8_t awsize = 3;   // 8 bytes per beat
    uint8_t awburst = 1;  // INCR
    uint8_t awqos = 0, awprot = 0;
    uint8_t last_wstrb = 0xFF;  // WSTRB of the last beat
    tlm::tlm_extension_base* clone() const override { return new Csi2AxiExtension(*this); }
    void copy_from(const tlm::tlm_extension_base& o) override { *this = static_cast<const Csi2AxiExtension&>(o); }
};

// Observation tap of the Packet Analyzer outputs (bp_* / emb_* after the
// discard gate, FS/FE); not part of the datapath since M3b.
class PayloadSink {
public:
    virtual ~PayloadSink() = default;
    virtual void beat(const PayloadBeat& b) = 0;
    virtual void frame_start(unsigned vc, uint16_t frame_number) = 0;
    virtual void frame_end(unsigned vc, uint16_t frame_number) = 0;
};

class Csi2RxController : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<Csi2RxController> csr_socket{"csr_socket"};
    // DMA AXI4 manager, write only (M4). Optional: a model without DDR leaves it
    // unbound and every DMA write then fails with DECERR.
    tlm_utils::simple_initiator_socket_optional<Csi2RxController> dma_socket{"dma_socket"};
    sc_core::sc_in<bool> resetn{"resetn"};  // global asynchronous reset, active low
    sc_core::sc_out<bool> irq{"irq"};       // csi2_irq (P01: PLIC source 7 on fx1_soc)

    SC_HAS_PROCESS(Csi2RxController);
    Csi2RxController(sc_core::sc_module_name nm, DphyRxWrapper& phy, Csi2ControllerConfig cfg = {});

    // ---- hw_* interface: datapath blocks (M3/M4), test injection in M2 ------
    // Reset epoch (M13): stamp an event with it at its source; an event with an
    // older epoch is never recorded.
    uint32_t epoch() const { return epoch_; }
    // Record an interrupt event (C08: dropped while a reset suppresses events).
    void hw_raise_irq(unsigned bit, uint32_t source_epoch);
    void hw_raise_irq(unsigned bit) { hw_raise_irq(bit, epoch_); }
    void hw_count(Counter c, uint32_t n = 1);  // saturating (C09)
    // Write hardware-owned bits of a RO register; `mask` must lie inside the
    // bits the datapath owns (reported as an error otherwise).
    void hw_set_ro(uint32_t offset, uint32_t mask, uint32_t value);
    bool hw_gsp_push(uint8_t vc, uint8_t dt, uint16_t data);  // false: FIFO full, entry lost
    void hw_latch_timestamp();                                // capture opened
    void hw_latch_timestamp_ns(uint64_t ns);                  // same with an explicit counter value
    void hw_set_buffer(unsigned side, BufState s, uint16_t frame_id);
    // DMA drain interface (M13, M16, M17). The DMA reports whether a capture
    // is in flight; while one is, an abort or reset cause leaves a drain
    // pending until hw_drain_complete().
    void hw_set_capture_in_flight(bool v) { capture_in_flight_ = v; }
    void hw_drain_complete();
    bool reset_drain_pending() const { return reset_drain_pending_; }
    bool disable_drain_pending() const { return disable_drain_pending_; }
    const sc_core::sc_event& drain_request_event() const { return drain_req_ev_; }
    const sc_core::sc_event& buffer_release_event() const { return release_ev_; }

    // ---- receive datapath (M3a/M3b)
    void set_payload_sink(PayloadSink* s) { sink_ = s; }
    // AXI4-Stream output towards the ISP; without a sink the path is always
    // ready and its beats are discarded. The DMA writes through dma_socket (M4).
    void set_stream_sink(AxisSink* s) { sys_.set_axis_sink(s); }
    const LaneManagement& lane_management() const { return lm_; }
    const PacketAnalyzer& packet_analyzer() const { return sys_.packet_analyzer(); }
    const SysDatapath& sys_datapath() const { return sys_; }
    // X/Z sampled on the PPI of an enabled lane (diagnostic, not in the CSR map).
    uint64_t ppi_x_samples() const { return ppi_x_; }

    // ---- observation for tests
    DphyAdapter& phy_adapter() { return adapter_; }
    uint32_t irq_status() const { return irq_status_; }
    bool reset_in_progress() const;

private:
    struct Counters {
        uint32_t packet = 0, frame = 0, crc = 0, frame_drop = 0;
        uint16_t ecc1 = 0, ecc2 = 0, lane_abort = 0, axi_slverr = 0, axi_decerr = 0;
    };

    void b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay);
    unsigned transport_dbg(tlm::tlm_generic_payload& tx);
    uint32_t csr_read(uint32_t word, bool side_effects);
    void csr_write(uint32_t word, uint32_t value, uint32_t lanes);
    static uint32_t merge(uint32_t old, uint32_t value, uint32_t lanes, uint32_t rw_mask) {
        const uint32_t m = lanes & rw_mask;
        return (old & ~m) | (value & m);
    }

    void reset_registers();          // resetn / full_reset scope
    void clear_datapath_status();    // soft_reset scope (C10)
    void start_reset(bool full);     // epoch, drain, PHY pulse for full
    void disable_cause();            // M16/M17
    void apply_phy_config();
    bool suppressed() const;
    void update_irq() { irq_ev_.notify(sc_core::SC_ZERO_TIME); }

    // LM/PA event routing to the register file (IRQ, counters, status).
    struct Glue : LmEvents, PaEvents {
        Csi2RxController& c;
        explicit Glue(Csi2RxController& ctl) : c(ctl) {}
        void lm_word(const MergedWord& w) override;
        void lm_abort(bool sync_err, bool fifo_full, bool unarmed, uint8_t lane_sync_seen) override;
        void lm_deskew_burst() override;
        void lm_state(LmState s) override;
        void lm_lanes_applied(unsigned enc) override;
        void pa_beat(const PayloadBeat& b) override;
        void pa_error(PaError e, int vc, unsigned line) override;
        void pa_packet_done() override;
        void pa_frame_start(unsigned vc, uint16_t n) override;
        void pa_frame_end(unsigned vc, uint16_t n) override;
        void pa_gsp(uint8_t vc, uint8_t dt, uint16_t data) override;
        void pa_null_blank() override;
        void pa_vc_unsupported() override;
    };
    // sys_clk-domain events to the register file.
    struct SysGlue : SysEvents {
        Csi2RxController& c;
        explicit SysGlue(Csi2RxController& ctl) : c(ctl) {}
        void sys_line_buffer_full() override;
        void sys_line_buffer_overflow() override;
        void sys_cdc_overflow() override;
        void sys_frame_dropped() override;
        void sys_capture_start(uint16_t frame_id, uint64_t timestamp_ns) override;
        void sys_capture_done() override;
        void sys_published(unsigned vc, const FrameInfo& info) override;
        void sys_resolution_mismatch(unsigned vc) override;
        void sys_err_wc_align() override;
        void sys_fmtbridge_overflow() override;
        void sys_frame_num_err(unsigned vc) override;
    };
    // DMA register side (ownership, FRAME_ID, BUFFER_ERROR_INFO, fault trace).
    struct DmaGlue : DmaEvents {
        Csi2RxController& c;
        explicit DmaGlue(Csi2RxController& ctl) : c(ctl) {}
        BufState dma_buf_state(unsigned side) override { return c.buf_[side & 1]; }
        void dma_set_buffer(unsigned side, BufState s, uint16_t frame_id) override;
        void dma_set_last_valid_line(unsigned side, uint16_t line) override;
        void dma_set_active_buffer(unsigned side) override;
        void dma_busy(bool busy) override;
        bool dma_claim_blocked() override { return c.reset_in_progress(); }
        void dma_buf_done(bool ok, uint32_t epoch) override;
        void dma_overflow(uint32_t epoch) override;
        void dma_axi_error(uint8_t awid, uint8_t resp, uint32_t addr, uint32_t epoch) override;
    };
    void error_capture(int vc);
    void update_sys_config();
    uint64_t timestamp_ns() const;

    void irq_method();
    void resetn_method();
    void byteclk_monitor();
    void stopstate_monitor();
    void sys_thread();
    void dma_thread();

    DphyRxWrapper& w_;
    Csi2ControllerConfig cfg_;
    DphyAdapter adapter_;

    bool in_reset_ = false;
    uint32_t epoch_ = 0;
    sc_core::sc_time ts_base_;  // timestamp counter origin (C07)

    // configuration (RW storage, by word)
    uint32_t core_config_ = 0, protocol_config_ = 0, phy_config_ = 0, metadata_dt_config_ = 0;
    uint32_t stream_config_ = 0, expected_resolution_ = 0, global_irq_enable_ = 0, irq_enable_ = 0;
    uint32_t metadata_record_id_ = 0, dma_control_ = 0, dma_buf0_ = 0, dma_buf1_ = 0, dma_size_ = 0;
    uint32_t dma_attr_ = 0, line_pitch_ = 0, tpg_control_ = 0, tpg_resolution_ = 0, tpg_blanking_ = 0;
    uint32_t emb_cfg0_ = 0, emb_cfg1_ = 0;

    // hardware-owned state
    std::array<uint32_t, 0x100 / 4> ro_{};  // latched RO words written through hw_set_ro
    uint32_t irq_status_ = 0;
    std::array<sc_core::sc_time, IRQ_COUNT> irq_set_time_{};
    std::array<bool, IRQ_COUNT> irq_set_valid_{};
    Counters cnt_;
    std::deque<uint32_t> gsp_;
    uint32_t ts_lo_ = 0, ts_hi_ = 0, ts_hi_shadow_ = 0;
    std::array<BufState, 2> buf_{BufState::HwOwned, BufState::HwOwned};
    uint8_t lane_sot_err_ = 0, lane_sot_sync_err_ = 0;
    std::array<bool, 4> stop_prev_{};

    bool capture_in_flight_ = false, reset_drain_pending_ = false, disable_drain_pending_ = false;

    LaneManagement lm_;
    SysDatapath sys_;
    SysConfig sys_cfg_;
    Glue glue_{*this};
    SysGlue sys_glue_{*this};
    DmaGlue dma_glue_{*this};
    sc_core::sc_event dma_wake_ev_;
    PayloadSink* sink_ = nullptr;
    uint64_t ppi_x_ = 0;
    sc_core::sc_time last_edge_ = sc_core::SC_ZERO_TIME;
    bool any_edge_ = false;
    sc_core::sc_event sys_wake_ev_;
    // soft_reset towards the rxbyteclkhs domain (C20): set on soft_reset 0->1,
    // cleared by the first byte-clock edge that resets Lane Management.
    bool bc_reset_pending_ = false;
    sc_core::sc_event irq_ev_, drain_req_ev_, release_ev_;
};

}  // namespace fx1::csi2
