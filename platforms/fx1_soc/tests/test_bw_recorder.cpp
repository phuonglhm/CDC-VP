// BwRecorder (fx1_soc --bw-report, plan CSI_ISP_BW1): AXI4-Stream and CSI
// payload totals per capture and per window, carry-in / carry-out across
// START / STOP, TKEEP tail and null-abort bytes, stall runs, the header and
// footer. Synthetic calls at known simulation times; expected values are
// counted by hand from the stimulus below, not from the unit under test.
//
//   test_fx1_bw_recorder <dir>      Exit 0 when every check holds.

#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <systemc>
#include <tlm_utils/simple_initiator_socket.h>

#include "../src/bw_recorder.h"
#include "fx1/fx1_memory_map.h"

using cdc::platforms::fx1_soc::BwRecorder;
using sc_core::sc_time;
using sc_core::SC_NS;

namespace {

int failures = 0;
#define CHECK(cond, what)                                                                  \
    do {                                                                                   \
        if (!(cond)) {                                                                     \
            std::cerr << "CHECK failed: " << (what) << " @ line " << __LINE__ << '\n';     \
            ++failures;                                                                    \
        }                                                                                  \
    } while (0)

struct Bench : sc_core::sc_module {
    BwRecorder& bw;
    bool done = false;
    SC_HAS_PROCESS(Bench);
    Bench(sc_core::sc_module_name n, BwRecorder& r) : sc_module(n), bw(r) { SC_THREAD(run); }
    void beat(std::uint8_t keep, bool last = false, std::uint8_t user = 0, bool ready = true)
    {
        bw.axis(true, keep, last, user, 1, ready);
        wait(sc_time(4, SC_NS));
    }
    void mark(fx1::SimControl::BwMarkKind k, std::uint32_t value, std::uint32_t window)
    {
        bw.mark({k, value, window, sc_core::sc_time_stamp()});
    }
    void run()
    {
        using K = fx1::SimControl::BwMarkKind;
        wait(sc_time(1000, SC_NS));
        bw.rx_frame_start(0, 1);
        bw.capture_start(1, 1, 1000, 1);  // links the open CSI frame to seq 1
        bw.rx_long_header(0, 0, 0x2C, 12, true);
        bw.rx_payload_bytes(4);  // before the window
        // Before the window: 3 full beats, 1 refused (stall run 1), 1 idle cycle (not counted).
        beat(0xFF, false, 1);
        beat(0xFF);
        beat(0xFF, false, 0, false);
        beat(0xFF);
        bw.axis(false, 0, false, 0, 1, true);
        mark(K::start, 0x11, 1);  // steady window 1
        bw.rx_payload_bytes(4);   // inside
        // Inside: two refused in a row (run 2), 3 full beats, a tail beat (TKEEP 0x0F: 4 bytes), the null abort beat.
        beat(0xFF, false, 0, false);
        beat(0xFF, false, 0, false);
        beat(0xFF);
        beat(0xFF);
        beat(0xFF);
        beat(0x0F, true);
        beat(0x00, true, 2);
        bw.rx_payload_bytes(4);
        bw.rx_packet_end(false, true);
        mark(K::stop, 0x2, 1);
        bw.rx_long_header(0, 0, 0x2C, 4, true);
        bw.rx_payload_bytes(4);  // after
        bw.rx_packet_end(false, false);  // CRC bad
        // After: 2 full beats.
        beat(0xFF);
        beat(0xFF, true);
        bw.rx_frame_end(0, 1);
        bw.capture_end(1, "ok");
        bw.capture_stream_end(1);
        bw.rx_long_header(0, 0, 0x2C, 4, false);  // no frame open: outside
        bw.rx_payload_bytes(3);
        bw.rx_packet_end(true, false);
        bw.rx_unclassified();
        mark(K::error, 0x7, 0);
        done = true;
    }
};

// Review BW1 F1: the marker through the real SimControl, with an incoming
// local delay on START and STOP (`early`), or written at its effective time
// with no delay (control). Both must give the window [10 ns, 20 ns): the AXIS
// beats and payload bytes at 12 and 18 ns, not those at 5 and 25 ns.
struct MarkBench : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<MarkBench> port{"port"};
    fx1::SimControl ctl{"ctl", false};
    BwRecorder& bw;
    bool early;
    bool done = false;
    SC_HAS_PROCESS(MarkBench);
    MarkBench(sc_core::sc_module_name n, BwRecorder& r, bool e) : sc_module(n), bw(r), early(e)
    {
        port.bind(ctl.socket);
        ctl.enable_bw_marker([this](const fx1::SimControl::BwMark& m) { bw.mark(m); });
        SC_THREAD(run);
    }
    void write_mark(std::uint32_t v, sc_time in)
    {
        unsigned char d[4];
        for (unsigned i = 0; i < 4; ++i) d[i] = static_cast<unsigned char>(v >> (8 * i));
        tlm::tlm_generic_payload tx;
        tx.set_command(tlm::TLM_WRITE_COMMAND);
        tx.set_address(FX1_SIM_CTRL_BW_MARK_OFF);
        tx.set_data_ptr(d);
        tx.set_data_length(4);
        tx.set_streaming_width(4);
        sc_time delay = in;
        port->b_transport(tx, delay);
        CHECK(tx.is_response_ok(), "marker write accepted");
    }
    void until(double ns) { wait(sc_time(ns, SC_NS) - sc_core::sc_time_stamp()); }
    void beat()
    {
        bw.axis(true, 0xFF, false, 0, 1, true);
        bw.rx_payload_bytes(4);
    }
    void run()
    {
        bw.rx_frame_start(0, 1);
        bw.rx_long_header(0, 0, 0x2C, 16, true);
        if (early) {
            until(2);
            write_mark(FX1_SIM_CTRL_BW_START | FX1_SIM_CTRL_BW_MODE_STEADY, sc_time(8, SC_NS));  // effective 10 ns
        }
        until(5);
        beat();
        if (!early) {
            until(10);
            write_mark(FX1_SIM_CTRL_BW_START | FX1_SIM_CTRL_BW_MODE_STEADY, sc_core::SC_ZERO_TIME);
        }
        until(12);
        beat();
        if (early) {
            until(14);
            write_mark(FX1_SIM_CTRL_BW_STOP, sc_time(6, SC_NS));  // effective 20 ns
        }
        until(18);
        beat();
        if (!early) {
            until(20);
            write_mark(FX1_SIM_CTRL_BW_STOP, sc_core::SC_ZERO_TIME);
        }
        until(25);
        beat();
        bw.rx_packet_end(false, true);
        bw.rx_frame_end(0, 1);
        // A second window, opened and closed later: windows are independent.
        until(40);
        write_mark(FX1_SIM_CTRL_BW_START | FX1_SIM_CTRL_BW_MODE_STEADY, sc_time(5, SC_NS));  // effective 45 ns
        until(50);
        bw.axis(true, 0x0F, true, 0, 2, true);
        write_mark(FX1_SIM_CTRL_BW_STOP, sc_time(5, SC_NS));  // effective 55 ns
        until(53);
        bw.axis(true, 0xFF, false, 0, 2, true);  // before the effective STOP: counted
        until(60);
        bw.axis(true, 0xFF, false, 0, 2, true);  // after it
        done = true;
    }
};

