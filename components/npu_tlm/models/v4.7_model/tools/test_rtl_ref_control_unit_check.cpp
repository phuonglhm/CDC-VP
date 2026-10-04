// Unit test of sauria_rtl::Control (control/rtl_ref_main_controller.h) without real feeders / PSM: checks that
// Control binds and compiles, and that ctx_status / feed_status progress sensibly with hand-driven responses
// (FIFO always holds data, outbuf_done / shift_done pulsed by hand to emulate the PSM). Not a bit-exact test
// (the differential test against NativeLaneACoreA covers that).
#define SC_ALLOW_DEPRECATED_IEEE_API

#include <systemc.h>
#include <iostream>

#include "control/rtl_ref_defaults.h"
#include "control/rtl_ref_main_controller.h"

using namespace sauria_rtl;

typedef Control<32, 32, /*PE_LAT=*/64, /*EXTRA_CSREG=*/1> ControlT;

SC_MODULE(TbRtlRefControlUnitCheck)
{
    sc_in<bool> i_clk;

    sc_signal<bool> rstn, soft_reset, start;
    sc_signal<bool> outbuf_done, finalwrite, shift_done;
    sc_signal<uint32_t> incntlim, act_reps, wei_reps, ncontexts, total_contexts, mvm_k;
    sc_signal<bool> act_done, act_til_done, act_fifo_empty, act_fifo_full, act_stall;
    sc_signal<bool> wei_done, wei_til_done, wei_fifo_empty, wei_fifo_full, wei_stall;
    sc_signal<bool> act_feeder_en, act_feeder_clear, act_start, act_valid, act_finalpush;
    sc_signal<bool> act_cnt_en, act_cnt_clear, act_clearfifo, act_pop_en, act_finalctx, softstall;
    sc_signal<bool> wei_feeder_en, wei_feeder_clear, wei_start, wei_valid, wei_finalpush;
    sc_signal<bool> wei_cnt_en, wei_cnt_clear, wei_clearfifo, wei_pop_en, wei_cswitch;
    sc_signal<uint32_t> context_id, global_context_id, local_context_id, out_tile_id;
    sc_signal<bool> outbuf_start, outbuf_reset, sa_clear, pipeline_en;
    sc_signal<sc_bv<32>> cswitch_arr;
    sc_signal<bool> done, feed_deadlock;

    ControlT *ctrl;
    int errors = 0;

    SC_CTOR(TbRtlRefControlUnitCheck)
    {
        ctrl = new ControlT("ctrl_inst");
        ctrl->i_clk(i_clk);
        ctrl->i_rstn(rstn);
        ctrl->i_soft_reset(soft_reset);
        ctrl->i_start(start);
        ctrl->i_outbuf_done(outbuf_done);
        ctrl->i_finalwrite(finalwrite);
        ctrl->i_shift_done(shift_done);
        ctrl->i_incntlim(incntlim);
        ctrl->i_act_reps(act_reps);
        ctrl->i_wei_reps(wei_reps);
        ctrl->i_ncontexts(ncontexts);
        ctrl->i_total_contexts(total_contexts);
        ctrl->i_mvm_k(mvm_k);
        ctrl->i_act_done(act_done);
        ctrl->i_act_til_done(act_til_done);
        ctrl->i_act_fifo_empty(act_fifo_empty);
        ctrl->i_act_fifo_full(act_fifo_full);
        ctrl->i_act_stall(act_stall);
        ctrl->i_wei_done(wei_done);
        ctrl->i_wei_til_done(wei_til_done);
        ctrl->i_wei_fifo_empty(wei_fifo_empty);
        ctrl->i_wei_fifo_full(wei_fifo_full);
        ctrl->i_wei_stall(wei_stall);
        ctrl->o_act_feeder_en(act_feeder_en);
        ctrl->o_act_feeder_clear(act_feeder_clear);
        ctrl->o_act_start(act_start);
        ctrl->o_act_valid(act_valid);
        ctrl->o_act_finalpush(act_finalpush);
        ctrl->o_act_cnt_en(act_cnt_en);
        ctrl->o_act_cnt_clear(act_cnt_clear);
        ctrl->o_act_clearfifo(act_clearfifo);
        ctrl->o_act_pop_en(act_pop_en);
        ctrl->o_act_finalctx(act_finalctx);
        ctrl->o_softstall(softstall);
        ctrl->o_wei_feeder_en(wei_feeder_en);
        ctrl->o_wei_feeder_clear(wei_feeder_clear);
        ctrl->o_wei_start(wei_start);
        ctrl->o_wei_valid(wei_valid);
        ctrl->o_wei_finalpush(wei_finalpush);
        ctrl->o_wei_cnt_en(wei_cnt_en);
        ctrl->o_wei_cnt_clear(wei_cnt_clear);
        ctrl->o_wei_clearfifo(wei_clearfifo);
        ctrl->o_wei_pop_en(wei_pop_en);
        ctrl->o_wei_cswitch(wei_cswitch);
        ctrl->o_context_id(context_id);
        ctrl->o_global_context_id(global_context_id);
        ctrl->o_local_context_id(local_context_id);
        ctrl->o_out_tile_id(out_tile_id);
        ctrl->o_outbuf_start(outbuf_start);
        ctrl->o_outbuf_reset(outbuf_reset);
        ctrl->o_sa_clear(sa_clear);
        ctrl->o_pipeline_en(pipeline_en);
        ctrl->o_cswitch_arr(cswitch_arr);
        ctrl->o_done(done);
        ctrl->o_feed_deadlock(feed_deadlock);

        // FX1_A3_SEAM_ORDER is enabled (rtl_ref_defaults.h): Control registers no SC_METHOD of its own;
        // seam_step() must be called in the right order every cycle (as in the npu_top.h wiring).
        SC_THREAD(run);
        sensitive << i_clk.pos();
    }

    void check(bool cond, const std::string &msg)
    {
        if (cond) { std::cout << "  [PASS] " << msg << std::endl; }
        else { std::cout << "  [FAIL] " << msg << std::endl; errors++; }
    }

    // With FX1_A3_SEAM_ORDER enabled, Control registers no SC_METHOD/SC_THREAD of its own; seam_step()
    // must be called on every clock edge (as seam_ordered_step() does in sauria_model's npu_top.h).
    // Every wait() in this test must be paired with this call.
    void tick(int n = 1)
    {
        for (int i = 0; i < n; i++)
        {
            wait();
            ctrl->seam_step();
        }
    }

    void run()
    {
        std::cout << "==================================================" << std::endl;
        std::cout << " RTL-REF CONTROL UNIT CHECK (stand-alone, no real feeder/PSM)" << std::endl;
        std::cout << "==================================================" << std::endl;

        rstn.write(false);
        soft_reset.write(false);
        start.write(false);
        outbuf_done.write(false);
        finalwrite.write(false);
        shift_done.write(false);
        incntlim.write(8);
        act_reps.write(1);
        wei_reps.write(1);
        ncontexts.write(1);
        total_contexts.write(1);
        mvm_k.write(8);
        act_done.write(false);
        act_til_done.write(false);
        // Emulate a FIFO that ALWAYS holds data (neither empty nor full) so FeedersFsm can move through
        // FIFO_FILL_WAIT->FIFO_FILL without a real feeder. A deliberate simplification for this unit test,
        // not real behaviour.
        act_fifo_empty.write(false);
        act_fifo_full.write(false);
        act_stall.write(false);
        wei_done.write(false);
        wei_til_done.write(false);
        wei_fifo_empty.write(false);
        wei_fifo_full.write(false);
        wei_stall.write(false);
        tick(5);
        rstn.write(true);
        tick(5);

        // NOTE: o_done differs from the native tree -- the RTL-reference version sets o_done only when
        // ctx_status == 21 (state DONE, after one full pass over all contexts), not whenever it is IDLE
        // (rtl_ref_main_controller.h: "o_done.write(ctx_out.ctx_status == 21)"). This test does not
        // complete a pass, so o_done = false even right after reset is correct.
        check(!done.read(), "o_done = false right after reset (true only when ctx_status==DONE=21)");
        check(!feed_deadlock.read(), "no deadlock after reset");

        std::cout << "\n[PULSE] i_start = true" << std::endl;
        start.write(true);
        tick();
        start.write(false);

        // Pulse outbuf_done/shift_done by hand to emulate a PSM that "finishes at once" -- this unit test
        // checks that Control moves through its states sensibly, not the real PSM.
        bool saw_outbuf_start_ever = false;
        int cycles_since_outbuf_start = -1;
        bool pulsed_outbuf_done = false;
        bool pulsed_shift_done = false;
        bool saw_pop_en_true = false;
        bool saw_pop_en_false_after_true = false;

        const int TOTAL_CYCLES = 200;
        for (int c = 0; c < TOTAL_CYCLES; c++)
        {
            tick();

            // Respond to EVERY o_outbuf_start pulse, not only the first: ContextFsm waits for the PSM to finish several
            // times within one context (ARRAY_PREP waits for outbuf_done_hold, FIRST_SHIFT needs a new shift_done,
            // likewise SCND_SHIFT / LAST_SHIFT), so the emulated PSM completes 2 cycles after each pulse.
            if (outbuf_start.read())
            {
                saw_outbuf_start_ever = true;
                cycles_since_outbuf_start = 0;
            }
            else if (cycles_since_outbuf_start >= 0)
            {
                cycles_since_outbuf_start++;
            }

            if (act_pop_en.read() || wei_pop_en.read())
            {
                saw_pop_en_true = true;
            }
            else if (saw_pop_en_true)
            {
                saw_pop_en_false_after_true = true;
            }

            if (cycles_since_outbuf_start == 2)
            {
                outbuf_done.write(true);
                shift_done.write(true);
                pulsed_outbuf_done = true;
                pulsed_shift_done = true;
            }
            else if (cycles_since_outbuf_start == 3)
            {
                outbuf_done.write(false);
                shift_done.write(false);
            }

            // ctx_status/feed_status are not exposed on ports, but o_context_id / o_pipeline_en /
            // o_act_feeder_en ... are enough to follow the progress qualitatively. o_act_feeder_en /
            // o_act_cnt_en serve as the proxy for "has left IDLE".
            if (c < 40 || (c % 10 == 0))
            {
                std::cout << "  [cyc " << c << "] act_feeder_en=" << act_feeder_en.read()
                          << " act_cnt_en=" << act_cnt_en.read()
                          << " act_pop_en=" << act_pop_en.read()
                          << " wei_pop_en=" << wei_pop_en.read()
                          << " pipeline_en=" << pipeline_en.read()
                          << " outbuf_start=" << outbuf_start.read()
                          << " sa_clear=" << sa_clear.read()
                          << " done=" << done.read()
                          << std::endl;
            }
        }

        // Goal of this stand-alone unit test (no real feeder/PSM): Control binds and compiles and moves
        // sensibly through several real states (not stuck from the start, no deadlock/abort). Reaching
        // the real DONE state (ctx_status == 21) would need a fake PSM that "finishes" at every
        // outbuf_done_hold point and through ARRAY_CSWITCH / SCND_SHIFT / LAST_SHIFT -- close to
        // re-implementing the PSM. Reaching DONE is covered by the differential test against
        // NativeLaneACoreA with the real Psm/feeders.
        check(!feed_deadlock.read(), "no deadlock during the whole run");
        check(saw_outbuf_start_ever, "at least one o_outbuf_start pulse (left ARRAY_PREP, passed START_FLAGS)");
        check(act_feeder_en.read() || wei_feeder_en.read(), "act/wei_feeder_en raised (feeder enabled)");
        check(saw_pop_en_true && saw_pop_en_false_after_true,
              "act_pop_en/wei_pop_en toggle on/off (the K count really runs, not stuck at one value)");

        std::cout << "\n==================================================" << std::endl;
        if (errors == 0) std::cout << "  [PASS] RTL-REF CONTROL UNIT CHECK." << std::endl;
        else std::cout << "  [FAIL] RTL-REF CONTROL UNIT CHECK: " << errors << " errors." << std::endl;
        std::cout << "==================================================" << std::endl;

        sc_stop();
    }
};

int sc_main(int argc, char **argv)
{
    sc_clock clk("clk", 10, SC_NS);
    TbRtlRefControlUnitCheck tb("TbRtlRefControlUnitCheck_inst");
    tb.i_clk(clk);
    sc_start();
    return 0;
}
