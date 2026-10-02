#pragma once
//
// 1:1 copy of sauria_model's control/context_switch_controller.h, itself a
// 1:1 port of RTL sauria_core/control/context_switch_controller.sv.
//
// ONLY CHANGE from the reference file: namespace `sauria` -> `sauria_rtl`, to avoid colliding
// with this tree's own `sauria::ContextSwitchController`-equivalent logic (there isn't one by
// this exact name today, but `sauria::Control`/`IfmapFeeder`/`WeightFeeder`/`Psm` DO already
// exist in `namespace sauria`, and this file is included alongside those in
// control/native_lane_a_core.h -- keeping everything in one `sauria_rtl` namespace for the whole
// ported core avoids any future name collision, not just the ones that would collide today).
// Logic below is otherwise byte-for-byte identical to the reference file -- do not "clean up" or
// reinterpret without re-checking against sauria_model and, ultimately, RTL
// sauria_core/control/context_switch_controller.sv.
//
// Reference header (functional notes):
//
// This module produces the array's accumulator-swap pulse and context_fsm.sv's i_cdone / i_cswitch_done inputs.
// Deriving them inside Control (a comp_cycles >= incntlim level, a self-wrapping margin counter) would fire the swap
// too early (at START_COMP, before real compute has accumulated) and could not re-trigger per context: the RTL's
// per-context retrigger comes from THIS module's cdone / cdone_hold latch, not from context_fsm's one-time
// cswitch_force pulse.
//
// Real RTL topology (see RTL/src/sauria_core/control/main_controller.sv,
// the wrapper that instantiates all three): context_fsm.sv produces
// o_cswitch_en/o_cswitch_force/o_cswitch_cnt_clear as OUTPUTS which feed INTO
// this module as i_cswitch_en/i_cswitch_force/i_clear; THIS module owns the
// real K-counter (incnt_q) and swap-delay counter (cscnt_q), gated by the
// REAL pop signals (i_wei_pop_en/i_act_pop_en, i.e. Control's actual
// o_wei_pop_en/o_act_pop_en outputs) and i_pipeline_en (in real RTL, feeders_fsm's
// o_pipeline_en, ported in rtl_ref_feeders_fsm.h).
// THIS module's outputs (o_cdone, o_cswitch_done, o_cswitch_arr) then feed
// BACK into context_fsm.sv's i_cdone/i_cswitch_done inputs and drive the real
// array pulse directly (context_fsm.sv does NOT produce o_cswitch_arr itself).
//
// Two-phase tick(): same convention as context_fsm.h/ifmap_idxcnt.h/
// feed_data_manager.h -- Phase 1 computes every combinational value from
// pre-tick register state, Phase 2 commits, mirroring a single clock edge.
// Every register below has its OWN, DIFFERENT enable condition per the RTL's
// always_ff blocks (some gate on i_clear/(i_pipeline_en||i_cswitch_force),
// some on i_pipeline_en alone, some on i_pipeline_en&&i_cswitch_en) -- do not
// "simplify" these to a single shared enable without re-checking the source.
//
// NOTE on o_cswitch_arr: real RTL staggers each of the X column-bits across
// X separate cycles (cswitch_arr_d[i] = 1 when cscnt_q == PE_LAT-EXTRA_CSREG+i),
// modeling the real per-column propagation wave, THEN sa_array.h's own
// cs_delay[y][x] queue (untouched, existing) adds a FURTHER per-(y,x) delay on
// top of that. This is ported faithfully below (not collapsed to "all bits at
// once" the way the old SC code's single o_cswitch_arr pulse did) since the
// per-(y,x) delay_len in sa_array.h already varies with x too, so pulsing
// exactly the right column bit at the right cycle should matter for
// correctness, not just be cosmetic.
//
#include <cstdint>

namespace sauria_rtl
{

    template <int X_DIM = 32, int Y_DIM = 32, int PE_LAT = 2, int EXTRA_CSREG = 0>
    class ContextSwitchController
    {
    public:
        static constexpr int CSWITCH_PROP_CYCLES = PE_LAT + X_DIM + Y_DIM - 1;

        struct Inputs
        {
            uint32_t incntlim = 0;      // i_incntlim
            bool clear = false;         // i_clear (wiring layer: cswitch_cnt_clear || soft_reset)
            bool pipeline_en = false;   // i_pipeline_en
            bool wei_pop_en = false;    // i_wei_pop_en (Control's real o_wei_pop_en)
            bool act_pop_en = false;    // i_act_pop_en (Control's real o_act_pop_en)
            bool cswitch_en = false;    // i_cswitch_en (from ContextFsm)
            bool cswitch_force = false; // i_cswitch_force (from ContextFsm)
        };

