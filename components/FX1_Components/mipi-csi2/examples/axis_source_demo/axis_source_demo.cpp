// CSI-2 RX -> AXI4-Stream source demo for the ISP owner (direct CSI->ISP plan,
// D2.1; contract: docs/DIRECT_CSI_ISP_CONTRACT.md §3, §13).
//
// The real CSI-2 RX controller model (fx1::csi2::Csi2RxController), programmed
// through its CSR socket, drives its own AXI4-Stream master (Block No. 12) into
// an example sink bound with set_stream_sink(). Nothing else of the SoC is
// needed: no ISP, NV12, firmware or CSI DMA. The detailed D-PHY RX model is
// instantiated because the controller is built on it; the TPG sessions do not
// use it, the sensor sessions send packets through it from the pad-level BFM.
//
//   fx1_csi2_axis_source_demo <session> [out_dir]
//     tpg_raw12    TPG 32x8 RAW12, TREADY = 1
//     tpg_raw10    TPG 32x8 RAW10, TREADY = 1
//     tpg_stall    TPG 32x8 RAW12, a fixed TREADY pattern: stalls on the SOF beat,
//                  mid-line, on an EOL beat, and every other cycle on one line
//     tpg_abort    TPG 32x8 RAW12, STREAM_CONFIG.axis_enable 1 -> 0 mid-frame:
//                  the held beats drain, then one null abort beat
//     bfm_tail     sensor BFM through the D-PHY, RAW12 34x8 on 2 lanes: the last
//                  beat of every line carries 2 samples, TKEEP = 0x0f
//     bfm_crc      as bfm_tail, line 3 with a corrupted CRC: the capture is dropped,
//                  a null abort beat follows; the next frame is whole
//     bfm_crc_last as bfm_crc, but the CRC of the LAST line (7) is corrupted: the
//                  whole payload (72 beats) leaves before the abort beat
//     bfm_wc       as bfm_tail, line 3 one byte short (WC not a whole RAW12 pixel
//                  pair): TUSER[1] on that line's last beat
//
// Writes <out_dir>/<session>.trace.csv (one row per sink call, i.e. per
// executed sys_clk cycle of the controller) and <session>.accepted.csv (the
// beats with TVALID && TREADY). Checks the accepted payload against the
// independent expected vectors of gen_expected.py (AXIS_DEMO_EXPECTED_DIR).
// Exit status 0 = PASS. The completion of the session is checked explicitly
// (a simulation that stops early is a FAIL).
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <functional>
#include <map>
#include <sstream>
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

#ifndef AXIS_DEMO_EXPECTED_DIR
#define AXIS_DEMO_EXPECTED_DIR "expected"
#endif

using namespace fx1::csi2;
using sc_core::sc_time;
using sc_core::SC_NS;
using sc_core::SC_US;
using Bytes = std::vector<uint8_t>;

namespace {

int g_fail = 0;
void fail(const char* what) {
    std::fprintf(stderr, "FAIL: %s\n", what);
    ++g_fail;
}

// ---------------------------------------------------------------- the sink
// What an ISP-side consumer implements: axis_cycle() is called once per
// executed sys_clk cycle with the values the master drives and returns
// TREADY for that cycle. A beat is transferred when tvalid && TREADY; a beat
// answered with TREADY = 0 is presented again, unchanged, on the next call.
// The function must not block (it runs inside the controller's process).
struct ExampleSink : AxisSink {
    std::function<bool(const AxisBeat&, size_t accepted, unsigned waited)> ready_policy;
    std::vector<AxisBeat> accepted;
    std::vector<uint64_t> accepted_cycle;
    std::ostringstream trace;
    sc_core::sc_event accepted_ev;
    unsigned waited = 0;  // cycles the current beat has been presented and refused

    ExampleSink() { trace << "cycle,time_ps,tvalid,tready,tdata,tkeep,tlast,tuser,tid,accepted\n"; }

