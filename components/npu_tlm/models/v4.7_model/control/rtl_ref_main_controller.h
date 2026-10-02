// SystemC Model for SAURIA NPU Core
// Controller FSM Block (Context FSM, Feeders FSM, Main Controller)

#ifndef SAURIA_RTL_MAIN_CONTROLLER_H
#define SAURIA_RTL_MAIN_CONTROLLER_H

#include <functional>
#include "sauria_types.h"
#include <map>
#include <sstream>
#include <fstream>
#include "debug.h"
#ifndef FX1_NO_PERF
#include "instrumentation/rtl_ref_perf_counters.h"
#endif
#ifdef FX1_A3_CONTEXT_FSM
#include "control/rtl_ref_context_fsm.h"
#include "control/rtl_ref_context_switch_controller.h"
#endif
#ifdef FX1_A3_FEEDERS_FSM
#if !defined(FX1_A3_CONTEXT_FSM)
#error "FX1_A3_FEEDERS_FSM requires FX1_A3_CONTEXT_FSM (it replaces wiring that only exists on that path)"
#endif
#include "control/rtl_ref_feeders_fsm.h"
#endif

namespace sauria_rtl
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
        // high while ContextFsm is in OBUF_BUSY_SHIFT -- the RTL's "soft-stall": the array keeps running but the operands
        // fed in must be ZERO.
        sc_out<bool> o_softstall{"o_softstall"};

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

#ifndef FX1_NO_PERF
        // Pha B: set by NpuTop::attach_perf. Per-tick, the FSM state histogram in
        // this counter is bumped (see ctrl_process). Non-owning; may stay null.
        sauria_rtl::PerfCounters *perf{nullptr};
#endif

        SC_CTOR(Control)
        {
#ifdef FX1_A3_SEAM_ORDER
            // npu_top calls seam_step() in order; no process of its own is registered.
#else
            SC_METHOD(ctrl_process);
            sensitive << i_clk.pos();
#endif
        }

    private:
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

#ifdef FX1_A3_CONTEXT_FSM
        // Full 1:1 ContextFsm drives control flow in
        // place of the switch(state) below. ctx_prev_status tracks the FSM's
        // own ctx_status across ticks so this wiring layer can detect
        // context-boundary edges itself -- context_fsm.sv does NOT expose that
        // boundary as a signal (its real per-context addr-counter reset lives
        // inside the feeders' own self-wrapping counter cascade, not as an
        // explicit clear pulse -- see context_fsm.h header note lines 43-51).
        sauria_rtl::ContextFsm<X_DIM, Y_DIM> ctx_fsm;
        // Real per-context cdone/cswitch_done/cswitch_arr generator --
        // context_fsm.sv does NOT compute these itself; see
        // control/rtl_ref_context_switch_controller.h's header comment for the RTL
        // wiring reference (main_controller.sv). The staggered cswitch_arr (cscnt_q == PE_LAT-EXTRA_CSREG+i per
        // column i) carries the column propagation; the array's per-(y,x) delay adds the row propagation, so the
        // farthest-PE total matches ContextSwitchController's o_cswitch_done threshold (PE_LAT+X+Y-1).
        // FX1_A3_EXTRA_CSREG normally comes from rtl_ref_defaults.h (RTL value 1); the fallback below is 0.
#ifndef FX1_A3_EXTRA_CSREG
#define FX1_A3_EXTRA_CSREG 0
#endif
        sauria_rtl::ContextSwitchController<X_DIM, Y_DIM, /*PE_LAT=*/2, /*EXTRA_CSREG=*/FX1_A3_EXTRA_CSREG> cs_ctrl;
        uint32_t ctx_prev_status{0};
    public:
        // Debug hooks (read-only, no effect on behaviour): ctx_status / feeders state for external tracing.
        uint32_t dbg_ctx_prev_status() const { return ctx_prev_status; }
        uint32_t dbg_context_cnt() const { return context_cnt; }
#ifdef FX1_A3_FEEDERS_FSM
        int dbg_feeders_state() const { return feeders_fsm.dbg_state(); }
        bool dbg_act_hold() const { return feeders_fsm.dbg_act_hold(); }
        bool dbg_wei_hold() const { return feeders_fsm.dbg_wei_hold(); }
#endif
        // Debug hook (read-only): ContextFsm's main_state_ pass-through.
        int dbg_main_state() const { return ctx_fsm.dbg_main_state(); }
    private:

        // Unused delay line (computed, result discarded): delays cs_out.cswitch_done by a fixed 6 cycles. Kept only as a
        // delay primitive; delaying cswitch_done also changes how many real pop / feed cycles happen, so it is not
        // applied. Candidate for removal (code change, needs its own regression).
        static constexpr int CSWITCH_DONE_SETTLE_DELAY = 6;
        bool cswitch_done_delay_[CSWITCH_DONE_SETTLE_DELAY]{};

        // "Drain window" -- used only when FX1_A3_FEEDERS_FSM is OFF (default ON: the real feeders_fsm tail replaces it).
        // ContextFsm closes pop_gate right after cs_out.cdone first asserts; the feeders' skew registers (row y needs y
        // extra shift cycles, column x needs x) need i_pop_en held a little longer to FLUSH already-popped tail values.
        // On cs_out.cdone's rising edge a (X_DIM-1)+(Y_DIM-1)-cycle countdown starts; while it runs, the FEEDER-facing pop enable
        // high (pop_now_for_feeders) even though ctx_out.pop_gate itself may
        // have already gone low. Deliberately does NOT touch what feeds
        // ContextSwitchController's own K-counter (cs_in.wei_pop_en/
        // act_pop_en stay driven by the ORIGINAL, unextended pop_now) --
        // extending that too could push its incnt_q_ past where cdone logic
        // expects for a genuine multi-context run. This is a wiring-layer-
        // only patch; context_fsm.h's ported RTL state machine itself is NOT
        // modified.
        uint32_t pop_drain_cnt_{0};
        bool cdone_prev_for_drain_{false};

#ifdef FX1_A3_FEEDERS_FSM
        // the THIRD sibling module of
        // main_controller.sv, ported at last. Root cause of Gap A2: with
        // it missing, FX1_A3_CONTEXT_FSM moved the context BOUNDARY onto real
        // RTL timing while the data FEED stayed on the hand-tuned approximation
        // below (pop_now + the drain window), leaving the two halves 36
        // feed-cycles apart. See control/feeders_fsm.h's header for the full
        // signal inventory this module takes over.
        //
        // FIFO positions = 16, NOT the RTL module default of 8: npu_top.h
        // instantiates both feeders with FIFO_DEPTH=16 (npu_top.h:38/125/126),
        // and this parameter only feeds FIFO_MAX_POS, i.e. how long
        // FIFO_EMPTYING drains before FINISH. Using 8 here would cut that drain
        // short by 8 cycles against the model's real FIFO depth.
        static constexpr int FEEDER_FIFO_POS = 16;
        sauria_rtl::FeedersFsm<X_DIM, Y_DIM, FEEDER_FIFO_POS, FEEDER_FIFO_POS> feeders_fsm;
        // set on a context boundary, consumed the following cycle to raise
        // feeders_start after the boundary's fsm_reset pulse has landed.
        bool feeders_rearm_pending_{false};

    public:
        // for npu_top to call in sequence (see FX1_A3_SEAM_ORDER).
        void seam_step()
        {
#ifdef FX1_A3_ORDER_PROBE
            fx1a3::order_ctrl() = ++fx1a3::order_seq();
            fx1a3::ctrl_tsim() = sc_core::sc_time_stamp().value();
#endif
            ctrl_process();
        }


    private:
        bool seam_order_pad_unused_{false};

    public:
        // Set by the top level (npu_top / RtlRefLaneACoreA). Only read under FX1_A3_EMPTY_DIRECT; nullptr otherwise.
        std::function<bool()> peek_act_empty_{};
        // FIFO FULL flag of THIS cycle. RTL feeders_fsm.sv reads it combinationally in both state_transitions and
        // output_logic; through sc_signal it would be one cycle late.
        std::function<bool()> peek_act_full_{};
        std::function<bool()> peek_wei_full_{};
        std::function<bool()> peek_act_stall_{};
        std::function<bool()> peek_wei_stall_{};
        std::function<bool()> peek_wei_empty_{};
