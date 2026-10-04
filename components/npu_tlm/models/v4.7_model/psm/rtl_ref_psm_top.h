// SystemC Model for SAURIA NPU Core
// Partial Sum Memory (PSM) Block with Parameterized Dimensions

// i_fsm_start pulse
// → phase 5: wait PSM_START_DELAY cycle, cscan=0, write=0
// → phase 1: write x0, cscan=0
// → phase 2: cscan=1, no write
// → phase 3: cscan=1, no write
// → phase 4: write x1..x15, cscan=1

#ifndef SAURIA_RTL_PSM_TOP_H
#define SAURIA_RTL_PSM_TOP_H

#include <fstream>
#include <string>
#include "sauria_types.h"
#include "debug.h"
#ifdef FX1_A3_PSM_SHIFT_FSM
#include "psm/rtl_ref_psm_shift_fsm.h"
#include "psm/rtl_ref_psm_idxcnt.h"
#ifdef FX1_A3_PSM_WDATA_MGR
#include "psm/rtl_ref_psm_shift_register.h"
#include "psm/rtl_ref_psm_wdata_manager.h"
#include "psm/rtl_ref_psm_rdata_manager.h"

// Debug hook (trace only): set of SRAM-C write addresses, used by the address-range dumps below.
#include <set>
inline std::set<unsigned int> g_fx1_wr_addrs;
// WRITE address range per context (at most 64 contexts)
inline unsigned int g_fx1_ctx_lo[64];
inline unsigned int g_fx1_ctx_hi[64];
inline unsigned long long g_fx1_ctx_n[64];
inline bool g_fx1_ctx_init = false;
inline unsigned long long g_fx1_wr_site[3] = {0,0,0};  // count per write block   // set of addresses WRITTEN
inline std::set<unsigned int> g_fx1_rd_addrs;   // tap dia chi DA DOC
inline unsigned long long g_fx1_shift_cnt = 0;   // T1c
inline unsigned long long g_fx1_cscan_cnt = 0;   // T1c
inline unsigned long long g_fx1_rd_sel = 0;      // beats taken from the READ path
inline unsigned long long g_fx1_rd_nonzero = 0;  // of which, beats with a NON-ZERO value
inline unsigned long long g_fx1_arr_sel = 0;     // beats taken from the array (control)

#endif
#endif

static constexpr uint32_t RTL_REF_PSM_START_DELAY = 2; // file-scope constant, named apart from psm/psm_top.h's PSM_START_DELAY so both headers can be included together.

namespace sauria_rtl
{
using namespace sauria; // A1 port: shared vector/mask types stay in ::sauria, class bodies move to ::sauria_rtl.

    template <
        int X_DIM = 32,
        int Y_DIM = 32,
        typename T_PSUM = float,
        int SRAMC_CAP = 2048>
    class Psm : public sc_module
    {
    public:
        // Clock & Reset
        sc_in<bool> i_clk{"i_clk"};
        sc_in<bool> i_rstn{"i_rstn"};

        // Data Inputs from Array Scan Chain
        sc_in<psum_vector_t<Y_DIM, T_PSUM>> i_c_arr{"i_c_arr"}; // Data entering from leftmost PE column

        // Memory Interface to SRAM C
        sc_in<uint32_t> i_out_base_addr{"i_out_base_addr"};
        sc_in<psum_vector_t<Y_DIM, T_PSUM>> i_sramc_rdata{"i_sramc_rdata"};
        sc_out<uint32_t> o_sramc_addr{"o_sramc_addr"};
        sc_out<bool> o_sramc_wren{"o_sramc_wren"};
        sc_out<bool> o_sramc_rden{"o_sramc_rden"};
        sc_out<sramc_mask_t<Y_DIM>> o_sramc_wmask{"o_sramc_wmask"};
        sc_out<psum_vector_t<Y_DIM, T_PSUM>> o_sramc_wdata{"o_sramc_wdata"};

        // Config Parameters
        sc_in<uint32_t> i_cxlim{"i_cxlim"};
        sc_in<uint32_t> i_cxstep{"i_cxstep"};
        sc_in<uint32_t> i_cklim{"i_cklim"};
        sc_in<uint32_t> i_ckstep{"i_ckstep"};
        sc_in<uint32_t> i_til_cylim{"i_til_cylim"};
        sc_in<uint32_t> i_til_cystep{"i_til_cystep"};
        sc_in<uint32_t> i_til_cklim{"i_til_cklim"};
        sc_in<uint32_t> i_til_ckstep{"i_til_ckstep"};
        sc_in<uint32_t> i_ncontexts{"i_ncontexts"};
        sc_in<uint32_t> i_total_contexts{"i_total_contexts"};
        sc_in<uint32_t> i_context_id{"i_context_id"};
        sc_in<bool> i_preload_en{"i_preload_en"};
        sc_in<sramc_mask_t<Y_DIM>> i_rows_active{"i_rows_active"}; // Active Rows config (Y bits)
#ifdef FX1_A3_PSM_INACTIVE_COLS
        // X_DIM - X_used, wired into psm_shift_fsm (RTL psm_top.sv:60; sauria_model/psm/psm_top.h:89-91). With 0 the
        // FSM never enters POSTREAD_SHIFT when X_used < X and the preload contribution is lost.
        sc_in<uint32_t> i_inactive_cols{"i_inactive_cols"};   // Number of INACTIVE columns (X - X_used)
#endif

        // Control Inputs from global FSM
        sc_in<bool> i_fsm_start{"i_fsm_start"};
        sc_in<bool> i_fsm_reset{"i_fsm_reset"};
        sc_in<bool> i_pipeline_en{"i_pipeline_en"};

        // Control Outputs to Global FSM / Array
        sc_out<bool> o_done{"o_done"};
        sc_out<bool> o_finalwrite{"o_finalwrite"};
        sc_out<bool> o_shift_done{"o_shift_done"};
        sc_out<bool> o_cscan_en{"o_cscan_en"}; // Directs array to shift out C chain

        // ==== combinational shadows of o_shift_done / o_done ====================
        // Through sc_signal Control would see these one cycle late, while the RTL wires them combinationally
        // (`cdone` is not delayed, so the ALL_BUSY_SHIFT race would be off by one). Control reads the shadows directly.
        bool seam_shift_done_{false};
        bool seam_psm_done_{false};

        // Every write of the two ports goes through these setters (reset and per-cycle defaults included), so the
        // shadows never go stale.
        void set_shift_done_(bool v)
        {
            o_shift_done.write(v);
            seam_shift_done_ = v;
        }
        void set_psm_done_(bool v)
        {
            o_done.write(v);
            seam_psm_done_ = v;
        }
        // =====================================================================

        // Data Outputs to Array (Preload values sent right-to-left)
        sc_out<psum_vector_t<Y_DIM, T_PSUM>> o_c_arr{"o_c_arr"};

        SC_CTOR(Psm)
        {
#ifndef FX1_A3_PSM_SEAM_ORDER
            SC_METHOD(psm_process);
            sensitive << i_clk.pos();
#endif
        }
        // Control's o_outbuf_start, read DIRECTLY (RTL: combinational wire).
        std::function<bool()> peek_fsm_start_{};
        // holds an `outbuf_start` pulse that falls into the reset window (see the reset branch).
        bool pending_start_{false};
        // ORDER_PROBE only -- pre-tick peek source, to compare the three sources.
        std::function<bool()> peek_fsm_start_pre_{};
        // Optional transform of every real SRAM-C write (row addr, data, mask) -- OBP on the write path (GVU Figure 5:
        // PSM -> PSUM stream -> Requant -> LUT_act -> PSUM SRAM). Empty
        // (default) = the write is unchanged, byte for byte; only has::HasNpuTop --obp-inline sets it.
        std::function<void(uint32_t, psum_vector_t<Y_DIM, T_PSUM> &, const sramc_mask_t<Y_DIM> &)> sramc_write_hook_{};
        // Const peek accessors (no side effects). Control reads them before Psm ticks; after Psm has ticked,
        // main_state_ already belongs to the NEXT cycle and the value would be one cycle early.
#ifdef FX1_A3_PSM_SHIFT_FSM
        bool peek_shift_done_now() const { return psm_fsm_.peek_shift_done(); }
        bool peek_done_now() const { return psm_fsm_.peek_done(); }
        // Debug hooks: FSM state / ctx_cnt pass-through (read-only, no effect on behaviour).
        int dbg_fsm_state() const { return psm_fsm_.dbg_main_state(); }
        uint32_t dbg_fsm_ctx_cnt() const { return psm_fsm_.dbg_ctx_cnt(); }
#endif   // `psm_fsm_` only exists under FX1_A3_PSM_SHIFT_FSM
#ifdef FX1_A3_PSM_SEAM_ORDER
        // the top level steps them in order (Control first, then Psm).
        void seam_step() { psm_process(); }
        // set by npu_top: points to Control's shadow seam_pipeline_en_.
        std::function<bool()> peek_pipe_en_{};
#endif

