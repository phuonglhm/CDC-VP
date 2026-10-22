// M3a/M3b end-to-end: CSI-2 packets from the pad-level BFM through the
// detailed D-PHY, the PPI sampling, Lane Management, Packet Analyzer and the
// sys_clk datapath of the controller to the AXI4-Stream output (checking sink);
// status, counters and IRQ read over the CSR. The Packet Analyzer tap
// (PayloadSink) is observed as well.
//
//   test_fx1_csi2_rx_phy <case>
//     frame_raw10_4lane | frame_raw12_2lane | frame_raw10_1lane
//     errors      CRC, ECC 1-bit, ECC 2-bit, lane abort (SoT sync), VC1 in one stream
//     unarmed     active_lanes = 4 with dl_enable = lanes 0-2 only (lane 3 never arms)
//     skew        lane skew absorbed by the delay FIFOs; skew beyond them aborts
//     soft_reset_aligned | soft_reset_latency   driver soft reset in the middle of a
//                 burst (Lane Management Aligned), CSR latency 0 / 20 ns (review M3a F1)
//     soft_reset_noclk   driver soft reset with the clock lane not in HS: ENOCLK
//     stall_drop  ISP stalls: Line Buffer full, frame dropped, next frame whole (M3b)
//     core_pulse_lat0 | _lat1 | _lat20   driver core_enable(0) then (1) in the middle of a
//                 line, ISP stalled, CSR latency 0 / 1 / 20 ns (review M3b F1, C37)
//     zero_wc     RAW10 packets with WC = 0 at frame start and between lines (review M3b F2, C38)
//     dma_sensor  frames through the PHY written to DDR by the DMA (M4a): RAW16 layout,
//                 ping-pong, ownership, FRAME_ID, IRQ order, drop with both sides owned
//     dma_tpg     Test Pattern Generator frames written to DDR, packed RAW12 (M4a)
//     dma_stream_backlog  stream and DMA on, ISP stalled while two frames end: no early
//                 completion, both sides exact after the release (review M4a F1)
//     drv_camera  FW-style sequence with the C99 driver only: PHY start, DMA config, 3 frames
//                 handled from the interrupt (verify, release), graceful stop (M4b)
//     drv_recover same, with a SLVERR on frame 2: buffer released with error, recovery (M4b)
//     tpg         Test Pattern Generator through the CSR, source switch to and from the sensor
//     bench_640x480   one 640x480 RAW10 frame on 4 lanes, wall-clock cost (label fx1_bench)
//
// Oracle: the packets built by the BFM's packet builder and the byte vectors
// they carry; status expectations from the register contract.
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include "fx1_csi2/axis_monitor.h"
#include "fx1_csi2/csi2_rx_controller.h"
#include "fx1_csi2/fx1_csi2_drv.h"
#include "fx1_csi2/fx1_csi2_regs.h"
#include "fx1_csi2_bfm/csi2_packet.h"
#include "fx1_csi2_bfm/dphy_tx_bfm.h"
#include "fx1_csi2_phy/dphy_rx_wrapper.h"
#include "tlm_mem.h"

using namespace fx1::csi2;
using sc_core::sc_time;
using sc_core::SC_NS;
using sc_core::SC_US;
using Bytes = std::vector<uint8_t>;

namespace {
int g_fail = 0;
#define CHECK(cond)                                                                         \
    do {                                                                                    \
        if (!(cond)) {                                                                      \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond);    \
            ++g_fail;                                                                       \
        }                                                                                   \
    } while (0)
