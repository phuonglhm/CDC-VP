// SystemC Model for SAURIA NPU Core
// Controller FSM Block (Context FSM, Feeders FSM, Main Controller)

#ifndef SAURIA_MAIN_CONTROLLER_H
#define SAURIA_MAIN_CONTROLLER_H

#include "sauria_types.h"
#include "debug.h"

namespace sauria
{

    template <
        int X_DIM = 32,
        int Y_DIM = 32,
        int PE_LAT = X_DIM + Y_DIM,
        int EXTRA_CSREG = 1,
        // Number of cycles to keep cnt_en high (issuing SRAM reads) BEFORE
        // enabling pop_en. This pre-fills the feeder FIFOs with real data so
        // the array does not consume empty (zero) vectors at the start of a
        // context. Matches the reference RTL FIFO_FILL behaviour.
        // Latency budget: 2 SRAM cycles (rden_q1 -> rden_q2 -> valid)
        //               + 1 emit->FIFO cycle = 3. Use 3 (tune if needed).
        int FILL_CYCLES = 3>
    class Control : public sc_module
    {
    public:
        // Clock & Reset
        sc_in<bool> i_clk{"i_clk"};
        sc_in<bool> i_rstn{"i_rstn"};
        sc_in<bool> i_soft_reset{"i_soft_reset"};

        // Host Control Inputs
        sc_in<bool> i_start{"i_start"}; // Starts NPU execution

        // Output Buffer feedback
        sc_in<bool> i_outbuf_done{"i_outbuf_done"};
        sc_in<bool> i_finalwrite{"i_finalwrite"};
        sc_in<bool> i_shift_done{"i_shift_done"};

        // Tiling & Loop limits (from Config registers)
        sc_in<uint32_t> i_incntlim{"i_incntlim"};
        sc_in<uint32_t> i_act_reps{"i_act_reps"};
        sc_in<uint32_t> i_wei_reps{"i_wei_reps"};
        sc_in<uint32_t> i_ncontexts{"i_ncontexts"};
        sc_in<uint32_t> i_total_contexts{"i_total_contexts"};
        sc_in<uint32_t> i_mvm_k{"i_mvm_k"};

        // Feeder feedback signals
        sc_in<bool> i_act_done{"i_act_done"};
        sc_in<bool> i_act_til_done{"i_act_til_done"};
        sc_in<bool> i_act_fifo_empty{"i_act_fifo_empty"};
        sc_in<bool> i_act_fifo_full{"i_act_fifo_full"};
        sc_in<bool> i_act_stall{"i_act_stall"};

        sc_in<bool> i_wei_done{"i_wei_done"};
        sc_in<bool> i_wei_til_done{"i_wei_til_done"};
        sc_in<bool> i_wei_fifo_empty{"i_wei_fifo_empty"};
        sc_in<bool> i_wei_fifo_full{"i_wei_fifo_full"};
        sc_in<bool> i_wei_stall{"i_wei_stall"};

        // Feeder Control Outputs
        sc_out<bool> o_act_feeder_en{"o_act_feeder_en"};
        sc_out<bool> o_act_feeder_clear{"o_act_feeder_clear"};
        sc_out<bool> o_act_start{"o_act_start"};
        sc_out<bool> o_act_valid{"o_act_valid"};
        sc_out<bool> o_act_finalpush{"o_act_finalpush"};
        sc_out<bool> o_act_cnt_en{"o_act_cnt_en"};
        sc_out<bool> o_act_cnt_clear{"o_act_cnt_clear"};
        sc_out<bool> o_act_clearfifo{"o_act_clearfifo"};
        sc_out<bool> o_act_pop_en{"o_act_pop_en"};
        sc_out<bool> o_act_finalctx{"o_act_finalctx"};

        sc_out<bool> o_wei_feeder_en{"o_wei_feeder_en"};
        sc_out<bool> o_wei_feeder_clear{"o_wei_feeder_clear"};
        sc_out<bool> o_wei_start{"o_wei_start"};
        sc_out<bool> o_wei_valid{"o_wei_valid"};
        sc_out<bool> o_wei_finalpush{"o_wei_finalpush"};
        sc_out<bool> o_wei_cnt_en{"o_wei_cnt_en"};
        sc_out<bool> o_wei_cnt_clear{"o_wei_cnt_clear"};
        sc_out<bool> o_wei_clearfifo{"o_wei_clearfifo"};
        sc_out<bool> o_wei_pop_en{"o_wei_pop_en"};
        sc_out<bool> o_wei_cswitch{"o_wei_cswitch"};