    bool axis_cycle(bool tvalid, const AxisBeat& b) override {
        const uint64_t t_ps = sc_core::sc_time_stamp().value() / 1000u;  // 1 fs resolution
        const uint64_t cycle = t_ps / 4000u;                             // 4 ns sys_clk grid
        const bool ready = !tvalid || !ready_policy || ready_policy(b, accepted.size(), waited);
        const bool acc = tvalid && ready;
        char row[160];
        std::snprintf(row, sizeof row, "%llu,%llu,%d,%d,%016llx,0x%02x,%d,%u,%u,%d\n", (unsigned long long)cycle,
                      (unsigned long long)t_ps, tvalid, ready, (unsigned long long)b.tdata, b.tkeep, b.tlast,
                      unsigned(b.tuser), unsigned(b.tid), acc);
        trace << row;
        waited = tvalid && !ready ? waited + 1 : 0;
        if (acc) {
            accepted.push_back(b);
            accepted_cycle.push_back(cycle);
            accepted_ev.notify(sc_core::SC_ZERO_TIME);
        }
        return ready;
    }
};

// ------------------------------------------------------- expected payload
struct Expected {
    uint64_t tdata;
    uint8_t tkeep;
    bool tlast;
    uint8_t tuser, tid;
};

std::vector<Expected> load_expected(const std::string& name) {
    std::vector<Expected> v;
    std::ifstream f(std::string(AXIS_DEMO_EXPECTED_DIR) + "/" + name + ".csv");
    std::string line;
    std::getline(f, line);  // header
    while (std::getline(f, line)) {
        unsigned y, b, keep, last, user, tid;
        char data[32];
        if (std::sscanf(line.c_str(), "%u,%u,%16[0-9a-f],0x%x,%u,%u,%u", &y, &b, data, &keep, &last, &user, &tid) == 7)
            v.push_back({std::stoull(data, nullptr, 16), uint8_t(keep), last != 0, uint8_t(user), uint8_t(tid)});
    }
    if (v.empty()) fail(("expected vectors not found: " + name).c_str());
    return v;
}

bool same(const AxisBeat& a, const Expected& e) {
    return a.tdata == e.tdata && a.tkeep == e.tkeep && a.tlast == e.tlast && a.tuser == e.tuser && a.tid == e.tid;
}

bool is_null_abort(const AxisBeat& a) { return a.tkeep == 0 && a.tlast && (a.tuser & 2u); }

// Sensor pattern of the BFM sessions (same formula as gen_expected.py).
uint16_t sensor_pixel(unsigned x, unsigned y, unsigned bits) {
    return uint16_t((x * 37u + y * 101u + 53u + 5u) & ((1u << bits) - 1u));
}

// ------------------------------------------------------------- test bench
struct Demo : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<Demo> socket{"socket"};
    sc_core::sc_signal<bool> resetn{"resetn", false};
    sc_core::sc_signal<bool> irq{"irq"};
    DphyRxWrapper phy{"phy_model"};
    Csi2RxController ctl;
    bfm::DphyTxBfm tx{"tx", phy.clkp, phy.clkn, phy.dp, phy.dn};
    ExampleSink sink;
    std::string session, out_dir;
    bool completed = false;

    SC_HAS_PROCESS(Demo);
    Demo(sc_core::sc_module_name nm, std::string s, std::string out)
        : sc_core::sc_module(nm), ctl("csi2", phy), session(std::move(s)), out_dir(std::move(out)) {
        socket.bind(ctl.csr_socket);
        ctl.resetn(resetn);
        ctl.irq(irq);
        ctl.set_stream_sink(&sink);  // the integration point for an ISP ingress
        SC_THREAD(run);
    }

