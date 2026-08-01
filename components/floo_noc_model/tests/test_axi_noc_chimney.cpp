// SPDX-License-Identifier: Apache-2.0
//
// A-2 composition test: AXI manager signals -> complete source chimney ->
// request mesh -> complete target chimney -> subordinate AXI signals, and the
// B/R response path back again.
//
// The three chimney blocks and the two meshes are individually RTL-signed.
// This test checks the wiring between them; it is not a new RTL cross-check.

#include "floo_noc_model/axi_noc.hpp"

#include <systemc>

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace {

using namespace floo::model;

constexpr unsigned width = 2;
constexpr unsigned height = 2;
using noc_t = axi_noc<width, height>;

int failures = 0;

void check(bool condition, const std::string& what)
{
    if (!condition) {
        std::cerr << "FAIL: " << what << '\n';
        ++failures;
    }
}

struct cycle_observation {
    bool manager_aw{false};
    bool manager_w{false};
    bool manager_ar{false};
    bool target_aw{false};
    bool target_w{false};
    bool target_ar{false};
    bool target_b{false};
    bool target_r{false};
    bool manager_b{false};
    bool manager_r{false};
    axi_aw_chan target_aw_payload{};
    axi_w_chan target_w_payload{};
    axi_ar_chan target_ar_payload{};
    axi_b_chan manager_b_payload{};
    axi_r_chan manager_r_payload{};
};

} // namespace