        struct Outputs
        {
            bool cdone = false;          // o_cdone -- feeds ContextFsm::Inputs::cdone
            bool cswitch_done = false;   // o_cswitch_done -- feeds ContextFsm::Inputs::cswitch_done
            uint32_t cswitch_arr = 0;    // o_cswitch_arr, bit i = column i (bit 0 = LSB = column 0)
        };

        void reset()
        {
            pop_shim_init_q_ = false;
            pop_shim_q2_ = false;
            incnt_q_ = 0;
            cdone_hold_ = false;
            cdone_q_ = false;
            cdone_shim_q1_ = false;
            cscnt_q_ = 0;
            cswitch_arr_q_ = 0;
        }

        Outputs tick(const Inputs &in)
        {
            // ---------- Phase 1: combinational (pre-tick register state) ----------

            // Debug hook: snapshot the registers BEFORE the tick updates them, so model traces sample like the RTL tape
            // (register value at the clock edge). Reading cscnt_q_ after tick() would be one cycle off.
            dbg_pre_cscnt_q_ = cscnt_q_;
            dbg_pre_cdone_hold_ = cdone_hold_;
            dbg_pre_cdone_shim_q1_ = cdone_shim_q1_;
            dbg_pre_incnt_q_ = incnt_q_;

            // --- Simple input counter (RTL lines ~120-155) ---
            uint32_t cdone_val = (in.incntlim > 1) ? (in.incntlim - 2) : 0;
            bool cdone_force_q1 = (in.incntlim == 1);
            bool cdone_force_q2 = (in.incntlim == 0);

            bool cdone = false;
            if ((incnt_q_ == cdone_val) && (pop_shim_q2_ || cdone_force_q2))
            {
                cdone = true;
            }
            uint32_t incnt_d = (incnt_q_ == in.incntlim) ? 0 : (incnt_q_ + 1);

            // --- Pop signal shimming (RTL lines ~95-105) ---
            bool pop_shim_init_d = pop_shim_init_q_;
            if (in.clear)
            {
                pop_shim_init_d = false;
            }
            else if (in.wei_pop_en && in.act_pop_en)
            {
                pop_shim_init_d = true;
            }

            // --- Context Switch counter trigger (RTL line ~245, needed early
            // for cdone_hold_d's deassert branch below) ---
            bool cscnt_trigger = (in.cswitch_en && cdone_shim_q1_) || in.cswitch_force;

            // --- Signal hold register (RTL lines ~163-180) ---
            bool cdone_hold_d = cdone_hold_;
            if (cdone && (in.pipeline_en || in.cswitch_force) && in.wei_pop_en && in.act_pop_en)
            {
                cdone_hold_d = true;
            }
            else if (cscnt_trigger && in.cswitch_en && (cscnt_q_ == 0) &&
                     (in.pipeline_en || in.cswitch_force))
            {
                cdone_hold_d = false;
            }

            // --- Context Switch Counter (RTL lines ~248-266) ---
            bool cscnt_flag = false;
            uint32_t cscnt_d = cscnt_q_;
            if (cscnt_q_ == (uint32_t)CSWITCH_PROP_CYCLES)
            {
                cscnt_flag = true;
                cscnt_d = 0;
            }
            else if (cscnt_trigger || (cscnt_q_ > 0))
            {
                cscnt_d = cscnt_q_ + 1;
            }

            // --- Context Switch Generation (RTL lines ~283-293): staggered
            // per-column bits, one column's bit set per distinct cscnt_q value ---
            uint32_t cswitch_arr_d = 0;
            for (int i = 0; i < X_DIM; i++)
            {
                if (cscnt_q_ == (uint32_t)(PE_LAT - EXTRA_CSREG + i))
                {
                    cswitch_arr_d |= (1u << i);
                }
            }

            // ---------- Outputs (combinational, RTL lines ~316-319) ----------
            Outputs out;
            out.cswitch_arr = cswitch_arr_q_; // registered value (1-cycle-old cswitch_arr_d)
            out.cdone = (cdone_q_ || (cdone_force_q1 && cdone)) &&
                        in.pipeline_en && in.wei_pop_en && in.act_pop_en;
            out.cswitch_done = cscnt_flag && in.pipeline_en && in.cswitch_en;
            dbg_cscnt_flag_ = cscnt_flag;   // diagnostics: compare with [RTL_TAPE2] field C

            // ---------- Phase 2: commit ----------
            // Snapshot every PRE-tick register value THIS phase still needs to
            // read, before any of them get overwritten below -- several next-
            // state expressions read a DIFFERENT register's pre-tick value
            // (e.g. pop_shim_q2's update reads pre-tick pop_shim_init_q, not
            // pop_shim_init_d), so committing in declaration order without
            // this snapshot would silently read already-updated values.
            const bool pre_pop_shim_init_q = pop_shim_init_q_;
            const bool pre_pop_shim_q2 = pop_shim_q2_;
            const bool pre_cdone_hold = cdone_hold_;

            // Each register below has its OWN enable condition, per the RTL's
            // individual always_ff blocks -- do not merge them.
            if (in.clear)
            {
                pop_shim_init_q_ = false;
            }
            else if (in.pipeline_en || in.cswitch_force)
            {
                pop_shim_init_q_ = pop_shim_init_d;
            }

            if (in.clear)
            {
                pop_shim_q2_ = false;
            }
            else if (in.pipeline_en || in.cswitch_force)
            {
                // Uses PRE-tick pop_shim_init_q (RTL: pop_shim_init_q, not _d)
                pop_shim_q2_ = pre_pop_shim_init_q && in.wei_pop_en && in.act_pop_en;
            }

            if (in.clear)
            {
                incnt_q_ = 0;
            }
            else if (in.pipeline_en && pre_pop_shim_q2)
            {
                // Uses PRE-tick pop_shim_q2 (RTL: pop_shim_q2, not this tick's
                // freshly-committed value)
                incnt_q_ = incnt_d;
            }

            if (in.clear)
            {
                cdone_hold_ = false;
            }
            else if (in.pipeline_en || in.cswitch_force)
            {
                cdone_hold_ = cdone_hold_d;
            }

            if (in.clear)
            {
                cdone_q_ = false;
            }
            else if (in.pipeline_en || in.cswitch_force)
            {
                cdone_q_ = cdone;
            }

            if (in.clear)
            {
                cdone_shim_q1_ = false;
            }
            else if (in.pipeline_en || in.cswitch_force)
            {
                // Uses PRE-tick cdone_hold (RTL: cdone_hold, not cdone_hold_d)
                cdone_shim_q1_ = pre_cdone_hold;
            }

            if (in.clear)
            {
                cscnt_q_ = 0;
            }
#ifdef FX1_A3_CSCNT_POP_DOMAIN
            // the propagation counter advances in the POP domain, like the operand stream; otherwise every permitted
            // beat WITHOUT a pop would make the cswitch reach the PEs one operand late.
            else if (in.pipeline_en && in.cswitch_en &&
                     in.wei_pop_en && in.act_pop_en)
#else
            else if (in.pipeline_en && in.cswitch_en)
#endif
            {
                cscnt_q_ = cscnt_d;
            }

            if (in.clear)
            {
                cswitch_arr_q_ = 0;
            }
            else if (in.pipeline_en)
            {
                cswitch_arr_q_ = cswitch_arr_d;
            }

            return out;
        }