    // CSR access over the controller's AXI4-Lite (TLM) socket.
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
        if (p.get_response_status() != tlm::TLM_OK_RESPONSE) fail("CSR access");
        return b[0] | b[1] << 8 | b[2] << 16 | uint32_t(b[3]) << 24;
    }
    uint32_t rd(uint32_t off) { return xfer(tlm::TLM_READ_COMMAND, off, 0); }
    void wr(uint32_t off, uint32_t v) { xfer(tlm::TLM_WRITE_COMMAND, off, v); }

    // Stream on, RAW16 (baseline STREAM_CONFIG reset 0x3 = enabled, packed).
    // stream_format may change only while the stream is disabled, and a single
    // write that disables the stream and changes the format still counts as a
    // change while enabled: disable first with the format unchanged, then set
    // the format, then enable. This is what fx1_csi2_stream_config(dev, 0, 0)
    // followed by fx1_csi2_stream_config(dev, 1, 1) writes.
    void stream_raw16() {
        wr(FX1_CSI2_STREAM_CONFIG, FX1_CSI2_STREAM_CONFIG_STREAM_FORMAT_MASK);  // 0x2: disabled, still packed
        wr(FX1_CSI2_STREAM_CONFIG, 0);                                         // 0x0: RAW16, disabled
        wr(FX1_CSI2_STREAM_CONFIG, FX1_CSI2_STREAM_CONFIG_AXIS_ENABLE_MASK);   // 0x1: RAW16, enabled
    }

    // Wait until n beats were accepted, bounded.
    bool wait_accepted(size_t n, sc_time limit) {
        const sc_time end = sc_core::sc_time_stamp() + limit;
        while (sink.accepted.size() < n && sc_core::sc_time_stamp() < end) wait(sc_time(1, SC_US), sink.accepted_ev);
        return sink.accepted.size() >= n;
    }

    // ------------------------------------------------------------- TPG
    // One 32x8 frame: long vertical blanking, the generator disabled once the
    // frame has left, so exactly one frame reaches the sink.
    void tpg_frame(bool raw10, const std::function<void()>& mid_frame = {}) {
        resetn.write(true);
        wait(1, SC_US);
        stream_raw16();
        wr(FX1_CSI2_TPG_RESOLUTION, (8u << 16) | 32u);
        wr(FX1_CSI2_TPG_BLANKING, 2000u << 16);  // 2000 lines of vertical blanking after the frame
        wr(FX1_CSI2_TPG_CONTROL,
           FX1_CSI2_TPG_CONTROL_TPG_ENABLE_MASK | (raw10 ? FX1_CSI2_TPG_CONTROL_TPG_PIXEL_FORMAT_MASK : 0u));
        if (mid_frame) mid_frame();
        else wait_accepted(64, sc_time(100, SC_US));  // 8 lines x 8 beats
        wr(FX1_CSI2_TPG_CONTROL, 0);                  // stop at the end of the frame in progress
        wait(2, SC_US);
    }

    void check_payload(const std::string& expected, size_t from = 0) {
        const auto exp = load_expected(expected);
        if (sink.accepted.size() - from != exp.size()) {
            std::fprintf(stderr, "accepted %zu beats, expected %zu\n", sink.accepted.size() - from, exp.size());
            fail("payload beat count");
            return;
        }
        for (size_t i = 0; i < exp.size(); ++i)
            if (!same(sink.accepted[from + i], exp[i])) {
                std::fprintf(stderr, "beat %zu: %016llx keep %02x last %d user %u, expected %016llx keep %02x\n", i,
                             (unsigned long long)sink.accepted[from + i].tdata, sink.accepted[from + i].tkeep,
                             sink.accepted[from + i].tlast, unsigned(sink.accepted[from + i].tuser),
                             (unsigned long long)exp[i].tdata, exp[i].tkeep);
                fail("payload beat differs from the independent expected vector");
                return;
            }
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
        if ((rd(FX1_CSI2_LANE_STATUS) & stop) != stop) fail("D-PHY data lanes not in stop state");
        wr(FX1_CSI2_CORE_CONFIG, FX1_CSI2_CORE_CONFIG_CORE_ENABLE_MASK);
        wr(FX1_CSI2_IRQ_STATUS, 0x00FFFFFF);
        stream_raw16();
        tx.clock_start();
        for (unsigned i = 0; i < 20 && !(rd(FX1_CSI2_PHY_STATUS) & FX1_CSI2_PHY_STATUS_CL_RXCLKACTIVEHS_MASK); ++i)
            wait(100, SC_NS);
        wait(200, SC_NS);
    }
    void send(const Bytes& pkt, unsigned lanes) {
        const auto split = bfm::distribute_lanes(pkt, lanes);
        std::vector<bfm::LaneBurst> lb(lanes);
        for (unsigned i = 0; i < lanes; ++i) lb[i].bytes = split[i];
        tx.send_burst(lb);
        wait(300, SC_NS);
    }
    // RAW12 34x8 frame; `line_mod` may alter one line's packet (index, packet).
    void bfm_frame(uint16_t fnum, const std::function<void(unsigned, Bytes&)>& line_mod = {}) {
        const unsigned lanes = 2, w = 34, h = 8;
        send(bfm::short_packet(0, bfm::DT_FS, fnum), lanes);
        for (unsigned y = 0; y < h; ++y) {
            std::vector<uint16_t> px(w);
            for (unsigned x = 0; x < w; ++x) px[x] = sensor_pixel(x, y, 12);
            Bytes payload = bfm::pack_raw12(px);
            Bytes pkt = bfm::long_packet(0, bfm::DT_RAW12, payload);
            if (line_mod) line_mod(y, pkt);
            send(pkt, lanes);
        }
        send(bfm::short_packet(0, bfm::DT_FE, fnum), lanes);
        wait(2, SC_US);
    }

    // ------------------------------------------------------------ sessions
    void s_tpg(bool raw10) {
        tpg_frame(raw10);
        check_payload(raw10 ? "tpg_raw10_32x8" : "tpg_raw12_32x8");
    }

    void s_tpg_stall() {
        // Accepted-beat index -> cycles TREADY stays low before that beat is taken.
        // 0 = the SOF beat; 19 = line 2, beat 3 (mid-line); 39 = the EOL beat of line 4.
        const std::map<size_t, unsigned> stall = {{0, 3}, {19, 4}, {39, 5}};
        sink.ready_policy = [stall](const AxisBeat&, size_t k, unsigned waited) {
            const auto it = stall.find(k);
            if (it != stall.end()) return waited >= it->second;
            if (k >= 48 && k < 56) return waited >= 1;  // line 6: every beat refused once
            return true;
        };
        tpg_frame(false);
        check_payload("tpg_raw12_32x8");  // stalls change timing only, never the payload
        // The held beat stays unchanged while refused: check in the trace.
        unsigned refused = 0;
        std::istringstream t(sink.trace.str());
        std::string row, prev;
        std::getline(t, row);
        while (std::getline(t, row)) {
            // columns: cycle,time,tvalid,tready,tdata,tkeep,tlast,tuser,tid,accepted
            std::vector<std::string> c;
            std::stringstream ss(row);
            for (std::string f; std::getline(ss, f, ',');) c.push_back(f);
            if (!prev.empty()) {
                std::vector<std::string> p;
                std::stringstream ps(prev);
                for (std::string f; std::getline(ps, f, ',');) p.push_back(f);
                if (p[2] == "1" && p[3] == "0") {
                    ++refused;
                    if (c[2] != "1" || c[4] != p[4] || c[5] != p[5] || c[6] != p[6] || c[7] != p[7] || c[8] != p[8])
                        fail("a refused beat changed before it was accepted");
                }
            }
            prev = row;
        }
        if (refused != 3 + 4 + 5 + 8) fail("stall pattern not applied as specified");
    }

    void s_tpg_abort() {
        tpg_frame(false, [this] {
            // Mid-frame: after line 3, beat 4 has been accepted, disable the stream.
            wait_accepted(3 * 8 + 4, sc_time(100, SC_US));
            wr(FX1_CSI2_STREAM_CONFIG, 0);  // axis_enable 1 -> 0: forced abort of the line in flight
            wait(5, SC_US);
        });
        const auto exp = load_expected("tpg_raw12_32x8");
        const size_t n = sink.accepted.size();
        if (n < 29 || !is_null_abort(sink.accepted.back())) {
            fail("no null abort beat closing the frame");
            return;
        }
        for (size_t i = 0; i + 1 < n; ++i)
            if (!same(sink.accepted[i], exp[i])) fail("a pixel beat before the abort differs");
        if (n - 1 >= 32) fail("the abort came after the end of line 3");
        std::printf("tpg_abort: %zu pixel beats, then KEEP=0x00 LAST=1 TUSER=0b10\n", n - 1);
    }

    void s_bfm_tail() {
        bring_up(2);
        bfm_frame(1);
        check_payload("bfm_raw12_34x8");
    }

    void s_bfm_crc() {
        bring_up(2);
        bfm_frame(1, [](unsigned y, Bytes& pkt) {
            if (y == 3) pkt.back() ^= 0xFF;  // CRC MSB
        });
        const size_t first = sink.accepted.size();
        bfm_frame(2);
        const auto exp = load_expected("bfm_raw12_34x8");
        size_t aborts = 0;
        for (size_t i = 0; i < first; ++i) aborts += is_null_abort(sink.accepted[i]);
        if (aborts != 1) fail("frame 1: expected exactly one null abort beat");
        if (first < 1 || !is_null_abort(sink.accepted[first - 1])) fail("frame 1 must end with the abort beat");
        for (size_t i = 0; i + 1 < first && i < exp.size(); ++i)
            if (!same(sink.accepted[i], exp[i])) fail("frame 1: a beat before the abort differs");
        if (first - 1 > 4 * 9) fail("frame 1: beats after the corrupted line 3");
        check_payload("bfm_raw12_34x8", first);  // frame 2 whole
        std::printf("bfm_crc: frame 1 %zu pixel beats then the abort beat; frame 2 whole (%zu beats)\n", first - 1,
                    sink.accepted.size() - first);
    }

    // A CRC error on the last line is known only after the whole payload has
    // left: H complete lines, then the abort beat. Payload complete is not
    // a validated frame.
    void s_bfm_crc_last() {
        bring_up(2);
        bfm_frame(1, [](unsigned y, Bytes& pkt) {
            if (y == 7) pkt.back() ^= 0xFF;  // CRC MSB of the last line
        });
        const size_t first = sink.accepted.size();
        bfm_frame(2);
        const auto exp = load_expected("bfm_raw12_34x8");
        if (first != exp.size() + 1) fail("frame 1: expected the whole payload, then one abort beat");
        for (size_t i = 0; i < exp.size(); ++i)
            if (!same(sink.accepted[i], exp[i])) fail("frame 1: a payload beat differs");
        if (!is_null_abort(sink.accepted[first - 1])) fail("frame 1 must end with the abort beat");
        check_payload("bfm_raw12_34x8", first);  // frame 2 whole
        std::printf("bfm_crc_last: frame 1 %zu payload beats (complete), then the abort beat; frame 2 whole (%zu beats)\n",
                    first - 1, sink.accepted.size() - first);
    }

    void s_bfm_wc() {
        bring_up(2);
        bfm_frame(1, [](unsigned y, Bytes& pkt) {
            if (y != 3) return;
            // Rebuild line 3 with one payload byte less: WC 50 is not a whole RAW12 pair (3 bytes).
            std::vector<uint16_t> px(34);
            for (unsigned x = 0; x < 34; ++x) px[x] = sensor_pixel(x, 3, 12);
            Bytes payload = bfm::pack_raw12(px);
            payload.pop_back();
            pkt = bfm::long_packet(0, bfm::DT_RAW12, payload);
        });
        bool flagged = false;
        for (const auto& b : sink.accepted) flagged |= (b.tuser & 2u) && b.tkeep != 0 && b.tlast;
        if (!flagged) fail("no data beat with TUSER[1] closing the misaligned line");
        std::printf("bfm_wc: %zu beats; TUSER[1] on a line's last data beat: %s\n", sink.accepted.size(),
                    flagged ? "yes" : "no");
    }

    void write_outputs() {
        std::ofstream(out_dir + "/" + session + ".trace.csv") << sink.trace.str();
        std::ofstream a(out_dir + "/" + session + ".accepted.csv");
        a << "index,cycle,tdata,tkeep,tlast,tuser,tid\n";
        for (size_t i = 0; i < sink.accepted.size(); ++i) {
            const auto& b = sink.accepted[i];
            char row[96];
            std::snprintf(row, sizeof row, "%zu,%llu,%016llx,0x%02x,%d,%u,%u\n", i,
                          (unsigned long long)sink.accepted_cycle[i], (unsigned long long)b.tdata, b.tkeep, b.tlast,
                          unsigned(b.tuser), unsigned(b.tid));
            a << row;
        }
    }

    void run() {
        const std::map<std::string, std::function<void()>> sessions = {
            {"tpg_raw12", [this] { s_tpg(false); }}, {"tpg_raw10", [this] { s_tpg(true); }},
            {"tpg_stall", [this] { s_tpg_stall(); }}, {"tpg_abort", [this] { s_tpg_abort(); }},
            {"bfm_tail", [this] { s_bfm_tail(); }},   {"bfm_crc", [this] { s_bfm_crc(); }},
            {"bfm_crc_last", [this] { s_bfm_crc_last(); }}, {"bfm_wc", [this] { s_bfm_wc(); }},
        };
        const auto it = sessions.find(session);
        if (it == sessions.end()) {
            fail(("unknown session " + session).c_str());
        } else {
            it->second();
            write_outputs();
            completed = true;
        }
        sc_core::sc_stop();
    }
};

}  // namespace

int sc_main(int argc, char** argv) {
    sc_core::sc_set_time_resolution(1, sc_core::SC_FS);  // required by the detailed D-PHY model
    const std::string session = argc > 1 ? argv[1] : "tpg_raw12";
    const std::string out = argc > 2 ? argv[2] : ".";
    Demo demo("demo", session, out);
    sc_core::sc_start(sc_time(20, sc_core::SC_MS));
    if (!demo.completed) fail("session did not complete");
    std::printf("%s: %s (%zu beats accepted)\n", session.c_str(), g_fail ? "FAIL" : "PASS", demo.sink.accepted.size());
    return g_fail ? 1 : 0;
}