        ///Task#5 retry v3: per-context SRAM-C drain
        // accessors (Gap A2 fix). Backing members stay private below.
        uint32_t get_active_context_id() const { return active_context_id; }
        // Read the last NON-EMPTY pass's addresses, not the live array -- see
        // last_good_addr_'s comment for the measurement that motivates this.
        uint32_t get_recorded_write_count() const { return last_good_write_idx_; }
        // Mask of the recorded write at idx (bit b set => lane b carries
        // real data). Zero means psm_idxcnt masked the whole beat out.
        uint64_t get_recorded_mask(uint32_t idx) const
        {
            return (idx < last_good_write_idx_) ? last_good_mask_[idx] : 0ULL;
        }

        // Runtime counter geometry, needed to turn a recorded flat element
        // index back into a column index.
        uint32_t get_ckstep() const { return i_ckstep.read(); }
        uint32_t get_til_cystep() const { return i_til_cystep.read(); }

        uint32_t get_recorded_addr(uint32_t idx) const
        {
            return (idx < last_good_write_idx_) ? last_good_addr_[idx] : 0;
        }

    private:
        uint32_t start_delay_cnt{0};
        bool start_q{false};

        uint32_t addr_reg{0};
        uint32_t shift_cnt{0};
        uint32_t delay_cnt{0};
        uint32_t write_limit_vectors{0};

#ifdef FX1_A3_PSM_SHIFT_FSM
        // One-cycle compensation: psm_process and grid_process (sa_array.h) are two independent SC_METHODs sensitive
        // only to i_clk.pos(), connected by a plain sc_signal. sc_signal::write() only becomes visible to .read() in
        // the NEXT delta cycle, so i_c_arr.read() here always returns the array's value from the END of the PREVIOUS
        // clock cycle: PSM.write(t) == array.o_c_arr(t - 1). This is SystemC signal semantics, not a
        // process-registration-order effect.
        //
        // Fix: extend the array-facing o_cscan_en by exactly 1 extra cycle
        // at the tail (OR with the previous cycle's fsm_out.cscan_en), and
        // gate the SRAM write on the PREVIOUS cycle's cscan_en instead of
        // the current one. This lets the array perform one additional real
        // shift so that when PSM's (inherently 1-cycle-stale) read finally
        // samples it, it captures the TRUE value from the intended scan
        // step. Confined entirely to this wiring layer -- psm_shift_fsm.h's
        // own ported state machine (1:1 from RTL) is untouched, since real
        // RTL doesn't have this artifact (a register's Q output is visible
        // combinationally to downstream logic in the SAME cycle; this delay
        // is a pure SystemC same-signal, independent-SC_METHOD artifact).
        bool cscan_en_prev_{false};
        // the value actually driven on o_cscan_en in the previous cycle, i.e. what the array consumes this cycle.
        bool cscan_sent_prev_{false};
        // copy of cscan_en advanced on PERMITTED beats, the same domain as the gather stage (cap_eff). Only read
        // under FX1_A3_CSCAN_ARR_BEAT.
        bool cscan_beat_{false};
#ifdef FX1_A3_CARR_GAP_HOLD
        // last REAL value driven on `o_c_arr`, HELD during gate-closed gaps (the array reads one delta late and would
        // latch 0 if 0 were driven).
        psum_vector_t<Y_DIM, T_PSUM> carr_gap_hold_{};
#endif
#ifdef FX1_A3_CSCAN_CAP_PIPE
        // copy delayed by ONE PERMITTED beat (not one raw cycle), used only by the gather into the shift register.
        bool cscan_cap_{false};
#endif
#ifdef FX1_A3_CSCAN_ARR_PIPE
        // separate copy for the PE array's scan gate, delayed by one PERMITTED beat.
        bool cscan_arr_{false};
#endif
#endif

        // cycle counter for cross-referencing PSM's per-row
        // scan timing against ContextFsm's own dbg_cycle trace (main_
        // controller.h), to check the "scan starts early / ends overlapping
        // a reset" hypothesis from Independent counter -- Psm has no
        // access to Control's dbg_cycle, but both tick on the same i_clk, so
        // values are directly comparable once synchronized to a common t=0
        // (e.g. first non-reset tick of the whole simulation).
        uint32_t dbg_cycle{0};

        uint32_t psm_context_cnt{0};
        uint32_t active_context_id{0};
#ifdef FX1_A3_PSM_CTX_OWNCNT
        // the WRITE side's own context counter (instead of capturing i_context_id).
        uint32_t psm_wr_ctx_{0};
        uint32_t psm_ctx_prev_seen_{0};
        bool     psm_ctx_started_{false};
#endif
        uint32_t context_addr_base{0};

        ///Task#5 retry v3: real per-context SRAM-C write
        // addresses, captured as they happen. Reset each time PSM's own FSM
        // restarts a scan pass (start_pulse, see below) -- for a real
        // context that needs multiple internal K-accumulation passes (Task
        // #5's measurement: fsm_out.done can pulse ~100x more often than
        // there are real contexts), this means recorded_addr_[] always
        // holds just the MOST RECENT pass's address set by the time it's
        // read -- exactly what's needed, since every pass writes the same
        // address set and the last one holds the fully-accumulated value.
        // The caller (tb_evaluate.cpp) must NOT read this on PSM's own
        // done pulse (measured to fire too often); it must wait for the
        // REAL context id (i_context_id / main_controller.h's own
        // entering_next_context edge) to actually change.
        static constexpr uint32_t MAX_RECORDED_WRITES = 128;
        uint32_t recorded_addr_[MAX_RECORDED_WRITES]{};
        uint64_t recorded_mask_[MAX_RECORDED_WRITES]{};
        uint64_t last_good_mask_[MAX_RECORDED_WRITES]{};
        uint32_t recorded_write_idx_{0};

        // The PSM restarts its scan several times per tile (an empty pass, the real write passes, then a trailing
        // empty pass that starts before the tile reports done). The trailing empty pass would reset
        // recorded_write_idx_ before a testbench reads it, so a copy of the most recent NON-EMPTY pass is kept. On each restart,
        // promote recorded_addr_[] into last_good_addr_[] only if that pass
        // actually wrote something; an empty pass leaves the copy untouched.
        // tb_evaluate.cpp reads the copy, never the live array.
        uint32_t last_good_addr_[MAX_RECORDED_WRITES]{};
        uint32_t last_good_write_idx_{0};

        // PSM scan timing phase
        // 0 = idle
        // 1 = write current scan_out as x0
        // 2 = enable scan, no write
        // 3 = wait scan_out update, no write
        // 4 = normal scan write x1..x15
        uint32_t psm_scan_phase{0};

        bool shifting{false};
        bool preload_mode{false};

#ifdef FX1_A3_PSM_SHIFT_FSM
        // 1:1-ported control-timing FSM + address counter (see psm/rtl_ref_psm_shift_fsm.h and rtl_ref_psm_idxcnt.h).
        sauria_rtl::PsmShiftFsm<X_DIM> psm_fsm_;
        // SRAMC_N (2nd template arg) = Y_DIM for this project
        // (real MEMC_W = Y*OC_W, traced from Python hw_versions.py:458, NOT
        // sauria_logic.sv's un-overridden default SRAMC_W=64 -- confirmed
        // against real OUT.CXLIM=64/CXSTEP=32 register values, which only
        // match the formula o_cxlim=Y_used+SRAMC_N / o_cxstep=SRAMC_N when
        // SRAMC_N=32=Y_DIM, not 2).
        sauria_rtl::PsmIdxCnt<11, Y_DIM> psm_idxcnt_;

#ifdef FX1_A3_PSM_WDATA_MGR
        // real output-side datapath (previously approximated
        // by calc_c_addr()+direct single-shot Y-vector write below). Ported
        // from psm_shift_register.sv + psm_wdata_manager.sv, now wired to
        // psm_idxcnt_'s real sram_addr/mask/wr_fifo_pop outputs (extended
        // above) instead of the old calc_c_addr() approximation.
        sauria_rtl::PsmShiftRegister<X_DIM, psum_vector_t<Y_DIM, T_PSUM>> psm_output_shreg_;
        sauria_rtl::PsmWdataManager<Y_DIM, Y_DIM, psum_vector_t<Y_DIM, T_PSUM>, T_PSUM> psm_wdata_mgr_;

        // the READ-back manager. Ported at but left
        // UNWIRED until now -- that gap is exactly why buff_din's else-branch
        // was a hardcoded zero and why buff_shift_en was missing RTL's
        // 4th term (`fifo_push`), leaving the output shift register empty by
        // the time WRITING began (measured in). Wiring it supplies both
        // psm_top.sv:266's `fifo_push` and :272's `buff_sram_din`.
        sauria_rtl::PsmRdataManager<Y_DIM, Y_DIM, psum_vector_t<Y_DIM, T_PSUM>, T_PSUM> psm_rdata_mgr_;

        // psm_top.sv's own SRAM read-data latency-equalization register
        // (lines 278-287): sramc_rdata_q <= i_sramc_rdata, enabled by
        // rd_feed_en. The rdata manager consumes the REGISTERED value.
        psum_vector_t<Y_DIM, T_PSUM> sramc_rdata_q_{};
#ifdef FX1_A3_SRAMC_RDEN_PHASE
        // the RTL's second SRAM read stage (gated by rden).
        psum_vector_t<Y_DIM, T_PSUM> sramc_s2_{};
        bool sramc_rden_prev_{false};
#endif

