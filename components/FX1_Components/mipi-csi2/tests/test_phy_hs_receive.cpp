// HS receive through the detailed D-PHY model: pad-level BFM -> mipirxphy ->
// PPI, checked byte for byte at the PPI.
//
//   test_fx1_dphy_hs <case>
//     1lane | 2lane | 4lane   CSI-2 long packet distributed over N lanes
//     skew                    4 lanes, lane 1..3 delayed by 3/8/5 UI
//     sot_err1                1-bit error in the sync byte  -> errsoths, data still aligned
//     sot_err2                >=2-bit error in the sync byte of every lane, zero
//                             payload, no bit offset within 1 bit of 0xB8 -> errsotsynchs
//     corrupt                 1 payload bit inverted on the wire -> exactly that byte differs
//     multi                   two bursts on 2 lanes, clock left running between them
//     phase                   4 lanes, clock launch phase 0..7/8 UI, clock restarted per burst
//     lead4                   lanes 1..3 lead lane 0 by 4 UI (noalign_error=1, default)
//     lead4_link              same with noalign_error=0: the whole link reports errsotsynchs
//
// Oracle: the byte vector the BFM was asked to transmit on each lane (built by
// the packet builder, never derived from PHY output). The PHY may append trail
// bytes after the payload with rxvalidhs=1 until it detects LP-11 (contract
// §4); at most ceil(trail_ui/8)+8 extra bytes are accepted and reported.
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <regex>
#include <string>
#include <vector>

#include <systemc>

#include "fx1_csi2_bfm/csi2_packet.h"
#include "fx1_csi2_bfm/dphy_tx_bfm.h"
#include "fx1_csi2_phy/dphy_rx_wrapper.h"
#include "fx1_csi2_phy/dphy_startup.h"

using namespace fx1::csi2;

namespace {
int g_fail = 0;
#define CHECK(cond)                                                                         \
    do {                                                                                    \
        if (!(cond)) {                                                                      \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond);    \
            ++g_fail;                                                                       \
        }                                                                                   \
    } while (0)

bool is1(const sc_dt::sc_logic& v) { return v == sc_dt::SC_LOGIC_1; }
bool is01(const sc_dt::sc_logic& v) { return v == sc_dt::SC_LOGIC_0 || v == sc_dt::SC_LOGIC_1; }

// Samples the PPI of every lane on the rising edge of rxbyteclkhs.
struct PpiMonitor : sc_core::sc_module {
    struct Lane {
        std::vector<uint8_t> bytes;
        unsigned syncs = 0, soterr = 0, sotsyncerr = 0, xvalid = 0, xdata = 0, active_cycles = 0;
        bool active = false;
    };
    SC_HAS_PROCESS(PpiMonitor);
    PpiMonitor(sc_core::sc_module_name nm, DphyRxWrapper& w) : sc_core::sc_module(nm), w_(w), lanes(4) {
        SC_METHOD(sample);
        sensitive << w_.rxbyteclkhs.posedge_event();
        dont_initialize();
    }
    void sample() {
        ++edges;
        for (int i = 0; i < 4; ++i) {
            Lane& l = lanes[i];
            const auto v = w_.rxvalidhs[i].read();
            if (!is01(v)) { ++l.xvalid; continue; }
            l.active = is1(w_.rxactivehs[i].read());
            l.active_cycles += l.active;
            l.syncs += is1(w_.rxsynchs[i].read());
            l.soterr += is1(w_.errsoths[i].read());
            l.sotsyncerr += is1(w_.errsotsynchs[i].read());
            if (is1(v)) {
                const auto d = w_.rxdatahs[i].read();
                if (!d.is_01()) { ++l.xdata; continue; }
                l.bytes.push_back(static_cast<uint8_t>(d.to_uint()));
            }
        }
    }
    void clear() { for (auto& l : lanes) l = Lane{}; }
    DphyRxWrapper& w_;
    std::vector<Lane> lanes;
    uint64_t edges = 0;
};

constexpr size_t kEotLatencyBytes = 8;