// Review BW1 R1: data at the very time of START / STOP, its callback before or
// after the marker's in the same simulation time. Window [8 ns, 20 ns) through
// the real SimControl (no delay); AXIS beats (8 B each) and 4 B of payload at
// 8, 12 and 20 ns. Every order must give 16 B AXIS / 8 B RX, first / last 8 / 12 ns.
//   order 0: marks first (control); 1: data first at 8 ns; 2: data first at 20 ns; 3: both
struct SameTickBench : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<SameTickBench> port{"port"};
    fx1::SimControl ctl{"ctl", false};
    BwRecorder& bw;
    int order;
    bool done = false;
    SC_HAS_PROCESS(SameTickBench);
    SameTickBench(sc_core::sc_module_name n, BwRecorder& r, int o) : sc_module(n), bw(r), order(o)
    {
        port.bind(ctl.socket);
        ctl.enable_bw_marker([this](const fx1::SimControl::BwMark& m) { bw.mark(m); });
        SC_THREAD(run);
    }
    void write_mark(std::uint32_t v)
    {
        unsigned char d[4];
        for (unsigned i = 0; i < 4; ++i) d[i] = static_cast<unsigned char>(v >> (8 * i));
        tlm::tlm_generic_payload tx;
        tx.set_command(tlm::TLM_WRITE_COMMAND);
        tx.set_address(FX1_SIM_CTRL_BW_MARK_OFF);
        tx.set_data_ptr(d);
        tx.set_data_length(4);
        tx.set_streaming_width(4);
        sc_time delay = sc_core::SC_ZERO_TIME;
        port->b_transport(tx, delay);
        CHECK(tx.is_response_ok(), "same-tick marker write accepted");
    }
    void data()
    {
        bw.axis(true, 0xFF, false, 0, 1, true);
        bw.rx_payload_bytes(4);
    }
    void until(double ns) { wait(sc_time(ns, SC_NS) - sc_core::sc_time_stamp()); }
    void run()
    {
        bw.rx_frame_start(0, 1);
        bw.rx_long_header(0, 0, 0x2C, 12, true);
        until(8);
        const bool data_first_start = order == 1 || order == 3, data_first_stop = order == 2 || order == 3;
        if (data_first_start) data();
        write_mark(FX1_SIM_CTRL_BW_START | FX1_SIM_CTRL_BW_MODE_STEADY);
        if (!data_first_start) data();
        until(12);
        data();
        until(20);
        if (data_first_stop) data();
        write_mark(FX1_SIM_CTRL_BW_STOP);
        if (!data_first_stop) data();
        until(30);
        bw.axis(false, 0, false, 0, 1, true);  // a later event: time moves on
        done = true;
    }
};