        // psm_top.sv's own address/wren shimming registers (lines 288-322),
        // NOT part of any submodule -- ported here directly as plain member
        // state, matching how this file already keeps other ad-hoc FSM
        // state as plain members.
        uint32_t sramc_addr_q1_{0}, sramc_addr_q2_{0}, sramc_addr_q3_{0};
        uint32_t sramc_addr_q4_{0};
        bool sramc_wren_q4_{false};
        bool sramc_wren_q1_{false}, sramc_wren_q2_{false}, sramc_wren_q3_{false};
#endif
#endif

        void dump_psm_trace(
            uint32_t context,
            uint32_t addr,
            uint32_t shift,
            const psum_vector_t<Y_DIM, T_PSUM> &data,
            uint32_t cyc = 0,
            uint64_t wmask_bits = ~0ULL)
        {
#ifndef FX1_A3_PSMWRITE_DUMP
            if (!FX1_DUMPS) return;
#endif  // narrow guard: enables psm_write_trace.csv only (debug hook, default off)
            // Only dump STD instance, avoid std/approx/gated duplicated traces.
            std::string inst_name = this->name();
            if (inst_name.find("NpuTop_std") == std::string::npos)
            {
                return;
            }

            static std::ofstream psm_trace("trace_sysc/psm_write_trace.csv");
            static bool psm_trace_header = false;
            static uint32_t psm_trace_count = 0;

            if (!psm_trace_header)
            {
                // "cyc" cross-references main_controller.h's [A3TRACE] dbg_cycle.
                psm_trace << "time,write_count,cyc,context,addr,shift_cnt,wmask";
                for (int y = 0; y < Y_DIM; y++)
                {
                    psm_trace << ",lane" << y;
                }
                psm_trace << "\n";
                psm_trace_header = true;
            }

            psm_trace
                << sc_core::sc_time_stamp() << ","
                << psm_trace_count << ","
                << cyc << ","
                << context << ","
                << addr << ","
                << shift << ","
                << wmask_bits;   // 0 = filler beat, memory NOT touched

            for (int y = 0; y < Y_DIM; y++)
            {
                psm_trace << "," << static_cast<int32_t>(data[y]);
            }

            psm_trace << "\n";
            psm_trace.flush();

            psm_trace_count++;
        }

        uint32_t get_write_limit_vectors()
        {
            if (i_cxlim.read() != 0)
            {
                return i_cxlim.read();
            }
            return 0;
        }

        uint32_t calc_c_addr(uint32_t global_context, uint32_t x)
        {
            uint32_t nctx = i_ncontexts.read();
            if (nctx == 0)
            {
                nctx = 1;
            }

            uint32_t cxlim = i_cxlim.read();
            if (cxlim == 0)
            {
                cxlim = X_DIM;
            }

            uint32_t out_tile = global_context / nctx;
            uint32_t local_ctx = global_context % nctx;

            uint32_t one_output_tile_elements =
                nctx * cxlim * Y_DIM;

            // SAURIA C/output layout:
            //
            //   C[out_tile][x][local_context][y]
            //
            // element base address for one Y-vector:
            //
            //   addr =
            //       out_tile  * (nctx * cxlim * Y_DIM)
            //     + x         * (nctx * Y_DIM)
            //     + local_ctx * Y_DIM
            //
            return out_tile * one_output_tile_elements + x * (nctx * Y_DIM) + local_ctx * Y_DIM;
        }