// Minimum Hamming distance between the 0xB8 leader (LSB first) and any 8-bit
// window of: HS-0 zeros, `sync`, then the lane's first payload bytes. A
// receiver tolerating one bit error in the leader can only lock if this is <= 1.
int leader_distance(uint8_t sync, const std::vector<uint8_t>& payload) {
    std::vector<int> s(16, 0);
    auto push = [&s](uint8_t b) { for (int k = 0; k < 8; ++k) s.push_back(b >> k & 1); };
    push(sync);
    for (size_t i = 0; i < payload.size() && i < 3; ++i) push(payload[i]);
    int best = 8;
    for (size_t i = 0; i + 8 <= s.size(); ++i) {
        int d = 0;
        for (int k = 0; k < 8; ++k) d += s[i + k] != (0xB8 >> k & 1);
        best = d < best ? d : best;
    }
    return best;
}

// A corrupted leader with the fewest flipped bits (at least two) whose
// distance to the leader is >= 2 at every bit offset, given the lane's
// following bytes.
uint8_t multi_bit_corrupt_sync(const std::vector<uint8_t>& payload) {
    for (int flips = 2; flips <= 8; ++flips)
        for (unsigned m = 0; m < 256; ++m)
            if (__builtin_popcount(m) == flips && leader_distance(static_cast<uint8_t>(0xB8 ^ m), payload) >= 2)
                return static_cast<uint8_t>(0xB8 ^ m);
    return 0xB8;
}

std::string hex(const std::vector<uint8_t>& v, size_t max = 24) {
    std::string s;
    char b[4];
    for (size_t i = 0; i < v.size() && i < max; ++i) { std::snprintf(b, sizeof b, "%02X ", v[i]); s += b; }
    if (v.size() > max) s += "...";
    return s;
}

// Compares received bytes with the expected payload; returns index of first
// mismatch or -1. Extra trailing bytes up to max_extra are tolerated.
long compare(const std::vector<uint8_t>& got, const std::vector<uint8_t>& exp, size_t max_extra) {
    for (size_t i = 0; i < exp.size(); ++i)
        if (i >= got.size() || got[i] != exp[i]) return static_cast<long>(i);
    if (got.size() > exp.size() + max_extra) return static_cast<long>(exp.size());
    return -1;
}

std::vector<uint8_t> test_packet(size_t width_px) {
    std::vector<uint16_t> px(width_px);
    for (size_t i = 0; i < px.size(); ++i) px[i] = static_cast<uint16_t>((i * 37 + 5) & 0x3FF);
    px[0] = 0x3FF; px[1] = 0x000; px[2] = 0x155; px[3] = 0x2AA;
    return bfm::long_packet(0, bfm::DT_RAW10, bfm::pack_raw10(px));
}

// Debug aid (not used by ctest): FX1_DPHY_LIST=<regex> lists matching channel
// names; FX1_DPHY_TRACE=<regex> prints every value change of matching 1-bit
// channels while `window` is set.
bool g_trace_window = false;

void walk(sc_core::sc_object* o, const std::regex& re, std::vector<sc_core::sc_object*>& out) {
    if (std::regex_search(o->name(), re)) out.push_back(o);
    for (auto* c : o->get_child_objects()) walk(c, re, out);
}

std::vector<sc_core::sc_object*> find_objects(const char* pattern) {
    std::vector<sc_core::sc_object*> out;
    const std::regex re(pattern);
    for (auto* o : sc_core::sc_get_top_level_objects()) walk(o, re, out);
    return out;
}

void install_tracers(const char* pattern) {
    for (auto* o : find_objects(pattern)) {
        auto* sig = dynamic_cast<sc_core::sc_signal_in_if<sc_dt::sc_logic>*>(o);
        if (!sig) continue;
        sc_core::sc_spawn_options opt;
        opt.spawn_method();
        opt.set_sensitivity(&sig->value_changed_event());
        opt.dont_initialize();
        const std::string name = o->name();
        sc_core::sc_spawn([sig, name] {
            if (g_trace_window)
                std::printf("TRACE %12.4f ns %s = %c\n", sc_core::sc_time_stamp().to_seconds() * 1e9, name.c_str(),
                            sig->read().to_char());
        }, nullptr, &opt);
    }
}

