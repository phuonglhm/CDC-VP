// SPDX-License-Identifier: SHL-0.51
//
// `axi_chimney_manager_response` — the manager-side response unpacker.
//
// The last test is the one that matters: it closes the loop against
// `axi_chimney_request`, so a reorder-buffer counter taken by an AW is released
// by the B that answers it. Nothing before this existed to do that — the timing
// harness drove `i_b_pop` by hand from the RTL's own response stream, which
// proves the counter behaves but not that anything in the model ever pops it.

#include "floo_noc_model/axi_chimney.hpp"

#include <systemc>

#include <cstdlib>
#include <iostream>

namespace {

using namespace floo::model;

int failures = 0;

void check(bool condition, const char* what)
{
    if (!condition) {
        std::cout << "FAIL: " << what << "\n";
        ++failures;
    }
}

coordinate make_coord(unsigned x, unsigned y)
{
    coordinate id{};
    id.x = x;
    id.y = y;
    id.port_id = 0;
    return id;
}

axi_rsp_flit make_rsp(axi_channel channel, std::uint64_t id, bool last = true)
{
    axi_rsp_flit flit{};
    flit.hdr.axi_ch = static_cast<unsigned>(channel);
    flit.hdr.last = true;
    flit.b.id = id;
    flit.b.resp = 0;
    flit.r.id = id;
    flit.r.data = 0xDEAD'BEEFull;
    flit.r.last = last;
    return flit;
}

/// Two zero-time starts, not one. The first lets the input write reach the
/// signal and the method run; the second lets the method's own output writes
/// become readable. Checking after a single delta reads the previous values.
void settle()
{
    sc_core::sc_start(sc_core::SC_ZERO_TIME);
    sc_core::sc_start(sc_core::SC_ZERO_TIME);
}

// ── the unpacker on its own ─────────────────────────────────────────────────
/// An `sc_module` rather than a plain struct so its signals get hierarchical
/// names. Two harnesses in one elaboration otherwise collide on every shared
/// signal name and SystemC renames them with a W505 warning per signal.
struct decode_harness : sc_core::sc_module {
    sc_core::sc_signal<axi_rsp_flit> rsp_data{"rsp_data"};
    sc_core::sc_signal<bool> rsp_valid{"rsp_valid"};
    sc_core::sc_signal<bool> rsp_ready{"rsp_ready"};
    sc_core::sc_signal<bool> b_rob_ready{"b_rob_ready"};
    sc_core::sc_signal<bool> r_rob_ready{"r_rob_ready"};
    sc_core::sc_signal<bool> b_pop{"b_pop"};
    sc_core::sc_signal<unsigned> b_pop_id{"b_pop_id"};
    sc_core::sc_signal<bool> r_pop{"r_pop"};
    sc_core::sc_signal<unsigned> r_pop_id{"r_pop_id"};
    sc_core::sc_signal<bool> r_pop_last{"r_pop_last"};
    sc_core::sc_signal<axi_b_chan> axi_b{"axi_b"};
    sc_core::sc_signal<bool> axi_b_valid{"axi_b_valid"};
    sc_core::sc_signal<axi_r_chan> axi_r{"axi_r"};
    sc_core::sc_signal<bool> axi_r_valid{"axi_r_valid"};

    axi_chimney_manager_response dut{"unpacker_alone"};