        sc_out<uint32_t> o_context_id{"o_context_id"};
        sc_out<uint32_t> o_global_context_id{"o_global_context_id"};
        sc_out<uint32_t> o_local_context_id{"o_local_context_id"};
        sc_out<uint32_t> o_out_tile_id{"o_out_tile_id"};

        // Output Buffer / PSM Control Outputs
        sc_out<bool> o_outbuf_start{"o_outbuf_start"};
        sc_out<bool> o_outbuf_reset{"o_outbuf_reset"};

        // Systolic Array Control Outputs
        sc_out<bool> o_sa_clear{"o_sa_clear"};
        sc_out<bool> o_pipeline_en{"o_pipeline_en"};
        sc_out<sc_bv<X_DIM>> o_cswitch_arr{"o_cswitch_arr"};

        // General Done status out
        sc_out<bool> o_done{"o_done"};
        sc_out<bool> o_feed_deadlock{"o_feed_deadlock"};

        // Internal Context FSM states matching context_fsm.sv
        enum ctrl_state_t
        {
            IDLE,
            START_FLAGS,
            ARRAY_PREP,
            ARRAY_FILL,
            FIRST_SHIFT,
            START_COMP,
            DRAIN_FEED,
            ARRAY_FLUSH,
            WAIT_CSWITCH,
            WAIT_CSWITCH_STALL,
            WAIT_OBUF,
            WAIT_OBUF_STALL,
            SCND_SHIFT,
            SCND_SHIFT_STALL,
            ALL_BUSY_SHIFT,
            ALL_BUSY,
            ARRAY_BUSY,
            OBUF_BUSY_SHIFT,
            FORCE_STALL,
            OBUF_BUSY,
            ARRAY_CSWITCH,
            ARRAY_CSWITCH_STALL,
            LAST_SHIFT,
            LAST_WAIT,
            DONE
        };

        ctrl_state_t get_state() const { return state; }
        std::string state_name(ctrl_state_t s) const
        {
            switch (s)
            {
                case IDLE: return "IDLE";
                case START_FLAGS: return "START_FLAGS";
                case ARRAY_PREP: return "ARRAY_PREP";
                case ARRAY_FILL: return "ARRAY_FILL";
                case FIRST_SHIFT: return "FIRST_SHIFT";
                case START_COMP: return "START_COMP";
                case DRAIN_FEED: return "DRAIN_FEED";
                case ARRAY_FLUSH: return "ARRAY_FLUSH";
                case WAIT_CSWITCH: return "WAIT_CSWITCH";
                case WAIT_CSWITCH_STALL: return "WAIT_CSWITCH_STALL";
                case WAIT_OBUF: return "WAIT_OBUF";
                case WAIT_OBUF_STALL: return "WAIT_OBUF_STALL";
                case SCND_SHIFT: return "SCND_SHIFT";
                case SCND_SHIFT_STALL: return "SCND_SHIFT_STALL";
                case ALL_BUSY_SHIFT: return "ALL_BUSY_SHIFT";
                case ALL_BUSY: return "ALL_BUSY";
                case ARRAY_BUSY: return "ARRAY_BUSY";
                case OBUF_BUSY_SHIFT: return "OBUF_BUSY_SHIFT";
                case FORCE_STALL: return "FORCE_STALL";
                case OBUF_BUSY: return "OBUF_BUSY";
                case ARRAY_CSWITCH: return "ARRAY_CSWITCH";
                case ARRAY_CSWITCH_STALL: return "ARRAY_CSWITCH_STALL";
                case LAST_SHIFT: return "LAST_SHIFT";
                case LAST_WAIT: return "LAST_WAIT";
                case DONE: return "DONE";
                default: return "UNKNOWN";
            }
        }

        SC_CTOR(Control)
        {
            SC_METHOD(ctrl_process);
            sensitive << i_clk.pos();
        }

    private:

        ctrl_state_t state{IDLE};
        ctrl_state_t prev_state{IDLE};
        uint32_t dbg_cycle{0};
        uint32_t cycle_cnt{0};
        uint32_t comp_cycles{0};
        uint32_t shift_cycles{0};
        uint32_t drain_cycles{0};
        uint32_t flush_cycles{0};
        uint32_t fill_cycles{0};
        uint32_t context_cnt{0};