struct Tb : sc_core::sc_module {
    SC_HAS_PROCESS(Tb);
    Tb(sc_core::sc_module_name nm, std::string which, bfm::DphyTxTiming timing)
        : sc_core::sc_module(nm), which_(std::move(which)), phy("dut"),
          st("startup", phy, cfg_for(which_)), tx("tx", phy.clkp, phy.clkn, phy.dp, phy.dn, timing), mon("mon", phy) {
        SC_THREAD(run);
    }
    static DphyStartupConfig cfg_for(const std::string& w) {
        DphyStartupConfig c;
        // Debug only: FX1_DPHY_TESTREGS="09=20,05=40" (hex address=value).
        if (w == "lead4_link") c.test_regs.clear();  // reset value: noalign_error=0
        if (const char* p = std::getenv("FX1_DPHY_TESTREGS")) {
            c.test_regs.clear();
            std::string spec(p);
            size_t pos = 0;
            while (pos < spec.size()) {
                const size_t end = spec.find(',', pos);
                const std::string item = spec.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
                const size_t eq = item.find('=');
                c.test_regs.emplace_back(static_cast<uint8_t>(std::stoul(item.substr(0, eq), nullptr, 16)),
                                         static_cast<uint8_t>(std::stoul(item.substr(eq + 1), nullptr, 16)));
                if (end == std::string::npos) break;
                pos = end + 1;
            }
        }
        c.lane_enable = w == "1lane" ? 0x1 : (w == "2lane" || w == "multi") ? 0x3 : 0xF;
        return c;
    }
    unsigned lanes_for() const { return cfg_for(which_).lane_enable == 0x1 ? 1 : cfg_for(which_).lane_enable == 0x3 ? 2 : 4; }

    void check_burst(const std::vector<bfm::LaneBurst>& lb, const char* tag, int expect_sot, long expect_mismatch_at = -1,
                     unsigned mismatch_lane = 0) {
        // Trail bytes plus the bytes delivered while the PHY detects LP-11
        // (26 ns LP filter + synchronisers): measured 7 at 800 Mb/s, 16 UI trail.
        const size_t max_extra = (tx.timing().trail_ui + 7) / 8 + kEotLatencyBytes;
        for (size_t i = 0; i < lb.size(); ++i) {
            const auto& l = mon.lanes[i];
            const long mm = compare(l.bytes, lb[i].bytes, max_extra);
            std::printf("  [%s] lane%zu: sent %zu got %zu (extra %ld) syncs=%u errsoths=%u errsotsynchs=%u xvalid=%u xdata=%u first_mismatch=%ld\n",
                        tag, i, lb[i].bytes.size(), l.bytes.size(), static_cast<long>(l.bytes.size()) - static_cast<long>(lb[i].bytes.size()),
                        l.syncs, l.soterr, l.sotsyncerr, l.xvalid, l.xdata, mm);
            if (l.bytes.size() > lb[i].bytes.size())
                std::printf("           trailing bytes: %s\n",
                            hex(std::vector<uint8_t>(l.bytes.begin() + lb[i].bytes.size(), l.bytes.end())).c_str());
            CHECK(l.xvalid == 0);
            CHECK(l.xdata == 0);
            if (expect_sot == 2) {
                CHECK(l.sotsyncerr >= 1);
                CHECK(l.syncs == 0);
                continue;
            }
            CHECK(l.syncs == 1);
            CHECK(l.sotsyncerr == 0);
            CHECK(expect_sot == 1 ? l.soterr == 1 : l.soterr == 0);
            if (expect_mismatch_at >= 0 && i == mismatch_lane) {
                CHECK(mm == expect_mismatch_at);
                // Only the flipped byte differs.
                for (size_t k = 0; k < lb[i].bytes.size() && k < l.bytes.size(); ++k)
                    if (static_cast<long>(k) != expect_mismatch_at) CHECK(l.bytes[k] == lb[i].bytes[k]);
            } else {
                CHECK(mm == -1);
            }
            CHECK(!l.active);  // burst closed
        }
    }