#ifdef FX1_A3_TILDONE_Q_GATE
        // RTL o_til_done of THIS cycle from the ifmap feeder, and the value the
        // feeder computed inside its previous tick (cross-check only).
        std::function<bool()> peek_act_til_done_rtl_{};
        std::function<bool()> peek_act_til_done_rtl_last_{};
        bool tdq_gate_prev_{false};
        bool tdq_gate_have_{false};
        uint64_t tdq_gate_pulses_{0};
        uint64_t tdq_gate_mis_{0};
        uint64_t tdq_gate_cyc_{0};
#endif

    private:
#endif
#endif
    public:
        // shadows of o_*_cnt_en. The RTL wires them combinationally (feeders_fsm.sv:193
        // assign o_act_cnt_en = act_cnt_en & !act_cnt_hold_q); an sc_signal between two SC_METHODs is one beat late.
        // With FX1_A3_SEAM_ORDER Control runs BEFORE the feeders, so the shadow holds THIS cycle's value.
        bool seam_act_cnt_en_{false};
        bool seam_wei_cnt_en_{false};
        bool seam_pipeline_en_{false};
        bool seam_outbuf_start_{false};
        // o_outbuf_start of THIS cycle (from two pre-tick ContextFsm REGISTERS), read directly by Psm; the RTL wires it
        // combinationally (sauria_logic.sv:305/459). Outside every guard: only called under FX1_A3_PSM_START_DIRECT, but
        // the function must exist in every configuration.
#ifdef FX1_A3_CONTEXT_FSM
        bool peek_outbuf_start_now() const { return ctx_fsm.peek_outbuf_start(); }
#endif   // `ctx_fsm` only exists under FX1_A3_CONTEXT_FSM
        // The whole `always_comb begin: output_logic` block of feeders_fsm.sv is combinational (0 delay in the RTL),
        // including o_act_valid / o_wei_valid (feeders_fsm.sv:553/562). Through sc_signal they would arrive one beat late
        // (valid_data_q1 rises late and element k = 0 is lost), so they are mirrored here for the feeders to read
        // directly. Field indices: see seam_fd() below.
        //   0 feeder_en  1 feeder_clear  2 clearfifo  3 finalpush
        //   4 cnt_clear  5 pop_en        6 cnt_en     7 cswitch(wei)/finalctx(act)
        bool seam_fd_a_[8]{};
        bool seam_fd_w_[8]{};
        bool seam_fd(bool wei, int i) const { return wei ? seam_fd_w_[i] : seam_fd_a_[i]; }
        void seam_set_fd_(const bool a[8], const bool w[8])
        {
            for (int i = 0; i < 8; i++) { seam_fd_a_[i] = a[i]; seam_fd_w_[i] = w[i]; }
        }
        bool seam_act_valid_{false};
#ifdef FX1_A3_START_DIRECT
        // `o_act_start` / `o_wei_start` are combinational in the RTL (feeders_fsm.sv output_logic, same block as
        // `o_act_valid`); copies for the feeders to read directly, not through sc_signal.
        bool seam_act_start_{false};
        bool seam_wei_start_{false};
#endif
        bool seam_wei_valid_{false};
        void seam_set_act_valid_(bool v) { o_act_valid.write(v); seam_act_valid_ = v; }
#ifdef FX1_A3_START_DIRECT
        void seam_set_act_start_(bool v) { o_act_start.write(v); seam_act_start_ = v; }
        void seam_set_wei_start_(bool v) { o_wei_start.write(v); seam_wei_start_ = v; }
#else
        void seam_set_act_start_(bool v) { o_act_start.write(v); }
        void seam_set_wei_start_(bool v) { o_wei_start.write(v); }
#endif
        void seam_set_wei_valid_(bool v) { o_wei_valid.write(v); seam_wei_valid_ = v; }
        void seam_set_pipeline_en_(bool v) { o_pipeline_en.write(v); seam_pipeline_en_ = v; }
        // shadow of o_outbuf_start (combinational to the PSM in the RTL, sauria_logic.sv:305/459; an sc_signal is one
        // cycle late). Written together with the port so Psm reads THIS cycle's value.
        void seam_set_outbuf_start_(bool v) { o_outbuf_start.write(v); seam_outbuf_start_ = v; }
        void seam_set_act_cnt_en_(bool v) { o_act_cnt_en.write(v); seam_act_cnt_en_ = v; }
        void seam_set_wei_cnt_en_(bool v) { o_wei_cnt_en.write(v); seam_wei_cnt_en_ = v; }

    private:

#ifdef FX1_A3_FSM_TAPE
        // Debug hook (default off): one line per cycle, NpuTop_std instance only.
        void fsm_tape_row(bool rst, int start, int outbuf_done, int shift_done,
                          int finalwrite, int feeders_done, int pipe_en,
                          int act_pop, int wei_pop, unsigned incntlim,
                          unsigned ctx_status, int pop_gate, int pipe_gate,
                          int cdone, int cswitch_done, unsigned cswitch_arr,
                          unsigned cs_incnt, int cs_hold, int cs_clear,
                          int cs_cswitch_en, int cs_cswitch_force,
                          int act_cnt_en, int wei_cnt_en,
                          int obstart)   // o_outbuf_start IN PHASE
        {
            static std::ofstream tp("trace_sysc/fsm_tape.csv");
            static bool hdr = false;
            static uint64_t tcyc = 0;
            if (!hdr)
            {
                tp << "tsim,cyc,rst,start,outbuf_done,shift_done,finalwrite,"
                      "feeders_done,pipe_en,act_pop,wei_pop,incntlim,"
                      "ctx_status,pop_gate,pipe_gate,cdone,cswitch_done,"
                      "cswitch_arr,cs_incnt,cs_hold,cs_clear,cs_cswitch_en,"
                      "cs_cswitch_force,act_cnt_en,wei_cnt_en,outbuf_start\n";
                hdr = true;
            }
            tp << sc_core::sc_time_stamp().value() << ","
               << tcyc++ << "," << (int)rst << "," << start << ","
               << outbuf_done << "," << shift_done << "," << finalwrite << ","
               << feeders_done << "," << pipe_en << "," << act_pop << ","
               << wei_pop << "," << incntlim << "," << ctx_status << ","
               << pop_gate << "," << pipe_gate << "," << cdone << ","
               << cswitch_done << "," << cswitch_arr << "," << cs_incnt << ","
               << cs_hold << "," << cs_clear << "," << cs_cswitch_en << ","
               << cs_cswitch_force << "," << act_cnt_en << ","
               << wei_cnt_en << "," << obstart << "\n";
        }
#endif

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
                seam_set_act_start_(false);
                seam_set_act_valid_(false);
                o_act_finalpush.write(false);
                seam_set_act_cnt_en_(false);
                o_act_cnt_clear.write(true);
                o_act_clearfifo.write(true);
                o_act_pop_en.write(false);
                o_act_finalctx.write(false);

                o_wei_feeder_en.write(false);
                o_wei_feeder_clear.write(true);
                seam_set_wei_start_(false);
                seam_set_wei_valid_(false);
                o_wei_finalpush.write(false);
                seam_set_wei_cnt_en_(false);
                o_wei_cnt_clear.write(true);
                o_wei_clearfifo.write(true);
                o_wei_pop_en.write(false);
                o_wei_cswitch.write(false);

                seam_set_outbuf_start_(false);
                o_outbuf_reset.write(true);

                o_sa_clear.write(true);
                seam_set_pipeline_en_(false);
                o_cswitch_arr.write(sc_bv<X_DIM>(0));

                o_context_id.write(0);
                o_global_context_id.write(0);
                o_local_context_id.write(0);
                o_out_tile_id.write(0);

                o_done.write(false);
                o_feed_deadlock.write(false);

                prev_state = IDLE;