    explicit decode_harness(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
    {
    dut.i_rsp_data(rsp_data);
    dut.i_rsp_valid(rsp_valid);
    dut.o_rsp_ready(rsp_ready);
    dut.i_b_rob_ready(b_rob_ready);
    dut.i_r_rob_ready(r_rob_ready);
    dut.o_b_pop(b_pop);
    dut.o_b_pop_id(b_pop_id);
    dut.o_r_pop(r_pop);
    dut.o_r_pop_id(r_pop_id);
    dut.o_r_pop_last(r_pop_last);
    dut.o_axi_b(axi_b);
    dut.o_axi_b_valid(axi_b_valid);
    dut.o_axi_r(axi_r);
    dut.o_axi_r_valid(axi_r_valid);
    }

    void run()
    {
    b_rob_ready.write(true);
    r_rob_ready.write(true);

    // Idle: nothing valid, nothing popped, and the link is not ready — an
    // unnamed channel must not be accepted by default.
    rsp_valid.write(false);
    rsp_data.write(axi_rsp_flit{});
    settle();
    check(!axi_b_valid.read() && !axi_r_valid.read(), "idle drives no AXI valid");
    check(!b_pop.read() && !r_pop.read(), "idle pops no counter");

    // A B response.
    rsp_valid.write(true);
    rsp_data.write(make_rsp(axi_channel::b, 5));
    settle();
    check(axi_b_valid.read(), "B flit raises the AXI B valid");
    check(!axi_r_valid.read(), "B flit leaves R idle");
    check(axi_b.read().id == 5, "B payload passes through unchanged");
    check(b_pop.read() && b_pop_id.read() == 5,
          "B flit pops the B counter by the id it carries");
    check(!r_pop.read(), "B flit does not pop the R counter");
    check(rsp_ready.read(), "link ready follows the B reorder buffer");

    // An R response.
    rsp_data.write(make_rsp(axi_channel::r, 3));
    settle();
    check(axi_r_valid.read() && !axi_b_valid.read(), "R flit selects R only");
    check(r_pop.read() && r_pop_id.read() == 3, "R flit pops the R counter");
    check(axi_r.read().data == 0xDEAD'BEEFull, "R payload passes through");
    check(r_pop_last.read(), "a final R beat reports RLAST");

    // A mid-burst beat still carries data to the manager, but must not tell
    // the reorder buffer that the transaction is finished.
    rsp_data.write(make_rsp(axi_channel::r, 3, /*last=*/false));
    settle();
    check(axi_r_valid.read(), "a non-final R beat still reaches the manager");
    check(r_pop.read(), "a non-final R beat still drives the buffer's valid");
    check(!r_pop_last.read(), "a non-final R beat does not report RLAST");

    // Back-pressure is per channel, not shared.
    b_rob_ready.write(false);
    rsp_data.write(make_rsp(axi_channel::b, 5));
    settle();
    check(!rsp_ready.read(), "a full B reorder buffer refuses the link");
    rsp_data.write(make_rsp(axi_channel::r, 3));
    settle();
    check(rsp_ready.read(), "an R flit is unaffected by the B buffer");
    b_rob_ready.write(true);

    // The `rsp` link never carries a request channel. Accepting one would
    // silently pop a counter that no response ever released.
    rsp_data.write(make_rsp(axi_channel::aw, 1));
    settle();
    check(!rsp_ready.read(), "a request channel on the rsp link is refused");
    check(!b_pop.read() && !r_pop.read(), "a request channel pops nothing");
    }
};

// ── the loop: request takes a counter, the unpacker gives it back ───────────
struct loop_harness : sc_core::sc_module {
    sc_core::sc_clock clk{"clk", 1, sc_core::SC_NS};
    sc_core::sc_signal<bool> rst_n{"rst_n"};
    sc_core::sc_signal<coordinate> node_id{"node_id"};

    sc_core::sc_signal<bool> aw_valid{"aw_valid"}, aw_ready{"aw_ready"};
    sc_core::sc_signal<axi_aw_chan> aw{"aw"};
    sc_core::sc_signal<coordinate> aw_dest{"aw_dest"};
    sc_core::sc_signal<bool> w_valid{"w_valid"}, w_ready{"w_ready"};
    sc_core::sc_signal<axi_w_chan> w{"w"};
    sc_core::sc_signal<bool> ar_valid{"ar_valid"}, ar_ready{"ar_ready"};
    sc_core::sc_signal<axi_ar_chan> ar{"ar"};
    sc_core::sc_signal<coordinate> ar_dest{"ar_dest"};
    sc_core::sc_signal<axi_req_flit> req_data{"req_data"};
    sc_core::sc_signal<bool> req_valid{"req_valid"}, req_ready{"req_ready"};

    sc_core::sc_signal<bool> b_pop{"b_pop"}, r_pop{"r_pop"};
    sc_core::sc_signal<unsigned> b_pop_id{"b_pop_id"}, r_pop_id{"r_pop_id"};
    sc_core::sc_signal<bool> r_pop_last{"r_pop_last"};
    sc_core::sc_signal<bool> b_rob_ready{"b_rob_ready"};
    sc_core::sc_signal<bool> r_rob_ready{"r_rob_ready"};
    sc_core::sc_signal<bool> manager_ready{"manager_ready", true};

    sc_core::sc_signal<axi_rsp_flit> rsp_data{"rsp_data"};
    sc_core::sc_signal<bool> rsp_valid{"rsp_valid"}, rsp_ready{"rsp_ready"};
    sc_core::sc_signal<axi_b_chan> axi_b{"axi_b"};
    sc_core::sc_signal<bool> axi_b_valid{"axi_b_valid"};
    sc_core::sc_signal<axi_r_chan> axi_r{"axi_r"};
    sc_core::sc_signal<bool> axi_r_valid{"axi_r_valid"};

    axi_chimney_request<3, 32> request{"request"};
    axi_chimney_manager_response unpacker{"unpacker_loop"};

    explicit loop_harness(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
    {
    request.i_clk(clk);
    request.i_rst_n(rst_n);
    request.i_node_id(node_id);
    request.i_aw_valid(aw_valid);
    request.o_aw_ready(aw_ready);
    request.i_aw(aw);
    request.i_aw_dest(aw_dest);
    request.i_w_valid(w_valid);
    request.o_w_ready(w_ready);
    request.i_w(w);
    request.i_ar_valid(ar_valid);
    request.o_ar_ready(ar_ready);
    request.i_ar(ar);
    request.i_ar_dest(ar_dest);
    request.o_req_data(req_data);
    request.o_req_valid(req_valid);
    request.i_req_ready(req_ready);
    request.i_b_pop(b_pop);
    request.i_b_pop_id(b_pop_id);
    request.i_r_pop(r_pop);
    request.i_r_pop_id(r_pop_id);
    request.i_r_pop_last(r_pop_last);
    request.o_b_rsp_ready(b_rob_ready);
    request.o_r_rsp_ready(r_rob_ready);
    request.i_b_rsp_ready(manager_ready);
    request.i_r_rsp_ready(manager_ready);

    unpacker.i_rsp_data(rsp_data);
    unpacker.i_rsp_valid(rsp_valid);
    unpacker.o_rsp_ready(rsp_ready);
    unpacker.i_b_rob_ready(b_rob_ready);
    unpacker.i_r_rob_ready(r_rob_ready);
    unpacker.o_b_pop(b_pop);
    unpacker.o_b_pop_id(b_pop_id);
    unpacker.o_r_pop(r_pop);
    unpacker.o_r_pop_id(r_pop_id);
    unpacker.o_r_pop_last(r_pop_last);
    unpacker.o_axi_b(axi_b);
    unpacker.o_axi_b_valid(axi_b_valid);
    unpacker.o_axi_r(axi_r);
    unpacker.o_axi_r_valid(axi_r_valid);
    }

    void run()
    {
    node_id.write(make_coord(0, 0));
    aw_dest.write(make_coord(1, 0));
    ar_dest.write(make_coord(1, 0));
    req_ready.write(true);
    rsp_valid.write(false);

    rst_n.write(false);
    sc_core::sc_start(4, sc_core::SC_NS);
    rst_n.write(true);
    sc_core::sc_start(1, sc_core::SC_NS);

    check(request.b_outstanding(7) == 0, "no B outstanding after reset");

    // Issue one AW with id 7 and let it be accepted onto the link.
    axi_aw_chan aw_beat{};
    aw_beat.id = 7;
    aw_beat.addr = 0x1000;
    aw_beat.size = 3;
    aw_beat.len = 0;
    aw.write(aw_beat);
    aw_valid.write(true);
    axi_w_chan w_beat{};
    w_beat.data = 0x11;
    w_beat.strb = 0xFF;
    w_beat.last = true;
    w.write(w_beat);
    w_valid.write(true);

    for (int cycle = 0; cycle < 8 && request.b_outstanding(7) == 0; ++cycle) {
        sc_core::sc_start(1, sc_core::SC_NS);
    }
    aw_valid.write(false);
    w_valid.write(false);

    check(request.b_outstanding(7) == 1,
          "an accepted AW takes one B reorder-buffer counter");

    // Now answer it. The unpacker must release the counter without the test
    // driving `i_b_pop` itself — that is the whole point of the module.
    rsp_data.write(make_rsp(axi_channel::b, 7));
    rsp_valid.write(true);
    sc_core::sc_start(1, sc_core::SC_NS);
    check(axi_b_valid.read(), "the B reaches the AXI manager port");
    rsp_valid.write(false);
    sc_core::sc_start(1, sc_core::SC_NS);

    check(request.b_outstanding(7) == 0,
          "the answering B releases the counter through the unpacker");

    // ---- a read burst releases once, on RLAST -------------------------
    //
    // This is the case the AW/B loop above cannot reach, and the one an
    // earlier revision got wrong: with RLAST tied high every beat released a
    // counter, so a four-beat read decremented four times.
    axi_ar_chan ar_beat{};
    ar_beat.id = 4;
    ar_beat.addr = 0x1000;
    ar_beat.size = 3;
    ar_beat.len = 3;   // four beats
    ar.write(ar_beat);
    ar_valid.write(true);
    for (int cycle = 0; cycle < 8 && request.r_outstanding(4) == 0; ++cycle) {
        sc_core::sc_start(1, sc_core::SC_NS);
    }
    ar_valid.write(false);
    sc_core::sc_start(1, sc_core::SC_NS);
    check(request.r_outstanding(4) == 1,
          "an accepted AR takes one R reorder-buffer counter");

    // Three non-final beats: data flows, the counter must not move.
    for (int beat = 0; beat < 3; ++beat) {
        rsp_data.write(make_rsp(axi_channel::r, 4, /*last=*/false));
        rsp_valid.write(true);
        sc_core::sc_start(1, sc_core::SC_NS);
        check(request.r_outstanding(4) == 1,
              "a non-final R beat leaves the counter alone");
    }

    // The final beat releases it, exactly once.
    rsp_data.write(make_rsp(axi_channel::r, 4, /*last=*/true));
    sc_core::sc_start(1, sc_core::SC_NS);
    rsp_valid.write(false);
    sc_core::sc_start(1, sc_core::SC_NS);
    check(request.r_outstanding(4) == 0,
          "the final R beat releases the counter once");

    // ---- back-pressure: the pop is qualified by the handshake ---------
    ar_beat.id = 2;
    ar.write(ar_beat);
    ar_valid.write(true);
    for (int cycle = 0; cycle < 8 && request.r_outstanding(2) == 0; ++cycle) {
        sc_core::sc_start(1, sc_core::SC_NS);
    }
    ar_valid.write(false);
    sc_core::sc_start(1, sc_core::SC_NS);
    check(request.r_outstanding(2) == 1, "second AR takes a counter");

    // Offer the final beat while the manager refuses it. `pop_i = pop &&
    // rsp_ready_i`, so nothing may be released until the handshake completes.
    manager_ready.write(false);
    rsp_data.write(make_rsp(axi_channel::r, 2, /*last=*/true));
    rsp_valid.write(true);
    sc_core::sc_start(2, sc_core::SC_NS);
    check(request.r_outstanding(2) == 1,
          "a stalled final R beat does not release the counter");

    // Release it, then withdraw the beat in the same breath. A response held
    // valid after its handshake keeps popping: the counter wraps to 31, 30, 29.
    // That is the environment's fault, not the model's — AXI never repeats an
    manager_ready.write(true);
    sc_core::sc_start(1, sc_core::SC_NS);
    check(request.r_outstanding(2) == 0,
          "the counter is released when the final beat is accepted");

    // Deliberately not checked here: what happens if the beat is left valid
    // after that. It keeps popping and the counter wraps to 31, 30, 29. That is
    // an AXI protocol violation by the environment, not a model defect, and
    // pinning it would need a clock-synchronised driver — `sc_start(1, SC_NS)`
    // stops *before* processing the edge at exactly that time, so a withdrawal
    // written between two `sc_start` calls lands one edge later than it reads.
    }
};

} // namespace

int sc_main(int, char*[])
{
    // Both harnesses are constructed before any `sc_start`: SystemC refuses
    // to create a primitive channel once simulation has begun.
    decode_harness decode{"decode"};
    loop_harness loop{"loop"};

    decode.run();
    loop.run();

    if (failures != 0) {
        std::cout << failures << " check(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "test_axi_chimney_manager_response: all checks passed\n";
    return EXIT_SUCCESS;
}