int sc_main(int, char**)
{
    const coordinate manager_id{0, 0};
    const coordinate target_id{1, 1};
    const unsigned manager_node = noc_t::node_index(manager_id);
    const unsigned target_node = noc_t::node_index(target_id);

    sc_core::sc_signal<bool> clk{"clk"};
    sc_core::sc_signal<bool> rst_n{"rst_n"};
    noc_t noc{"noc"};
    noc.i_clk(clk);
    noc.i_rst_n(rst_n);

    auto& manager = noc.manager(manager_node);
    auto& target = noc.subordinate(target_node);

    // Every node has both AXI sides. Tie off sides that this test does not use;
    // this is also the contract A-3's platform wrapper now satisfies.
    for (unsigned node = 0; node < noc_t::num_nodes; ++node) {
        auto& m = noc.manager(node);
        m.aw_valid.write(false);
        m.w_valid.write(false);
        m.ar_valid.write(false);
        m.aw_dest.write(coordinate{});
        m.ar_dest.write(coordinate{});
        m.b_ready.write(false);
        m.r_ready.write(false);

        auto& s = noc.subordinate(node);
        s.aw_ready.write(false);
        s.w_ready.write(false);
        s.ar_ready.write(false);
        s.b_valid.write(false);
        s.r_valid.write(false);
    }

    manager.aw_dest.write(target_id);
    manager.ar_dest.write(target_id);
    target.aw_ready.write(true);
    target.w_ready.write(true);
    target.ar_ready.write(true);

    unsigned cycle = 0;
    std::vector<axi_aw_chan> target_aw_seen;
    std::vector<axi_w_chan> target_w_seen;
    std::vector<axi_ar_chan> target_ar_seen;
    std::vector<axi_b_chan> manager_b_seen;
    std::vector<axi_r_chan> manager_r_seen;

    const auto tick = [&]() {
        clk.write(false);
        sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));

        cycle_observation seen{};
        seen.manager_aw = manager.aw_valid.read() && manager.aw_ready.read();
        seen.manager_w = manager.w_valid.read() && manager.w_ready.read();
        seen.manager_ar = manager.ar_valid.read() && manager.ar_ready.read();
        seen.target_aw = target.aw_valid.read() && target.aw_ready.read();
        seen.target_w = target.w_valid.read() && target.w_ready.read();
        seen.target_ar = target.ar_valid.read() && target.ar_ready.read();
        seen.target_b = target.b_valid.read() && target.b_ready.read();
        seen.target_r = target.r_valid.read() && target.r_ready.read();
        seen.manager_b = manager.b_valid.read() && manager.b_ready.read();
        seen.manager_r = manager.r_valid.read() && manager.r_ready.read();
        seen.target_aw_payload = target.aw.read();
        seen.target_w_payload = target.w.read();
        seen.target_ar_payload = target.ar.read();
        seen.manager_b_payload = manager.b.read();
        seen.manager_r_payload = manager.r.read();

        if (seen.target_aw) target_aw_seen.push_back(seen.target_aw_payload);
        if (seen.target_w) target_w_seen.push_back(seen.target_w_payload);
        if (seen.target_ar) target_ar_seen.push_back(seen.target_ar_payload);
        if (seen.manager_b) manager_b_seen.push_back(seen.manager_b_payload);
        if (seen.manager_r) manager_r_seen.push_back(seen.manager_r_payload);

        // Requests and responses must not leak into either unused coordinate.
        for (unsigned node = 0; node < noc_t::num_nodes; ++node) {
            if (node == manager_node || node == target_node) continue;
            const auto& unused_s = noc.subordinate(node);
            const auto& unused_m = noc.manager(node);
            check(!unused_s.aw_valid.read() && !unused_s.w_valid.read()
                      && !unused_s.ar_valid.read(),
                  "a request leaked into unused node " + std::to_string(node));
            check(!unused_m.b_valid.read() && !unused_m.r_valid.read(),
                  "a response leaked into unused node " + std::to_string(node));
        }

        clk.write(true);
        sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));
        clk.write(false);
        sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));
        ++cycle;
        return seen;
    };

    clk.write(false);
    rst_n.write(false);
    sc_core::sc_start(sc_core::SC_ZERO_TIME);
    for (unsigned count = 0; count < 4; ++count) tick();
    rst_n.write(true);
    for (unsigned count = 0; count < 2; ++count) tick();

    // ---- two-beat write -------------------------------------------------
    //
    // The manager offers AW and W together. The request chimney must serialize
    // AW, W0, W1 on one wormhole-routed packet.
    axi_aw_chan aw{};
    aw.id = 5;
    aw.addr = 0x8000'1040;
    aw.len = 1;
    aw.size = 3;
    aw.burst = 1;
    aw.cache = 0xA;
    aw.prot = 0x3;
    aw.qos = 0x7;
    aw.user = 0x55;
    manager.aw.write(aw);
    manager.aw_valid.write(true);

    const std::vector<axi_w_chan> write_beats{
        axi_w_chan{0x0123'4567'89AB'CDEF, 0xFF, false, 0x11},
        axi_w_chan{0xFEDC'BA98'7654'3210, 0xF0, true, 0x22},
    };
    unsigned offered_w = 0;
    manager.w.write(write_beats[offered_w]);
    manager.w_valid.write(true);

    bool aw_accepted = false;
    for (unsigned watchdog = 0; watchdog < 128
         && (!aw_accepted || offered_w < write_beats.size()); ++watchdog) {
        const auto seen = tick();
        if (seen.manager_aw) {
            aw_accepted = true;
            manager.aw_valid.write(false);
        }
        if (seen.manager_w) {
            ++offered_w;
            if (offered_w < write_beats.size()) {
                manager.w.write(write_beats[offered_w]);
            } else {
                manager.w_valid.write(false);
            }
        }
    }
    check(aw_accepted, "write AW did not enter the source chimney");
    check(offered_w == write_beats.size(),
          "both write beats did not enter the source chimney");

    for (unsigned watchdog = 0; watchdog < 128
         && (target_aw_seen.size() < 1 || target_w_seen.size() < 2);
         ++watchdog) {
        tick();
    }
    check(target_aw_seen.size() == 1,
          "target subordinate must accept exactly one AW");
    check(target_w_seen.size() == 2,
          "target subordinate must accept exactly two W beats");
    if (target_aw_seen.size() == 1) {
        axi_aw_chan expected = aw;
        expected.id = noc_t::chimney_type::downstream_id;
        check(target_aw_seen.front() == expected,
              "AW fields must survive except for the downstream ID rewrite");
    }
    if (target_w_seen.size() == 2) {
        check(target_w_seen[0] == write_beats[0],
              "first W beat must survive the composed request path");
        check(target_w_seen[1] == write_beats[1],
              "second W beat and WLAST must survive the composed request path");
    }
    check(noc.chimney(manager_node).b_outstanding(5) == 1,
          "accepted AW must hold one B ordering counter");
    check(noc.chimney(target_node).aw_meta_occupancy() == 1,
          "target chimney must retain one AW metadata entry");

    // Return B while the manager is stalled. The target may inject it, but the
    // final response and its ordering counter must stay stable until BREADY.
    axi_b_chan b{};
    b.id = 7; // the frozen subordinate-side ID, all ones
    b.resp = 2;
    b.user = 0xB5;
    target.b.write(b);
    target.b_valid.write(true);
    bool target_b_accepted = false;
    for (unsigned watchdog = 0; watchdog < 128 && !target_b_accepted;
         ++watchdog) {
        target_b_accepted = tick().target_b;
    }
    target.b_valid.write(false);
    check(target_b_accepted, "target chimney did not accept B");

    bool stalled_b_visible = false;
    axi_b_chan stalled_b{};
    for (unsigned watchdog = 0; watchdog < 128 && !stalled_b_visible;
         ++watchdog) {
        tick();
        stalled_b_visible = manager.b_valid.read();
        stalled_b = manager.b.read();
    }
    check(stalled_b_visible, "B did not reach the manager-side chimney");
    check(stalled_b.id == aw.id && stalled_b.resp == b.resp
              && stalled_b.user == b.user,
          "B must restore the manager ID and preserve response fields");
    for (unsigned count = 0; count < 3; ++count) {
        tick();
        check(manager.b_valid.read() && manager.b.read() == stalled_b,
              "stalled B must remain valid and stable");
        check(noc.chimney(manager_node).b_outstanding(5) == 1,
              "stalled B must not release its ordering counter");
    }
    manager.b_ready.write(true);
    for (unsigned watchdog = 0; watchdog < 128 && manager_b_seen.empty();
         ++watchdog) {
        tick();
    }
    check(manager_b_seen.size() == 1, "manager must accept exactly one B");
    check(noc.chimney(manager_node).b_outstanding(5) == 0,
          "accepted B must release the B ordering counter");
    check(noc.chimney(target_node).aw_meta_occupancy() == 0,
          "accepted B must release target AW metadata");
    manager.b_ready.write(false);

    // ---- three-beat read ------------------------------------------------
    axi_ar_chan ar{};
    ar.id = 3;
    ar.addr = 0x9000'2080;
    ar.len = 2;
    ar.size = 3;
    ar.burst = 1;
    ar.cache = 0x5;
    ar.prot = 0x2;
    ar.qos = 0x9;
    ar.user = 0x33;
    manager.ar.write(ar);
    manager.ar_valid.write(true);
    bool ar_accepted = false;
    for (unsigned watchdog = 0; watchdog < 128 && !ar_accepted; ++watchdog) {
        ar_accepted = tick().manager_ar;
    }
    manager.ar_valid.write(false);
    check(ar_accepted, "read AR did not enter the source chimney");

    for (unsigned watchdog = 0; watchdog < 128 && target_ar_seen.empty();
         ++watchdog) {
        tick();
    }
    check(target_ar_seen.size() == 1,
          "target subordinate must accept exactly one AR");
    if (target_ar_seen.size() == 1) {
        axi_ar_chan expected = ar;
        expected.id = 7;
        check(target_ar_seen.front() == expected,
              "AR fields must survive except for the downstream ID rewrite");
    }
    check(noc.chimney(manager_node).r_outstanding(3) == 1,
          "accepted AR must hold one R ordering counter");
    check(noc.chimney(target_node).ar_meta_occupancy() == 1,
          "target chimney must retain one AR metadata entry");

    const std::vector<axi_r_chan> read_beats{
        axi_r_chan{7, 0x1111'2222'3333'4444, 0, false, 0x41},
        axi_r_chan{7, 0x5555'6666'7777'8888, 1, false, 0x42},
        axi_r_chan{7, 0x9999'AAAA'BBBB'CCCC, 2, true, 0x43},
    };

    for (std::size_t beat = 0; beat < read_beats.size(); ++beat) {
        // Stall the middle beat at the manager to prove that the composed
        // ready path reaches all the way back without releasing the counter.
        const bool stall = beat == 1;
        manager.r_ready.write(!stall);
        target.r.write(read_beats[beat]);
        target.r_valid.write(true);

        bool target_r_accepted = false;
        for (unsigned watchdog = 0; watchdog < 128 && !target_r_accepted;
             ++watchdog) {
            target_r_accepted = tick().target_r;
        }
        target.r_valid.write(false);
        check(target_r_accepted,
              "target chimney did not accept R beat " + std::to_string(beat));

        if (stall) {
            bool stalled_r_visible = false;
            axi_r_chan stalled_r{};
            for (unsigned watchdog = 0; watchdog < 128 && !stalled_r_visible;
                 ++watchdog) {
                tick();
                stalled_r_visible = manager.r_valid.read();
                stalled_r = manager.r.read();
            }
            check(stalled_r_visible, "stalled R did not reach the manager");
            axi_r_chan expected = read_beats[beat];
            expected.id = ar.id;
            check(stalled_r == expected,
                  "stalled R must restore ID and preserve every field");
            for (unsigned count = 0; count < 3; ++count) {
                tick();
                check(manager.r_valid.read() && manager.r.read() == stalled_r,
                      "stalled R must remain valid and stable");
                check(noc.chimney(manager_node).r_outstanding(3) == 1,
                      "stalled R must not release the read counter");
            }
            manager.r_ready.write(true);
        }

        const std::size_t wanted = beat + 1;
        for (unsigned watchdog = 0;
             watchdog < 128 && manager_r_seen.size() < wanted; ++watchdog) {
            tick();
        }
        check(manager_r_seen.size() == wanted,
              "manager did not accept R beat " + std::to_string(beat));
        if (manager_r_seen.size() >= wanted) {
            axi_r_chan expected = read_beats[beat];
            expected.id = ar.id;
            check(manager_r_seen[beat] == expected,
                  "R beat must restore ID and preserve data/RESP/LAST/user");
        }
        const unsigned expected_outstanding =
            beat + 1 == read_beats.size() ? 0 : 1;
        check(noc.chimney(manager_node).r_outstanding(3)
                  == expected_outstanding,
              "R counter must release only on the accepted RLAST beat");
    }
    manager.r_ready.write(false);

    check(noc.chimney(target_node).ar_meta_occupancy() == 0,
          "accepted RLAST must release target AR metadata");
    check(target_aw_seen.size() == 1 && target_w_seen.size() == 2
              && target_ar_seen.size() == 1,
          "the target must see no duplicated request beats");
    check(manager_b_seen.size() == 1 && manager_r_seen.size() == 3,
          "the manager must see no duplicated response beats");

    if (failures == 0) {
        std::cout << "axi_noc chimney composition PASS in " << cycle
                  << " cycles\n";
    }
    return failures == 0 ? 0 : 1;
}