        // read-only observers for the CSC audit. Additive; no logic change.
        uint32_t dbg_incnt() const { return incnt_q_; }
        bool dbg_pop_shim_q2() const { return pop_shim_q2_; }
        bool dbg_cdone_hold() const { return cdone_hold_; }

        // --- Debug hooks (const, no effect on behaviour) ---
        // Match field C of the RTL tape: cscnt_q, cscnt_flag, cdone_hold, cdone_shim_q1.
        // PRE-TICK: same sampling as the RTL tape (clock edge).
        uint32_t peek_cscnt_q() const { return dbg_pre_cscnt_q_; }
        bool peek_cdone_hold() const { return dbg_pre_cdone_hold_; }
        bool peek_cdone_shim_q1() const { return dbg_pre_cdone_shim_q1_; }
        uint32_t peek_incnt_q() const { return dbg_pre_incnt_q_; }
        // cscnt_flag is combinational from the pre-update cscnt_q_, so it already has the right semantics.
        bool peek_cscnt_flag() const { return dbg_cscnt_flag_; }

    private:
        bool dbg_cscnt_flag_{false};
        uint32_t dbg_pre_cscnt_q_{0};
        bool dbg_pre_cdone_hold_{false};
        bool dbg_pre_cdone_shim_q1_{false};
        uint32_t dbg_pre_incnt_q_{0};
        bool pop_shim_init_q_{false};
        bool pop_shim_q2_{false};
        uint32_t incnt_q_{0};
        bool cdone_hold_{false};
        bool cdone_q_{false};
        bool cdone_shim_q1_{false};
        uint32_t cscnt_q_{0};
        uint32_t cswitch_arr_q_{0};
    };

} // namespace sauria_rtl
