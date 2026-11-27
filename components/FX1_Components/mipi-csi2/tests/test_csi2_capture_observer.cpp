// VP-only capture observer and stream sideband of the CSI-2 RX controller
// (direct CSI->ISP plan D2a; contract docs/DIRECT_CSI_ISP_CONTRACT.md §9,
// P25): capture_start / capture_end / capture_stream_end / source_reset and
// AxisBeat::vp_capture_seq / vp_frame_id, on the real controller.
//
//   test_csi2_capture_observer <session>
//     tpg        TPG frames back to back: start before the SOF beat, every beat
//                of the capture's sequence, Ok outcome, the stream boundary
//                after the capture's last beat; sequence +1 per capture
//     tpg_stall  the same with TREADY held low for long stretches (terminals
//                come before the beats leave; boundaries still after them)
//     wrap       FRAME_ID preset to 16382: 16383, 1, 2 (0 skipped); the 64-bit
//                sequence keeps counting; soft_reset: source_reset with the
//                new epoch, the sequence survives
//     bfm_missing_fe  sensor BFM through the D-PHY: a capture without its FE is
//                closed by the next FS: MissingFe; the next capture Ok
//     bfm_crc    sensor BFM: a CRC error: Dropped, the abort beat carries the
//                capture's sequence, the boundary after it; the next capture Ok
//     stop_line  TPG 64x32, CORE_CONFIG = 0 between two lines of a capture
//                (D2a review F1): Dropped, one abort beat carrying it, the
//                boundary after the abort
//     stop_line_stall  the same with TREADY stalling
//     stop_midline_stall  CORE_CONFIG = 0 inside a line held by the stall
//
// Exit status 0 = PASS.
#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include "fx1_csi2/csi2_rx_controller.h"
#include "fx1_csi2/fx1_csi2_regs.h"
#include "fx1_csi2_bfm/csi2_packet.h"
#include "fx1_csi2_bfm/dphy_tx_bfm.h"
#include "fx1_csi2_phy/dphy_rx_wrapper.h"

using namespace fx1::csi2;
using sc_core::sc_time;
using sc_core::SC_NS;
using sc_core::SC_US;
using Bytes = std::vector<uint8_t>;

namespace {

int g_fail = 0;
void check(bool ok, const std::string& what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what.c_str());
        ++g_fail;
    }
}

// One ordered log of everything the observer and the sink saw.
struct Ev {
    enum Kind { Start, End, StreamEnd, Reset, Beat } k;
    uint64_t seq = 0;
    uint16_t fid = 0;
    CaptureOutcome out = CaptureOutcome::Ok;
    uint32_t epoch = 0;
    AxisBeat beat{};
};

struct Recorder : CaptureObserver, AxisSink {
    std::vector<Ev> log;
    // TREADY policy: accepted beats so far, cycles the current beat has waited.
    std::function<bool(size_t, unsigned)> ready = [](size_t, unsigned) { return true; };
    size_t beats = 0;
    unsigned waited = 0;
    void capture_start(uint64_t seq, uint16_t fid, uint64_t, uint32_t epoch) override {
        Ev e{Ev::Start};
        e.seq = seq;
        e.fid = fid;
        e.epoch = epoch;
        log.push_back(e);
    }
    void capture_end(uint64_t seq, CaptureOutcome o) override {
        Ev e{Ev::End};
        e.seq = seq;
        e.out = o;
        log.push_back(e);
    }
    void capture_stream_end(uint64_t seq) override {
        Ev e{Ev::StreamEnd};
        e.seq = seq;
        log.push_back(e);
    }
    void source_reset(uint32_t epoch) override {
        Ev e{Ev::Reset};
        e.epoch = epoch;
        log.push_back(e);
    }
    bool axis_cycle(bool tvalid, const AxisBeat& b) override {
        if (!tvalid) return true;
        const bool r = ready(beats, waited);
        waited = r ? 0 : waited + 1;
        if (r) {
            Ev e{Ev::Beat};
            e.beat = b;
            e.seq = b.vp_capture_seq;
            e.fid = b.vp_frame_id;
            log.push_back(e);
            ++beats;
        }
        return r;
    }
    size_t count(Ev::Kind k) const {
        size_t n = 0;
        for (const Ev& e : log) n += e.k == k;
        return n;
    }
};