#if defined(FX1_A3_FSM_TAPE) && defined(FX1_A3_CONTEXT_FSM)
                // Debug hook (default off): input tape -- a line with rst = 1 tells an isolated model to call reset() on
                // that beat.
                if (std::string(this->name()).find("NpuTop_std") !=
                    std::string::npos)
                {
                    fsm_tape_row(true, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
                }
#endif

#ifdef FX1_A3_CONTEXT_FSM
                ctx_fsm.reset();
                cs_ctrl.reset();
                ctx_prev_status = 0;
                for (auto &d : cswitch_done_delay_)
                    d = false;
                pop_drain_cnt_ = 0;
                cdone_prev_for_drain_ = false;
#ifdef FX1_A3_FEEDERS_FSM
                feeders_fsm.reset();
                feeders_rearm_pending_ = false;
#endif
#endif
                return;
            }

            // Simple deadlock monitoring logic matching main_controller.sv
            bool act_deadlock = i_act_fifo_empty.read() && i_wei_fifo_full.read();
            bool wei_deadlock = i_act_fifo_full.read() && i_wei_fifo_empty.read();
            o_feed_deadlock.write(act_deadlock || wei_deadlock);

            // Fetch limits from registers
#ifdef FX1_A3_INCNTLIM_FIX
            // RTL feeds the CSC's i_incntlim straight from
            // config_regs.o_incntlim, with no mvm_k in between. This model had
            // the priority inverted, so CON.INCNTLIM=783 was never read even
            // though both the write and the wiring were correct.
            uint32_t incntlim = i_incntlim.read();
            if (incntlim == 0)
            {
                incntlim = i_mvm_k.read();
            }
#else
            uint32_t incntlim = i_mvm_k.read();
            if (incntlim == 0)
            {
                incntlim = i_incntlim.read();
            }
#endif

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

            seam_set_act_start_(false);
            seam_set_wei_start_(false);

            o_act_cnt_clear.write(false);
            o_wei_cnt_clear.write(false);

            seam_set_outbuf_start_(false);
            o_cswitch_arr.write(sc_bv<X_DIM>(0));

            dbg_cycle++;

#if !defined(FX1_A3_CTRL_HISTOGRAM_NO_PERF) && !defined(FX1_A3_CONTEXT_FSM)
            // Pha B: attribute this clock to the FSM state active during it. RAW
            // histogram; tools/ Python folds states into stage buckets. Additive
            // only -- never affects control flow. (Under FX1_A3_CONTEXT_FSM the
            // equivalent bump uses ctx_out.ctx_status instead -- see below --
            // since `state` is never advanced by that path.)
            if (perf && (int)state < sauria_rtl::PerfCounters::NUM_CTRL_STATES)
                perf->ctrl_state_cycles[(int)state]++;
#endif

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