        void ctrl_process()
        {
            if (!i_rstn.read() || i_soft_reset.read())
            {
                state = IDLE;
                cycle_cnt = 0;
                comp_cycles = 0;
                shift_cycles = 0;
                context_cnt = 0;
                dbg_cycle = 0;
                drain_cycles = 0;
                flush_cycles = 0;

                o_act_feeder_en.write(false);
                o_act_feeder_clear.write(true);
                o_act_start.write(false);
                o_act_valid.write(false);
                o_act_finalpush.write(false);
                o_act_cnt_en.write(false);
                o_act_cnt_clear.write(true);
                o_act_clearfifo.write(true);
                o_act_pop_en.write(false);
                o_act_finalctx.write(false);

                o_wei_feeder_en.write(false);
                o_wei_feeder_clear.write(true);
                o_wei_start.write(false);
                o_wei_valid.write(false);
                o_wei_finalpush.write(false);
                o_wei_cnt_en.write(false);
                o_wei_cnt_clear.write(true);
                o_wei_clearfifo.write(true);
                o_wei_pop_en.write(false);
                o_wei_cswitch.write(false);

                o_outbuf_start.write(false);
                o_outbuf_reset.write(true);

                o_sa_clear.write(true);
                o_pipeline_en.write(false);
                o_cswitch_arr.write(sc_bv<X_DIM>(0));

                o_context_id.write(0);
                o_global_context_id.write(0);
                o_local_context_id.write(0);
                o_out_tile_id.write(0);

                o_done.write(false);
                o_feed_deadlock.write(false);

                prev_state = IDLE;
                return;
            }

            // Simple deadlock monitoring logic matching main_controller.sv
            bool act_deadlock = i_act_fifo_empty.read() && i_wei_fifo_full.read();
            bool wei_deadlock = i_act_fifo_full.read() && i_wei_fifo_empty.read();
            o_feed_deadlock.write(act_deadlock || wei_deadlock);

            // Fetch limits from registers
            uint32_t incntlim = i_mvm_k.read();
            if (incntlim == 0)
            {
                incntlim = i_incntlim.read();
            }

            if (incntlim == 0)
            {
                incntlim = 1;
            }
            uint32_t ncontexts = i_ncontexts.read();
            if (ncontexts == 0)
                ncontexts = 1;

            // total_contexts = ncontexts * output_tiles;
            // For 1-output-tile tests, total_contexts == ncontexts.
            // For current multi-output-tile test, ncontexts = 3, total_contexts = 6

            uint32_t total_contexts = i_total_contexts.read();
            if (total_contexts == 0)
                total_contexts = ncontexts;

            o_act_start.write(false);
            o_wei_start.write(false);

            o_act_cnt_clear.write(false);
            o_wei_cnt_clear.write(false);

            o_outbuf_start.write(false);
            o_cswitch_arr.write(sc_bv<X_DIM>(0));

            dbg_cycle++;

            // if ((dbg_cycle % 100) == 0)
            // {
            //     DBG_COUT << "[CTRL DEBUG] cycle=" << dbg_cycle
            //               << " state=" << state
            //               << " start=" << i_start.read()
            //               << " incntlim=" << i_incntlim.read()
            //               << " act_reps=" << i_act_reps.read()
            //               << " wei_reps=" << i_wei_reps.read()
            //               << "ncontexts=" << i_ncontexts.read()
            //               << "context_cnt=" << context_cnt
            //               << " act_cnt_en=" << o_act_cnt_en.read()
            //               << " wei_cnt_en=" << o_wei_cnt_en.read()
            //               << " act_pop_en=" << o_act_pop_en.read()
            //               << " wei_pop_en=" << o_wei_pop_en.read()
            //               << " psm_start=" << o_outbuf_start.read()
            //               << " shift_done=" << i_shift_done.read()
            //               << " done=" << o_done.read()
            //               << std::endl;
            // }

            switch (state)
            {
            case IDLE:
                o_done.write(false);

                o_sa_clear.write(false);

                o_act_feeder_en.write(false);
                o_wei_feeder_en.write(false);

                o_act_feeder_clear.write(false);
                o_act_clearfifo.write(false);

                o_wei_feeder_clear.write(false);
                o_wei_clearfifo.write(false);

                o_outbuf_reset.write(false);
                o_pipeline_en.write(false);

                o_act_cnt_en.write(false);
                o_wei_cnt_en.write(false);
                o_act_pop_en.write(false);
                o_wei_pop_en.write(false);

                if (i_start.read())
                {
                    context_cnt = 0;
                    cycle_cnt = 0;
                    comp_cycles = 0;
                    drain_cycles = 0;
                    shift_cycles = 0;

                    state = START_FLAGS;
                }
                break;

            case START_FLAGS:
            {
                // Enable feeders and pulse start for current context
                o_act_feeder_en.write(true);
                o_wei_feeder_en.write(true);

                o_act_start.write(true);
                o_wei_start.write(true);

                o_act_valid.write(true);
                o_wei_valid.write(true);

                // Pulse counter clear at the beginning of each context.
                // Feeders detect this as rising edge.
                o_act_cnt_clear.write(true);
                o_wei_cnt_clear.write(true);

                o_pipeline_en.write(true);

                uint32_t local_context = context_cnt % ncontexts;
                uint32_t out_tile = context_cnt / ncontexts;
                DBG_COUT << "[CTRL CONTEXT START]"
                          << " global_context = " << context_cnt
                          << " / " << total_contexts
                          << " local_context = " << local_context
                          << " out_tile = " << out_tile
                          << " ncontexts = " << ncontexts
                          << std::endl;

                // Send GLOBAL context id to feeders and PSM
                // For current test this must run 0 ... 5, not  0.. 2
                o_context_id.write(local_context);

                // Explicit IDs for correct routing
                o_global_context_id.write(context_cnt);
                o_local_context_id.write(local_context);
                o_out_tile_id.write(out_tile);
                state = ARRAY_PREP;
                break;
            }

            case ARRAY_PREP:
                o_act_cnt_clear.write(false);
                o_wei_cnt_clear.write(false);

                // Start issuing SRAM read requests for BOTH feeders, but do
                // NOT pop yet. Popping now would feed empty FIFOs (zeros) into
                // the array because data is still in flight through the SRAM
                // read latency + emit pipeline.
                o_act_cnt_en.write(true);
                o_wei_cnt_en.write(true);

                o_act_pop_en.write(false);
                o_wei_pop_en.write(false);

                comp_cycles = 0;
                fill_cycles = 0;
                state = ARRAY_FILL;
                break;

            case ARRAY_FILL:
                // Keep filling both FIFOs. cnt_en stays high so address
                // generation continues; pop stays disabled. After FILL_CYCLES
                // both feeders have real data queued and are phase-aligned,
                // because they share the same cnt_clear pulse and the same
                // fill window.
                o_act_cnt_en.write(true);
                o_wei_cnt_en.write(true);

                o_act_pop_en.write(false);
                o_wei_pop_en.write(false);

                o_pipeline_en.write(true);
                o_outbuf_start.write(false);
                o_cswitch_arr.write(sc_bv<X_DIM>(0));

                fill_cycles++;

                if (fill_cycles >= (uint32_t)FILL_CYCLES)
                {
                    comp_cycles = 0;
                    state = START_COMP;

                    DBG_COUT << "[CTRL FILL DONE]"
                              << " context=" << context_cnt
                              << " fill_cycles=" << fill_cycles
                              << " (FILL_CYCLES=" << FILL_CYCLES << ")"
                              << std::endl;
                }
                break;

            case START_COMP:
            {
                bool feeder_stall = i_act_stall.read() || i_wei_stall.read();

                o_act_feeder_en.write(true);
                o_wei_feeder_en.write(true);

                // Vẫn cho feeder tiếp tục đọc/prefetch SRAM.
                o_act_cnt_en.write(true);
                o_wei_cnt_en.write(true);

                if (feeder_stall)
                {
                    // Không pop vector rỗng vào array.
                    // Không tăng comp_cycles.
                    o_act_pop_en.write(false);
                    o_wei_pop_en.write(false);

                    if ((dbg_cycle % 64) == 0)
                    {
                        DBG_COUT << "[CTRL STALL]"
                                  << " cycle=" << dbg_cycle
                                  << " comp_cycles=" << comp_cycles
                                  << " act_stall=" << i_act_stall.read()
                                  << " wei_stall=" << i_wei_stall.read()
                                  << std::endl;
                    }

                    break;
                }

                o_act_pop_en.write(true);
                o_wei_pop_en.write(true);

                comp_cycles++;

                if (comp_cycles >= incntlim)
                {
                    drain_cycles = 0;
                    state = DRAIN_FEED;

                    DBG_COUT << "[CTRL DRAIN START]"
                              << " cycle=" << dbg_cycle
                              << " comp_cycles=" << comp_cycles
                              << " incntlim=" << incntlim
                              << std::endl;
                }

                break;
            }

            case DRAIN_FEED:
            {
                // ---------------------------------------------------------
                // Physical systolic tail drain.
                //
                // START_COMP has popped exactly K logical vectors.
                // However the actual SA input stream is skewed:
                //   - A lanes are delayed by y
                //   - B lanes are delayed by x
                //   - PE multiply has pipeline latency
                //
                // Therefore after K logical pops, we must keep pop_en=1 for
                // tail cycles so delayed A/B values can still reach all PEs.
                //
                // IMPORTANT:
                // Do NOT insert pop_en=0 bubbles here. That breaks A/B timing.
                // ---------------------------------------------------------

                o_act_feeder_en.write(true);
                o_wei_feeder_en.write(true);

                // No new SRAM requests.
                o_act_cnt_en.write(false);
                o_wei_cnt_en.write(false);

                // Keep draining physical skew/tail from feeders into SA.
                o_act_pop_en.write(true);
                o_wei_pop_en.write(true);

                o_pipeline_en.write(true);
                o_outbuf_start.write(false);
                o_cswitch_arr.write(sc_bv<X_DIM>(0));

                drain_cycles++;

                // Correctness-first conservative tail.
                // PE_LAT is already X_DIM + Y_DIM in your template default.
                // Add extra margin for multiplier pipeline and SystemC clocking.
                const uint32_t tail_limit =
                    static_cast<uint32_t>(X_DIM + Y_DIM + PE_LAT + 8);

                if ((drain_cycles % 8) == 0)
                {
                    DBG_COUT << "[CTRL TAIL DRAIN]"
                              << " cycle=" << dbg_cycle
                              << " drain_cycles=" << drain_cycles
                              << " limit=" << tail_limit
                              << std::endl;
                }

                if (drain_cycles >= tail_limit)
                {
                    state = ARRAY_CSWITCH;
                    cycle_cnt = 0;

                    DBG_COUT << "[CTRL TAIL DRAIN DONE]"
                              << " cycle=" << dbg_cycle
                              << " drain_cycles=" << drain_cycles
                              << std::endl;
                }

                break;
            }

            // case ARRAY_FLUSH:
            // {
            //     // Feeder đã pop đủ vector thật.
            //     // Bây giờ đưa zero vào array thêm vài cycle để các MAC cuối lan hết.
            //     o_act_feeder_en.write(true);
            //     o_wei_feeder_en.write(true);

            //     o_act_cnt_en.write(false);
            //     o_wei_cnt_en.write(false);

            //     o_act_pop_en.write(true);
            //     o_wei_pop_en.write(true);

            //     o_pipeline_en.write(true);
            //     o_outbuf_start.write(false);
            //     o_cswitch_arr.write(sc_bv<X_DIM>(0));

            //     flush_cycles++;

            //     // Need X_DIM + Y_DIM cycles so the LAST reduction term can
            //     // propagate all the way down the anti-diagonal of the array
            //     // (A travels X_DIM columns, B travels Y_DIM rows). Using only
            //     // X_DIM truncates accumulation for high-y / high-x PEs.
            //     // uint32_t flush_limit = PE_LAT;

            //     uint32_t flush_limit = PE_LAT;
            //     if ((flush_cycles % 4) == 0)
            //     {
            //         DBG_COUT << "[CTRL ARRAY FLUSH]"
            //                   << " cycle=" << dbg_cycle
            //                   << " flush_cycles=" << flush_cycles
            //                   << " limit=" << flush_limit
            //                   << std::endl;
            //     }

            //     if (flush_cycles >= flush_limit)
            //     {
            //         state = ARRAY_CSWITCH;

            //         DBG_COUT << "[CTRL ARRAY FLUSH DONE]"
            //                   << " cycle=" << dbg_cycle
            //                   << " flush_cycles=" << flush_cycles
            //                   << std::endl;
            //     }
            //     // if (flush_cycles >= 128)
            //     // {
            //     //     state = ARRAY_CSWITCH;

            //     //     DBG_COUT << "[CTRL ARRAY FLUSH FIXED DONE]"
            //     //               << " cycle=" << dbg_cycle
            //     //               << " flush_cycles=" << flush_cycles
            //     //               << std::endl;
            //     // }

            //     break;
            // }
            case ARRAY_CSWITCH:
                // Pulse accumulator Context Switch to grid elements
                o_cswitch_arr.write(sc_bv<X_DIM>(~0)); // Write 1s to swap context
                o_act_pop_en.write(false);
                o_wei_pop_en.write(false);
                o_act_cnt_en.write(false);
                o_wei_cnt_en.write(false);
                cycle_cnt = 0;
                state = WAIT_CSWITCH;
                break;

            case WAIT_CSWITCH:
            {
                o_cswitch_arr.write(sc_bv<X_DIM>(0));

                // Keep pipeline active while local delayed cswitch propagates.
                o_pipeline_en.write(true);

                o_act_pop_en.write(false);
                o_wei_pop_en.write(false);
                o_act_cnt_en.write(false);
                o_wei_cnt_en.write(false);

                cycle_cnt++;

                // SA local cswitch is delayed by roughly:
                //   y + x + 1 + mul_lat + CSWITCH_EXTRA_MARGIN + extra_csreg
                //
                // Use conservative wait for correctness-first model.
                const uint32_t cswitch_wait_limit =
                    static_cast<uint32_t>(X_DIM + Y_DIM + PE_LAT + 16);

                if ((cycle_cnt % 8) == 0)
                {
                    DBG_COUT << "[CTRL WAIT CSWITCH]"
                              << " cycle=" << dbg_cycle
                              << " wait=" << cycle_cnt
                              << " limit=" << cswitch_wait_limit
                              << std::endl;
                }

                if (cycle_cnt >= cswitch_wait_limit)
                {
                    state = WAIT_OBUF;

                    DBG_COUT << "[CTRL WAIT CSWITCH DONE]"
                              << " cycle=" << dbg_cycle
                              << " wait=" << cycle_cnt
                              << std::endl;
                }

                break;
            }

            case WAIT_OBUF:
                // Start PSM collection for this context
                o_outbuf_start.write(true);
                shift_cycles = 0;
                state = LAST_WAIT;
                break;

            case LAST_WAIT:
                shift_cycles++;

                // Wait for PSM/scan-chain to complete one context.
                // Use either PSM shift_done or conservative fixed wait.
                if (i_shift_done.read() || shift_cycles >= (uint32_t)(PE_LAT + 8))
                {
                    if ((context_cnt + 1) < total_contexts)
                    {
                        context_cnt++;

                        // Prepare next global context.

                        cycle_cnt = 0;
                        comp_cycles = 0;
                        shift_cycles = 0;
                        drain_cycles = 0;
                        flush_cycles = 0;
                        fill_cycles = 0;

                        state = START_FLAGS;
                    }
                    else
                    {
                        state = DONE;
                    }
                }
                break;

            case DONE:
                o_done.write(true);

                o_act_feeder_en.write(false);
                o_wei_feeder_en.write(false);

                o_act_cnt_en.write(false);
                o_wei_cnt_en.write(false);

                o_act_pop_en.write(false);
                o_wei_pop_en.write(false);

                o_pipeline_en.write(false);

                DBG_COUT << "[CTRL DONE] "
                          << " completed_global_contexts = " << total_contexts
                          << " ncontexts_per_tile = " << ncontexts
                          << std::endl;

                o_context_id.write(0);
                o_global_context_id.write(0);
                o_local_context_id.write(0);
                o_out_tile_id.write(0);
                state = IDLE;
                break;

            default:
                state = IDLE;
                break;
            }

            // if (state != prev_state)
            // {
            //     DBG_COUT << "[CTRL STATE] "
            //               << prev_state
            //               << " -> "
            //               << state
            //               << " at cycle "
            //               << dbg_cycle
            //               << std::endl;
            //     prev_state = state;
            // }
        }
    };

} // namespace sauria

#endif // SAURIA_MAIN_CONTROLLER_H