std::vector<std::string> lines_with(const std::vector<std::string>& all, const std::string& ev)
{
    std::vector<std::string> r;
    for (const auto& l : all)
        if (l.find("\"ev\":\"" + ev + "\"") != std::string::npos) r.push_back(l);
    return r;
}
bool has(const std::string& l, const std::string& kv) { return l.find(kv) != std::string::npos; }

}  // namespace

int sc_main(int argc, char** argv)
{
    sc_core::sc_set_time_resolution(1, sc_core::SC_PS);
    const std::string dir = argc > 1 ? argv[1] : ".";
    const std::string path = dir + "/bw_recorder.jsonl";
    std::vector<std::string> all;
    const std::string early_path = dir + "/bw_marker_early.jsonl", control_path = dir + "/bw_marker_control.jsonl";
    {
        // Every module is built before the simulation starts (SystemC elaboration).
        BwRecorder bw(path, "", {}), bw_early(early_path, "", {}), bw_control(control_path, "", {});
        bw.header({{"package", BwRecorder::q("test \"q\"")}});
        bw_early.header({});
        bw_control.header({});
        Bench b("bench", bw);
        MarkBench me("mark_early", bw_early, true), mc("mark_control", bw_control, false);
        std::vector<std::unique_ptr<BwRecorder>> tick_bw;
        std::vector<std::unique_ptr<SameTickBench>> tick_bench;
        for (int o = 0; o < 4; ++o) {
            tick_bw.push_back(std::make_unique<BwRecorder>(dir + "/bw_same_tick_" + std::to_string(o) + ".jsonl", "",
                                                           BwRecorder::ddr_reader{}));
            tick_bw.back()->header({});
            tick_bench.push_back(
                std::make_unique<SameTickBench>(("same_tick_" + std::to_string(o)).c_str(), *tick_bw.back(), o));
        }
        sc_core::sc_start(sc_time(1, sc_core::SC_US) * 2.0);
        CHECK(b.done && me.done && mc.done, "benches completed");
        for (auto& tb : tick_bench) CHECK(tb->done, "same-tick bench completed");
        for (auto& r : tick_bw) r->finish("pass");
        bw.finish("pass");
        bw_early.finish("pass");
        bw_control.finish("pass");
    }
    std::ifstream in(path);
    for (std::string l; std::getline(in, l);) all.push_back(l);
    CHECK(!all.empty() && has(all.front(), "\"ev\":\"header\"") && has(all.front(), "\"schema\":\"fx1-bw/1\""),
          "header first, schema");
    CHECK(!all.empty() && has(all.front(), "\"time_resolution_fs\":1000"), "time resolution from SystemC (1 ps)");
    CHECK(!all.empty() && has(all.front(), "\"package\":\"test \\\"q\\\"\""), "strings escaped");
    CHECK(!all.empty() && has(all.back(), "\"ev\":\"footer\"") && has(all.back(), "\"complete\":1") &&
              has(all.back(), "\"window_open\":0"),
          "footer last, complete, no window open");
    // Window 1: 2 stalls (run 2), 5 accepted (3 full + tail + abort), 8*3 + 4 = 28 bytes, 1 abort.
    const auto win = lines_with(all, "axis_window");
    CHECK(win.size() == 1, "one axis_window");
    if (win.size() == 1) {
        const auto& w = win[0];
        CHECK(has(w, "\"window\":1,") && has(w, "\"valid_cycles\":7,") && has(w, "\"accepted_beats\":5,") &&
                  has(w, "\"useful_bytes\":28,") && has(w, "\"abort_beats\":1,") && has(w, "\"stall_cycles\":2,") &&
                  has(w, "\"max_stall_run\":2,") && has(w, "\"tlast_beats\":2,") && has(w, "\"sof_beats\":0,"),
              "window totals: only beats inside [START, STOP)");
        // The window's first beat (refused) at START, its last accepted beat 6 cycles later.
        CHECK(has(w, "\"t_first\":1016000,") && has(w, "\"t_last\":1040000,"), "window first / last beat times");
    }
    // Capture 1: every beat, before, inside and after the window.
    const auto cap = lines_with(all, "axis_capture");
    CHECK(cap.size() == 1, "one axis_capture at the stream boundary");
    if (cap.size() == 1) {
        const auto& c = cap[0];
        CHECK(has(c, "\"seq\":1,") && has(c, "\"valid_cycles\":13,") && has(c, "\"accepted_beats\":10,") &&
                  has(c, "\"useful_bytes\":68,") && has(c, "\"stall_cycles\":3,") && has(c, "\"max_stall_run\":2,") &&
                  has(c, "\"sof_beats\":1,") && has(c, "\"tlast_beats\":3,"),
              "capture totals: carry-in and carry-out included");
    }
    const auto rxw = lines_with(all, "rx_window");
    // Inside [1016, 1044) ns: the 4 B at START (1016 ns). The 4 B at 1044 ns lie at
    // STOP's own time: out, although their callback came before the STOP mark
    // (review BW1 R1: membership by time, not by callback order).
    CHECK(rxw.size() == 1 && has(rxw[0], "\"image_bytes\":4,") && has(rxw[0], "\"packets\":0,"),
          "rx window: the 4 bytes at START in, those at STOP's time out, no header inside");
    const auto rx = lines_with(all, "rx_frame");
    CHECK(rx.size() == 1, "one CSI frame");
    if (rx.size() == 1) {
        CHECK(has(rx[0], "\"seq\":1,") && has(rx[0], "\"image_bytes\":16,") && has(rx[0], "\"image_packets\":2,") &&
                  has(rx[0], "\"crc_bad\":1,") && has(rx[0], "\"not_forwarded_bytes\":0,") && has(rx[0], "\"t_fe\":"),
              "CSI frame: linked to seq 1, 16 bytes received, one bad CRC");
    }
    CHECK(has(all.back(), "\"rx_outside_bytes\":3,") && has(all.back(), "\"rx_outside_unclassified\":1}"),
          "payload outside any frame counted in the footer");
    // F1: marker through SimControl with incoming delays (early) and without (control).
    for (const bool early : {true, false}) {
        const std::string mpath = early ? early_path : control_path;
        std::vector<std::string> ml;
        std::ifstream mi(mpath);
        for (std::string l; std::getline(mi, l);) ml.push_back(l);
        const std::string tag = early ? "early marker: " : "control: ";
        const auto aw = lines_with(ml, "axis_window");
        const auto rw = lines_with(ml, "rx_window");
        CHECK(aw.size() == 2 && rw.size() == 2, tag + "two windows written");
        if (aw.size() == 2 && rw.size() == 2) {
            CHECK(has(aw[0], "\"window\":1,") && has(aw[0], "\"useful_bytes\":16,") &&
                      has(aw[0], "\"accepted_beats\":2,") && has(aw[0], "\"t_first\":12000,") &&
                      has(aw[0], "\"t_last\":18000,"),
                  tag + "window 1 AXIS: the beats at 12 and 18 ns only");
            CHECK(has(rw[0], "\"image_bytes\":8,") && has(rw[0], "\"t_start\":10000,") &&
                      has(rw[0], "\"t_stop\":20000,") && has(rw[0], "\"drained\":1"),
                  tag + "window 1 RX: 8 bytes in [10, 20) ns, effective times");
            CHECK(has(aw[1], "\"window\":2,") && has(aw[1], "\"useful_bytes\":12,") &&
                      has(aw[1], "\"accepted_beats\":2,"),
                  tag + "window 2: the beats at 50 and 53 ns (before its effective STOP), not 60 ns");
        }
        // The window line is written only after its effective STOP time has passed.
        std::size_t stop_line = 0, win_line = 0;
        for (std::size_t i = 0; i < ml.size(); ++i) {
            if (has(ml[i], "\"kind\":\"stop\"") && !stop_line) stop_line = i;
            if (has(ml[i], "\"ev\":\"axis_window\"") && !win_line) win_line = i;
        }
        CHECK(stop_line && win_line > stop_line, tag + "window 1 written after its STOP mark");
    }
    // R1: same-tick order of data and marks.
    for (int o = 0; o < 4; ++o) {
        std::vector<std::string> tl;
        std::ifstream ti(dir + "/bw_same_tick_" + std::to_string(o) + ".jsonl");
        for (std::string l; std::getline(ti, l);) tl.push_back(l);
        const auto aw = lines_with(tl, "axis_window");
        const auto rw = lines_with(tl, "rx_window");
        const std::string tag = "same tick, order " + std::to_string(o) + ": ";
        CHECK(aw.size() == 1 && rw.size() == 1, tag + "one window");
        if (aw.size() == 1 && rw.size() == 1) {
            CHECK(has(aw[0], "\"useful_bytes\":16,") && has(aw[0], "\"accepted_beats\":2,") &&
                      has(aw[0], "\"t_first\":8000,") && has(aw[0], "\"t_last\":12000,"),
                  tag + "AXIS 16 B, the beats at 8 and 12 ns (data at START in, at STOP out)");
            CHECK(has(rw[0], "\"image_bytes\":8,") && has(rw[0], "\"t_start\":8000,") && has(rw[0], "\"t_stop\":20000,"),
                  tag + "RX 8 B in [8, 20) ns");
        }
    }
    const auto marks = lines_with(all, "mark");
    CHECK(marks.size() == 3 && has(marks[0], "\"mode\":\"steady\"") && has(marks[2], "\"kind\":\"error\""),
          "marks: start (steady), stop, error");
    std::cout << "test_fx1_bw_recorder: " << (failures ? "FAIL" : "PASS") << " (" << failures << " failures)\n";
    return failures ? 1 : 0;
}