    void run() {
        wait(st.done_event());
        if (!st.done()) { std::printf("startup timed out\n"); ++g_fail; sc_core::sc_stop(); return; }
        std::printf("startup done at %.3f us\n", sc_core::sc_time_stamp().to_seconds() * 1e6);
        CHECK(st.config_ok());
        wait(2, sc_core::SC_US);  // LP receivers settle into stop state after enable_lanes
        CHECK(is1(phy.stopstateclk.read()));
        tx.clock_start();
        wait(1, sc_core::SC_US);
        std::printf("clock HS: rxclkactivehs=%s stopstateclk=%s byteclk edges in 1us=%llu\n",
                    is1(phy.rxclkactivehs.read()) ? "1" : "0", is1(phy.stopstateclk.read()) ? "1" : "0",
                    static_cast<unsigned long long>(mon.edges));
        CHECK(is1(phy.rxclkactivehs.read()));
        CHECK(mon.edges > 0);

        const unsigned n = lanes_for();
        if (which_ == "phase") {
            run_phase_sweep(n);
            return;
        }
        const auto pkt = test_packet(which_ == "multi" ? 32 : 64);
        auto lanes = bfm::distribute_lanes(pkt, n);
        std::vector<bfm::LaneBurst> lb(n);
        for (unsigned i = 0; i < n; ++i) lb[i].bytes = lanes[i];
        int expect_sot = 0;
        long mismatch_at = -1;
        if (which_ == "skew") { lb[1].skew_ui = 3; lb[2].skew_ui = 8; lb[3].skew_ui = 5; }
        if (which_.rfind("lead", 0) == 0) {  // lead<k>[_link]: lanes 1..3 lead lane 0 by k UI
            lb[0].skew_ui = static_cast<unsigned>(std::stoul(which_.substr(4)));
            if (which_.find("_link") != std::string::npos) expect_sot = 2;  // whole link fails
        }
        if (which_ == "sot_err1") { for (auto& b : lb) b.sync = 0xB8 ^ 0x04; expect_sot = 1; }
        if (which_ == "sot_err2") {
            // Zero payload: every window over it is 4 bits from 0xB8, so the
            // receiver cannot lock on payload content instead of the leader.
            for (auto& b : lb) b.bytes.assign(24, 0x00);
            for (auto& b : lb) {
                b.sync = multi_bit_corrupt_sync(b.bytes);
                CHECK(b.sync != 0xB8);
                std::printf("  corrupted leader 0x%02X (distance %d)\n", b.sync, leader_distance(b.sync, b.bytes));
            }
            expect_sot = 2;
        }
        if (which_ == "corrupt") { lb[0].flip_bits = {10 * 8 + 3}; mismatch_at = 10; }

        mon.clear();
        const sc_core::sc_time t0 = sc_core::sc_time_stamp();
        g_trace_window = true;
        std::printf("burst start %.4f ns, sync starts at grid+%u UI\n", t0.to_seconds() * 1e9,
                    static_cast<unsigned>(std::ceil(tx.timing().t_lpx / tx.timing().ui)) + tx.timing().hs_zero_ui);
        tx.send_burst(lb);
        wait(200, sc_core::SC_NS);  // clock keeps running so the lanes can close the burst
        g_trace_window = false;
        std::printf("burst of %zu bytes on %u lane(s) took %.3f us sim\n", pkt.size(), n,
                    (sc_core::sc_time_stamp() - t0).to_seconds() * 1e6);
        check_burst(lb, "burst1", expect_sot, mismatch_at, 0);
        for (unsigned i = 0; i < n; ++i) CHECK(is1(phy.stopstatedata[i].read()));

        if (which_ == "multi") {
            const auto pkt2 = bfm::short_packet(0, bfm::DT_FE, 0x1234);
            auto l2 = bfm::distribute_lanes(pkt2, n);
            for (unsigned i = 0; i < n; ++i) lb[i].bytes = l2[i];
            mon.clear();
            tx.send_burst(lb);
            wait(200, sc_core::SC_NS);
            check_burst(lb, "burst2", 0);
        }

        tx.clock_stop();
        wait(1, sc_core::SC_US);
        std::printf("clock stopped: rxclkactivehs=%s stopstateclk=%s\n", is1(phy.rxclkactivehs.read()) ? "1" : "0",
                    is1(phy.stopstateclk.read()) ? "1" : "0");
        CHECK(is1(phy.stopstateclk.read()));
        sc_core::sc_stop();
    }