#ifdef FX1_A3_CONTEXT_FSM
            // -----------------------------------------------------------------
            // Full 1:1 ContextFsm drives
            // control flow. Maps ContextFsm::Outputs onto the SAME sc_out ports
            // the old switch(state) below drives, per the RTL mapping notes in
            // control/rtl_ref_context_fsm.h. Do not re-derive the signal mapping without
            // re-reading that header comment.
            // -----------------------------------------------------------------
            {
                bool feeder_stall = i_act_stall.read() || i_wei_stall.read();

                typename sauria_rtl::ContextFsm<X_DIM, Y_DIM>::Inputs ctx_in;
                ctx_in.soft_reset = false; // handled by the top-of-method reset branch
                ctx_in.start = i_start.read();
                // NOTE: Control's OWN i_outbuf_done
                // port is ALREADY wired (npu_top.h: ctrl_inst->i_outbuf_done
                // (s_psm_done), fed by psm_inst->o_done -- a REAL, separate
                // signal from i_shift_done (fed by psm_inst->o_shift_done). An
                // earlier version of this wiring used i_shift_done for BOTH,
                // which was harmless ONLY because the pre-existing (guard-off)
                // psm_top.h happens to assert o_shift_done and o_done together
                // in the same "done" block. Once psm_top.h's real PsmShiftFsm
                // port is wired in (o_shift_done becomes a broad "still active"
                // signal, no longer equal to o_done), this distinction matters
                // -- use the REAL, already-available i_outbuf_done port instead.
                ctx_in.outbuf_done = i_outbuf_done.read();
                ctx_in.shift_done = i_shift_done.read();
#ifdef FX1_A3_FINALWRITE_PORT
                // In the RTL finalwrite is a PSM OUTPUT (psm_shift_fsm.sv:191 assign o_finalwrite = (ctx_cnt ==
                // (i_ncontexts+2))), passed straight into context_fsm (main_controller.sv:130). ctx_cnt counts contexts on
                // the DRAIN side, two contexts behind the compute-side context_cnt below. i_finalwrite is wired in npu_top.h
                // (psm o_finalwrite -> s_psm_finalwrite -> ctrl i_finalwrite).
                // Only meaningful with FX1_A3_PSM_SHIFT_FSM (the old PSM path keeps o_finalwrite = false).
                ctx_in.finalwrite = i_finalwrite.read();
#elif defined(FX1_A3_FINALWRITE_PERTILE)
                // context_cnt resets to 0 EVERY tile, so it runs 0..ncontexts-1; comparing it with total_contexts
                // (= ncontexts x output tiles) would never be true with > 1 output tile. Per tile, compare with ncontexts.
                ctx_in.finalwrite = ((context_cnt + 1) >= ncontexts);
#else
                ctx_in.finalwrite = ((context_cnt + 1) >= total_contexts);
#endif
                ctx_in.feeders_done = true;
                ctx_in.pipeline_en = !feeder_stall;
                // ctx_in.cdone / ctx_in.cswitch_done filled in below, AFTER
                // running ContextSwitchController with THIS tick's cswitch_en/
                // cswitch_force (compute_outputs()/commit() split exists
                // exactly for this -- see context_fsm.h).

                auto ctx_out = ctx_fsm.compute_outputs(ctx_in);

                bool pop_now = ctx_out.pop_gate && !feeder_stall;

#ifdef FX1_A3_FEEDERS_FSM
                // ---------------------------------------------------------
                // real feeders_fsm.sv, settled BETWEEN
                // ContextFsm's combinational outputs and everything that
                // consumes pop_en/pipeline_en. Order is forced by the RTL
                // topology (main_controller.sv): pipeline_gate/pop_gate come
                // OUT of context_fsm and go INTO feeders_fsm, whose
                // o_pipeline_en then goes BACK into both siblings.
                //
                // Verified there is no combinational loop before relying on
                // this order: context_fsm.h's compute_outputs() never reads
                // in.pipeline_en (only its commit() does, context_fsm.h:681)
                // and out.pipeline_gate is a pure registered-state function
                // (context_fsm.h:625).
                // ---------------------------------------------------------
                // Arming. The RTL runs feeders_fsm ONCE per convolution and rolls the contexts inside it with the
                // repetition counters. Without FX1_A3_NO_CTX_REARM the FSM is instead re-armed at every context boundary
                // (the same edge as the feeders' cnt_clear): fsm_reset pulses on the edge (-> IDLE), feeders_start is held
                // the next cycle (-> CNT_START), because the transition logic checks fsm_reset FIRST and would swallow a
                // same-cycle start.
#ifdef FX1_A3_NO_CTX_REARM
                // Default (FX1_A3_NO_CTX_REARM): no re-arm, as the RTL -- feeders_fsm runs ONCE per convolution and the
                // tiling counters accumulate across contexts (a re-arm would clear the counter cascade long before the
                // til_y loop completes). til_done then rises after all (til_x, til_y) pairs have been swept.
                const bool fd_ctx_edge = false;
#else
                const bool fd_edge_first =
                    ((ctx_out.ctx_status == 1) && (ctx_prev_status != 1));
                const bool fd_edge_next =
                    ((ctx_prev_status == 17 || ctx_prev_status == 18) &&
                     (ctx_out.ctx_status == 11 || ctx_out.ctx_status == 14));
#ifdef FX1_A3_REARM_PER_TILE
                // re-arm ONCE PER OUTPUT TILE, not per context.
                // chose the context boundary; measured the cost: the
                // feeder then re-sweeps its whole tiling range once per context
                // (319 arms instead of 32 contexts' worth of work), which is the
                // single number behind all three symptoms (read/address 39.87 vs
                // 4.00, cdone 319 vs 32, dumps/context 40 vs 4 -- all ratio 9.97).
                // Real RTL arms feeders_fsm once per convolution and rolls the
                // contexts INSIDE it; this model cannot do that (ContextFsm owns
                // the context loop), but the tile boundary is the closest legal
                // arm point: each output tile preloads its own SRAM contents, so
                // the feeder genuinely must restart there and only there.
                //
                // context_cnt still holds the PREVIOUS context index here (it is
                // incremented ~260 lines below), so the context being ENTERED is
                // context_cnt+1 on fd_edge_next and 0 on fd_edge_first. Since
                // out_tile = ctx / ncontexts, a tile boundary is exactly
                // (entered_ctx % ncontexts) == 0. ncontexts is forced >= 1 above.
                const bool fd_ctx_edge =
                    fd_edge_first ||
                    (fd_edge_next && (((context_cnt + 1) % ncontexts) == 0));
#else
                const bool fd_ctx_edge = fd_edge_first || fd_edge_next;
#endif
#endif

                typename sauria_rtl::FeedersFsm<X_DIM, Y_DIM, FEEDER_FIFO_POS, FEEDER_FIFO_POS>::Inputs fd_in;
                fd_in.pipeline_gate = ctx_out.pipeline_gate;
                fd_in.feeders_start = ctx_out.feeders_start || feeders_rearm_pending_;
                fd_in.fsm_reset = ctx_out.feeders_reset || fd_ctx_edge; // soft_reset handled by the top-level reset branch
                fd_in.pop_gate = ctx_out.pop_gate;
                fd_in.act_reps = i_act_reps.read();
                fd_in.wei_reps = i_wei_reps.read();
                fd_in.act_done = i_act_done.read(); // [UNUSED in RTL, kept for interface fidelity]
#ifdef FX1_A3_TILDONE_Q_GATE
                // RTL feeders_fsm.sv:203 counts i_act_til_done = ifmap_idxcnt.sv:274
                // til_done_q & i_cnt_en of THIS cycle. The sc_signal below carries the
                // feeder's post-commit value gated by LAST cycle's cnt_en, so a stall
                // right after til_done lets the pulse reach the repetition counter and
                // the shim while RTL still holds it. Read the RTL value directly
                // (Control runs before the feeder in NativeLaneACoreA tick order).
                if (peek_act_til_done_rtl_)
                {
                    if (tdq_gate_have_ && peek_act_til_done_rtl_last_)
                    {
                        const bool actual = peek_act_til_done_rtl_last_();
                        if (actual != tdq_gate_prev_ && tdq_gate_mis_++ < 20)
                            std::cerr << "[TDQ_GATE_MISMATCH] " << this->name()
                                      << " cyc=" << tdq_gate_cyc_ << " peek=" << tdq_gate_prev_
                                      << " feeder=" << actual << "\n";
                    }
                    fd_in.act_til_done = peek_act_til_done_rtl_();
                    tdq_gate_prev_ = fd_in.act_til_done;
                    tdq_gate_have_ = true;
                    if (fd_in.act_til_done &&
                        (++tdq_gate_pulses_ <= 8 || (tdq_gate_pulses_ & 255) == 0))
                        std::cerr << "[TDQ_GATE] " << this->name() << " cyc=" << tdq_gate_cyc_
                                  << " pulses=" << tdq_gate_pulses_
                                  << " mismatches=" << tdq_gate_mis_ << "\n";
                }
                else
                {
                    fd_in.act_til_done = i_act_til_done.read();
                }
                tdq_gate_cyc_++;
#else
                fd_in.act_til_done = i_act_til_done.read();
#endif
#ifdef FX1_A3_EMPTY_DIRECT
                // read THIS cycle's flag directly. sc_signal is one beat late because both SC_METHODs are sensitive to
                // i_clk.pos(); the RTL is combinational (feeders_fsm sees o_fifo_empty in the same cycle).
                fd_in.act_fifo_empty = peek_act_empty_ ? peek_act_empty_()
                                                       : i_act_fifo_empty.read();
#else
                fd_in.act_fifo_empty = i_act_fifo_empty.read();
#endif
                fd_in.act_fifo_full = i_act_fifo_full.read();
                fd_in.act_fifo_full_now = fd_in.act_fifo_full;
#ifdef FX1_A3_STALL_DIRECT
                fd_in.act_stall = peek_act_stall_ ? peek_act_stall_() : i_act_stall.read();
#else
                fd_in.act_stall = i_act_stall.read();
#endif
#ifdef FX1_A3_TILDONE_Q_GATE
                fd_in.act_fifo_full_peek = peek_act_full_ ? peek_act_full_() : fd_in.act_fifo_full;
                fd_in.act_stall_peek = peek_act_stall_ ? peek_act_stall_() : fd_in.act_stall;
#endif
                fd_in.wei_done = i_wei_done.read();
                fd_in.wei_til_done = i_wei_til_done.read();
#ifdef FX1_A3_EMPTY_DIRECT
                fd_in.wei_fifo_empty = peek_wei_empty_ ? peek_wei_empty_()
                                                       : i_wei_fifo_empty.read();
#else
                fd_in.wei_fifo_empty = i_wei_fifo_empty.read();
#endif
                fd_in.wei_fifo_full = i_wei_fifo_full.read();
                fd_in.wei_fifo_full_now = fd_in.wei_fifo_full;
#ifdef FX1_A3_STALL_DIRECT
                fd_in.wei_stall = peek_wei_stall_ ? peek_wei_stall_() : i_wei_stall.read();
#else
                fd_in.wei_stall = i_wei_stall.read();
#endif

                auto fd_out = feeders_fsm.compute_outputs(fd_in);
#ifdef FX1_A3_HOLD_TRACE
                // Debug hook (default off): when *_cnt_hold_q rises relative to the end of the K loop. RTL
                // feeders_fsm.sv:177 -- one side's fifo_empty only stalls the pipeline while THAT side's cnt_hold is low.
                {
                    static std::ofstream ht("trace_sysc/hold_trace.csv");
                    static bool ht_hdr = false;
                    if (std::string(this->name()).find("NpuTop_std") !=
                        std::string::npos)
                    {
                        if (!ht_hdr)
                        {
                            ht << "tsim,cyc,ctx,act_hold,wei_hold,pipeline_en,"
                                  "act_empty,wei_empty,act_pop,wei_pop,"
                                  "act_tildone,wei_tildone,fsm_state,"
                                  "wei_done,act_rep,wei_rep,act_ov,wei_ov"
                               << std::endl;
                            ht_hdr = true;
                        }
                        ht << sc_core::sc_time_stamp().value() << ","
                           << dbg_cycle << "," << context_cnt << ","
                           << (int)feeders_fsm.dbg_act_hold() << ","
                           << (int)feeders_fsm.dbg_wei_hold() << ","
                           << (int)fd_out.pipeline_en << ","
                           << (int)fd_in.act_fifo_empty << ","
                           << (int)fd_in.wei_fifo_empty << ","
                           << (int)fd_out.act_pop_en << ","
                           << (int)fd_out.wei_pop_en << ","
                           << (int)fd_in.act_til_done << ","
                           << (int)fd_in.wei_til_done << ","
                           << feeders_fsm.dbg_state() << ","
                           << (int)fd_in.wei_done << ","
                           << feeders_fsm.dbg_act_rep() << ","
                           << feeders_fsm.dbg_wei_rep() << ","
                           << (int)feeders_fsm.dbg_act_ov() << ","
                           << (int)feeders_fsm.dbg_wei_ov() << "\n";
                    }
                }
#endif

                // Arm on the boundary cycle, consume it the next cycle (see the
                // fsm_reset-wins note above).
                if (fd_ctx_edge)
                    feeders_rearm_pending_ = true;
                else if (feeders_rearm_pending_)
                    feeders_rearm_pending_ = false;
#endif

                typename sauria_rtl::ContextSwitchController<X_DIM, Y_DIM, /*PE_LAT=*/2, /*EXTRA_CSREG=*/FX1_A3_EXTRA_CSREG>::Inputs cs_in;
                cs_in.incntlim = incntlim;
                cs_in.clear = ctx_out.cswitch_cnt_clear; // soft_reset handled by top-level reset branch (calls cs_ctrl.reset())
#ifdef FX1_A3_FEEDERS_FSM
                // THE Gap A2 seam: these three inputs are exactly the
                // wires feeders_fsm owns in real RTL (main_controller.sv lines
                // 160/161/162). Previously fed from ContextFsm's own
                // pipeline_gate/pop_gate, which is why the K-counter advanced
                // on a different schedule than the data actually being fed.
                cs_in.pipeline_en = fd_out.pipeline_en;
                cs_in.wei_pop_en = fd_out.wei_pop_en;
                cs_in.act_pop_en = fd_out.act_pop_en;
#else
                cs_in.pipeline_en = ctx_out.pipeline_gate; // same wire as the array's i_pipeline_en in real RTL (main_controller.sv)
                cs_in.wei_pop_en = pop_now;
                cs_in.act_pop_en = pop_now;
#endif
                cs_in.cswitch_en = ctx_out.cswitch_en;
                cs_in.cswitch_force = ctx_out.cswitch_force;

                auto cs_out = cs_ctrl.tick(cs_in);

                // cswitch_done is passed through undelayed: it also gates the o_pop_gate = 1 states (WAIT_CSWITCH /
                // WAIT_OBUF / ARRAY_CSWITCH), so delaying it would change the computation itself. The delay line below is
                // computed but unused (see cswitch_done_delay_).
                ctx_in.cdone = cs_out.cdone;

                // "drain window" fix -- see pop_drain_cnt_'s own
                // declaration comment. Start the countdown on cdone's rising
                // edge; the feeder-facing pop stays forced high while it
                // counts down, independent of ctx_out.pop_gate's own value.
                bool cdone_rise_for_drain = cs_out.cdone && !cdone_prev_for_drain_;
                cdone_prev_for_drain_ = cs_out.cdone;
                if (cdone_rise_for_drain)
                {
                    // wei_feeder.h has the SAME per-column skew mechanism
                    // (skew_regs[X_DIM], column x needs x cycles). max(X_DIM,
                    // Y_DIM)-1 alone fixed 127/128 but left the single
                    // farthest corner (row=Y_DIM-1,col=X_DIM-1) still wrong --
                    // that PE needs BOTH the row skew AND column skew to
                    // finish draining, i.e. (X_DIM-1)+(Y_DIM-1) combined, not
                    // just the larger of the two alone.
                    constexpr int drain_dim = (X_DIM - 1) + (Y_DIM - 1);
                    pop_drain_cnt_ = (drain_dim > 0) ? (uint32_t)drain_dim : 0u;
                }
                bool pop_now_for_feeders = pop_now || (pop_drain_cnt_ > 0);
                if (pop_drain_cnt_ > 0)
                {
                    pop_drain_cnt_--;
                }
#ifdef FX1_A3_FEEDERS_FSM
                //'s hand-tuned drain window is SUPERSEDED by the real
                // FSM's own tail (FINAL_PUSH_BOTH -> FINAL_PUSH_WAIT ->
                // EMPTY_WAIT -> FIFO_EMPTYING). Left computed but unused so
                // this guard stays a pure A/B switch against the old path.
                (void)pop_now_for_feeders;
#endif

                bool cswitch_done_delayed = cswitch_done_delay_[0];
                for (int i = 0; i + 1 < CSWITCH_DONE_SETTLE_DELAY; i++)
                    cswitch_done_delay_[i] = cswitch_done_delay_[i + 1];
                cswitch_done_delay_[CSWITCH_DONE_SETTLE_DELAY - 1] = cs_out.cswitch_done;
                (void)cswitch_done_delayed; // unused (see cswitch_done_delay_)
#ifdef FX1_A3_CSC_AUDIT
                {
                    static std::ofstream ca("trace_sysc/csc_audit.csv");
                    static bool ca_hdr = false;
                    static uint64_t ca_cyc = 0;
                    static std::map<std::string, uint32_t> ca_incnt_max;
                    static std::map<std::string, uint64_t> ca_bothpop;
                    static std::map<std::string, uint64_t> ca_cdone_n;
                    static std::map<std::string, std::string> ca_last;
                    if (!ca_hdr)
                    {
                        ca << "cyc,inst,incntlim,cdone_val,incnt,incnt_max,pop_shim_q2,"
                              "wei_pop,act_pop,both_pop_cycles,pipeline_en,cdone,"
                              "cdone_n,cdone_hold,cswitch_en,cswitch_force,clear,"
                              "pop_gate,pipe_gate,ctx_status"
                           << "\n";
                        ca_hdr = true;
                    }
                    std::string cnm = this->name();
                    const uint32_t iv = cs_ctrl.dbg_incnt();
                    if (iv > ca_incnt_max[cnm]) ca_incnt_max[cnm] = iv;
                    const bool bothp = cs_in.wei_pop_en && cs_in.act_pop_en;
                    if (bothp) ca_bothpop[cnm]++;
                    if (cs_out.cdone) ca_cdone_n[cnm]++;
                    const uint32_t cdv = (cs_in.incntlim > 1) ? (cs_in.incntlim - 2) : 0u;

                    std::ostringstream ck;
                    ck << iv << "_" << (int)cs_ctrl.dbg_pop_shim_q2() << (int)bothp
                       << (int)cs_in.wei_pop_en << (int)cs_in.act_pop_en
                       << (int)cs_in.pipeline_en << (int)cs_out.cdone
                       << (int)cs_ctrl.dbg_cdone_hold() << (int)cs_in.cswitch_en
                       << (int)cs_in.cswitch_force << (int)cs_in.clear;
#ifdef FX1_A3_CSC_AUDIT_FULL
                    // Debug hook (default off): no change-compression; one line per permitted beat, so the line index IS the
                    // beat index.
                    const bool ca_emit = (bool)cs_in.pipeline_en;
#else
                    const bool ca_emit = (ca_last[cnm] != ck.str());
#endif
                    ca_last[cnm] = ck.str();
                    if (ca_emit)
                    {
                        ca << ca_cyc << "," << cnm << "," << cs_in.incntlim << ","
                           << cdv << "," << iv << "," << ca_incnt_max[cnm] << ","
                           << (int)cs_ctrl.dbg_pop_shim_q2() << ","
                           << (int)cs_in.wei_pop_en << "," << (int)cs_in.act_pop_en << ","
                           << ca_bothpop[cnm] << "," << (int)cs_in.pipeline_en << ","
                           << (int)cs_out.cdone << "," << ca_cdone_n[cnm] << ","
                           << (int)cs_ctrl.dbg_cdone_hold() << ","
                           << (int)cs_in.cswitch_en << "," << (int)cs_in.cswitch_force
                           << "," << (int)cs_in.clear
                           << "," << (int)ctx_out.pop_gate
                           << "," << (int)ctx_out.pipeline_gate
                           << "," << ctx_out.ctx_status << "\n";
#ifndef FX1_A3_CSC_AUDIT_FULL
                        ca.flush();
#endif
                    }
                    ca_cyc++;
                }
#endif
                ctx_in.cswitch_done = cs_out.cswitch_done;
#ifdef FX1_A3_FEEDERS_FSM
                // Both of these were placeholders while feeders_fsm was
                // unported: pipeline_en was approximated as !feeder_stall (see
                // context_switch_controller.h's header note, lines 22-27) and
                // feeders_done was hardcoded true. ContextFsm reads them only
                // in commit(), so overriding here -- after the feeders FSM has
                // settled -- is the correct injection point.
                ctx_in.pipeline_en = fd_out.pipeline_en;
#ifdef FX1_A3_FEEDERS_DONE_TRUE
                // Diagnostic A/B switch (default off): pins i_feeders_done to true (behaviour without feeders_fsm). Not a fix.
                ctx_in.feeders_done = true;
#else
                ctx_in.feeders_done = fd_out.feeders_done;
#endif
#endif
#if defined(FX1_A3_FSM_TAPE)
                // Debug hook (default off): one line EVERY cycle, unfiltered.
                if (std::string(this->name()).find("NpuTop_std") !=
                    std::string::npos)
                {
                    fsm_tape_row(false,
                                 (int)ctx_in.start,
                                 (int)ctx_in.outbuf_done,
                                 (int)ctx_in.shift_done,
                                 (int)ctx_in.finalwrite,
                                 (int)ctx_in.feeders_done,
                                 (int)ctx_in.pipeline_en,
                                 (int)cs_in.act_pop_en,
                                 (int)cs_in.wei_pop_en,
                                 (unsigned)cs_in.incntlim,
                                 (unsigned)ctx_out.ctx_status,
                                 (int)ctx_out.pop_gate,
                                 (int)ctx_out.pipeline_gate,
                                 (int)cs_out.cdone,
                                 (int)cs_out.cswitch_done,
                                 (unsigned)cs_out.cswitch_arr,
                                 (unsigned)cs_ctrl.dbg_incnt(),
                                 (int)cs_ctrl.dbg_cdone_hold(),
                                 (int)cs_in.clear,
                                 (int)cs_in.cswitch_en,
                                 (int)cs_in.cswitch_force,
#ifdef FX1_A3_FEEDERS_FSM
                                 (int)fd_out.act_cnt_en,
                                 (int)fd_out.wei_cnt_en,
                                 (int)ctx_out.outbuf_start);
#else
                                 0, 0, (int)ctx_out.outbuf_start);
#endif
                }