#define CHECK_EQ(a, b)                                                                                     \
    do {                                                                                                   \
        const auto va_ = (a);                                                                              \
        const auto vb_ = (b);                                                                              \
        if (va_ != vb_) {                                                                                  \
            std::fprintf(stderr, "%s:%d: CHECK_EQ failed: %s = 0x%llX, expected 0x%llX\n", __FILE__, __LINE__, \
                         #a, (unsigned long long)va_, (unsigned long long)vb_);                            \
            ++g_fail;                                                                                      \
        }                                                                                                  \
    } while (0)

// Bits 0-10, 12-14, 19, 21, 22 (resolution_mismatch, bit 20, is checked where
// EXPECTED_RESOLUTION is programmed).
constexpr uint32_t kErrorIrqs = 0x0007FFu | (1u << IRQ_LINE_BUFFER_FULL) | (1u << IRQ_BUFFER_OVERFLOW) |
                                (1u << IRQ_FRAME_DROPPED) | (1u << IRQ_LANE_ALIGN_ABORT) | (1u << IRQ_ERR_WC_ALIGN) |
                                (1u << IRQ_FMTBRIDGE_OVERFLOW);

struct Sink : PayloadSink {
    std::vector<Bytes> lines;
    Bytes cur;
    unsigned sof = 0, fs = 0, fe = 0, beats = 0;
    bool raw10 = false;
    void beat(const PayloadBeat& b) override {
        ++beats;
        if (b.sol) cur.clear();
        sof += b.sof;
        raw10 = b.raw10;
        for (unsigned i = 0; i < 4; ++i)
            if (b.byte_en >> i & 1) cur.push_back(b.bytes[i]);
        if (b.eol) lines.push_back(cur);
    }
    void frame_start(unsigned, uint16_t) override { ++fs; }
    void frame_end(unsigned, uint16_t) override { ++fe; }
};

// TPG pattern oracle (MAS 6.5, D11), independent of the generator: band =
// min(7, coord / base), colour bars of MAS Table 625, RGGB sites.
uint16_t tpg_oracle(unsigned x, unsigned y, unsigned w, bool raw10) {
    static const bool R[8] = {1, 1, 0, 0, 1, 1, 0, 0}, G[8] = {1, 1, 1, 1, 0, 0, 0, 0}, B[8] = {1, 0, 1, 0, 1, 0, 1, 0};
    const unsigned band = std::min(7u, x / (w >> 3));
    const bool on = (y % 2 == 0 && x % 2 == 0) ? R[band] : (y % 2 == 1 && x % 2 == 1) ? B[band] : G[band];
    return on ? (raw10 ? 0x3FF : 0xFFF) : 0;
}

std::vector<Bytes> stream_lines(const AxisMonitor& m, size_t from = 0) {
    std::vector<Bytes> v;
    for (size_t i = from; i < m.lines().size(); ++i) v.push_back(m.lines()[i].bytes);
    return v;
}

std::vector<uint16_t> line_pixels(unsigned width, bool raw10, unsigned line) {
    std::vector<uint16_t> px(width);
    for (unsigned x = 0; x < width; ++x) px[x] = static_cast<uint16_t>((x * 37 + line * 101 + 5) & (raw10 ? 0x3FF : 0xFFF));
    return px;
}
Bytes line_payload(unsigned width, bool raw10, unsigned line) {
    const auto px = line_pixels(width, raw10, line);
    return raw10 ? bfm::pack_raw10(px) : bfm::pack_raw12(px);
}
Bytes raw16_of(const std::vector<uint16_t>& px) {
    Bytes b;
    for (uint16_t v : px) {
        b.push_back(static_cast<uint8_t>(v));
        b.push_back(static_cast<uint8_t>(v >> 8));
    }
    return b;
}

struct Tb : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<Tb> socket{"socket"};
    sc_core::sc_signal<bool> resetn{"resetn", false};
    sc_core::sc_signal<bool> irq{"irq"};
    DphyRxWrapper phy{"phy_model"};
    Csi2RxController ctl;
    bfm::DphyTxBfm tx{"tx", phy.clkp, phy.clkn, phy.dp, phy.dn};
    Sink sink;
    AxisMonitor axis;
    TlmMem ddr{"ddr", 0x80000000ull, 4u << 20};
    std::string which;

    SC_HAS_PROCESS(Tb);
    static Csi2ControllerConfig cfg_for(const std::string& w) {
        Csi2ControllerConfig c;
        if (w == "soft_reset_latency" || w == "core_pulse_lat20") c.csr_latency = sc_time(20, SC_NS);
        if (w == "core_pulse_lat1") c.csr_latency = sc_time(1, SC_NS);  // low ~2 ns: shorter than sys_clk
        return c;
    }
    Tb(sc_core::sc_module_name nm, std::string w)
        : sc_core::sc_module(nm), ctl("csi2", phy, cfg_for(w)), which(std::move(w)) {
        socket.bind(ctl.csr_socket);
        ctl.resetn(resetn);
        ctl.irq(irq);
        ctl.set_payload_sink(&sink);
        ctl.set_stream_sink(&axis);
        ctl.dma_socket.bind(ddr.socket);
        SC_THREAD(run);
    }

    uint32_t xfer(tlm::tlm_command cmd, uint32_t off, uint32_t v) {
        unsigned char b[4] = {uint8_t(v), uint8_t(v >> 8), uint8_t(v >> 16), uint8_t(v >> 24)};
        tlm::tlm_generic_payload p;
        sc_time d = sc_core::SC_ZERO_TIME;
        p.set_command(cmd);
        p.set_address(off);
        p.set_data_ptr(b);
        p.set_data_length(4);
        p.set_streaming_width(4);
        socket->b_transport(p, d);
        if (d != sc_core::SC_ZERO_TIME) wait(d);  // consume the access latency the target returned
        CHECK(p.get_response_status() == tlm::TLM_OK_RESPONSE);
        return b[0] | b[1] << 8 | b[2] << 16 | uint32_t(b[3]) << 24;
    }
    uint32_t rd(uint32_t off) { return xfer(tlm::TLM_READ_COMMAND, off, 0); }
    // C99 driver HAL over the same socket.
    static uint32_t hal_read32(void* ctx, uint32_t off) { return static_cast<Tb*>(ctx)->rd(off); }
    static void hal_write32(void* ctx, uint32_t off, uint32_t v) { static_cast<Tb*>(ctx)->wr(off, v); }
    static void hal_delay_us(void*, uint32_t us) { sc_core::wait(sc_time(us, SC_US)); }
    void wr(uint32_t off, uint32_t v) { xfer(tlm::TLM_WRITE_COMMAND, off, v); }
    bool poll(uint32_t off, uint32_t mask, uint32_t want, sc_time step, unsigned n) {
        for (unsigned i = 0; i < n && (rd(off) & mask) != want; ++i) wait(step);
        return (rd(off) & mask) == want;
    }

    // MAS initial sequence: PHY on, stop state, core enabled, IRQ clear, clock lane HS.
    void bring_up(unsigned lanes, uint32_t dl_mask = 0, bool start_clock = true) {
        resetn.write(true);
        wait(1, SC_US);
        const uint32_t enc = lanes == 1 ? 0 : lanes == 2 ? 1 : 3;
        if (!dl_mask) dl_mask = (1u << lanes) - 1;
        wr(FX1_CSI2_PROTOCOL_CONFIG, enc);
        wr(FX1_CSI2_PHY_CONFIG, FX1_CSI2_PHY_CONFIG_PHY_ENABLE_MASK | FX1_CSI2_PHY_CONFIG_CL_ENABLE_MASK |
                                    (dl_mask << FX1_CSI2_PHY_CONFIG_DL_ENABLE_SHIFT));
        uint32_t stop = 0;
        for (unsigned i = 0; i < 4; ++i)
            if (dl_mask >> i & 1) stop |= 1u << (8 * i);
        CHECK(poll(FX1_CSI2_LANE_STATUS, stop, stop, sc_time(10, SC_US), 60));
        wr(FX1_CSI2_CORE_CONFIG, FX1_CSI2_CORE_CONFIG_CORE_ENABLE_MASK);
        wr(FX1_CSI2_IRQ_STATUS, 0x00FFFFFF);
        if (!start_clock) return;
        tx.clock_start();
        CHECK(poll(FX1_CSI2_PHY_STATUS, FX1_CSI2_PHY_STATUS_CL_RXCLKACTIVEHS_MASK, FX1_CSI2_PHY_STATUS_CL_RXCLKACTIVEHS_MASK,
                   sc_time(100, SC_NS), 20));
        wait(200, SC_NS);
        CHECK_EQ((rd(FX1_CSI2_PROTOCOL_CONFIG) >> FX1_CSI2_PROTOCOL_CONFIG_ACTIVE_LANES_APPLIED_SHIFT) & 3, enc);
    }

    // One packet in one HS burst (MAS: one packet per burst, C16).
    void send(const Bytes& pkt, unsigned lanes, const std::function<void(std::vector<bfm::LaneBurst>&)>& mod = {}) {
        const auto split = bfm::distribute_lanes(pkt, lanes);
        std::vector<bfm::LaneBurst> lb(lanes);
        for (unsigned i = 0; i < lanes; ++i) lb[i].bytes = split[i];
        if (mod) mod(lb);
        tx.send_burst(lb);
        wait(300, SC_NS);  // EoT detection and drain
    }

    std::vector<Bytes> send_frame(unsigned lanes, unsigned width, unsigned height, bool raw10, uint16_t fnum = 1) {
        std::vector<Bytes> lines;
        send(bfm::short_packet(0, bfm::DT_FS, fnum), lanes);
        for (unsigned y = 0; y < height; ++y) {
            lines.push_back(line_payload(width, raw10, y));
            send(bfm::long_packet(0, raw10 ? bfm::DT_RAW10 : bfm::DT_RAW12, lines.back()), lanes);
        }
        send(bfm::short_packet(0, bfm::DT_FE, fnum), lanes);
        return lines;
    }

    void check_clean() {
        CHECK_EQ(rd(FX1_CSI2_IRQ_STATUS) & kErrorIrqs, 0u);
        CHECK_EQ(rd(FX1_CSI2_ECC_ERROR_COUNT), 0u);
        CHECK_EQ(rd(FX1_CSI2_CRC_ERROR_COUNT), 0u);
        CHECK_EQ(rd(FX1_CSI2_LANE_ABORT_COUNT), 0u);
        CHECK_EQ(rd(FX1_CSI2_LANE_ALIGN_STATUS), 0u);  // ST_IDLE, no abort recorded
        CHECK_EQ(ctl.ppi_x_samples(), 0u);
        CHECK_EQ(ctl.lane_management().dropped_bytes(), 0u);
        CHECK(axis.violations().empty());
        CHECK_EQ(ctl.sys_datapath().cdc_lost_words(), 0u);
        CHECK_EQ(rd(FX1_CSI2_FRAME_DROP_COUNT), 0u);
    }

    // ------------------------------------------------------------------ cases
    void frame(unsigned lanes, bool raw10) {
        bring_up(lanes);
        wr(FX1_CSI2_EXPECTED_RESOLUTION, (8u << 16) | 32u);
        const auto t0 = sc_core::sc_time_stamp();
        const auto lines = send_frame(lanes, 32, 8, raw10);
        // The sensor stops after the Frame End: the capture completes without a
        // next Frame Start (plan M3: completion not deferred to the next FS).
        CHECK(rd(FX1_CSI2_IRQ_STATUS) & (1u << IRQ_FRAME_RECEIVED));
        CHECK_EQ(rd(FX1_CSI2_FRAME_COUNT), 1u);
        CHECK(stream_lines(axis) == lines);  // every line already at the ISP, TLAST included
        // A generic short packet inside the stream reaches the FIFO.
        send(bfm::short_packet(0, 0x08, 0xA5A5), lanes);
        std::printf("  frame 32x8 %s on %u lane(s): %.2f us simulated, %llu sys_clk cycles\n", raw10 ? "RAW10" : "RAW12",
                    lanes, (sc_core::sc_time_stamp() - t0).to_seconds() * 1e6,
                    (unsigned long long)ctl.sys_datapath().cycles());
        CHECK(sink.lines == lines);
        CHECK(sink.sof == 1 && sink.fs == 1 && sink.fe == 1 && sink.raw10 == raw10);
        CHECK_EQ(rd(FX1_CSI2_PACKET_COUNT), 8u);
        CHECK_EQ(rd(FX1_CSI2_GENERIC_SHORT_PACKET), (0xA5A5u << 8) | 0x08u);
        // Stream output: packed (STREAM_CONFIG reset), SOF on the first beat only, TID 0.
        for (size_t i = 0; i < axis.lines().size(); ++i) {
            CHECK_EQ(axis.lines()[i].sof, i == 0);
            CHECK(!axis.lines()[i].err && axis.lines()[i].tid == 0);
        }
        // Frame context published at FE.
        const uint32_t wc = raw10 ? 40u : 48u;
        CHECK_EQ(rd(FX1_CSI2_VC0_IMAGE_INFO1), (8u << 16) | wc);
        CHECK_EQ(rd(FX1_CSI2_VC0_IMAGE_INFO2), raw10 ? 0x2Bu : 0x2Cu);
        CHECK_EQ(rd(FX1_CSI2_IRQ_STATUS) & (1u << IRQ_RESOLUTION_MISMATCH), 0u);
        CHECK(rd(FX1_CSI2_TIMESTAMP_LO) != 0u);
        CHECK_EQ(rd(FX1_CSI2_BUFFER_STATUS), 0u);  // Line Buffer empty, no overflow
        CHECK(ctl.sys_datapath().cdc_max_fill() <= 3);
        check_clean();
    }

    void errors() {
        const unsigned n = 4;
        bring_up(n);
        wr(FX1_CSI2_EXPECTED_RESOLUTION, (4u << 16) | 32u);
        // (a) payload bit inverted on the wire: CRC error, frame-level error at FE.
        send(bfm::short_packet(0, bfm::DT_FS, 1), n);
        send(bfm::long_packet(0, bfm::DT_RAW10, line_payload(32, true, 0)), n);
        send(bfm::long_packet(0, bfm::DT_RAW10, line_payload(32, true, 1)), n,
             [](auto& lb) { lb[1].flip_bits = {5 * 8 + 2}; });
        CHECK(rd(FX1_CSI2_IRQ_STATUS) & (1u << IRQ_CRC_ERROR));
        CHECK_EQ(rd(FX1_CSI2_CRC_ERROR_COUNT), 1u);
        // C33: the failing line (1-based) of VC0 in capture 1.
        CHECK_EQ(rd(FX1_CSI2_ERROR_CAPTURE), (1u << FX1_CSI2_ERROR_CAPTURE_FAULT_FRAME_ID_SHIFT) | 2u);
        send(bfm::short_packet(0, bfm::DT_FE, 1), n);
        CHECK(rd(FX1_CSI2_IRQ_STATUS) & (1u << IRQ_FRAME_LEVEL_ERR_VC0));
        CHECK_EQ(sink.lines.size(), 2u);  // cut-through: the corrupted line is still delivered
        // Control FSM dropped the capture: frame_dropped, no frame_received, abort beat to the ISP.
        CHECK(rd(FX1_CSI2_IRQ_STATUS) & (1u << IRQ_FRAME_DROPPED));
        CHECK_EQ(rd(FX1_CSI2_IRQ_STATUS) & (1u << IRQ_FRAME_RECEIVED), 0u);
        CHECK_EQ(rd(FX1_CSI2_FRAME_DROP_COUNT), 1u);
        CHECK_EQ(rd(FX1_CSI2_FRAME_COUNT), 0u);
        CHECK(axis.abort_beats() >= 1);
        wr(FX1_CSI2_IRQ_STATUS, 0x00FFFFFF);
        // (b) single-bit header error: corrected, line intact.
        Bytes p = bfm::long_packet(0, bfm::DT_RAW10, line_payload(32, true, 2));
        p[1] ^= 0x04;  // WC bit
        send(bfm::short_packet(0, bfm::DT_FS, 2), n);
        send(p, n);
        CHECK(rd(FX1_CSI2_IRQ_STATUS) & (1u << IRQ_ECC_1BIT));
        CHECK_EQ(rd(FX1_CSI2_ECC_ERROR_COUNT), 1u);
        CHECK(sink.lines.back() == line_payload(32, true, 2));
        // (c) two-bit header error: discarded, fault_vc unknown.
        const size_t before = sink.lines.size();
        p = bfm::long_packet(0, bfm::DT_RAW10, line_payload(32, true, 3));
        p[0] ^= 0x01;
        p[2] ^= 0x80;
        send(p, n);
        CHECK(rd(FX1_CSI2_IRQ_STATUS) & (1u << IRQ_ECC_2BIT));
        CHECK_EQ(rd(FX1_CSI2_ECC_ERROR_COUNT), 0x00010001u);
        CHECK_EQ(sink.lines.size(), before);
        // Channel unknown: fault_vc 3, newest capture (2), line 0xFFFF (C33).
        CHECK_EQ(rd(FX1_CSI2_ERROR_CAPTURE), (3u << FX1_CSI2_ERROR_CAPTURE_FAULT_VC_SHIFT) |
                                                 (2u << FX1_CSI2_ERROR_CAPTURE_FAULT_FRAME_ID_SHIFT) | 0xFFFFu);
        // (d) leader of lane 2 corrupted beyond recovery: the PHY reports SoT
        // sync error, Lane Management aborts the burst.
        send(bfm::long_packet(0, bfm::DT_RAW10, line_payload(32, true, 4)), n, [](auto& lb) { lb[2].sync = 0xBD; });
        CHECK(rd(FX1_CSI2_IRQ_STATUS) & (1u << IRQ_LANE_ALIGN_ABORT));
        CHECK_EQ(rd(FX1_CSI2_LANE_ABORT_COUNT), 1u);
        // With this PHY the synchronised lanes deliver their data and ~7 trail
        // bytes before the PHY reports errsotsynchs, so the 6-byte usable delay
        // FIFO fills first: cause = abort_fifo_full; abort_lane_sync_seen = 1011
        // identifies lane 2 (spec_decisions C19).
        const uint32_t las = rd(FX1_CSI2_LANE_ALIGN_STATUS);
        CHECK_EQ(las, FX1_CSI2_LANE_ALIGN_STATUS_ABORT_FIFO_FULL_MASK |
                          (0xBu << FX1_CSI2_LANE_ALIGN_STATUS_ABORT_LANE_SYNC_SEEN_SHIFT));
        CHECK_EQ(sink.lines.size(), before);
        send(bfm::short_packet(0, bfm::DT_FE, 2), n);
        CHECK(rd(FX1_CSI2_IRQ_STATUS) & (1u << IRQ_FRAME_LEVEL_ERR_VC0));  // abort inside the open frame
        // (e) packet on VC1 (linear profile): dropped, no error interrupt.
        wr(FX1_CSI2_IRQ_STATUS, 0x00FFFFFF);
        send(bfm::short_packet(1, bfm::DT_FS, 1), n);
        send(bfm::long_packet(1, bfm::DT_RAW10, line_payload(32, true, 5)), n);
        CHECK(rd(FX1_CSI2_CORE_STATUS) & FX1_CSI2_CORE_STATUS_VC_UNSUPPORTED_SEEN_MASK);
        CHECK_EQ(rd(FX1_CSI2_IRQ_STATUS) & kErrorIrqs, 0u);
        // (f) recovery: a clean frame afterwards, whole at the ISP.
        sink.lines.clear();
        const size_t from = axis.lines().size();
        const auto lines = send_frame(n, 32, 4, true, 3);
        CHECK(sink.lines == lines);
        CHECK(stream_lines(axis, from) == lines);
        CHECK(axis.lines()[from].sof);
        CHECK_EQ(rd(FX1_CSI2_IRQ_STATUS) & (kErrorIrqs | (1u << IRQ_RESOLUTION_MISMATCH)), 0u);
        CHECK_EQ(rd(FX1_CSI2_FRAME_COUNT), 1u);
        CHECK_EQ(rd(FX1_CSI2_FRAME_DROP_COUNT), 2u);  // captures 1 and 2 (D06: ended = delivered + dropped)
        CHECK(axis.violations().empty());
    }

    // M3b: the ISP stalls; the stream FIFO and Line Buffer fill and the frame
    // is dropped at FULL_WATERMARK on the sensor path (D-PHY never
    // back-pressured); the next frame reaches the ISP whole.
    void stall_drop() {
        const unsigned n = 4;
        bring_up(n);
        bool stall = false;
        axis.set_ready([&](uint64_t) { return !stall; });
        const auto l1 = send_frame(n, 640, 2, true, 1);
        CHECK(stream_lines(axis) == l1);
        const size_t from = axis.lines().size();
        stall = true;
        send_frame(n, 640, 16, true, 2);  // 16 x 160 entries > FULL_WATERMARK
        CHECK(rd(FX1_CSI2_IRQ_STATUS) & (1u << IRQ_LINE_BUFFER_FULL));
        CHECK(rd(FX1_CSI2_IRQ_STATUS) & (1u << IRQ_FRAME_DROPPED));
        CHECK_EQ(rd(FX1_CSI2_FRAME_DROP_COUNT), 1u);
        CHECK_EQ(rd(FX1_CSI2_BUFFER_STATUS) & FX1_CSI2_BUFFER_STATUS_LINEBUF_OVERFLOW_MASK, 0u);
        const unsigned max_fill = ctl.sys_datapath().line_buffer().max_fill();
        std::printf("  stall: Line Buffer max fill %u, stream lines before resume %zu\n", max_fill, axis.lines().size() - from);
        CHECK(max_fill >= 1740 && max_fill < 2048);
        stall = false;
        wait(2, SC_US);
        CHECK(axis.abort_beats() >= 1);
        const size_t from3 = axis.lines().size();
        const auto l3 = send_frame(n, 640, 3, true, 3);
        CHECK(stream_lines(axis, from3) == l3);
        CHECK(axis.lines()[from3].sof);
        CHECK_EQ(rd(FX1_CSI2_FRAME_COUNT), 2u);
        CHECK_EQ(rd(FX1_CSI2_FRAME_DROP_COUNT), 1u);
        CHECK(axis.violations().empty());
        CHECK_EQ(ctl.lane_management().dropped_bytes(), 0u);
    }

    // Review M3b F1 (C37): core_enable cleared and set again by the driver while
    // a line is being received and the ISP is stalled. The interrupted capture
    // is dropped once, its data never heads the next frame, which is exact.
    void core_pulse() {
        const unsigned n = 4;
        bring_up(n);
        wr(FX1_CSI2_EXPECTED_RESOLUTION, (1u << 16) | 8u);
        fx1_csi2_hal hal{&Tb::hal_read32, &Tb::hal_write32, &Tb::hal_delay_us, this};
        fx1_csi2_dev dev{};
        CHECK(fx1_csi2_probe(&dev, &hal) == FX1_CSI2_OK);
        bool stall = false;
        axis.set_ready([&](uint64_t) { return !stall; });
        send(bfm::short_packet(0, bfm::DT_FS, 1), n);
        bool sent = false;
        sc_core::sc_spawn([&] {
            send(bfm::long_packet(0, bfm::DT_RAW10, line_payload(800, true, 0)), n);
            sent = true;
        });
        for (unsigned i = 0; i < 2000 && sink.beats < 40; ++i) wait(5, SC_NS);
        CHECK(sink.beats >= 40);  // part of the line is past Packet Analyzer
        stall = true;
        wait(100, SC_NS);
        const sc_time t0 = sc_core::sc_time_stamp();
        CHECK(fx1_csi2_core_enable(&dev, 0) == FX1_CSI2_OK);
        const sc_time t1 = sc_core::sc_time_stamp();
        CHECK(fx1_csi2_core_enable(&dev, 1) == FX1_CSI2_OK);
        // core_enable reads 0 from the first write to the second: the read and
        // the write of the second call, 2 x CSR latency.
        std::printf("  core_enable 0 -> 1 calls: %.1f ns, %.1f ns\n", (t1 - t0).to_seconds() * 1e9,
                    (sc_core::sc_time_stamp() - t1).to_seconds() * 1e9);
        // The stop is in progress until the drop sequence has run and the abort
        // of the stream master has completed (C37): with the ISP stalled it
        // cannot complete, the held beat stays valid.
        const uint32_t rip = FX1_CSI2_CORE_STATUS_RESET_IN_PROGRESS_MASK;
        CHECK(rd(FX1_CSI2_CORE_STATUS) & rip);
        while (!sent) wait(100, SC_NS);
        wait(500, SC_NS);
        CHECK(rd(FX1_CSI2_CORE_STATUS) & rip);
        CHECK(ctl.sys_datapath().axis_master().tvalid());
        CHECK_EQ(axis.abort_beats(), 0u);
        stall = false;
        CHECK(poll(FX1_CSI2_CORE_STATUS, rip, 0, sc_time(10, SC_NS), 100));
        CHECK(axis.abort_beats() >= 1);  // accepted before busy cleared
        wait(1, SC_US);
        send(bfm::short_packet(0, bfm::DT_FE, 1), n);  // FE of the interrupted frame
        CHECK_EQ(rd(FX1_CSI2_FRAME_DROP_COUNT), 1u);
        CHECK_EQ(rd(FX1_CSI2_FRAME_COUNT), 0u);
        CHECK(axis.abort_beats() >= 1);
        const size_t from = axis.lines().size();
        const auto lines = send_frame(n, 8, 1, true, 2);
        CHECK(stream_lines(axis, from) == lines);
        unsigned sof = 0;
        for (size_t i = from; i < axis.lines().size(); ++i) sof += axis.lines()[i].sof;
        CHECK(sof == 1 && axis.lines().size() > from && axis.lines()[from].sof);
        CHECK_EQ(rd(FX1_CSI2_FRAME_COUNT), 1u);
        CHECK_EQ(rd(FX1_CSI2_FRAME_DROP_COUNT), 1u);
        // Idle pulse: nothing to drop; the next frame is delivered.
        const uint64_t aborts = axis.abort_beats();
        CHECK(fx1_csi2_core_enable(&dev, 0) == FX1_CSI2_OK);
        CHECK(fx1_csi2_core_enable(&dev, 1) == FX1_CSI2_OK);
        CHECK(poll(FX1_CSI2_CORE_STATUS, FX1_CSI2_CORE_STATUS_RESET_IN_PROGRESS_MASK, 0, sc_time(10, SC_NS), 100));
        CHECK_EQ(axis.abort_beats(), aborts);
        const size_t from2 = axis.lines().size();
        const auto l2 = send_frame(n, 8, 1, true, 3);
        CHECK(stream_lines(axis, from2) == l2);
        CHECK_EQ(rd(FX1_CSI2_FRAME_COUNT), 2u);
        CHECK_EQ(rd(FX1_CSI2_FRAME_DROP_COUNT), 1u);
        CHECK(axis.violations().empty());
    }

    // M4a: the DMA writes the frames of the sensor into DDR (RAW16, the
    // Requirement V4 baseline), ping-pong with ownership and FRAME_ID.
    void dma_sensor() {
        const unsigned n = 4, w = 64, h = 4, stride = 2 * w + 16;
        bring_up(n);
        const uint32_t b0 = 0x80000000u, b1 = 0x80100000u, size = 0x10000;
        wr(FX1_CSI2_EXPECTED_RESOLUTION, (h << 16) | w);
        wr(FX1_CSI2_STREAM_CONFIG, 0);  // DDR-only milestone: stream path off (D02)
        wr(FX1_CSI2_DMA_BUFFER0_ADDR, b0);
        wr(FX1_CSI2_DMA_BUFFER1_ADDR, b1);
        wr(FX1_CSI2_DMA_BUFFER_SIZE, size);
        wr(FX1_CSI2_LINE_PITCH, (2 * w) | (16u << FX1_CSI2_LINE_PITCH_LINE_PADDING_SHIFT));
        wr(FX1_CSI2_DMA_ATTR, 0xFFu << FX1_CSI2_DMA_ATTR_MAX_BURST_LEN_SHIFT);
        wr(FX1_CSI2_DMA_CONTROL, FX1_CSI2_DMA_CONTROL_DMA_ENABLE_MASK | FX1_CSI2_DMA_CONTROL_DMA_START_MASK |
                                     FX1_CSI2_DMA_CONTROL_DMA_FORMAT_MASK | FX1_CSI2_DMA_CONTROL_ERR_FRAME_POLICY_MASK);
        auto check_side = [&](uint32_t base, unsigned seed) {
            bool ok = true;
            for (unsigned y = 0; y < h; ++y) {
                ok &= ddr.read(base + y * stride, 2 * w) == raw16_of(line_pixels(w, true, seed + y));
                ok &= ddr.untouched(base + y * stride + 2 * w, stride - 2 * w);
            }
            ok &= ddr.untouched(base + h * stride, size - h * stride);
            return ok;
        };
        // Frame 1 -> side 0.
        auto frame = [&](uint16_t fnum, unsigned seed) {
            send(bfm::short_packet(0, bfm::DT_FS, fnum), n);
            for (unsigned y = 0; y < h; ++y) send(bfm::long_packet(0, bfm::DT_RAW10, line_payload(w, true, seed + y)), n);
            send(bfm::short_packet(0, bfm::DT_FE, fnum), n);
            wait(1, SC_US);
        };
        frame(1, 0);
        CHECK(check_side(b0, 0));
        CHECK_EQ(rd(FX1_CSI2_BUFFER_OWNERSHIP), 1u);  // buf0 SW_OWNED_VALID
        CHECK_EQ(rd(FX1_CSI2_FRAME_ID) & 0x3FFFu, 1u);
        CHECK_EQ(rd(FX1_CSI2_IRQ_STATUS) & ((1u << IRQ_BUF_DONE_OK) | (1u << IRQ_FRAME_RECEIVED)),
                 (1u << IRQ_BUF_DONE_OK) | (1u << IRQ_FRAME_RECEIVED));
        CHECK_EQ(rd(FX1_CSI2_FRAME_COUNT), 1u);
        CHECK_EQ(rd(FX1_CSI2_DMA_STATUS), 0u);  // not busy, active_buffer 0
        for (const auto& wlog : ddr.log) {
            CHECK_EQ(wlog.awid, 0u);
            CHECK_EQ(wlog.len, 2u * w);  // one burst per line (16 beats)
            CHECK_EQ(wlog.awlen, 15u);
        }
        // Frame 2 -> side 1; frame 3 -> both sides owned: dropped, nothing written.
        wr(FX1_CSI2_IRQ_STATUS, 0x00FFFFFF);
        frame(2, 10);
        CHECK(check_side(b1, 10));
        CHECK_EQ(rd(FX1_CSI2_BUFFER_OWNERSHIP), 0x5u);
        CHECK_EQ(rd(FX1_CSI2_FRAME_ID), (2u << 16) | 1u);
        CHECK_EQ(rd(FX1_CSI2_DMA_STATUS), 2u);  // active_buffer 1
        const size_t writes = ddr.log.size();
        frame(3, 20);
        CHECK_EQ(ddr.log.size(), writes);
        CHECK_EQ(rd(FX1_CSI2_FRAME_DROP_COUNT), 1u);
        CHECK(rd(FX1_CSI2_IRQ_STATUS) & (1u << IRQ_FRAME_DROPPED));
        CHECK(check_side(b0, 0) && check_side(b1, 10));  // nothing overwritten
        // Release side 0 by the CSR: frame 4 lands there.
        wr(FX1_CSI2_BUFFER_RELEASE, FX1_CSI2_BUFFER_RELEASE_RELEASE_BUF0_MASK);
        CHECK_EQ(rd(FX1_CSI2_BUFFER_OWNERSHIP), 0x4u);
        frame(4, 30);
        CHECK(check_side(b0, 30));
        CHECK_EQ(rd(FX1_CSI2_FRAME_ID), (2u << 16) | 4u);
        CHECK_EQ(rd(FX1_CSI2_FRAME_COUNT), 3u);
        CHECK_EQ(rd(FX1_CSI2_FRAME_DROP_COUNT), 1u);  // 4 captures = 3 delivered + 1 dropped (D06)
        CHECK_EQ(rd(FX1_CSI2_IRQ_STATUS) & (1u << IRQ_RESOLUTION_MISMATCH), 0u);
        check_clean_dma();
        // M09 (review M4a F2): dma_format / stream_format are changed only with
        // their path disabled and idle; otherwise a VP warning.
        const int w0 = sc_core::sc_report_handler::get_count("fx1/csi2/ctrl");
        const uint32_t on = FX1_CSI2_DMA_CONTROL_DMA_ENABLE_MASK | FX1_CSI2_DMA_CONTROL_DMA_START_MASK |
                            FX1_CSI2_DMA_CONTROL_ERR_FRAME_POLICY_MASK;
        wr(FX1_CSI2_DMA_CONTROL, on);  // format 1 -> 0 while enabled
        CHECK_EQ(sc_core::sc_report_handler::get_count("fx1/csi2/ctrl"), w0 + 1);
        wr(FX1_CSI2_DMA_CONTROL, 0);
        CHECK_EQ(rd(FX1_CSI2_DMA_STATUS) & FX1_CSI2_DMA_STATUS_DMA_BUSY_MASK, 0u);
        wr(FX1_CSI2_DMA_CONTROL, FX1_CSI2_DMA_CONTROL_DMA_FORMAT_MASK);  // disabled and idle: allowed
        CHECK_EQ(sc_core::sc_report_handler::get_count("fx1/csi2/ctrl"), w0 + 1);
        wr(FX1_CSI2_STREAM_CONFIG, FX1_CSI2_STREAM_CONFIG_AXIS_ENABLE_MASK);  // enable, format unchanged
        CHECK_EQ(sc_core::sc_report_handler::get_count("fx1/csi2/ctrl"), w0 + 1);
        wr(FX1_CSI2_STREAM_CONFIG, FX1_CSI2_STREAM_CONFIG_AXIS_ENABLE_MASK | FX1_CSI2_STREAM_CONFIG_STREAM_FORMAT_MASK);
        CHECK_EQ(sc_core::sc_report_handler::get_count("fx1/csi2/ctrl"), w0 + 2);
    }

    // Review M4a F1 through the PHY: stream and DMA both on, ISP stalled.
    void dma_stream_backlog() {
        const unsigned n = 4, w = 128, h = 4, bpl = 160;
        bring_up(n);
        const uint32_t b0 = 0x80000000u, b1 = 0x80100000u;
        bool stall = true;
        axis.set_ready([&](uint64_t) { return !stall; });
        wr(FX1_CSI2_STREAM_CONFIG, FX1_CSI2_STREAM_CONFIG_AXIS_ENABLE_MASK | FX1_CSI2_STREAM_CONFIG_STREAM_FORMAT_MASK);
        wr(FX1_CSI2_DMA_BUFFER0_ADDR, b0);
        wr(FX1_CSI2_DMA_BUFFER1_ADDR, b1);
        wr(FX1_CSI2_DMA_BUFFER_SIZE, 0x1000);
        wr(FX1_CSI2_LINE_PITCH, bpl);
        wr(FX1_CSI2_DMA_ATTR, 0xFFu << FX1_CSI2_DMA_ATTR_MAX_BURST_LEN_SHIFT);
        wr(FX1_CSI2_DMA_CONTROL, FX1_CSI2_DMA_CONTROL_DMA_ENABLE_MASK | FX1_CSI2_DMA_CONTROL_DMA_START_MASK);
        std::vector<Bytes> l1, l2;
        auto frame = [&](uint16_t fnum, unsigned seed, std::vector<Bytes>& out) {
            send(bfm::short_packet(0, bfm::DT_FS, fnum), n);
            for (unsigned y = 0; y < h; ++y) {
                out.push_back(line_payload(w, true, seed + y));
                send(bfm::long_packet(0, bfm::DT_RAW10, out.back()), n);
            }
            send(bfm::short_packet(0, bfm::DT_FE, fnum), n);
        };
        frame(1, 0, l1);
        frame(2, 10, l2);
        wait(2, SC_US);
        CHECK_EQ(rd(FX1_CSI2_FRAME_COUNT), 0u);  // data still behind the stalled ISP
        CHECK_EQ(rd(FX1_CSI2_BUFFER_OWNERSHIP), 0u);
        CHECK_EQ(rd(FX1_CSI2_IRQ_STATUS) & (1u << IRQ_BUF_DONE_OK), 0u);
        stall = false;
        wait(5, SC_US);
        bool ok = true;
        for (unsigned y = 0; y < h; ++y) {
            ok &= ddr.read(b0 + y * bpl, bpl) == l1[y];
            ok &= ddr.read(b1 + y * bpl, bpl) == l2[y];
        }
        CHECK(ok);
        CHECK_EQ(rd(FX1_CSI2_BUFFER_OWNERSHIP), 0x5u);
        CHECK_EQ(rd(FX1_CSI2_FRAME_COUNT), 2u);
        CHECK_EQ(rd(FX1_CSI2_FRAME_DROP_COUNT), 0u);
        CHECK_EQ(axis.lines().size(), 2u * h);
        CHECK(axis.violations().empty());
        check_clean_dma();
    }

    // M4a: TPG frames to DDR, packed RAW12, through the CSR only.
    void dma_tpg() {
        const unsigned w = 32, h = 8, bpl = 48;
        bring_up(4);
        const uint32_t b0 = 0x80200000u, b1 = 0x80300000u;
        wr(FX1_CSI2_STREAM_CONFIG, 0);
        wr(FX1_CSI2_DMA_BUFFER0_ADDR, b0);
        wr(FX1_CSI2_DMA_BUFFER1_ADDR, b1);
        wr(FX1_CSI2_DMA_BUFFER_SIZE, 0x1000);
        wr(FX1_CSI2_LINE_PITCH, bpl);
        wr(FX1_CSI2_DMA_ATTR, 0xFFu << FX1_CSI2_DMA_ATTR_MAX_BURST_LEN_SHIFT);
        wr(FX1_CSI2_DMA_CONTROL, FX1_CSI2_DMA_CONTROL_DMA_ENABLE_MASK | FX1_CSI2_DMA_CONTROL_DMA_START_MASK);
        wr(FX1_CSI2_TPG_RESOLUTION, (h << 16) | w);
        wr(FX1_CSI2_TPG_CONTROL, FX1_CSI2_TPG_CONTROL_TPG_ENABLE_MASK);  // RAW12
        CHECK(poll(FX1_CSI2_BUFFER_OWNERSHIP, 0xFu, 0x5u, sc_time(100, SC_NS), 100));
        wr(FX1_CSI2_TPG_CONTROL, 0);
        wait(2, SC_US);
        bool ok = true;
        for (uint32_t base : {b0, b1})
            for (unsigned y = 0; y < h; ++y) {
                std::vector<uint16_t> px(w);
                for (unsigned x = 0; x < w; ++x) px[x] = tpg_oracle(x, y, w, false);
                ok &= ddr.read(base + y * bpl, bpl) == bfm::pack_raw12(px);
            }
        CHECK(ok);
        CHECK(ddr.untouched(b0 + h * bpl, 0x1000 - h * bpl) && ddr.untouched(b1 + h * bpl, 0x1000 - h * bpl));
        CHECK_EQ(rd(FX1_CSI2_FRAME_ID), (2u << 16) | 1u);
        // Every TPG capture ended once: 2 delivered, the rest dropped (both sides owned).
        CHECK_EQ(rd(FX1_CSI2_FRAME_COUNT), 2u);
        CHECK_EQ(rd(FX1_CSI2_PACKET_COUNT), 0u);
        CHECK_EQ(rd(FX1_CSI2_DMA_STATUS) & FX1_CSI2_DMA_STATUS_DMA_BUSY_MASK, 0u);
    }

    void check_clean_dma() {
        CHECK_EQ(rd(FX1_CSI2_IRQ_STATUS) & ((1u << IRQ_BUFFER_OVERFLOW) | (1u << IRQ_AXI_WRITE_ERROR) | (1u << IRQ_BUF_DONE_ERR)), 0u);
        CHECK_EQ(rd(FX1_CSI2_AXI_ERROR_COUNT), 0u);
        CHECK_EQ(ctl.ppi_x_samples(), 0u);
        CHECK_EQ(ctl.sys_datapath().cdc_lost_words(), 0u);
    }

    // M4b: the sequence FW runs, through the C99 driver only.
    void drv_camera(bool inject_error) {
        const unsigned n = 4, w = 64, h = 4, bpl = 2 * w;
        resetn.write(true);
        wait(1, SC_US);
        fx1_csi2_hal hal{&Tb::hal_read32, &Tb::hal_write32, &Tb::hal_delay_us, this};
        fx1_csi2_dev dev{};
        CHECK(fx1_csi2_probe(&dev, &hal) == FX1_CSI2_OK);
        CHECK(fx1_csi2_phy_start(&dev, n, 1000) == FX1_CSI2_OK);
        CHECK(fx1_csi2_core_enable(&dev, 1) == FX1_CSI2_OK);
        wr(FX1_CSI2_EXPECTED_RESOLUTION, (h << 16) | w);
        CHECK(fx1_csi2_stream_config(&dev, 0, 0) == FX1_CSI2_OK);  // DDR-only (D02)
        fx1_csi2_dma_cfg c{};
        c.base[0] = 0x80000000u;
        c.base[1] = 0x80100000u;
        c.size = 0x10000;
        c.bytes_per_line = static_cast<uint16_t>(bpl);
        c.max_burst_len = 255;
        c.raw16 = 1;
        c.release_on_error = 1;
        CHECK(fx1_csi2_dma_config(&dev, &c) == FX1_CSI2_OK);
        const uint32_t mask = (1u << IRQ_BUF_DONE_OK) | (1u << IRQ_BUF_DONE_ERR) | (1u << IRQ_FRAME_DROPPED) |
                              (1u << IRQ_AXI_WRITE_ERROR) | (1u << IRQ_FRAME_RECEIVED);
        CHECK(fx1_csi2_irq_enable(&dev, mask) == FX1_CSI2_OK);
        CHECK(fx1_csi2_dma_start(&dev) == FX1_CSI2_OK);
        tx.clock_start();
        wait(1, SC_US);
        unsigned writes = 0;
        if (inject_error)
            ddr.hook = [&](tlm::tlm_generic_payload&) {
                ++writes;
                return writes == h + 2 ? tlm::TLM_GENERIC_ERROR_RESPONSE : tlm::TLM_OK_RESPONSE;  // frame 2, line 1
            };
        unsigned ok_frames = 0, err_frames = 0;
        for (uint16_t f = 1; f <= 3; ++f) {
            const unsigned seed = 10 * f;
            send(bfm::short_packet(0, bfm::DT_FS, f), n);
            for (unsigned y = 0; y < h; ++y) send(bfm::long_packet(0, bfm::DT_RAW10, line_payload(w, true, seed + y)), n);
            send(bfm::short_packet(0, bfm::DT_FE, f), n);
            // Interrupt service: wait for the line, read and clear, handle the side.
            for (unsigned k = 0; k < 200 && !irq.read(); ++k) wait(50, SC_NS);
            CHECK(irq.read());
            const uint32_t pending = fx1_csi2_irq_handle(&dev);
            wait(10, SC_NS);
            CHECK(!irq.read());
            for (unsigned side = 0; side < 2; ++side) {
                fx1_csi2_buf_info bi{};
                CHECK(fx1_csi2_get_buffer(&dev, side, &bi) == FX1_CSI2_OK);
                if (bi.state == FX1_CSI2_BUF_HW_OWNED || bi.frame_id != f) continue;
                const uint32_t base = c.base[side];
                if (bi.state == FX1_CSI2_BUF_SW_OWNED_VALID) {
                    CHECK(pending & (1u << IRQ_BUF_DONE_OK));
                    bool exact = true;
                    for (unsigned y = 0; y < h; ++y) exact &= ddr.read(base + y * bpl, bpl) == raw16_of(line_pixels(w, true, seed + y));
                    CHECK(exact);
                    ++ok_frames;
                } else {
                    CHECK(pending & (1u << IRQ_BUF_DONE_ERR));
                    CHECK(pending & (1u << IRQ_AXI_WRITE_ERROR));
                    fx1_csi2_dma_fault flt{};
                    CHECK(fx1_csi2_get_dma_fault(&dev, &flt) == FX1_CSI2_OK);
                    CHECK_EQ(flt.resp, 2u);
                    CHECK_EQ(flt.addr, base + bpl);  // line 1 of the side
                    CHECK_EQ(flt.slverr_count, 1u);
                    CHECK_EQ(bi.last_valid_line, 0u);
                    CHECK(ddr.read(base, bpl) == raw16_of(line_pixels(w, true, seed)));
                    ++err_frames;
                }
                CHECK(fx1_csi2_buffer_release(&dev, side) == FX1_CSI2_OK);
            }
        }
        CHECK_EQ(ok_frames, inject_error ? 2u : 3u);
        CHECK_EQ(err_frames, inject_error ? 1u : 0u);
        fx1_csi2_status st{};
        CHECK(fx1_csi2_get_status(&dev, &st) == FX1_CSI2_OK);
        CHECK_EQ(st.frame_count, 3u);  // an error release counts as delivered (D06)
        CHECK_EQ(st.frame_drop_count, 0u);
        CHECK(fx1_csi2_dma_stop(&dev, 100) == FX1_CSI2_OK);
        CHECK_EQ(rd(FX1_CSI2_DMA_CONTROL) & (FX1_CSI2_DMA_CONTROL_DMA_ENABLE_MASK | FX1_CSI2_DMA_CONTROL_DMA_START_MASK), 0u);
        CHECK_EQ(rd(FX1_CSI2_BUFFER_OWNERSHIP), 0u);
        // After the stop the sensor keeps sending: nothing reaches DDR.
        const size_t wlog = ddr.log.size();
        send(bfm::short_packet(0, bfm::DT_FS, 4), n);
        send(bfm::long_packet(0, bfm::DT_RAW10, line_payload(w, true, 0)), n);
        send(bfm::short_packet(0, bfm::DT_FE, 4), n);
        CHECK_EQ(ddr.log.size(), wlog);
        CHECK_EQ(ctl.ppi_x_samples(), 0u);
    }

    // Review M3b F2 (C38): an image packet with WC = 0 does not consume the Start
    // of Frame; one SOF per frame, on its first pixel line.
    void zero_wc() {
        const unsigned n = 4;
        bring_up(n);
        wr(FX1_CSI2_EXPECTED_RESOLUTION, (2u << 16) | 32u);
        const Bytes empty;
        std::vector<Bytes> lines;
        send(bfm::short_packet(0, bfm::DT_FS, 1), n);
        send(bfm::long_packet(0, bfm::DT_RAW10, empty), n);  // at frame start
        for (unsigned y = 0; y < 2; ++y) {
            lines.push_back(line_payload(32, true, y));
            send(bfm::long_packet(0, bfm::DT_RAW10, lines.back()), n);
        }
        send(bfm::short_packet(0, bfm::DT_FE, 1), n);
        send(bfm::short_packet(0, bfm::DT_FS, 2), n);
        lines.push_back(line_payload(32, true, 5));
        send(bfm::long_packet(0, bfm::DT_RAW10, lines.back()), n);
        send(bfm::long_packet(0, bfm::DT_RAW10, empty), n);  // between lines
        lines.push_back(line_payload(32, true, 6));
        send(bfm::long_packet(0, bfm::DT_RAW10, lines.back()), n);
        send(bfm::short_packet(0, bfm::DT_FE, 2), n);
        CHECK(stream_lines(axis) == lines);
        CHECK_EQ(axis.lines().size(), 4u);
        if (axis.lines().size() == 4)
            CHECK(axis.lines()[0].sof && !axis.lines()[1].sof && axis.lines()[2].sof && !axis.lines()[3].sof);
        CHECK_EQ(rd(FX1_CSI2_FRAME_COUNT), 2u);
        CHECK_EQ(rd(FX1_CSI2_VC0_IMAGE_INFO1), (2u << 16) | 40u);  // the empty packets are not lines
        CHECK_EQ(rd(FX1_CSI2_PACKET_COUNT), 6u);                   // but they are packets
        CHECK_EQ(unsigned(ctl.packet_analyzer().context(0).line_count), 2u);  // Frame Supervisor: real lines only
        CHECK_EQ(rd(FX1_CSI2_IRQ_STATUS) & (1u << IRQ_RESOLUTION_MISMATCH), 0u);
        check_clean();
    }

    // M3b: the Test Pattern Generator through the CSR; it bypasses the PHY and
    // Packet Analyzer (no PACKET_COUNT), and the source switches back to the sensor.
    void tpg() {
        const unsigned n = 4;
        bring_up(n);
        wr(FX1_CSI2_TPG_RESOLUTION, (8u << 16) | 32u);
        // tpg_dual_vc_en is stored but unsupported in the linear build (C28): a
        // VP warning, and the generator still emits VC0 only (TID 0).
        const int warn0 = sc_core::sc_report_handler::get_count("fx1/csi2/ctrl");
        wr(FX1_CSI2_TPG_CONTROL, FX1_CSI2_TPG_CONTROL_TPG_ENABLE_MASK | FX1_CSI2_TPG_CONTROL_TPG_PIXEL_FORMAT_MASK |
                                     FX1_CSI2_TPG_CONTROL_TPG_DUAL_VC_EN_MASK);
        CHECK_EQ(sc_core::sc_report_handler::get_count("fx1/csi2/ctrl"), warn0 + 1);
        CHECK(rd(FX1_CSI2_TPG_CONTROL) & FX1_CSI2_TPG_CONTROL_TPG_DUAL_VC_EN_MASK);
        CHECK(poll(FX1_CSI2_CORE_STATUS, FX1_CSI2_CORE_STATUS_ACTIVE_SRC_MASK | FX1_CSI2_CORE_STATUS_SRC_SWITCH_PENDING_MASK,
                   FX1_CSI2_CORE_STATUS_ACTIVE_SRC_MASK, sc_time(100, SC_NS), 20));
        wait(1, SC_US);  // a 32x8 frame is 66 sys_clk cycles
        wr(FX1_CSI2_TPG_CONTROL, 0);  // stop at the end of the frame in progress
        CHECK(poll(FX1_CSI2_CORE_STATUS, FX1_CSI2_CORE_STATUS_ACTIVE_SRC_MASK, 0, sc_time(100, SC_NS), 50));
        wait(200, SC_NS);  // the tail of the last frame leaves Format Bridge and the stream master
        const uint32_t tpg_frames = rd(FX1_CSI2_FRAME_COUNT);
        CHECK(tpg_frames >= 3);
        CHECK_EQ(axis.lines().size(), 8u * tpg_frames);  // whole frames only
        bool exact = true;
        for (size_t i = 0; i < axis.lines().size(); ++i) {
            std::vector<uint16_t> px(32);
            for (unsigned x = 0; x < 32; ++x) px[x] = tpg_oracle(x, i % 8, 32, true);
            exact &= axis.lines()[i].bytes == bfm::pack_raw10(px) && axis.lines()[i].sof == (i % 8 == 0) &&
                     axis.lines()[i].tid == 0;
        }
        CHECK(exact);
        CHECK_EQ(rd(FX1_CSI2_PACKET_COUNT), 0u);
        CHECK_EQ(rd(FX1_CSI2_VC0_IMAGE_INFO1), 0u);  // no geometry on the TPG path
        // Back to the sensor: switch pending until the sensor's first Start of Frame.
        CHECK(rd(FX1_CSI2_CORE_STATUS) & FX1_CSI2_CORE_STATUS_SRC_SWITCH_PENDING_MASK);
        const size_t from = axis.lines().size();
        const auto lines = send_frame(n, 32, 4, true, 1);
        CHECK(stream_lines(axis, from) == lines);
        CHECK_EQ(rd(FX1_CSI2_CORE_STATUS) & FX1_CSI2_CORE_STATUS_SRC_SWITCH_PENDING_MASK, 0u);
        CHECK_EQ(rd(FX1_CSI2_FRAME_COUNT), tpg_frames + 1);
        check_clean();
    }

    // One 640x480 RAW10 frame on 4 lanes through the detailed PHY (cost for M5/M6).
    void bench() {
        const unsigned n = 4;
        bring_up(n);
        wr(FX1_CSI2_EXPECTED_RESOLUTION, (480u << 16) | 640u);
        const auto w0 = std::chrono::steady_clock::now();
        const auto t0 = sc_core::sc_time_stamp();
        const uint64_t c0 = ctl.sys_datapath().cycles();
        const auto lines = send_frame(n, 640, 480, true, 1);
        const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - w0).count();
        const double sim = (sc_core::sc_time_stamp() - t0).to_seconds();
        std::printf("  bench 640x480 RAW10 4 lanes: %.3f ms simulated, %.1f s wall, %.1f us simulated per wall second, "
                    "%llu sys_clk cycles, %.1f KB/s payload\n",
                    sim * 1e3, wall, sim * 1e6 / wall, (unsigned long long)(ctl.sys_datapath().cycles() - c0),
                    640.0 * 480 * 10 / 8 / wall / 1e3);
        CHECK(stream_lines(axis) == lines);
        CHECK_EQ(rd(FX1_CSI2_FRAME_COUNT), 1u);
        CHECK_EQ(rd(FX1_CSI2_VC0_IMAGE_INFO1), (480u << 16) | 800u);
        CHECK_EQ(rd(FX1_CSI2_IRQ_STATUS) & (1u << IRQ_RESOLUTION_MISMATCH), 0u);
        check_clean();
    }

    void unarmed() {
        bring_up(4, 0x7);  // lane 3 not enabled at the D-PHY
        send(bfm::short_packet(0, bfm::DT_FS, 1), 4, [](auto& lb) { lb[3].bytes.clear(); });
        // Lane 3 never arms; the trail bytes of lanes 0-2 fill their FIFOs
        // before the burst closes: abort_fifo_full, sync_seen 0111 (C19).
        const uint32_t las = rd(FX1_CSI2_LANE_ALIGN_STATUS);
        CHECK_EQ(las, FX1_CSI2_LANE_ALIGN_STATUS_ABORT_FIFO_FULL_MASK |
                          (0x7u << FX1_CSI2_LANE_ALIGN_STATUS_ABORT_LANE_SYNC_SEEN_SHIFT));
        CHECK_EQ(rd(FX1_CSI2_LANE_ABORT_COUNT), 1u);
        CHECK(sink.fs == 0);
    }

    void skew() {
        const unsigned n = 4;
        bring_up(n);
        // Lanes 1..3 late by 4, 8 and 16 UI (up to 2 bytes): absorbed.
        auto skewed = [](unsigned l1, unsigned l2, unsigned l3) {
            return [=](std::vector<bfm::LaneBurst>& lb) {
                lb[1].skew_ui = l1;
                lb[2].skew_ui = l2;
                lb[3].skew_ui = l3;
            };
        };
        send(bfm::short_packet(0, bfm::DT_FS, 1), n, skewed(4, 8, 16));
        const Bytes l0 = line_payload(32, true, 0);
        send(bfm::long_packet(0, bfm::DT_RAW10, l0), n, skewed(4, 8, 16));
        send(bfm::short_packet(0, bfm::DT_FE, 1), n, skewed(4, 8, 16));
        CHECK(sink.lines.size() == 1 && sink.lines[0] == l0);
        check_clean();
        // Lane 3 late by 64 UI (8 bytes) > usable FIFO depth (6 bytes): fifo_full abort.
        send(bfm::long_packet(0, bfm::DT_RAW10, l0), n, skewed(0, 0, 64));
        const uint32_t las = rd(FX1_CSI2_LANE_ALIGN_STATUS);
        CHECK(las & FX1_CSI2_LANE_ALIGN_STATUS_ABORT_FIFO_FULL_MASK);
        CHECK_EQ((las & FX1_CSI2_LANE_ALIGN_STATUS_ABORT_LANE_SYNC_SEEN_MASK) >> FX1_CSI2_LANE_ALIGN_STATUS_ABORT_LANE_SYNC_SEEN_SHIFT, 0x7u);
        CHECK_EQ(rd(FX1_CSI2_LANE_ABORT_COUNT), 1u);
    }

    // Review M3a F1: the driver's soft reset must reach Lane Management.
    void soft_reset_mid_burst() {
        const unsigned n = 4;
        bring_up(n);
        fx1_csi2_hal hal{&Tb::hal_read32, &Tb::hal_write32, &Tb::hal_delay_us, this};
        fx1_csi2_dev dev{};
        CHECK(fx1_csi2_probe(&dev, &hal) == FX1_CSI2_OK);
        send(bfm::short_packet(0, bfm::DT_FS, 1), n);
        const Bytes big = line_payload(800, true, 0);  // 1000 bytes: ~2.5 us on 4 lanes
        bool sent = false;
        sc_core::sc_spawn([&] {
            send(bfm::long_packet(0, bfm::DT_RAW10, big), n);
            sent = true;
        });
        for (unsigned i = 0; i < 2000 && ctl.lane_management().state() != LmState::Aligned; ++i) wait(5, SC_NS);
        CHECK(ctl.lane_management().state() == LmState::Aligned);
        const uint64_t resets0 = ctl.lane_management().resets();
        const sc_time t0 = sc_core::sc_time_stamp();
        const int rc = fx1_csi2_soft_reset(&dev, 100);
        if (which == "soft_reset_latency") {
            // The HAL consumes the configured 20 ns per access: the two soft_reset
            // writes are 20 ns apart and their effect is still a single reset.
            CHECK(sc_core::sc_time_stamp() - t0 >= sc_time(20, SC_NS));
        }
        std::printf("  soft reset in Aligned: rc=%d after %.3f us, LM resets %llu -> %llu, LM state %u\n", rc,
                    (sc_core::sc_time_stamp() - t0).to_seconds() * 1e6, (unsigned long long)resets0,
                    (unsigned long long)ctl.lane_management().resets(), unsigned(ctl.lane_management().state()));
        CHECK(rc == FX1_CSI2_OK);
        CHECK(ctl.lane_management().resets() > resets0);  // applied by a byte-clock edge before OK
        CHECK(sc_core::sc_time_stamp() - t0 <= sc_time(110, SC_US));
        while (!sent) wait(100, SC_NS);
        wait(1, SC_US);
        // The rest of the burst has no SoT: it is discarded as unarmed, nothing delivered.
        CHECK(sink.lines.empty());
        CHECK_EQ(rd(FX1_CSI2_LANE_ALIGN_STATUS), FX1_CSI2_LANE_ALIGN_STATUS_ABORT_UNARMED_MASK);
        // Recovery: a clean frame.
        wr(FX1_CSI2_IRQ_STATUS, 0x00FFFFFF);
        const auto lines = send_frame(n, 32, 4, true, 2);
        CHECK(sink.lines == lines);
        CHECK_EQ(rd(FX1_CSI2_IRQ_STATUS) & kErrorIrqs, 0u);
    }

    void soft_reset_noclk() {
        bring_up(4, 0, false);  // stop state, clock lane in LP-11: no byte clock
        fx1_csi2_hal hal{&Tb::hal_read32, &Tb::hal_write32, &Tb::hal_delay_us, this};
        fx1_csi2_dev dev{};
        CHECK(fx1_csi2_probe(&dev, &hal) == FX1_CSI2_OK);
        const uint64_t resets0 = ctl.lane_management().resets();
        CHECK(fx1_csi2_soft_reset(&dev, 100) == FX1_CSI2_ENOCLK);
        CHECK(ctl.lane_management().resets() == resets0);  // not applied: no edge
        CHECK_EQ(rd(FX1_CSI2_CORE_STATUS) & FX1_CSI2_CORE_STATUS_RESET_IN_PROGRESS_MASK, 0u);
        tx.clock_start();  // the pending request is applied by the first edge
        wait(1, SC_US);
        CHECK(ctl.lane_management().resets() > resets0);
        const auto lines = send_frame(4, 32, 2, true);
        CHECK(sink.lines == lines);
    }

    void run() {
        const std::map<std::string, std::function<void()>> cases = {
            {"soft_reset_aligned", [this] { soft_reset_mid_burst(); }},
            {"soft_reset_latency", [this] { soft_reset_mid_burst(); }},
            {"soft_reset_noclk", [this] { soft_reset_noclk(); }},
            {"frame_raw10_4lane", [this] { frame(4, true); }},
            {"frame_raw12_2lane", [this] { frame(2, false); }},
            {"frame_raw10_1lane", [this] { frame(1, true); }},
            {"errors", [this] { errors(); }},
            {"unarmed", [this] { unarmed(); }},
            {"skew", [this] { skew(); }},
            {"stall_drop", [this] { stall_drop(); }},
            {"core_pulse_lat0", [this] { core_pulse(); }},
            {"core_pulse_lat1", [this] { core_pulse(); }},
            {"core_pulse_lat20", [this] { core_pulse(); }},
            {"zero_wc", [this] { zero_wc(); }},
            {"dma_sensor", [this] { dma_sensor(); }},
            {"dma_tpg", [this] { dma_tpg(); }},
            {"dma_stream_backlog", [this] { dma_stream_backlog(); }},
            {"drv_camera", [this] { drv_camera(false); }},
            {"drv_recover", [this] { drv_camera(true); }},
            {"tpg", [this] { tpg(); }},
            {"bench_640x480", [this] { bench(); }},
        };
        const auto it = cases.find(which);
        if (it == cases.end()) {
            std::fprintf(stderr, "unknown case %s\n", which.c_str());
            ++g_fail;
        } else {
            it->second();
        }
        sc_core::sc_stop();
    }
};
}  // namespace

int sc_main(int argc, char** argv) {
    sc_core::sc_set_time_resolution(1, sc_core::SC_FS);
    const std::string which = argc > 1 ? argv[1] : "frame_raw10_4lane";
    Tb tb("tb", which);
    sc_core::sc_start(which == "bench_640x480" ? 100 : 20, sc_core::SC_MS);
    if (sc_core::sc_get_status() != sc_core::SC_STOPPED) {
        std::printf("case did not finish\n");
        ++g_fail;
    }
    std::printf("%s: %s\n", which.c_str(), g_fail ? "FAIL" : "PASS");
    return g_fail ? 1 : 0;
}