    // Clock-to-data launch phase sweep: one burst per phase, clock lane
    // restarted (LP-11 -> HS) before each burst. Every phase must pass.
    void run_phase_sweep(unsigned n) {
        const auto pkt = test_packet(64);
        auto lanes = bfm::distribute_lanes(pkt, n);
        std::vector<bfm::LaneBurst> lb(n);
        for (unsigned i = 0; i < n; ++i) lb[i].bytes = lanes[i];
        const size_t max_extra = (tx.timing().trail_ui + 7) / 8 + kEotLatencyBytes;
        for (int k = 0; k < 8; ++k) {
            const double phase = k / 8.0;
            tx.clock_stop();
            wait(200, sc_core::SC_NS);
            tx.set_clock_phase_ui(phase);
            g_trace_window = k == 0;
            tx.clock_start();
            wait(1, sc_core::SC_US);
            mon.clear();
            g_trace_window = k == 0;
            tx.send_burst(lb);
            wait(200, sc_core::SC_NS);
            g_trace_window = false;
            bool ok = true;
            for (unsigned i = 0; i < n; ++i) {
                const auto& l = mon.lanes[i];
                ok &= l.syncs == 1 && l.soterr == 0 && l.sotsyncerr == 0 && l.xdata == 0 && l.xvalid == 0 &&
                      compare(l.bytes, lb[i].bytes, max_extra) == -1;
            }
            std::printf("  phase %.3f UI: %s\n", phase, ok ? "ok" : "FAIL");
            CHECK(ok);
            if (!ok)
                for (unsigned i = 0; i < n; ++i) {
                    const auto& l = mon.lanes[i];
                    std::printf("    lane%u: got %zu syncs=%u errsoths=%u errsotsynchs=%u xvalid=%u xdata=%u mismatch=%ld head=%s\n", i,
                                l.bytes.size(), l.syncs, l.soterr, l.sotsyncerr, l.xvalid, l.xdata,
                                compare(l.bytes, lb[i].bytes, max_extra), hex(l.bytes, 8).c_str());
                }
        }
        tx.clock_stop();
        sc_core::sc_stop();
    }

    std::string which_;
    DphyRxWrapper phy;
    DphyStartup st;
    bfm::DphyTxBfm tx;
    PpiMonitor mon;
};
}  // namespace

int sc_main(int argc, char** argv) {
    sc_core::sc_set_time_resolution(1, sc_core::SC_FS);
    const std::string which = argc > 1 ? argv[1] : "1lane";
    const auto t0 = std::chrono::steady_clock::now();
    bfm::DphyTxTiming timing;
    if (argc > 2) timing.hs_zero_ui = static_cast<unsigned>(std::stoul(argv[2]));  // characterisation only
    std::printf("hs_zero_ui=%u\n", timing.hs_zero_ui);
    if (argc > 3) timing.clk_phase_ui = std::stod(argv[3]);  // characterisation only
    Tb tb("tb", which, timing);
    if (const char* p = std::getenv("FX1_DPHY_LIST")) {
        for (auto* o : find_objects(p)) std::printf("%s [%s]\n", o->name(), o->kind());
        return 0;
    }
    if (const char* p = std::getenv("FX1_DPHY_TRACE")) install_tracers(p);
    sc_core::sc_start(5, sc_core::SC_MS);
    if (sc_core::sc_get_status() != sc_core::SC_STOPPED) { std::printf("simulation did not finish\n"); ++g_fail; }
    const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::printf("sim_time=%.3f us wall=%.2f s\n", sc_core::sc_time_stamp().to_seconds() * 1e6, wall);
    std::printf("%s: %s\n", which.c_str(), g_fail ? "FAIL" : "PASS");
    return g_fail ? 1 : 0;
}