#endif
                ctx_fsm.commit(ctx_in);
#ifdef FX1_A3_FEEDERS_FSM
                feeders_fsm.commit();
#endif

#ifdef FX1_A3_DEBUG_TRACE
                if (ctx_out.ctx_status != 0 || ctx_prev_status != 0 || feeder_stall)
                {
                    std::cerr << "[A3TRACE] t=" << sc_core::sc_time_stamp()
                              << " cyc=" << dbg_cycle
                              << " prev_status=" << ctx_prev_status
                              << " status=" << ctx_out.ctx_status
                              << " start=" << ctx_in.start
                              << " cdone=" << ctx_in.cdone
                              << " cswitch_done=" << ctx_in.cswitch_done
                              << " incntlim=" << incntlim
                              << " pop_gate=" << ctx_out.pop_gate
                              << " pop_now=" << pop_now
                              << " feeder_stall=" << feeder_stall
                              << " i_act_stall=" << i_act_stall.read()
                              << " i_wei_stall=" << i_wei_stall.read()
                              << " cswitch_en=" << ctx_out.cswitch_en
                              << " cswitch_force=" << ctx_out.cswitch_force
                              << " cswitch_cnt_clear=" << ctx_out.cswitch_cnt_clear
                              << " sa_clear=" << ctx_out.sa_clear
                              << " outbuf_start=" << ctx_out.outbuf_start
                              << " outbuf_reset=" << ctx_out.outbuf_reset
                              << " pipeline_gate=" << ctx_out.pipeline_gate
                              << " cswitch_arr=" << cs_out.cswitch_arr
                              << " shift_done=" << ctx_in.shift_done
                              << " done=" << ctx_out.done
#ifdef FX1_A3_FEEDERS_FSM
                              << " feed_status=" << fd_out.feed_status
                              << " fd_pop=" << fd_out.act_pop_en
                              << " fd_cnt_en=" << fd_out.act_cnt_en
                              << " fd_pipe_en=" << fd_out.pipeline_en
                              << " fd_done=" << fd_out.feeders_done
                              << " act_fifo_empty=" << fd_in.act_fifo_empty
                              << " wei_fifo_empty=" << fd_in.wei_fifo_empty
#endif
                              << " obuf_hold=" << (int)ctx_fsm.peek_outbuf_done_hold()
                              << " cscnt_q=" << cs_ctrl.peek_cscnt_q()
                              << " cscnt_flag=" << (int)cs_ctrl.peek_cscnt_flag()
                              << " cdone_hold=" << (int)cs_ctrl.peek_cdone_hold()
                              << " cdone_shim=" << (int)cs_ctrl.peek_cdone_shim_q1()
                              << " incnt_q=" << cs_ctrl.peek_incnt_q()
                              << std::endl;
                }