uint16_t sensor_pixel(unsigned x, unsigned y) { return uint16_t((x * 37u + y * 101u + 58u) & 0xFFFu); }

struct Bench : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<Bench> socket{"socket"};
    sc_core::sc_signal<bool> resetn{"resetn", false};
    sc_core::sc_signal<bool> irq{"irq"};
    DphyRxWrapper phy{"phy_model"};
    Csi2RxController ctl;
    bfm::DphyTxBfm tx{"tx", phy.clkp, phy.clkn, phy.dp, phy.dn};
    Recorder rec;
    std::string session;
    bool completed = false;

    SC_HAS_PROCESS(Bench);
    Bench(sc_core::sc_module_name nm, std::string s) : sc_core::sc_module(nm), ctl("csi2", phy), session(std::move(s)) {
        socket.bind(ctl.csr_socket);
        ctl.resetn(resetn);
        ctl.irq(irq);
        ctl.set_stream_sink(&rec);
        ctl.set_capture_observer(&rec);
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
        if (d != sc_core::SC_ZERO_TIME) wait(d);
        check(p.get_response_status() == tlm::TLM_OK_RESPONSE, "CSR access");
        return b[0] | b[1] << 8 | b[2] << 16 | uint32_t(b[3]) << 24;
    }
    uint32_t rd(uint32_t off) { return xfer(tlm::TLM_READ_COMMAND, off, 0); }
    void wr(uint32_t off, uint32_t v) { xfer(tlm::TLM_WRITE_COMMAND, off, v); }
    void stream_raw16() {
        wr(FX1_CSI2_STREAM_CONFIG, FX1_CSI2_STREAM_CONFIG_STREAM_FORMAT_MASK);
        wr(FX1_CSI2_STREAM_CONFIG, 0);
        wr(FX1_CSI2_STREAM_CONFIG, FX1_CSI2_STREAM_CONFIG_AXIS_ENABLE_MASK);
    }

    // `n` TPG frames of 32x8 back to back (short vertical blanking).
    void tpg_frames(unsigned n) {
        wr(FX1_CSI2_TPG_RESOLUTION, (8u << 16) | 32u);
        wr(FX1_CSI2_TPG_BLANKING, 4u << 16);
        const size_t starts = rec.count(Ev::Start);
        wr(FX1_CSI2_TPG_CONTROL, FX1_CSI2_TPG_CONTROL_TPG_ENABLE_MASK);
        for (unsigned i = 0; i < 2000 && rec.count(Ev::Start) < starts + n; ++i) wait(1, SC_US);
        wr(FX1_CSI2_TPG_CONTROL, 0);  // the frame in progress completes
        wait(200, SC_US);
    }

    // The ordering rules of the log, for every capture seen.
    void check_log(const std::string& what, CaptureOutcome want) {
        std::vector<uint64_t> started;
        for (size_t i = 0; i < rec.log.size(); ++i) {
            const Ev& e = rec.log[i];
            if (e.k == Ev::Start) {
                if (!started.empty()) check(e.seq == started.back() + 1, what + ": sequence +1 per capture");
                started.push_back(e.seq);
                continue;
            }
            if (e.k != Ev::Beat) continue;
            const bool abort = e.beat.tkeep == 0 && e.beat.tlast && (e.beat.tuser & 2u);
            // An abort beat after the boundary of the last capture it could
            // belong to carries 0, "no capture" (D2a review F1).
            check(e.seq != 0 || abort, what + ": every data beat carries a capture sequence");
            if (e.seq == 0) continue;
            bool seen_start = false, seen_stream_end = false;
            uint16_t fid = 0;
            for (size_t j = 0; j < i; ++j) {
                const Ev& p = rec.log[j];
                if (p.k == Ev::Start && p.seq == e.seq) {
                    seen_start = true;
                    fid = p.fid;
                }
                if (p.k == Ev::StreamEnd && p.seq == e.seq) seen_stream_end = true;
            }
            check(seen_start, what + ": capture_start before the capture's beats");
            check(!seen_stream_end, what + ": no beat of a capture after its stream boundary");
            if (!abort) check(e.fid == fid, what + ": vp_frame_id = the capture's FRAME_ID");
        }
        for (const uint64_t s : started) {
            size_t ends = 0, sends = 0;
            for (const Ev& e : rec.log) {
                if (e.k == Ev::End && e.seq == s) {
                    ++ends;
                    if (want == CaptureOutcome::Ok) check(e.out == CaptureOutcome::Ok, what + ": outcome Ok");
                }
                sends += e.k == Ev::StreamEnd && e.seq == s;
            }
            check(ends == 1, what + ": one capture_end per capture");
            check(sends == 1, what + ": one capture_stream_end per capture");
        }
        check(!started.empty(), what + ": captures seen");
    }

    void s_tpg(bool stall) {
        resetn.write(true);
        wait(1, SC_US);
        stream_raw16();
        if (stall) rec.ready = [](size_t k, unsigned waited) { return k % 8 != 0 || waited >= 300; };  // 1.2 us per line
        tpg_frames(4);
        check_log(stall ? "tpg_stall" : "tpg", CaptureOutcome::Ok);
        if (stall) {  // a terminal came before the capture's last beat left
            bool early = false;
            for (size_t i = 0; i < rec.log.size(); ++i)
                if (rec.log[i].k == Ev::End)
                    for (size_t j = i + 1; j < rec.log.size(); ++j)
                        early = early || (rec.log[j].k == Ev::Beat && rec.log[j].seq == rec.log[i].seq);
            check(early, "tpg_stall: capture_end before the capture's last beat (the case the boundary is for)");
        }
    }

    void s_wrap() {
        resetn.write(true);
        wait(1, SC_US);
        stream_raw16();
        ctl.debug_set_frame_id_counter(16382);
        tpg_frames(3);
        std::vector<uint16_t> fids;
        std::vector<uint64_t> seqs;
        for (const Ev& e : rec.log)
            if (e.k == Ev::Start) {
                fids.push_back(e.fid);
                seqs.push_back(e.seq);
            }
        check(fids.size() >= 3 && fids[0] == 16383 && fids[1] == 1 && fids[2] == 2, "wrap: FRAME_ID 16383, 1, 2");
        check_log("wrap", CaptureOutcome::Ok);
        // soft_reset: source_reset with the controller's new epoch; the sequence survives.
        const uint32_t epoch = ctl.epoch();
        wr(FX1_CSI2_CORE_CONFIG, FX1_CSI2_CORE_CONFIG_CORE_ENABLE_MASK | FX1_CSI2_CORE_CONFIG_SOFT_RESET_MASK);
        wr(FX1_CSI2_CORE_CONFIG, FX1_CSI2_CORE_CONFIG_CORE_ENABLE_MASK);
        wait(10, SC_US);
        check(rd(FX1_CSI2_STREAM_CONFIG) == FX1_CSI2_STREAM_CONFIG_AXIS_ENABLE_MASK, "wrap: STREAM_CONFIG kept by soft_reset");
        bool reset_seen = false;
        for (const Ev& e : rec.log) reset_seen = reset_seen || (e.k == Ev::Reset && e.epoch == epoch + 1);
        check(reset_seen, "wrap: source_reset with the new epoch");
        tpg_frames(1);
        uint64_t last = 0;
        uint32_t start_epoch = 0;
        for (const Ev& e : rec.log)
            if (e.k == Ev::Start) {
                last = e.seq;
                start_epoch = e.epoch;
            }
        check(!seqs.empty() && last > seqs.back(), "wrap: the sequence survives soft_reset");
        check(start_epoch == epoch + 1, "wrap: capture_start reports the new epoch");
    }

    // ------------------------------------------------------ sensor (BFM)
    void bring_up(unsigned lanes) {
        resetn.write(true);
        wait(1, SC_US);
        const uint32_t enc = lanes == 1 ? 0 : lanes == 2 ? 1 : 3;
        const uint32_t dl = (1u << lanes) - 1;
        wr(FX1_CSI2_PROTOCOL_CONFIG, enc);
        wr(FX1_CSI2_PHY_CONFIG, FX1_CSI2_PHY_CONFIG_PHY_ENABLE_MASK | FX1_CSI2_PHY_CONFIG_CL_ENABLE_MASK |
                                    (dl << FX1_CSI2_PHY_CONFIG_DL_ENABLE_SHIFT));
        uint32_t stop = 0;
        for (unsigned i = 0; i < lanes; ++i) stop |= 1u << (8 * i);
        for (unsigned i = 0; i < 60 && (rd(FX1_CSI2_LANE_STATUS) & stop) != stop; ++i) wait(10, SC_US);
        check((rd(FX1_CSI2_LANE_STATUS) & stop) == stop, "D-PHY data lanes in stop state");
        wr(FX1_CSI2_CORE_CONFIG, FX1_CSI2_CORE_CONFIG_CORE_ENABLE_MASK);
        stream_raw16();
        tx.clock_start();
        for (unsigned i = 0; i < 20 && !(rd(FX1_CSI2_PHY_STATUS) & FX1_CSI2_PHY_STATUS_CL_RXCLKACTIVEHS_MASK); ++i)
            wait(100, SC_NS);
        wait(200, SC_NS);
    }
    void send(const Bytes& pkt) {
        const auto split = bfm::distribute_lanes(pkt, 2);
        std::vector<bfm::LaneBurst> lb(2);
        for (unsigned i = 0; i < 2; ++i) lb[i].bytes = split[i];
        tx.send_burst(lb);
        wait(300, SC_NS);
    }
    void bfm_frame(uint16_t fnum, bool with_fe, const std::function<void(unsigned, Bytes&)>& mod = {}) {
        send(bfm::short_packet(0, bfm::DT_FS, fnum));
        for (unsigned y = 0; y < 8; ++y) {
            std::vector<uint16_t> px(32);
            for (unsigned x = 0; x < 32; ++x) px[x] = sensor_pixel(x, y);
            Bytes pkt = bfm::long_packet(0, bfm::DT_RAW12, bfm::pack_raw12(px));
            if (mod) mod(y, pkt);
            send(pkt);
        }
        if (with_fe) send(bfm::short_packet(0, bfm::DT_FE, fnum));
        wait(2, SC_US);
    }
    std::vector<std::pair<uint64_t, CaptureOutcome>> ends() const {
        std::vector<std::pair<uint64_t, CaptureOutcome>> v;
        for (const Ev& e : rec.log)
            if (e.k == Ev::End) v.push_back({e.seq, e.out});
        return v;
    }

    void s_bfm_missing_fe() {
        bring_up(2);
        bfm_frame(1, false);  // FE lost
        bfm_frame(2, true);
        wait(50, SC_US);
        const auto e = ends();
        check(e.size() == 2, "bfm_missing_fe: two terminals");
        check(e.size() == 2 && e[0].second == CaptureOutcome::MissingFe, "bfm_missing_fe: first capture MissingFe");
        check(e.size() == 2 && e[1].second == CaptureOutcome::Ok, "bfm_missing_fe: second capture Ok");
        check_log("bfm_missing_fe", CaptureOutcome::MissingFe);
    }

    void s_bfm_crc() {
        bring_up(2);
        bfm_frame(1, true, [](unsigned y, Bytes& pkt) {
            if (y == 3) pkt.back() ^= 0xFF;
        });
        bfm_frame(2, true);
        wait(50, SC_US);
        const auto e = ends();
        check(e.size() == 2 && e[0].second == CaptureOutcome::Dropped, "bfm_crc: first capture Dropped");
        check(e.size() == 2 && e[1].second == CaptureOutcome::Ok, "bfm_crc: second capture Ok");
        bool abort_tagged = false;
        for (const Ev& v : rec.log)
            if (v.k == Ev::Beat && v.beat.tkeep == 0 && v.beat.tlast && (v.beat.tuser & 2u))
                abort_tagged = !e.empty() && v.seq == e[0].first;
        check(abort_tagged, "bfm_crc: the abort beat carries the dropped capture's sequence");
        check_log("bfm_crc", CaptureOutcome::Dropped);
    }

    // CORE_CONFIG = 0 inside a capture: after `beats` accepted beats.
    void s_stop(const std::string& what, size_t beats, bool stall) {
        resetn.write(true);
        wait(1, SC_US);
        stream_raw16();
        if (stall) rec.ready = [](size_t k, unsigned waited) { return k % 8 != 0 || waited >= 300; };
        wr(FX1_CSI2_TPG_RESOLUTION, (32u << 16) | 64u);  // 16 beats per line (RAW16)
        wr(FX1_CSI2_TPG_BLANKING, (4u << 16) | 1000u);   // 4 us between lines
        wr(FX1_CSI2_TPG_CONTROL, FX1_CSI2_TPG_CONTROL_TPG_ENABLE_MASK);
        for (unsigned i = 0; i < 200000 && rec.beats < beats; ++i) wait(4, SC_NS);
        check(rec.beats >= beats, what + ": beats streamed before the stop");
        wr(FX1_CSI2_CORE_CONFIG, 0);
        wait(20, SC_US);
        size_t aborts = 0, data_before = 0;
        uint64_t abort_seq = 0;
        for (const Ev& e : rec.log) {
            if (e.k != Ev::Beat) continue;
            if (e.beat.tkeep == 0 && e.beat.tlast && (e.beat.tuser & 2u)) {
                ++aborts;
                abort_seq = e.seq;
            } else if (!aborts) {
                ++data_before;
            }
        }
        check(aborts == 1 && abort_seq == 1, what + ": one abort beat, carrying the capture");
        check(rec.log.back().k == Ev::StreamEnd && rec.log.back().seq == 1, what + ": the boundary after the abort");
        if (beats % 16 != 0) check(data_before % 16 != 0, what + ": stopped inside a line");
        else check(data_before % 16 == 0, what + ": stopped between lines");
        check_log(what, CaptureOutcome::Dropped);
        check(rec.count(Ev::Start) == 1, what + ": one capture");
    }

    void run() {
        if (session == "stop_line") s_stop(session, 16, false);
        else if (session == "stop_line_stall") s_stop(session, 16, true);
        else if (session == "stop_midline_stall") s_stop(session, 8, true);
        else if (session == "tpg") s_tpg(false);
        else if (session == "tpg_stall") s_tpg(true);
        else if (session == "wrap") s_wrap();
        else if (session == "bfm_missing_fe") s_bfm_missing_fe();
        else if (session == "bfm_crc") s_bfm_crc();
        else check(false, "unknown session " + session);
        completed = true;
        sc_core::sc_stop();
    }
};

}  // namespace

int sc_main(int argc, char** argv) {
    sc_core::sc_set_time_resolution(1, sc_core::SC_FS);  // the detailed D-PHY model
    const std::string session = argc > 1 ? argv[1] : "tpg";
    Bench b("bench", session);
    sc_core::sc_start(sc_time(20, sc_core::SC_MS));
    check(b.completed, "session completed");
    std::printf("%s: %s (%zu events, %zu beats)\n", session.c_str(), g_fail ? "FAIL" : "PASS", b.rec.log.size(),
                b.rec.beats);
    return g_fail ? 1 : 0;
}