        void psm_process()
        {
            if (!i_rstn.read() || i_fsm_reset.read())
            {
                // o_sramc_addr.write(i_out_base_addr.read());
                o_sramc_addr.write(0);
                o_sramc_wren.write(false);
                o_sramc_rden.write(false);
                o_sramc_wmask.write(sramc_mask_t<Y_DIM>());
                o_sramc_wdata.write(psum_vector_t<Y_DIM, T_PSUM>());

                set_psm_done_(false);
                o_finalwrite.write(false);
                set_shift_done_(false);
                o_cscan_en.write(false);
                o_c_arr.write(psum_vector_t<Y_DIM, T_PSUM>());

                addr_reg = 0;
                shift_cnt = 0;
                delay_cnt = 0;
                write_limit_vectors = 0;
                psm_scan_phase = 0;

                start_delay_cnt = 0;
                start_q = false;

                shifting = false;
                preload_mode = false;

                psm_context_cnt = 0;
                context_addr_base = 0;
                active_context_id = 0;
                dbg_cycle = 0;
                recorded_write_idx_ = 0;
                // last_good_* is deliberately NOT cleared on i_fsm_reset:
                // that signal (Control's o_outbuf_reset) also fires at each
                // tile's DONE, i.e. exactly when
                // tb_evaluate.cpp is about to read the drained addresses --
                // clearing here would wipe them again. Only a true hardware
                // reset clears it.
                if (!i_rstn.read())
                {
                    last_good_write_idx_ = 0;
                }

#ifdef FX1_A3_PSM_SHIFT_FSM
                psm_fsm_.reset();
                psm_idxcnt_.reset();
                cscan_en_prev_ = false;
                cscan_sent_prev_ = false;
                cscan_beat_ = false;
#ifdef FX1_A3_CSCAN_CAP_PIPE
                cscan_cap_ = false;
#endif
#ifdef FX1_A3_CSCAN_ARR_PIPE
                cscan_arr_ = false;
#endif
#ifdef FX1_A3_PSM_WDATA_MGR
                psm_output_shreg_.reset();
                psm_wdata_mgr_.reset();
                psm_rdata_mgr_.reset();
                sramc_rdata_q_ = psum_vector_t<Y_DIM, T_PSUM>();
#ifdef FX1_A3_SRAMC_RDEN_PHASE
                sramc_s2_ = psum_vector_t<Y_DIM, T_PSUM>();
                sramc_rden_prev_ = false;
#endif
                sramc_addr_q1_ = sramc_addr_q2_ = sramc_addr_q3_ = 0;
                sramc_wren_q1_ = sramc_wren_q2_ = sramc_wren_q3_ = false;
#endif
#endif
#ifdef FX1_A3_PSM_START_DIRECT
                // The first `outbuf_start` pulse can fall into psm_process's RESET window. Through an sc_signal the 1-cycle
                // delay would carry it past the reset; with the peek it does not, so it is latched and consumed on the
                // first ACTIVE cycle.
                if (peek_fsm_start_ && peek_fsm_start_())
                    pending_start_ = true;
#endif
                return;
            }

            dbg_cycle++;
#ifdef FX1_A3_PSM_SEAM_ORDER
            // Control has already stepped, so the shadow holds THIS cycle's value.
            const bool pipe_en_now_ = peek_pipe_en_ ? peek_pipe_en_()
                                                    : i_pipeline_en.read();
#else
            const bool pipe_en_now_ = i_pipeline_en.read();
#endif
#ifdef FX1_A3_PSM_START_DIRECT
            const bool fsm_start_now_ =
                (peek_fsm_start_ ? peek_fsm_start_() : i_fsm_start.read())
                || pending_start_;
            pending_start_ = false;         // consumed exactly once
#else
            const bool fsm_start_now_ = i_fsm_start.read();
#endif
#ifdef FX1_A3_ORDER_PROBE
            {
                fx1a3::order_psm() = ++fx1a3::order_seq();
                static std::ofstream op("trace_sysc/order_probe.csv");
                static bool oph = false;
                static long opn = 0;
                if (!oph) { op << "cyc,first,sc_signal,shadow,peek\n"; oph = true; }
                const bool sig = i_fsm_start.read();
                const bool sha = peek_fsm_start_     ? peek_fsm_start_()     : false;
                const bool pre = peek_fsm_start_pre_ ? peek_fsm_start_pre_() : false;
                if (opn < 3000 && (sig || sha || pre || opn < 30))
                {
                    // compare TIMESTAMPS, not sequence numbers (the broken version).
                    const bool ctrl_da_chay =
                        (fx1a3::ctrl_tsim() == sc_core::sc_time_stamp().value());
                    op << dbg_cycle << ","
                       << (ctrl_da_chay ? "CTRL_FIRST" : "PSM_FIRST") << ","
                       << (int)sig << "," << (int)sha << "," << (int)pre << "\n";
                    op.flush(); opn++;
                }
            }
#endif

            // Default outputs each cycle
            o_sramc_wren.write(false);
            o_sramc_rden.write(false);
            set_psm_done_(false);
            set_shift_done_(false);
            o_finalwrite.write(false);
            o_cscan_en.write(false);

#ifdef FX1_A3_PSM_SHIFT_FSM
            // -----------------------------------------------------------------
            // PsmShiftFsm / PsmIdxCnt drive the RTL scan-out control timing (o_cscan_en's exact assertion pattern).
            // force_write_path (without FX1_A3_PSM_REAL_PRELOAD) bypasses the preload-warmup ctx_cnt < 3 branch.
            // -----------------------------------------------------------------
            {
                bool start_pulse = fsm_start_now_ && !start_q;
                start_q = fsm_start_now_;

                typename sauria_rtl::PsmShiftFsm<X_DIM>::Inputs fsm_in;
                fsm_in.fsm_start = fsm_start_now_;
                fsm_in.fsm_reset = false; // handled by the top-of-method reset branch (early return)
                fsm_in.ncontexts = i_ncontexts.read();
#ifdef FX1_A3_PSM_INACTIVE_COLS
                // Without this value the FSM never enters POSTREAD_SHIFT (psm_shift_fsm.sv:272), the preload read from
                // SRAM-C is not shifted far enough to reach its array column, and it contributes 0 when X_used < X.
                fsm_in.inactive_cols = i_inactive_cols.read();
#else
                fsm_in.inactive_cols = 0;        // Psm has no real i_inactive_cols port
#endif
                fsm_in.pipeline_en = pipe_en_now_;
#ifdef FX1_A3_PSM_REAL_PRELOAD
                // wire the REAL preload_en (the port already
                // exists, was just ignored) and drop force_write_path, so the
                // FSM can actually enter its READ states -- without this the
                // read path (and therefore psm_rdata_manager, wired in)
                // is never activated at all: rd_feeder_en stays false forever,
                // so fifo_push is always false and fifo_din always zero.
                fsm_in.preload_en = i_preload_en.read();
                fsm_in.force_write_path = false;
#else
                fsm_in.preload_en = false;      // scoped out -- see psm_shift_fsm.h header note
                fsm_in.force_write_path = true;  // scoped out -- bypasses preload-warmup ctx_cnt<3 branch
#endif

                auto fsm_out = psm_fsm_.compute_outputs(fsm_in);
#ifdef FX1_A3_PSM_TAPE
                // Debug hook (default off): one line EVERY cycle (unfiltered), std instance only; joins fsm_tape.csv on tsim.
                if (std::string(this->name()).find("NpuTop_std") !=
                    std::string::npos)
                {
                    static std::ofstream pt("trace_sysc/psm_tape.csv");
                    static bool pt_hdr = false;
                    if (!pt_hdr)
                    {
                        pt << "tsim,cyc,out_status,fsm_start,start_pulse,"
                              "cscan_en,pipeline_en,scan_cnt,ctx_cnt,cyc_cnt,"
                              "shift_cnt,done,shift_done,finalwrite\n";
                        pt_hdr = true;
                    }
                    pt << sc_core::sc_time_stamp().value() << ","
                       << dbg_cycle << ","
                       << fsm_out.out_status << ","
                       << (int)fsm_start_now_ << ","
                       << (int)start_pulse << ","
                       << (int)fsm_out.cscan_en << ","
                       << (int)pipe_en_now_ << ","
                       << psm_fsm_.dbg_scan_cnt() << ","
                       << psm_fsm_.dbg_ctx_cnt() << ","
                       << psm_fsm_.dbg_cyc_cnt() << ","
                       << shift_cnt << ","
                       << (int)fsm_out.done << ","
                       << (int)fsm_out.shift_done << ","
                       << (int)fsm_out.finalwrite << "\n";
                }
#endif
#ifdef FX1_A3_PSMROUND_PROBE
                // Debug hook (default off): counts PSM rounds. FINISH (out_status == 11) is the only state that increments
                // ctx_cnt (RTL psm_shift_fsm.sv:681/684), so n_finish == ctx_cnt; finalwrite needs ctx_cnt == ncontexts+2.
                if (std::string(this->name()).find("NpuTop_std") != std::string::npos)
                {
                    static std::ofstream pr("trace_sysc/psm_round.csv");
                    static bool pr_hdr = false;
                    static int pr_prev = -1;
                    static uint64_t pr_nstart = 0, pr_nfinish = 0, pr_nfw = 0;
                    if (!pr_hdr)
                    {
                        pr << "tsim,ev,out_status,finalwrite,n_start,n_finish,"
                              "ncontexts\n";
                        pr_hdr = true;
                    }
                    const int pr_st = (int)fsm_out.out_status;
                    const bool pr_fin = (pr_st == 11 && pr_prev != 11);
                    if (start_pulse) pr_nstart++;
                    if (pr_fin) pr_nfinish++;
                    if (fsm_out.finalwrite) pr_nfw++;
                    if (start_pulse || pr_fin || (fsm_out.finalwrite && pr_nfw < 4))
                    {
                        pr << sc_core::sc_time_stamp().value() << ","
                           << (start_pulse ? "start" : (pr_fin ? "finish" : "fw"))
                           << "," << pr_st << "," << (int)fsm_out.finalwrite << ","
                           << pr_nstart << "," << pr_nfinish << ","
                           << i_ncontexts.read() << "\n";
                        pr.flush();
                    }
                    pr_prev = pr_st;
                }
#endif

                // Self-caught bug (found via demo_gemm_32x32 showing IDENTICAL
                // values to the pre-PsmShiftFsm wiring, i.e. zero effect): the
                // real ContextFsm can pulse o_outbuf_start MORE THAN ONCE per
                // context (a priming pulse at FIRST_SHIFT, before real compute
                // has happened, THEN the real one at SCND_SHIFT_STALL -- see
                //'s own tracing). The pre-existing model guards
                // against this with `if (start_pulse && !shifting)`; an
                // earlier version of THIS block reset shift_cnt/
                // active_context_id on EVERY start_pulse unconditionally,
                // corrupting an in-progress capture sequence when the second
                // (real) pulse arrived mid-scan. Gate on out_status==0 (IDLE)
                // instead -- PsmShiftFsm's own transition logic already
                // ignores i_fsm_start while busy (IDLE's is the only branch
                // that reads it), so this exactly mirrors that same
                // "ignore a start while already active" protection.
                if (start_pulse && fsm_out.out_status == 0)
                {
                    // Promote the pass that just ended, if it wrote anything.
                    // An empty (priming/trailing) pass leaves the copy alone.
                    if (recorded_write_idx_ > 0)
                    {
                        for (uint32_t i = 0; i < recorded_write_idx_; i++)
                        {
                            last_good_addr_[i] = recorded_addr_[i];
                            last_good_mask_[i] = recorded_mask_[i];
                        }
                        last_good_write_idx_ = recorded_write_idx_;
                    }

#ifdef FX1_A3_DRAIN_DIAG
                    // Debug hook (default off): prints old_wc vs lastgood_wc at each scan restart, to check the per-context
                    // drain (last_good_addr_ below) on a new case.
                    {
                        std::string inst_name = this->name();
                        if (inst_name.find("NpuTop_std") != std::string::npos)
                        {
                            std::cout << "[DIAG CTXSTART] cyc=" << dbg_cycle
                                      << " old_ctx=" << active_context_id
                                      << " old_wc=" << recorded_write_idx_
                                      << " lastgood_wc=" << last_good_write_idx_
                                      << " new_ctx=" << i_context_id.read()
                                      << std::endl;
                        }
                    }
#endif

#ifdef FX1_A3_PSM_CTX_OWNCNT
                    // count the PSM's OWN write passes instead of capturing the controller's context id.
                    {
                        const uint32_t nctx_ = i_ncontexts.read();
                        const uint32_t cur_ = i_context_id.read();
                        if (psm_ctx_started_ && cur_ < psm_ctx_prev_seen_)
                        {
                            psm_wr_ctx_ = 0; // next tile
                        }
                        else if (psm_ctx_started_ && recorded_write_idx_ > 0)
                        {
                            psm_wr_ctx_ =
                                (nctx_ > 0) ? ((psm_wr_ctx_ + 1) % nctx_) : 0;
                        }
                        psm_ctx_prev_seen_ = cur_;
                        psm_ctx_started_ = true;
                        active_context_id = psm_wr_ctx_;
                    }
#elif defined(FX1_A3_PSM_CTX_LAG1)
                    // Diagnostic experiment (default off), not the adopted behaviour.
                    {
                        const uint32_t nctx_ = i_ncontexts.read();
                        const uint32_t cur_ = i_context_id.read();
                        active_context_id =
                            (nctx_ > 0) ? ((cur_ + nctx_ - 1) % nctx_) : cur_;
                    }
#else
                    active_context_id = i_context_id.read();
#endif
                    shift_cnt = 0;
                    write_limit_vectors = get_write_limit_vectors();
                    recorded_write_idx_ = 0;

                    DBG_COUT << "[PSM A3 START]"
                              << " global_context = " << active_context_id
                              << " / total_contexts = " << i_total_contexts.read()
                              << " ncontexts = " << i_ncontexts.read()
                              << std::endl;
                }

                // psm_shift_fsm computes o_sramc_rden correctly
                // (true in RD_CNT_START/READING/READING_LAT_WAIT, matching
                // psm_shift_fsm.sv:432/458/484) and psm_top.sv:225 wires it
                // straight out -- but this port never consumed it, leaving
                // o_sramc_rden hardcoded false. sram_top.h:372 gates the read
                // on it, so o_sramc_rdata never got real data -> sramc_rdata_q_
                // = 0 -> psm_rdata_manager's fifo_din = 0 -> preload accumulate
                // contributed nothing. Self-gating: without REAL_PRELOAD the
                // read states are unreachable, so this stays false.
                o_sramc_rden.write(fsm_out.sramc_rden);

                typename sauria_rtl::PsmIdxCnt<11, Y_DIM>::Inputs idx_in;
                idx_in.cnt_en = fsm_out.cnt_en;
                idx_in.cnt_clear = fsm_out.cnt_clear;
                idx_in.wr_flag = fsm_out.wr_flag;
                idx_in.cxlim = i_cxlim.read();
                idx_in.cxstep = i_cxstep.read();
                idx_in.cklim = i_cklim.read();
                idx_in.ckstep = i_ckstep.read();
                idx_in.til_cylim = i_til_cylim.read();
                idx_in.til_cystep = i_til_cystep.read();
                idx_in.til_cklim = i_til_cklim.read();
                idx_in.til_ckstep = i_til_ckstep.read();

                {
                    static bool printed_idxcfg = false;
                    std::string inst_name = this->name();
                    if (!printed_idxcfg && inst_name.find("NpuTop_std") != std::string::npos)
                    {
                        printed_idxcfg = true;
                        std::cout << "[PSM IDXCFG] cxlim=" << idx_in.cxlim
                                  << " cxstep=" << idx_in.cxstep
                                  << " cklim=" << idx_in.cklim
                                  << " ckstep=" << idx_in.ckstep
                                  << " til_cylim=" << idx_in.til_cylim
                                  << " til_cystep=" << idx_in.til_cystep
                                  << " til_cklim=" << idx_in.til_cklim
                                  << " til_ckstep=" << idx_in.til_ckstep
                                  << " ncontexts=" << i_ncontexts.read()
                                  << std::endl;
                    }
                }
#ifdef FX1_A3_PSM_WDATA_MGR
                idx_in.start = fsm_out.cnt_start;
#endif

                auto idx_out = psm_idxcnt_.tick(idx_in);

#ifdef FX1_A3_IDXCNT_TRACE
                {
                    std::string inst_name = this->name();
                    if (inst_name.find("NpuTop_std") != std::string::npos &&
                        (fsm_out.cnt_en || fsm_out.cnt_clear || fsm_out.cnt_start))
                    {
                        static std::ofstream idxcnt_trace("trace_sysc/idxcnt_trace.csv");
                        static bool idxcnt_trace_header = false;
                        if (!idxcnt_trace_header)
                        {
                            idxcnt_trace << "cyc,active_context,cnt_en,cnt_clear,cnt_start,wr_flag,sram_addr,mask,done,til_done\n";
                            idxcnt_trace_header = true;
                        }
                        idxcnt_trace
                            << dbg_cycle << ","
                            << active_context_id << ","
                            << fsm_out.cnt_en << ","
                            << fsm_out.cnt_clear << ","
                            << fsm_out.cnt_start << ","
                            << fsm_out.wr_flag << ","
                            << idx_out.sram_addr << ","
                            << idx_out.mask << ","
                            << idx_out.done << ","
                            << idx_out.til_done << "\n";
                        idxcnt_trace.flush();
                    }
                }
#endif

                fsm_in.done = idx_out.done;
                fsm_in.til_done = idx_out.til_done;
                psm_fsm_.commit(fsm_in);
#ifdef FX1_A3_PSM_DONE_NEXT
                // Diagnostic option (default off): after commit, main_state_ is the NEXT cycle's state; recompute the output
                // logic on it and push next cycle's shift_done / done into the shadows, so Control sees shift_done one
                // cycle early. compute_outputs() only writes two cached_* values that are overwritten at the start of the
                // next cycle before commit() uses them, so the extra call is safe.
                {
                    auto nxt = psm_fsm_.compute_outputs(fsm_in);
                    seam_shift_done_ = nxt.shift_done;
                    seam_psm_done_   = nxt.done;
                }
#endif

#ifdef FX1_A3_PSM_WDATA_MGR
                // real RTL datapath (psm_top.sv lines 262-324),
                // replacing the calc_c_addr()+direct-write approximation.
                {
                    // temporary isolation toggles (all default
                    // OFF = full real behavior) to determine WHICH of the 3
                    // things this pipeline changed vs the old calc_c_addr()
                    // path -- (a) fsm_out.buff_shift feeding buff_shift_en/
                    // buff_din for the first time anywhere in this codebase,
                    // (b) the real idxcnt-derived address, (c) the real
                    // idxcnt-derived mask -- is actually responsible for the
                    // improvement. Each compiles independently; not
                    // meant to be kept on together with normal operation.
#ifdef FX1_A3_ISO_NO_BUFFSHIFT
                    const bool iso_buff_shift = false;
#else
                    const bool iso_buff_shift = fsm_out.buff_shift;
#endif

                    // ---- Read Data Manager (psm_top.sv:154-172) ----
                    // WIRED as of this change (was the last
                    // ported-but-unwired module; its absence forced
                    // buff_sram_din=0 and dropped fifo_push from
                    // buff_shift_en, which measured as "shift register
                    // is empty by the time WRITING starts").
                    //
                    // Runs BEFORE buff_shift_en/buff_din are computed because
                    // both consume its combinational outputs this same cycle
                    // (RTL: fifo_push and buff_sram_din are plain wires off
                    // this submodule). Its i_sramc_data is the REGISTERED
                    // sramc_rdata_q_ (psm_top.sv:278-287), read pre-tick here
                    // and committed at the end of this block.
                    typename sauria_rtl::PsmRdataManager<Y_DIM, Y_DIM,
                        psum_vector_t<Y_DIM, T_PSUM>, T_PSUM>::Inputs rdata_in;
                    rdata_in.sramc_data = sramc_rdata_q_;
                    {
                        uint64_t ra = 0;
                        sramc_mask_t<Y_DIM> ra_bits = i_rows_active.read();
                        for (int j = 0; j < Y_DIM; j++)
                            if (ra_bits[j])
                                ra |= (uint64_t(1) << j);
                        rdata_in.rows_active = ra;
                    }
                    rdata_in.feeder_en = fsm_out.rd_feeder_en;
                    rdata_in.clearbuff = fsm_out.rd_feeder_clear;
                    rdata_in.mask = idx_out.mask;

                    auto rdata_out = psm_rdata_mgr_.tick(rdata_in);

                    // buff_shift_en (psm_top.sv:266) -- now with RTL's full
                    // 4 terms including fifo_push.
                    //
                    // The array_out / buff_din capture has the same sc_signal staleness as the simple path below
                    // (cscan_en_prev_): i_c_arr.read() is the array's value from the END of the PREVIOUS cycle, so the
                    // capture is gated on cscan_en_prev_ instead of fsm_out.cscan_en.
#if defined(FX1_A3_CSCAN_CAP_NODELAY)
                    // drops the EXTRA beat of delay: `o_c_arr` is written AFTER step() and `i_c_arr.read()` lags one cycle
                    // (sc_signal semantics); the two cancel, so the value read at beat t already equals the RTL's
                    // `o_c_arr` at beat t.
                    const bool cscan_cap_eff = fsm_out.cscan_en;
#elif defined(FX1_A3_CSCAN_CAP_PIPE)
                    const bool cscan_cap_eff = cscan_cap_;
#else
#ifdef FX1_A3_CSCAN_CAP_SENT
                    // grouped by the array signal just consumed.
                    const bool cscan_cap_eff = cscan_sent_prev_;
#else
                    const bool cscan_cap_eff = cscan_en_prev_;
#endif
#endif
#if defined(FX1_A3_CSCAN_CAP_PIPE) && defined(FX1_A3_CSCAN_CAP_FLUSH)
                    // complete an outstanding gather beat AS SOON AS the scan window closes. Waiting for the next PERMITTED
                    // beat while the cycles after the window are stalled would land it after the write phase has released
                    // (a column would be lost).
                    const bool cscan_cap_flush_ = cscan_cap_eff && !fsm_out.cscan_en;
                    const bool cscan_cap_fire =
                        cscan_cap_eff && (pipe_en_now_ || cscan_cap_flush_);
#else
                    const bool cscan_cap_fire = cscan_cap_eff && pipe_en_now_;
#endif
                    const bool buff_shift_en =
                        (cscan_cap_fire) ||
                        iso_buff_shift ||
                        rdata_out.fifo_push ||
                        psm_wdata_mgr_.peek_fifo_pop();
                    if (buff_shift_en)    ++g_fx1_shift_cnt;   // T1c
                    if (fsm_out.cscan_en) ++g_fx1_cscan_cnt;   // T1c

                    psum_vector_t<Y_DIM, T_PSUM> array_out = i_c_arr.read();
                    // buff_din (psm_top.sv:272) -- the else-branch is the REAL
                    // read-back bus (o_fifo_din) now, not a hardcoded zero.
                    psum_vector_t<Y_DIM, T_PSUM> buff_din =
                        (iso_buff_shift || cscan_cap_fire)
                            ? array_out
                            : rdata_out.fifo_din;
                    // observation only -- behaviour unchanged.
                    if (iso_buff_shift || cscan_cap_fire) {
                        ++g_fx1_arr_sel;
                    } else {
                        ++g_fx1_rd_sel;
                        for (int _y = 0; _y < Y_DIM; ++_y)
                            if (rdata_out.fifo_din[_y] != 0) { ++g_fx1_rd_nonzero; break; }
                    }

#ifdef FX1_A3_ISO_NO_SHREG_DELAY
                    // Bypass the shift register's X_DIM-cycle delay entirely
                    // (still call tick() to keep its internal state
                    // consistent/advancing, but ignore its delayed output).
                    psm_output_shreg_.tick(buff_din, buff_shift_en, fsm_out.buff_clear);
                    psum_vector_t<Y_DIM, T_PSUM> buff_dout = buff_din;
#else
                    psum_vector_t<Y_DIM, T_PSUM> buff_dout =
                        psm_output_shreg_.tick(buff_din, buff_shift_en, fsm_out.buff_clear);
#endif

#ifdef FX1_A3_CARR_DRIVE
                    // Drive o_cscan_en on the LIVE path (RTL psm_top.sv:227 takes it straight from psm_shift_fsm); otherwise
                    // it keeps its reset value and the PEs never latch i_c (sc_reg_en = (i_cscan_en | cswitch) & pipeline_en).
                    // These two signals must move TOGETHER with o_c_arr.
                    o_cscan_en.write(fsm_out.cscan_en || cscan_en_prev_);

                    // mirrors RTL psm_top.sv:324
                    //     assign o_c_arr = (o_cscan_en) ? buff_dout_arr : 0;
                    // so the preload read from SRAM-C actually reaches the PE array.
                    {
#ifdef FX1_A3_CARR_GAP_HOLD
                        // Same gate expression as below: change both together.
                        const bool carr_gate_open_ =
#  ifdef FX1_A3_CARR_MUX_BEAT
                            (pipe_en_now_ ? (bool)fsm_out.cscan_en : cscan_beat_);
#  elif defined(FX1_A3_CARR_MUX_MATCH)
                            (bool)fsm_out.cscan_en;
#  else
                            ((bool)fsm_out.cscan_en || cscan_en_prev_);
#  endif
#endif
                        psum_vector_t<Y_DIM, T_PSUM> carr_out;
                        // Same condition as o_cscan_en above. Gating on fsm_out.cscan_en alone would make the array latch 0 on
                        // the EXTENDED beat and erase the value just loaded (two gates one beat apart cancel each other).
#ifdef FX1_A3_CARR_MUX_MATCH
                        // RTL: `assign o_c_arr = (o_cscan_en) ? buff_dout_arr : 0;`
                        // -- the mux uses the SAME signal as the array's latch gate. A wider condition would pump a stale value
                        // into the first column of the scan chain.
                        if (fsm_out.cscan_en)
#else
                        if (fsm_out.cscan_en || cscan_en_prev_)
#endif
#ifdef FX1_A3_CARR_POSTTICK
                            // Take the value AFTER the shift: o_c_arr reaches the array through an sc_signal, one cycle later than
                            // in the RTL.
                            carr_out = psm_output_shreg_.out_now();
#else
                            carr_out = buff_dout;
#endif
#ifdef FX1_A3_CARR_GAP_HOLD
                        if (carr_gate_open_) carr_gap_hold_ = carr_out;
                        else                 carr_out = carr_gap_hold_;
#endif
                        o_c_arr.write(carr_out);
                    }
#endif

#ifdef FX1_A3_IDXCNT_TRACE
                    {
                        std::string inst_name = this->name();
                        if (inst_name.find("NpuTop_std") != std::string::npos &&
                            fsm_out.out_status >= 1 && fsm_out.out_status <= 10)
                        {
                            static std::ofstream shreg_trace("trace_sysc/shreg_trace.csv");
                            static bool shreg_trace_header = false;
                            if (!shreg_trace_header)
                            {
                                shreg_trace << "tsim,cyc,out_status,cscan_en,buff_shift,wr_feeder_en,buff_shift_en,wdata_peek_pop,idx_sram_addr,idx_mask,array_out0,buff_din0,buff_dout0,rd_feeder_en,sramc_rden,addr_q3,i_rdata0,rdata_q0,rd_push,rd_din0,cscan_prev,cscan_eff,carr_would_be,cap_eff,pipe_en\n";
                                shreg_trace_header = true;
                            }
                            shreg_trace << sc_core::sc_time_stamp().value() << ",";
                            shreg_trace
                                << dbg_cycle << ","
                                << fsm_out.out_status << ","
                                << fsm_out.cscan_en << ","
                                << fsm_out.buff_shift << ","
                                << fsm_out.wr_feeder_en << ","
                                << buff_shift_en << ","
                                << psm_wdata_mgr_.peek_fifo_pop() << ","
                                << idx_out.sram_addr << ","
                                << idx_out.mask << ","
                                << static_cast<int32_t>(array_out[0]) << ","
                                << static_cast<int32_t>(buff_din[0]) << ","
                                << static_cast<int32_t>(buff_dout[0]) << ","
                                << fsm_out.rd_feeder_en << ","
                                << fsm_out.sramc_rden << ","
                                << sramc_addr_q3_ << ","
                                << static_cast<int32_t>(i_sramc_rdata.read()[0]) << ","
                                << static_cast<int32_t>(sramc_rdata_q_[0]) << ","
                                << rdata_out.fifo_push << ","
                                << static_cast<int32_t>(rdata_out.fifo_din[0]) << ","
                                << cscan_en_prev_ << ","
                                << (fsm_out.cscan_en || cscan_en_prev_) << ","
                                << static_cast<int32_t>(
                                       (fsm_out.cscan_en || cscan_en_prev_)
                                           ? buff_dout[0]
                                           : T_PSUM(0)) << ","
                            << (int)cscan_cap_eff << ","
                            << (int)pipe_en_now_ << "\n";
                            shreg_trace.flush();
                        }
                    }
#endif

                    typename sauria_rtl::PsmWdataManager<Y_DIM, Y_DIM,
                        psum_vector_t<Y_DIM, T_PSUM>, T_PSUM>::Inputs wdata_in;
                    wdata_in.fifo_dout = buff_dout;
                    wdata_in.feeder_en = fsm_out.wr_feeder_en;
                    wdata_in.clearbuff = fsm_out.wr_feeder_clear;
                    wdata_in.mask = idx_out.mask;
                    wdata_in.fifo_pop = idx_out.wr_fifo_pop;

#ifdef FX1_A3_IDXCNT_TRACE
                    const uint32_t pre_elm_cnt = psm_wdata_mgr_.peek_elm_cnt();
                    const uint64_t pre_mask_q1 = psm_wdata_mgr_.peek_mask_q1();
                    const bool pre_fifo_pop_q = psm_wdata_mgr_.peek_fifo_pop();
#endif
                    auto wdata_out = psm_wdata_mgr_.tick(wdata_in);
#ifdef FX1_A3_IDXCNT_TRACE
                    {
                        std::string inst_name = this->name();
                        if (inst_name.find("NpuTop_std") != std::string::npos &&
                            fsm_out.wr_feeder_en)
                        {
                            static std::ofstream wdata_trace("trace_sysc/wdata_internal_trace.csv");
                            static bool wdata_trace_header = false;
                            if (!wdata_trace_header)
                            {
                                wdata_trace << "cyc,active_context,sram_addr,idxmask,wr_fifo_pop,pre_elm_cnt,pre_mask_q1,pre_fifo_pop_q,real_addr_preshim,wr_feeder_en\n";
                                wdata_trace_header = true;
                            }
                            wdata_trace
                                << dbg_cycle << ","
                                << active_context_id << ","
                                << idx_out.sram_addr << ","
                                << idx_out.mask << ","
                                << idx_out.wr_fifo_pop << ","
                                << pre_elm_cnt << ","
                                << pre_mask_q1 << ","
                                << pre_fifo_pop_q << ","
                                << fsm_out.wr_feeder_en << "\n";
                            wdata_trace.flush();
                        }
                    }
#endif

                    // SRAM read-data latency-equalization register
                    // (psm_top.sv:278-287): committed AFTER the rdata manager
                    // consumed its pre-tick value above, mirroring the single
                    // clock edge.
#ifdef FX1_A3_SRAMC_RDEN_PHASE
                    // SRAM stage 2 (gated by rden) + the psm_top.sv stage.
                    if (sramc_rden_prev_)
                        sramc_s2_ = i_sramc_rdata.read();
                    if (fsm_out.rd_feeder_en)
                        sramc_rdata_q_ = sramc_s2_;
                    sramc_rden_prev_ = fsm_out.sramc_rden;
#else
                    if (fsm_out.rd_feeder_en)
                        sramc_rdata_q_ = i_sramc_rdata.read();
#endif

                    // Address/wren shimming (psm_top.sv:292-322). Each RHS
                    // below is read BEFORE being overwritten later in this
                    // same block (q3 first, then q2, then q1), correctly
                    // mirroring simultaneous register updates.
                    if (fsm_out.wr_feeder_clear)
                    {
                        sramc_addr_q1_ = sramc_addr_q2_ = sramc_addr_q3_ = 0;
                        sramc_wren_q1_ = sramc_wren_q2_ = sramc_wren_q3_ = false;
#ifdef FX1_A3_WRADDR_Q4
                        sramc_addr_q4_ = 0;
                        sramc_wren_q4_ = false;
#endif
                    }
                    else if (fsm_out.wr_feeder_en)
                    {
#ifdef FX1_A3_WRADDR_Q4
                        // read BEFORE q3 is overwritten -- same pattern as q3 <- q2.
                        sramc_addr_q4_ = sramc_addr_q3_;
                        sramc_wren_q4_ = sramc_wren_q3_;
#endif
                        sramc_addr_q3_ = sramc_addr_q2_;
                        sramc_addr_q2_ = sramc_addr_q1_;
                        // psm_idxcnt_'s sram_addr increments by
                        // 1 per real output column (its own internal /SRAMC_N
                        // division, see psm_idxcnt.h), but the READ-BACK side
                        // (tb_demo.cpp's get_sramc_addr()+sram_top.h's host
                        // path, UNCHANGED, traced in full in) expects
                        // block index = x*Y_DIM (matching calc_c_addr()'s own
                        // convention exactly). Scale here to realign the two
                        // -- without this, only x=0 (and one coincidental
                        // boundary point) would land on a block the read
                        // side actually visits.
                        sramc_addr_q1_ = idx_out.sram_addr;
                        g_fx1_rd_addrs.insert((unsigned int)sramc_addr_q1_);  //
                        sramc_wren_q3_ = sramc_wren_q2_;
                        sramc_wren_q2_ = sramc_wren_q1_;
                        sramc_wren_q1_ = fsm_out.sramc_wren;
                    }

#ifdef FX1_A3_ISO_OLD_ADDR
                    const uint32_t real_addr = calc_c_addr(active_context_id, shift_cnt);
#elif defined(FX1_A3_ADDR_SHIM2)
                    // measured evidence says every value lands
                    // exactly ONE block (Y_DIM elements) too late relative to
                    // its address -- data of addr=N is written at addr=N+32,
                    // consistently across all 33 addresses. Taking the address
                    // one shim stage EARLIER (q2 instead of q3) shifts the
                    // address pipeline by one write-beat to test whether that
                    // is the whole misalignment.
                    const uint32_t real_addr = fsm_out.wr_flag ? sramc_addr_q2_ : idx_out.sram_addr;
#elif defined(FX1_A3_WRADDR_Q4)
                    const uint32_t real_addr = fsm_out.wr_flag ? sramc_addr_q4_ : idx_out.sram_addr;
#else
                    const uint32_t real_addr = fsm_out.wr_flag ? sramc_addr_q3_ : idx_out.sram_addr;
#endif
#ifdef FX1_A3_WRADDR_Q4
                    const bool real_wren = fsm_out.wr_flag ? sramc_wren_q4_ : false;
#else
                    const bool real_wren = fsm_out.wr_flag ? sramc_wren_q3_ : false;
#endif

                    // psm_top.sv:321 drives o_sramc_addr with a
                    // CONTINUOUS assign --
                    //     assign o_sramc_addr = (wr_flag) ? sramc_addr_q3 : sramc_addr_d;
                    // so during the READ phase (wr_flag=0) the address comes
                    // from the live idxcnt value. This port computed real_addr
                    // correctly (the ternary above) but only pushed it to the
                    // port INSIDE the  write block, so reads saw
                    // a stale address left over from the previous write. That is
                    // why wiring o_sramc_rden alone changed nothing.
                    // Drive it unconditionally, matching the continuous assign.
                    o_sramc_addr.write(real_addr);

                    if (real_wren)
                    {
                        // SRAMC_N==Y_DIM for this project (see psm_idxcnt_'s
                        // declaration comment above), so this is a 1:1
                        // element<->word-slot reassembly, not a real packing
                        // operation.
                        psum_vector_t<Y_DIM, T_PSUM> wr_vec;
                        sramc_mask_t<Y_DIM> wr_mask;
                        for (int j = 0; j < Y_DIM; j++)
                        {
                            wr_vec[j] = wdata_out.sramc_wdata[j];
#ifdef FX1_A3_ISO_OLD_MASK
                            wr_mask[j] = i_rows_active.read()[j];
#else
                            wr_mask[j] = (wdata_out.sramc_wmask >> j) & 1u;
#endif
                        }

                        o_sramc_wren.write(true);
                        o_sramc_addr.write(real_addr);
                        o_sramc_wmask.write(wr_mask);
                        if (sramc_write_hook_)
                        {
                            psum_vector_t<Y_DIM, T_PSUM> hooked = wr_vec;
                            sramc_write_hook_(real_addr, hooked, wr_mask);
                            o_sramc_wdata.write(hooked);
                        }
                        else
                            o_sramc_wdata.write(wr_vec);
                        {   // record the address range per context
                            if (!g_fx1_ctx_init) {
                                for (int _c = 0; _c < 64; ++_c) {
                                    g_fx1_ctx_lo[_c] = 0xFFFFFFFFu;
                                    g_fx1_ctx_hi[_c] = 0; g_fx1_ctx_n[_c] = 0;
                                }
                                g_fx1_ctx_init = true;
                            }
                            unsigned int _c = (unsigned int)active_context_id & 63u;
                            unsigned int _a = (unsigned int)real_addr;
                            if (_a < g_fx1_ctx_lo[_c]) g_fx1_ctx_lo[_c] = _a;
                            if (_a > g_fx1_ctx_hi[_c]) g_fx1_ctx_hi[_c] = _a;
                            ++g_fx1_ctx_n[_c];
                            g_fx1_wr_addrs.insert(_a);
                        }
                        {
                            uint64_t mb = 0;
                            for (int j = 0; j < Y_DIM; j++)
                                if (wr_mask[j]) mb |= (1ULL << j);
                            dump_psm_trace(active_context_id, real_addr, shift_cnt,
                                           wr_vec, dbg_cycle, mb);
                        }

                        if (recorded_write_idx_ < MAX_RECORDED_WRITES)
                        {
                            // the mask is NOT redundant. psm_idxcnt
                            // asserts wren for beats whose mask is all-zero (the
                            // x_idx > 0 beats, psm_idxcnt.sv:217-218 -- measured on
                            // test3 as exactly half of all recorded writes). Without
                            // the mask the drain cannot tell a real write from a
                            // masked-out filler beat, and the filler beats shift the
                            // column mapping.
                            uint64_t m = 0;
                            for (int b = 0; b < Y_DIM; b++)
                                if (wr_mask[b])
                                    m |= (1ULL << b);
                            recorded_mask_[recorded_write_idx_] = m;
                            recorded_addr_[recorded_write_idx_++] = real_addr;
                        }
                    }
                    if (fsm_out.cscan_en && shift_cnt < write_limit_vectors)
                        shift_cnt++;
                }
#else
                // Data path: capture+write whenever the PREVIOUS cycle's
                // cscan_en was asserted (see cscan_en_prev_'s comment above). i_c_arr.read() this
                // cycle is (by unavoidable SystemC sc_signal semantics) the
                // array's TRUE value from exactly 1 cycle ago -- i.e. from
                // the cycle cscan_en_prev_ was asserted -- so gating on
                // cscan_en_prev_ (not fsm_out.cscan_en directly) makes this
                // read land on the CORRECT column's data instead of stale
                // pre-scan garbage on write #0 and a systematic 1-cycle
                // lag on every write after. shift_cnt/addressing unchanged.
                if (cscan_en_prev_ && shift_cnt < write_limit_vectors)
                {
                    psum_vector_t<Y_DIM, T_PSUM> array_out = i_c_arr.read();
                    uint32_t wr_addr = calc_c_addr(active_context_id, shift_cnt);
                    g_fx1_wr_addrs.insert((unsigned int)wr_addr); ++g_fx1_wr_site[0];
                    o_sramc_wren.write(true);
                    o_sramc_addr.write(wr_addr);
                    o_sramc_wmask.write(i_rows_active.read());
                    o_sramc_wdata.write(array_out);
                    dump_psm_trace(active_context_id, wr_addr, shift_cnt, array_out, dbg_cycle);
                    shift_cnt++;
                }
#endif

#ifdef FX1_A3_DONE_GAP_TRACE
                // Debug hook (default off): cycle gap between the fsm_out.done pulse and the LAST SRAM-C write of the same
                // context (the WDATA_MGR path's 3-stage address/wren shim, sramc_addr_q1_/q2_/q3_, can delay the last
                // write past done). Read-only, no behavior change; compare against the
                // per-write cyc/context columns already in
                // trace_sysc/psm_write_trace.csv (dump_psm_trace, unguarded).
                if (fsm_out.done)
                {
                    std::string inst_name = this->name();
                    if (inst_name.find("NpuTop_std") != std::string::npos)
                    {
                        static std::ofstream done_trace("trace_sysc/psm_done_trace.csv");
                        static bool done_trace_header = false;
                        if (!done_trace_header)
                        {
                            done_trace << "cyc,context,shift_cnt,wc\n";
                            done_trace_header = true;
                        }
                        done_trace << dbg_cycle << "," << active_context_id << "," << shift_cnt << "," << recorded_write_idx_ << "\n";
                        done_trace.flush();
                    }
                }
#endif

                set_psm_done_(fsm_out.done);
                o_finalwrite.write(fsm_out.finalwrite);
                set_shift_done_(fsm_out.shift_done);
                // Extend the ARRAY-facing cscan_en by exactly 1 extra tail
                // cycle (OR with previous cycle's raw FSM value) so the array
                // performs one additional real shift -- this is what lets the
                // (inherently 1-cycle-stale) read above land on the correct
                // value instead of needing X_DIM+1 cycles the FSM itself
                // doesn't budget for. psm_shift_fsm.h's own state machine
                // timing (scan_cnt_, state transitions) is completely
                // unaffected -- only this array-facing wire is extended.
#ifndef FX1_A3_CSCAN_ARR_BEAT_OFF
                // The `cscan_en` copy the ARRAY sees advances in the SAME domain as the gather stage (permitted beats) and
                // pre-compensates one sc_signal cycle. If the array side advanced on raw cycles while the gather side
                // advanced on beats, entering PREWRITE_SHIFT during a pipeline stall would shift column 0 out before the
                // gather opens, and it would be lost. When every cycle is a permitted beat this equals the raw-cycle form.
                // `-DFX1_A3_CSCAN_ARR_BEAT_OFF` selects the raw-cycle form for A/B comparisons.
                {
                    const bool cscan_beat_prev = cscan_beat_;
                    const bool cscan_beat_next =
                        pipe_en_now_ ? (bool)fsm_out.cscan_en
                                             : cscan_beat_prev;
#ifdef FX1_A3_CSCAN_BEAT_NOOR
                    // No OR with the previous beat. The OR stretches the pulse by one cycle to compensate the sc_signal delay
                    // on the SCAN-OUT direction; once the SCAN-IN direction is driven too (FX1_A3_CARR_DRIVE), that extra
                    // cycle becomes a spurious shift for the array.
                    o_cscan_en.write(cscan_beat_next);
#else
                    o_cscan_en.write(cscan_beat_next || cscan_beat_prev);
#endif
                    cscan_beat_ = cscan_beat_next;
                }
#elif defined(FX1_A3_CSCAN_ARR_PIPE) && defined(FX1_A3_CSCAN_ARR_DELAY)
                // DELAY by one permitted beat instead of stretching the tail.
                o_cscan_en.write(cscan_arr_);
#elif defined(FX1_A3_CSCAN_ARR_PIPE)
                o_cscan_en.write(fsm_out.cscan_en || cscan_arr_);
#else
                o_cscan_en.write(fsm_out.cscan_en || cscan_en_prev_);
#endif
                cscan_sent_prev_ = (fsm_out.cscan_en || cscan_en_prev_);
#ifdef FX1_A3_CSCAN_PREV_BEAT
                // the compensation advances in the PERMITTED-BEAT domain, like every consumer of it (the array's scan
                // register and the PSM gather are both gated by i_pipeline_en). With no stalled beat inside the window this
                // is the identity.
                if (pipe_en_now_)
                    cscan_en_prev_ = fsm_out.cscan_en;
#else
                cscan_en_prev_ = fsm_out.cscan_en;
#endif
#ifdef FX1_A3_CSCAN_ARR_PIPE
                if (pipe_en_now_)
                    cscan_arr_ = fsm_out.cscan_en;
#endif
#ifdef FX1_A3_CSCAN_CAP_PIPE
#ifdef FX1_A3_CSCAN_CAP_FLUSH
                // clear the flag as soon as the outstanding gather beat completes, so it does not fire again during the
                // write phase.
                if (pipe_en_now_ || (cscan_cap_ && !fsm_out.cscan_en))
                    cscan_cap_ = fsm_out.cscan_en;
#else
                if (pipe_en_now_)
                    cscan_cap_ = fsm_out.cscan_en;
#endif
#endif
            }
            return;
#endif

            bool start_pulse = fsm_start_now_ && !start_q;
            start_q = fsm_start_now_;

            if (start_pulse && !shifting)
            {
                shifting = true;
                shift_cnt = 0;
                delay_cnt = 0;
                start_delay_cnt = 0;

                preload_mode = false;

                write_limit_vectors = get_write_limit_vectors();

                // Latch global context id from controller.
                // For multi-output-tile test this must be 0..5.
                active_context_id = i_context_id.read();

                context_addr_base = 0;
                addr_reg = 0;

                // Do not scan or write immediately.
                o_cscan_en.write(false);
                o_sramc_wren.write(false);

                psm_scan_phase = 5;

                DBG_COUT << "[PSM START]"
                          << " global_context = " << active_context_id
                          << " / total_contexts = " << i_total_contexts.read()
                          << " ncontexts = " << i_ncontexts.read()
                          << std::endl;

                return;
            }

            if (!shifting)
            {
                o_cscan_en.write(false);
                return;
            }

            // ---------------------------------------------------------
            // Phase 5: wait after FSM start before reading scan_out.
            // This gives local cswitch/swap time to complete across SA.
            // No scan, no write in this phase.
            // ---------------------------------------------------------
            if (psm_scan_phase == 5)
            {
                o_cscan_en.write(false);
                o_sramc_wren.write(false);

                if (start_delay_cnt >= RTL_REF_PSM_START_DELAY)
                {
                    psm_scan_phase = 1;

                    DBG_COUT << "[PSM START DELAY DONE]"
                              << " context=" << psm_context_cnt
                              << " delay_cnt=" << start_delay_cnt
                              << std::endl;
                }
                else
                {
                    start_delay_cnt++;
                }

                return;
            }

            // ---------------------------------------------------------
            // Phase 1: write current i_c_arr as x0, no scan advance
            // ---------------------------------------------------------
            if (psm_scan_phase == 1)
            {
                psum_vector_t<Y_DIM, T_PSUM> array_out = i_c_arr.read();

                // uint32_t wr_addr = calc_c_addr(active_context_id, shift_cnt);

                // o_cscan_en.write(false);

                // o_sramc_wren.write(true);
                // o_sramc_addr.write(i_out_base_addr.read() + wr_addr);
                // o_sramc_wmask.write(i_rows_active.read());
                // // write_data[y] = i_sramc_r_data.read()[y] + array_out[y];
                // o_sramc_wdata.write(array_out);
                // dump_psm_trace(active_context_id, i_out_base_addr.read() + wr_addr, shift_cnt, array_out);

                uint32_t wr_addr = calc_c_addr(active_context_id, shift_cnt);
                o_cscan_en.write(false);
                o_sramc_wren.write(true);
                // SRAMC uses local address. Do not add DRAM out_base here
                o_sramc_addr.write(wr_addr);
                g_fx1_wr_addrs.insert((unsigned int)wr_addr); ++g_fx1_wr_site[1];   //
                o_sramc_wmask.write(i_rows_active.read());
                o_sramc_wdata.write(array_out);
                dump_psm_trace(active_context_id, wr_addr, shift_cnt, array_out);
                shift_cnt++;

                psm_scan_phase = 2;
                return;
            }

            // ---------------------------------------------------------
            // Phase 2: enable scan, no write
            // SA sees this on the next clock.
            // ---------------------------------------------------------
            if (psm_scan_phase == 2)
            {
                o_cscan_en.write(true);
                o_sramc_wren.write(false);

                psm_scan_phase = 3;
                return;
            }

            // ---------------------------------------------------------
            // Phase 3: keep scan enabled, wait one cycle
            // for scan_out/i_c_arr to update.
            // ---------------------------------------------------------
            if (psm_scan_phase == 3)
            {
                o_cscan_en.write(true);
                o_sramc_wren.write(false);

                psm_scan_phase = 4;
                return;
            }

            // ---------------------------------------------------------
            // Phase 4: normal scan writes x1..x15
            // ---------------------------------------------------------
            if (shift_cnt < write_limit_vectors)
            {
                psum_vector_t<Y_DIM, T_PSUM> array_out = i_c_arr.read();

                // uint32_t wr_addr = calc_c_addr(active_context_id, shift_cnt);

                // o_cscan_en.write(true);

                // o_sramc_wren.write(true);
                // o_sramc_addr.write(i_out_base_addr.read() + wr_addr);
                // o_sramc_wmask.write(i_rows_active.read());
                // // write_data[y] = i_sramc_r_data.read()[y] + array_out[y];
                // o_sramc_wdata.write(array_out);

                // dump_psm_trace(active_context_id, i_out_base_addr.read() + wr_addr, shift_cnt, array_out);

                uint32_t wr_addr = calc_c_addr(active_context_id, shift_cnt);
                g_fx1_wr_addrs.insert((unsigned int)wr_addr); ++g_fx1_wr_site[2];

                o_cscan_en.write(true);

                o_sramc_wren.write(true);
                o_sramc_addr.write(wr_addr);
                o_sramc_wmask.write(i_rows_active.read());
                o_sramc_wdata.write(array_out);

                dump_psm_trace(active_context_id, i_out_base_addr.read() + wr_addr, shift_cnt, array_out);

                shift_cnt++;
                return;
            }

            // ---------------------------------------------------------
            // Done current context
            // ---------------------------------------------------------
            shifting = false;
            psm_scan_phase = 0;

            o_cscan_en.write(false);
            o_sramc_wren.write(false);
            o_sramc_rden.write(false);
            set_shift_done_(true);
            set_psm_done_(true);

            uint32_t nctx = i_ncontexts.read();
            if (nctx == 0)
            {
                nctx = 1;
            }

            if ((psm_context_cnt + 1) < nctx)
            {
                psm_context_cnt++;
            }
            else
            {
                psm_context_cnt = 0;
            }

            DBG_COUT << "[PSM DONE]"
                      << " global_context = " << active_context_id
                      << std::endl;
        }
        // else
        // {
        //     o_sramc_wren.write(false);
        //     o_sramc_rden.write(false);
        // }
    };
} // namespace sauria_rtl

#endif // SAURIA_RTL_PSM_TOP_H