#endif

                // Context-boundary bookkeeping. context_fsm.sv does NOT expose
                // a "next context begins now" signal -- in real RTL that reset
                // lives inside the feeders' own self-wrapping address-counter
                // cascade (ifmap_idxcnt.sv), not as a pulse from this FSM. SC's
                // current (guard-off-path) feeder mechanism instead needs an
                // explicit cnt_clear once per context, so this wiring layer
                // derives the boundary itself from ctx_status's own transitions:
                //   - ctx_status arriving at 1 (START_FLAGS) for the first time
                //     after IDLE == context 0 starting.
                //   - ctx_status arriving at 11 (ALL_BUSY_SHIFT) or 14
                //     (OBUF_BUSY_SHIFT) directly FROM 17/18 (ARRAY_CSWITCH[_STALL])
                //     == the next context's overlapped popping starting (this is
                //     exactly the Gap A2 context-overlap this port exists to add
                //     -- context N+1 begins popping before context N's own
                //     cswitch/drain has fully finished).
                bool entering_first_context =
                    (ctx_out.ctx_status == 1) && (ctx_prev_status != 1);
                bool entering_next_context =
                    (ctx_prev_status == 17 || ctx_prev_status == 18) &&
                    (ctx_out.ctx_status == 11 || ctx_out.ctx_status == 14);
                bool new_context_edge = entering_first_context || entering_next_context;

                if (entering_first_context)
                {
                    context_cnt = 0;
                }
                else if (entering_next_context)
                {
                    context_cnt++;
                }

                if (new_context_edge)
                {
                    uint32_t local_context = context_cnt % ncontexts;
                    uint32_t out_tile = context_cnt / ncontexts;
                    o_context_id.write(local_context);
                    o_global_context_id.write(context_cnt);
                    o_local_context_id.write(local_context);
                    o_out_tile_id.write(out_tile);

                    DBG_COUT << "[CTRL A3 CONTEXT START]"
                              << " global_context=" << context_cnt
                              << " / " << total_contexts
                              << std::endl;
                }

                bool feeders_active = !ctx_out.feeders_reset;
                (void)feeders_active;

#ifdef FX1_A3_FEEDERS_FSM
                // Every feeder-facing control now comes from the real ported
                // FSM instead of being derived from ContextFsm's state.
                o_act_feeder_en.write(fd_out.act_feeder_en);
                o_wei_feeder_en.write(fd_out.wei_feeder_en);

                o_act_feeder_clear.write(fd_out.act_feeder_clear);
                o_act_clearfifo.write(fd_out.act_clearfifo);
                o_wei_feeder_clear.write(fd_out.wei_feeder_clear);
                o_wei_clearfifo.write(fd_out.wei_clearfifo);

                seam_set_act_start_(fd_out.act_start);
                seam_set_wei_start_(fd_out.wei_start);
                seam_set_act_valid_(fd_out.act_valid);
                seam_set_wei_valid_(fd_out.wei_valid);
#ifdef FX1_A3_FDFSM_DIRECT
                {
                    const bool fa[8] = {fd_out.act_feeder_en, fd_out.act_feeder_clear,
                                        fd_out.act_clearfifo, fd_out.act_finalpush,
                                        fd_out.act_cnt_clear, fd_out.act_pop_en,
                                        fd_out.act_cnt_en,    fd_out.act_finalctx};
                    const bool fw[8] = {fd_out.wei_feeder_en, fd_out.wei_feeder_clear,
                                        fd_out.wei_clearfifo, fd_out.wei_finalpush,
                                        fd_out.wei_cnt_clear, fd_out.wei_pop_en,
                                        fd_out.wei_cnt_en,    fd_out.wei_cswitch};
                    seam_set_fd_(fa, fw);
                }
#endif

                // Previously NEVER written on the guard-on path -- both ports
                // sat at their reset value for the whole run even though
                // npu_top.h wires them to the feeders (npu_top.h:298/309 ->
                // 334/391).
                o_act_finalpush.write(fd_out.act_finalpush);
                o_wei_finalpush.write(fd_out.wei_finalpush);
                o_act_finalctx.write(fd_out.act_finalctx);
                o_wei_cswitch.write(fd_out.wei_cswitch);

                // DELIBERATE DEVIATION FROM RTL -- do not "fix" this to a bare
                // fd_out.act_cnt_clear without reading this note.
                // feeders_fsm.sv holds o_act_cnt_clear LOW through every active
                // state (high only in IDLE/FINISH), because in real RTL the
                // per-context address reset lives inside the feeders' own
                // self-wrapping counter cascade (ifmap_idxcnt.sv, driven by
                // i_finalctx). THIS model's ifmap_feeder does NOT use that
                // cascade for its data path: ifmap_idxcnt is instantiated only
                // as a cycle-cost meter under FX1_A2_IDXCNT_THROTTLE, with
                // finalctx/cnt_clear hardcoded false (ifmap_feeder.h:828-829),
                // while the real address registers (addr_reg/act_x_cnt/
                // act_y_cnt/act_ch_cnt/act_tx_cnt/act_ty_cnt) are reset by the
                // RISING EDGE of i_cnt_clear (ifmap_feeder.h:563-567; wei_feeder
                // .h:277-281 does the same). Driving cnt_clear from the FSM
                // alone would therefore never reset the feeders between
                // contexts. OR in the derived per-context edge to keep that
                // contract while still honouring the FSM's own IDLE/FINISH
                // clears.
#ifdef FX1_A3_FULL_TRACE
                {
                    static std::ofstream ft("trace_sysc/full_trace.csv");
                    static bool ft_hdr = false;
                    static uint64_t ft_cyc = 0;
                    static std::map<std::string, std::string> ft_last;
                    static std::map<std::string, uint64_t> n_ccl, n_hold, n_fin,
                        n_tdone, n_pipe, n_aempty, n_astall;
                    if (!ft_hdr)
                    {
                        ft << "cyc,inst,fsm_state,pipe_gate,pipe_en,"
                              "fd_act_ccl,new_ctx_edge,act_ccl_out,n_act_ccl,"
                              "act_hold,wei_hold,n_hold,act_fin,n_fin,"
                              "act_empty,wei_empty,n_aempty,act_stall,n_astall,"
                              "act_done,act_til_done,n_tdone,wei_done,wei_til_done,"
                              "act_cnt_en,wei_cnt_en,act_pop,wei_pop,"
                              "incnt,pop_shim,cdone,n_pipe,ctxid" << "\n";
                        ft_hdr = true;
                    }
                    std::string fn = this->name();
                    const bool ccl_out = fd_out.act_cnt_clear || new_context_edge;
                    if (ccl_out) n_ccl[fn]++;
                    if (feeders_fsm.dbg_act_hold()) n_hold[fn]++;
                    if (feeders_fsm.dbg_act_fin()) n_fin[fn]++;
                    if (i_act_til_done.read()) n_tdone[fn]++;
                    if (fd_out.pipeline_en) n_pipe[fn]++;
                    if (i_act_fifo_empty.read()) n_aempty[fn]++;
                    if (i_act_stall.read()) n_astall[fn]++;

                    std::ostringstream fk;
                    fk << feeders_fsm.dbg_state() << (int)fd_out.pipeline_en
                       << (int)fd_out.act_cnt_clear << (int)new_context_edge
                       << (int)feeders_fsm.dbg_act_hold() << (int)feeders_fsm.dbg_wei_hold()
                       << (int)feeders_fsm.dbg_act_fin() << (int)i_act_fifo_empty.read()
                       << (int)i_wei_fifo_empty.read() << (int)i_act_stall.read()
                       << (int)i_act_til_done.read() << (int)i_wei_done.read()
                       << (int)fd_out.act_cnt_en << (int)fd_out.act_pop_en
                       << (int)fd_out.wei_pop_en << "_" << cs_ctrl.dbg_incnt();
                    if (ft_last[fn] != fk.str())
                    {
                        ft_last[fn] = fk.str();
                        ft << ft_cyc << "," << fn << "," << feeders_fsm.dbg_state() << ","
                           << (int)ctx_out.pipeline_gate << "," << (int)fd_out.pipeline_en << ","
                           << (int)fd_out.act_cnt_clear << "," << (int)new_context_edge << ","
                           << (int)ccl_out << "," << n_ccl[fn] << ","
                           << (int)feeders_fsm.dbg_act_hold() << "," << (int)feeders_fsm.dbg_wei_hold() << ","
                           << n_hold[fn] << "," << (int)feeders_fsm.dbg_act_fin() << "," << n_fin[fn] << ","
                           << (int)i_act_fifo_empty.read() << "," << (int)i_wei_fifo_empty.read() << ","
                           << n_aempty[fn] << "," << (int)i_act_stall.read() << "," << n_astall[fn] << ","
                           << (int)i_act_done.read() << "," << (int)i_act_til_done.read() << ","
                           << n_tdone[fn] << "," << (int)i_wei_done.read() << ","
                           << (int)i_wei_til_done.read() << ","
                           << (int)fd_out.act_cnt_en << "," << (int)fd_out.wei_cnt_en << ","
                           << (int)fd_out.act_pop_en << "," << (int)fd_out.wei_pop_en << ","
                           << cs_ctrl.dbg_incnt() << "," << (int)cs_ctrl.dbg_pop_shim_q2() << ","
                           << (int)cs_out.cdone << "," << n_pipe[fn] << ","
                           << ctx_out.ctx_status << "\n";
                        ft.flush();
                    }
                    ft_cyc++;
                }
#endif
#ifdef FX1_A3_CNTCLEAR_RTL
                // no "|| new_context_edge": the RTL feeders (FX1_A3_IFMAP_FEEDER_RTL / WEI_FEEDER_RTL) read the real
                // i_cnt_clear + i_finalctx / i_cswitch and roll their own ifmap_idxcnt / wei_idxcnt cascade, so a per-context
                // clear pulse is redundant and would also clear the outer counter stages.
                o_act_cnt_clear.write(fd_out.act_cnt_clear);
                o_wei_cnt_clear.write(fd_out.wei_cnt_clear);
#elif defined(FX1_A3_CNTCLEAR_ACT_ONLY)
                o_act_cnt_clear.write(fd_out.act_cnt_clear);
                o_wei_cnt_clear.write(fd_out.wei_cnt_clear || new_context_edge);
#elif defined(FX1_A3_CNTCLEAR_WEI_ONLY)
                o_act_cnt_clear.write(fd_out.act_cnt_clear || new_context_edge);
                o_wei_cnt_clear.write(fd_out.wei_cnt_clear);
#else
                o_act_cnt_clear.write(fd_out.act_cnt_clear || new_context_edge);
                o_wei_cnt_clear.write(fd_out.wei_cnt_clear || new_context_edge);
#endif

                seam_set_act_cnt_en_(fd_out.act_cnt_en);
                seam_set_wei_cnt_en_(fd_out.wei_cnt_en);

                o_act_pop_en.write(fd_out.act_pop_en);
                o_wei_pop_en.write(fd_out.wei_pop_en);

                seam_set_pipeline_en_(fd_out.pipeline_en);
#else
                o_act_feeder_en.write(feeders_active);
                o_wei_feeder_en.write(feeders_active);

                o_act_feeder_clear.write(ctx_out.feeders_reset);
                o_act_clearfifo.write(ctx_out.feeders_reset);
                o_wei_feeder_clear.write(ctx_out.feeders_reset);
                o_wei_clearfifo.write(ctx_out.feeders_reset);

                seam_set_act_start_(ctx_out.feeders_start);
                seam_set_wei_start_(ctx_out.feeders_start);
                seam_set_act_valid_(feeders_active);
                seam_set_wei_valid_(feeders_active);

                o_act_cnt_clear.write(new_context_edge);
                o_wei_cnt_clear.write(new_context_edge);
                seam_set_act_cnt_en_(feeders_active);
                seam_set_wei_cnt_en_(feeders_active);

                o_act_pop_en.write(pop_now_for_feeders);
                o_wei_pop_en.write(pop_now_for_feeders);

                seam_set_pipeline_en_(ctx_out.pipeline_gate);
#endif

                o_softstall.write(ctx_out.ctx_status == 14u);
                seam_set_outbuf_start_(ctx_out.outbuf_start);
                o_outbuf_reset.write(ctx_out.outbuf_reset);

                o_sa_clear.write(ctx_out.sa_clear);
                // Real per-column staggered array-swap pulse from
                // ContextSwitchController -- NOT a single
                // "all bits at once" pulse, and NOT context_fsm's cswitch_force
                // (which fires once at START_COMP, BEFORE any real popping --
                // confirmed via tracing demo_gemm_32x32 to snapshot near-zero
                // partial sums into sa_array.h's mac_sc_q, causing 100% wrong
                // output; this is the bug ContextSwitchController fixes).
                o_cswitch_arr.write(sc_bv<X_DIM>(cs_out.cswitch_arr));

                // NOTE: ctx_out.done is RTL-faithful ("not busy", true at BOTH
                // IDLE and DONE -- see context_fsm.h's IDLE case). But npu_top.h's
                // done_latch_logic() expects Control's o_done port to be the
                // OLD code's narrow contract instead: false throughout IDLE,
                // true for exactly the one tick ctx_status==DONE(21), then false
                // again next tick. Forwarding ctx_out.done directly made o_done
                // read true continuously while parked in IDLE (before any start),
                // which done_latch_logic() latched as "already done" before a run
                // even began -- an integration contract, not a ContextFsm issue.
                o_done.write(ctx_out.ctx_status == 21);

#ifndef FX1_A3_CTRL_HISTOGRAM_NO_PERF
                // ctx_status indexes ContextFsm::MainState (22 entries), NOT ctrl_state_t (25 entries) that perf_counters.h
                // uses to PRINT names; MainState lacks ARRAY_FILL / DRAIN_FEED / ARRAY_FLUSH, so labels from index 3 on would
                // be shifted. Map to ctrl_state_t so both paths share one scale. (TIMING totals (total - IDLE) are
                // unaffected: IDLE = 0 in both enums.)
                {
                    static const int kMainToCtrl[22] = {
                        0,  // IDLE
                        1,  // START_FLAGS
                        2,  // ARRAY_PREP
                        4,  // FIRST_SHIFT        (ctrl_state_t skips ARRAY_FILL=3)
                        5,  // START_COMP
                        8,  // WAIT_CSWITCH       (skips DRAIN_FEED=6, ARRAY_FLUSH=7)
                        9,  // WAIT_CSWITCH_STALL
                        10, // WAIT_OBUF
                        11, // WAIT_OBUF_STALL
                        12, // SCND_SHIFT
                        13, // SCND_SHIFT_STALL
                        14, // ALL_BUSY_SHIFT
                        15, // ALL_BUSY
                        16, // ARRAY_BUSY
                        17, // OBUF_BUSY_SHIFT
                        18, // FORCE_STALL
                        19, // OBUF_BUSY
                        20, // ARRAY_CSWITCH
                        21, // ARRAY_CSWITCH_STALL
                        22, // LAST_SHIFT
                        23, // LAST_WAIT
                        24  // DONE
                    };
                    const uint32_t cs = ctx_out.ctx_status;
                    if (perf && cs < 22)
                    {
                        const int mapped = kMainToCtrl[cs];
                        if (mapped < sauria_rtl::PerfCounters::NUM_CTRL_STATES)
                            perf->ctrl_state_cycles[mapped]++;
                    }
                }
#endif

                ctx_prev_status = ctx_out.ctx_status;
            }
#else
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
                seam_set_pipeline_en_(false);

                seam_set_act_cnt_en_(false);
                seam_set_wei_cnt_en_(false);
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

                seam_set_act_start_(true);
                seam_set_wei_start_(true);

                seam_set_act_valid_(true);
                seam_set_wei_valid_(true);

                // Pulse counter clear at the beginning of each context.
                // Feeders detect this as rising edge.
                o_act_cnt_clear.write(true);
                o_wei_cnt_clear.write(true);

                seam_set_pipeline_en_(true);

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
                seam_set_act_cnt_en_(true);
                seam_set_wei_cnt_en_(true);

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
                seam_set_act_cnt_en_(true);
                seam_set_wei_cnt_en_(true);

                o_act_pop_en.write(false);
                o_wei_pop_en.write(false);

                seam_set_pipeline_en_(true);
                seam_set_outbuf_start_(false);
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

                // Feeders keep reading / prefetching SRAM.
                seam_set_act_cnt_en_(true);
                seam_set_wei_cnt_en_(true);

                if (feeder_stall)
                {
                    // Do not pop an empty vector into the array.
                    // Do not increment comp_cycles.
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
                seam_set_act_cnt_en_(false);
                seam_set_wei_cnt_en_(false);

                // Keep draining physical skew/tail from feeders into SA.
                o_act_pop_en.write(true);
                o_wei_pop_en.write(true);

                seam_set_pipeline_en_(true);
                seam_set_outbuf_start_(false);
                o_cswitch_arr.write(sc_bv<X_DIM>(0));

                drain_cycles++;

                // Correctness-first conservative tail.
                // PE_LAT is already X_DIM + Y_DIM in your template default.
                // Physical systolic drain: last operand reaches PE(X-1,Y-1) after
                // ~X+Y skew, then mul_lat(=2) multiply-pipeline stages. The old
                // +PE_LAT term (PE_LAT = X+Y here) DOUBLE-COUNTED the traversal,
                // roughly doubling the drain. Tighten to X+Y + generous fixed margin
                // (16 >> mul_lat); verified by make check (multi-context/64x64/conv
                // exercise the full tail).
                const uint32_t tail_limit =
                    static_cast<uint32_t>(X_DIM + Y_DIM + 16);

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
            //     // Feeders have popped every real vector.
            //     // Feed zeros for a few more cycles so the last MACs propagate.
            //     o_act_feeder_en.write(true);
            //     o_wei_feeder_en.write(true);

            //     seam_set_act_cnt_en_(false);
            //     seam_set_wei_cnt_en_(false);

            //     o_act_pop_en.write(true);
            //     o_wei_pop_en.write(true);

            //     seam_set_pipeline_en_(true);
            //     seam_set_outbuf_start_(false);
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
                seam_set_act_cnt_en_(false);
                seam_set_wei_cnt_en_(false);
                cycle_cnt = 0;
                state = WAIT_CSWITCH;
                break;

            case WAIT_CSWITCH:
            {
                o_cswitch_arr.write(sc_bv<X_DIM>(0));

                // Keep pipeline active while local delayed cswitch propagates.
                seam_set_pipeline_en_(true);

                o_act_pop_en.write(false);
                o_wei_pop_en.write(false);
                seam_set_act_cnt_en_(false);
                seam_set_wei_cnt_en_(false);

                cycle_cnt++;

                // SA local cswitch is delayed by roughly:
                //   y + x + 1 + mul_lat + CSWITCH_EXTRA_MARGIN + extra_csreg
                //
                // Use conservative wait for correctness-first model.
                // Context-switch propagation ~ x+y+1+mul_lat+cs_delay(delay_len=10).
                // Old +PE_LAT(=X+Y) double-counted the traversal. Tighten to X+Y +
                // margin 24 (covers mul_lat + cs delay + slack); make-check gated.
                const uint32_t cswitch_wait_limit =
                    static_cast<uint32_t>(X_DIM + Y_DIM + 24);

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
                seam_set_outbuf_start_(true);
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

                seam_set_act_cnt_en_(false);
                seam_set_wei_cnt_en_(false);

                o_act_pop_en.write(false);
                o_wei_pop_en.write(false);

                seam_set_pipeline_en_(false);

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
#endif // FX1_A3_CONTEXT_FSM

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

} // namespace sauria_rtl

#endif // SAURIA_RTL_MAIN_CONTROLLER_H
