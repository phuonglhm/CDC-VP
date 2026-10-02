// rtl_ref_npu_top.h -- faithful port of sauria_model's npu_top.h. Namespace sauria_rtl (not sauria) + its own header
// guard, like sram/rtl_ref_sram_top.h. The class name `NpuTop` is kept (distinguished by namespace). Only the includes
// point to the rtl_ref_* modules (main_controller, ifmap/wei_feeder, sa_array, psm_top, sram_top, rtl_ref_config_regs),
// and WeightFeeder gets one extra template argument `0` (IS_LANE_B). All other wiring and logic is the original's,
// so the host-bus / config-register path is sauria_model's own, not a hand-written wiring layer.
// Additions below (backdoor SRAM access, DMA host-side access, OBP hook, debug accessors) are testbench / HAS
// integration helpers; none changes the hardware logic.

#ifndef SAURIA_RTL_NPU_TOP_H
#define SAURIA_RTL_NPU_TOP_H

#include <fstream>
#include <sstream>
#include <vector>
#include "sauria_types.h"
#include "debug.h"
// FX1_A3_* / FX1_DUMPS macro defaults -- MUST be included before the rtl_ref_* modules below (this file does not
// include control/native_lane_a_core.h, which normally pulls it in).
#include "control/rtl_ref_defaults.h"
// rtl_ref_config_regs.h instead of config_regs.h -- see rtl_ref_config_regs.h / rtl_ref_config_map.h.
#include "rtl_ref_config_regs.h"
#include "control/rtl_ref_main_controller.h"
#include "data_feeder/rtl_ref_ifmap_feeder.h"
#include "data_feeder/rtl_ref_wei_feeder.h"
#include "systolic_array/rtl_ref_sa_array.h"
#include "psm/rtl_ref_psm_top.h"
#include "sram/rtl_ref_sram_top.h"

#ifndef FX1_NO_PERF
#include "instrumentation/rtl_ref_perf_counters.h"
#endif

namespace sauria_rtl
{
    using namespace sauria;

    template <
        int X_DIM = 32,
        int Y_DIM = 32,
        typename T_ACT = float,
        typename T_WEI = float,
        typename T_PSUM = float,
        int SRAMA_CAP = 1024,
        int SRAMB_CAP = 1024,
        int SRAMC_CAP = 2048,
        int FIFO_DEPTH = 16,
        // RTL sauria_pkg.sv has SEPARATE depths -- ACT_FIFO_POSITIONS=5
        // (total registers = Positions*M, M=3) and WEI_FIFO_POSITIONS=4
        // (total registers = Positions). This model used one FIFO_DEPTH=16 for
        // both, making the WEIGHT fifo 4x too deep -- which is exactly why
        // wei_fifo_empty measured 0 for the whole run, leaving
        // pipeline_en driven solely by the ACT side.
        // Guarded: unlike the other six substrate fixes this one CHANGES TIMING
        // on the guard-off path too, so it must be A/B-measured, not assumed.
        int PE_LAT = X_DIM + Y_DIM,
        int EXTRA_CSREG = 1>
    class NpuTop : public sc_module
    {
    public:
#ifdef FX1_A3_FIFO_DEPTH_SPLIT
        static constexpr int ACT_FIFO_POS = 5;
        static constexpr int WEI_FIFO_POS = 4;
#else
        static constexpr int ACT_FIFO_POS = FIFO_DEPTH;
        static constexpr int WEI_FIFO_POS = FIFO_DEPTH;
#endif

        // Clocks & Resets
        sc_in<bool> i_clk{"i_clk"};
        sc_in<bool> i_rstn{"i_rstn"};
        sc_in<bool> i_soft_reset{"i_soft_reset"};

        // Host Control Interface
        sc_in<bool> i_start{"i_start"};
        sc_out<bool> o_done{"o_done"};
        sc_out<bool> o_deadlock{"o_deadlock"};

        sc_in<uint32_t> i_mvm_k{"i_mvm_k"};
        // Host Memory Port (AXI interface modeling)
        sc_in<uint32_t> i_host_addr{"i_host_addr"};
        sc_in<bool> i_host_wren{"i_host_wren"};
        sc_in<bool> i_host_rden{"i_host_rden"};
        sc_in<host_data_t> i_host_wdata{"i_host_wdata"};
        sc_in<host_mask_t> i_host_wmask{"i_host_wmask"};
        sc_out<host_data_t> o_host_rdata{"o_host_rdata"};

        // Runtime config configurations (threshold, select, and tiled loop counts)
        sc_in<float> i_threshold{"i_threshold"};
        sc_in<sc_bv<3>> i_select{"i_select"};
        sc_in<uint32_t> i_total_contexts{"i_total_contexts"};

#ifndef FX1_NO_PERF
        // Non-owning perf pointer. Set it, THEN construct/run. Helper below also
        // re-routes to the array in case it is set after construction.
        sauria_rtl::PerfCounters *perf{nullptr};
        void attach_perf(sauria_rtl::PerfCounters *p)
        {
            perf = p;
            if (array_inst)
                array_inst->perf = p;
            if (ctrl_inst)          // Pha B: controller FSM-state histogram
                ctrl_inst->perf = p;
            if (act_feeder)         // A1: per-feeder cycle/byte counters (SRAM-A)
                act_feeder->perf = p;
            if (wei_feeder)         // A1: per-feeder cycle/byte counters (SRAM-B)
                wei_feeder->perf = p;
            if (sram_inst)          // SRAM traffic measured at the memories
                sram_inst->perf = p;
        }
#endif
        ///Task#5 retry v3: passthrough accessors so
        // tb_evaluate.cpp (a separate sc_module, reached via a raw pointer --
        // see npu_std_ptr_ there) can read PSM's per-context write state and
        // the REAL context id directly. Observation-only, no new sc_out
        // ports/signals needed.
        //
        // Task #5 measured that PSM's own o_done (fsm_out.done) pulses far
        // more often than there are real contexts (K-accumulation streams
        // through many internal write passes per context before the value
        // is final) -- so the drain trigger must be s_global_context_id
        // actually CHANGING (the same signal main_controller.h's own
        // entering_next_context/entering_first_context edges are built on,
        // already confirmed RTL-faithful), not PSM's own done pulse.
        uint32_t get_real_context_id() const { return s_global_context_id.read(); }

        // Passthrough to Sram::peek_sramc_npu -- see that method's comment.
        // Lets tb_evaluate.cpp drain a context's SRAM-C contents without
        // consuming simulated cycles.
        const psum_vector_t<Y_DIM, T_PSUM> &peek_sramc(uint32_t addr) const
        {
            return sram_inst->peek_sramc_npu(addr);
        }

#ifdef FX1_A3_SRAM_BACKDOOR_LOAD
        // Testbench-only backdoor load (off by default): writes straight into Sram::write_bank_data, skipping the host-bus
        // protocol (2 simulated cycles per element). No hardware logic change -- a public pass-through because
        // sram_inst is private. bank_id as write_bank_data: 0/1 = WEI (B), 2/3 = ACT (A), 4/5 = PSUM (C).
        void load_sram_backdoor(int bank_id, uint32_t offset_bytes, const uint8_t *src_data,
                                 uint32_t size_bytes)
        {
            sram_inst->write_bank_data(bank_id, offset_bytes, src_data, size_bytes);
        }

        // Testbench-only backdoor read of SRAM-C (off by default), symmetric to load_sram_backdoor above: reading SRAM-C
        // over the host bus after every tile costs about as many cycles as the tile's compute. Pass-through to
        // read_bank_data() in sram/rtl_ref_sram_top.h; no hardware logic change.
        void read_sram_backdoor(int bank_id, uint32_t offset_bytes, uint8_t *dest_data,
                                uint32_t size_bytes)
        {
            sram_inst->read_bank_data(bank_id, offset_bytes, dest_data, size_bytes);
        }

        // DMA of has::HasNpuTop onto the HOST half of the ping-pong SRAM (the half the core is
        // not using), passthrough to write_host_side()/read_host_side() in sram/rtl_ref_sram_top.h.
        void write_sram_host_side(int bank_id, uint32_t offset_bytes, const uint8_t *src, uint32_t size_bytes)
        {
            sram_inst->write_host_side(bank_id, offset_bytes, src, size_bytes);
        }
        void read_sram_host_side(int bank_id, uint32_t offset_bytes, uint8_t *dst, uint32_t size_bytes)
        {
            sram_inst->read_host_side(bank_id, offset_bytes, dst, size_bytes);
        }
        // OBP on the PSM -> SRAM-C write path, pass-through to Psm::sramc_write_hook_.
        void set_sramc_write_hook(std::function<void(uint32_t, psum_vector_t<Y_DIM, T_PSUM> &, const sramc_mask_t<Y_DIM> &)> f)
        {
            psm_inst->sramc_write_hook_ = std::move(f);
        }
#endif

        uint32_t get_psm_active_context_id() const { return psm_inst->get_active_context_id(); }
        uint32_t get_psm_recorded_write_count() const { return psm_inst->get_recorded_write_count(); }
        uint64_t get_psm_recorded_mask(uint32_t idx) const
        {
            return psm_inst->get_recorded_mask(idx);
        }
        uint32_t get_psm_ckstep() const { return psm_inst->get_ckstep(); }
        uint32_t get_psm_til_cystep() const { return psm_inst->get_til_cystep(); }

        uint32_t get_psm_recorded_addr(uint32_t idx) const { return psm_inst->get_recorded_addr(idx); }

        // Debug hooks (read-only accessors, no effect on behaviour): feeder / controller handshake signals.
        bool dbg_act_fifo_empty() const { return s_act_fifo_empty.read(); }
        bool dbg_wei_fifo_full() const { return s_wei_fifo_full.read(); }
        bool dbg_act_start() const { return s_act_start.read(); }
        bool dbg_wei_start() const { return s_wei_start.read(); }
        bool dbg_act_valid() const { return s_act_valid.read(); }
        bool dbg_wei_valid() const { return s_wei_valid.read(); }
        bool dbg_act_done() const { return s_act_done.read(); }
        bool dbg_wei_done() const { return s_wei_done.read(); }
        bool dbg_act_hold() const { return ctrl_inst->dbg_act_hold(); }
        bool dbg_wei_hold() const { return ctrl_inst->dbg_wei_hold(); }
        int dbg_feeders_state() const { return ctrl_inst->dbg_feeders_state(); }
        bool dbg_act_til_done_q() const { return act_feeder->dbg_last_til_done_rtl(); }
        bool dbg_act_cnt_en() const { return act_feeder->dbg_cnt_en_rtl(); }
        // Debug hooks: npu_top-level signals read directly, BEFORE the feeders gate them with fifo_full_any / stall_any.
        bool dbg_act_cnt_en_raw() const { return s_act_cnt_en.read(); }
        bool dbg_act_cnt_clear_raw() const { return s_act_cnt_clear.read(); }
        bool dbg_start_internal() const { return s_start_internal.read(); }
        bool dbg_act_feeder_en_raw() const { return s_act_feeder_en.read(); }
        // Debug hook: Control's same-cycle internal value (before sc_signal), to compare with `dbg_act_cnt_en_raw()`.
        bool dbg_act_cnt_en_seam() const { return ctrl_inst->seam_act_cnt_en_; }
        bool dbg_act_push0() const { return act_feeder->dbg_push0_rtl(); }
        bool dbg_act_empty0() const { return act_feeder->dbg_empty0_rtl(); }
        uint32_t dbg_act_elm0() const { return act_feeder->dbg_elm0_rtl(); }
        uint32_t dbg_act_nfree0() const { return act_feeder->dbg_nfree0_rtl(); }
        bool dbg_act_stall_any() const { return act_feeder->dbg_stall_any_rtl(); }
        bool dbg_act_finalpush() const { return act_feeder->dbg_finalpush_rtl(); }
        // Current weight (B) address of wei_feeder (read-only, s_sramb_addr). Lets a testbench see when a two-buffer
        // weight tile crosses ONE buffer (81 KB / ROWS_B rows) to switch i_select between the halves.
        uint32_t dbg_sramb_addr() const { return s_sramb_addr.read(); }

        // Constructor
        SC_HAS_PROCESS(NpuTop);
        NpuTop(sc_module_name nm, const PeConfig &pe_cfg = PeConfig()) : sc_module(nm)
        {
            // Instantiate submodules
            sram_inst = new Sram<X_DIM, Y_DIM, T_ACT, T_WEI, T_PSUM, SRAMA_CAP, SRAMB_CAP, SRAMC_CAP>("sram_inst");
            ctrl_inst = new Control<X_DIM, Y_DIM, PE_LAT, EXTRA_CSREG>("ctrl_inst");
            act_feeder = new IfmapFeeder<Y_DIM, T_ACT, SRAMA_CAP, ACT_FIFO_POS>("act_feeder");
            wei_feeder = new WeightFeeder<X_DIM, T_WEI, SRAMB_CAP, WEI_FIFO_POS, 0>("wei_feeder");
#ifdef FX1_A3_EMPTY_DIRECT
            // the controller reads the same-cycle fifo_empty flag directly, not through sc_signal.
            {
                auto *af = act_feeder;
                auto *wf = wei_feeder;
                ctrl_inst->peek_act_empty_ = [af]() { return af->peek_fifo_empty_now(); };
#ifdef FX1_A3_CNT_EN_DIRECT
                auto *ci = ctrl_inst;
                act_feeder->seam_peek_cnt_en_ = [ci]() { return ci->seam_act_cnt_en_; };
                wei_feeder->seam_peek_cnt_en_ = [ci]() { return ci->seam_wei_cnt_en_; };
#endif
#ifdef FX1_A3_PIPE_EN_DIRECT
                auto *ci2 = ctrl_inst;
                act_feeder->seam_peek_pipe_en_ = [ci2]() { return ci2->seam_pipeline_en_; };
                wei_feeder->seam_peek_pipe_en_ = [ci2]() { return ci2->seam_pipeline_en_; };
#endif
#ifdef FX1_A3_VALID_DIRECT
                // combinational valid in the RTL -- feeders read Control directly.
                auto *ci3 = ctrl_inst;
                act_feeder->seam_peek_valid_ = [ci3]() { return ci3->seam_act_valid_; };
                wei_feeder->seam_peek_valid_ = [ci3]() { return ci3->seam_wei_valid_; };
#endif
#ifdef FX1_A3_START_DIRECT
                // `o_act_start` is in feeders_fsm.sv's `always_comb begin: output_logic` like `o_act_valid` => combinational,
                // 0 delay in the RTL. Through sc_signal it would be one beat late: `x_transition_flag` rises late, the first
                // gather group starts late and SRAM word 0 is skipped.
                auto *ci5 = ctrl_inst;
                act_feeder->seam_peek_start_ = [ci5]() { return ci5->seam_act_start_; };
#endif
#ifdef FX1_A3_FDFSM_DIRECT
                // the combinational output_logic group -- feeders read Control directly.
                auto *ci4 = ctrl_inst;
                act_feeder->seam_peek_fd_ = [ci4](int i) { return ci4->seam_fd(false, i); };
                wei_feeder->seam_peek_fd_ = [ci4](int i) { return ci4->seam_fd(true, i); };
#endif
                ctrl_inst->peek_wei_empty_ = [wf]() { return wf->peek_fifo_empty_now(); };
#ifdef FX1_A3_STALL_DIRECT
                ctrl_inst->peek_act_stall_ = [af]() { return af->peek_stall_now(); };
                ctrl_inst->peek_wei_stall_ = [wf]() { return wf->peek_stall_now(); };
#endif
#if defined(FX1_A3_TILDONE_Q_GATE) && defined(FX1_A3_IFMAP_FEEDER_RTL)
                // RTL-exact act til_done path into feeders_fsm (see main_controller.h).
                // peek_act_stall_ is only READ under TILDONE_Q_GATE unless STALL_DIRECT.
                ctrl_inst->peek_act_til_done_rtl_ = [af]() { return af->peek_til_done_rtl_now(); };
                ctrl_inst->peek_act_til_done_rtl_last_ = [af]() { return af->dbg_last_til_done_rtl(); };
                ctrl_inst->peek_act_full_ = [af]() { return af->peek_fifo_full_now(); };
                if (!ctrl_inst->peek_act_stall_)
                    ctrl_inst->peek_act_stall_ = [af]() { return af->peek_stall_now(); };
#endif
            }
#endif
            array_inst = new SystolicArray<X_DIM, Y_DIM, T_ACT, T_WEI, T_PSUM>("array_inst", pe_cfg);
            psm_inst = new Psm<X_DIM, Y_DIM, T_PSUM, SRAMC_CAP>("psm_inst");
#ifdef FX1_A3_PSM_START_DIRECT
            // The RTL wires o_outbuf_start -> i_fsm_start combinationally (sauria_logic.sv:305/459). Psm runs BEFORE
            // Control, so it PEEKs ContextFsm's pre-tick registers instead of an sc_signal.
            { auto *cc = ctrl_inst;
              // PEEK pre-tick (Psm runs BEFORE Control -- measured).
              psm_inst->peek_fsm_start_ = [cc]() { return cc->peek_outbuf_start_now(); }; }
#endif
#ifdef FX1_A3_ORDER_PROBE
            { auto *cc = ctrl_inst;
              psm_inst->peek_fsm_start_pre_ = [cc]() { return cc->peek_outbuf_start_now(); };
              if (!psm_inst->peek_fsm_start_)
                  psm_inst->peek_fsm_start_ = [cc]() { return cc->seam_outbuf_start_; }; }
#endif
            config_regs_inst = new ConfigRegs<32, 32, X_DIM, Y_DIM, 2, 15, 15, 15, DILP_W, 8>("config_regs_inst");

#ifndef FX1_NO_PERF
            // Forward the externally-attached perf struct down to the array.
            // attach_perf() must be called before sc_start() if used.
            array_inst->perf = perf;
            ctrl_inst->perf = perf;   // Pha B: controller FSM-state histogram
            act_feeder->perf = perf;  // A1: per-feeder counters (SRAM-A act)
            wei_feeder->perf = perf;  // A1: per-feeder counters (SRAM-B wei)
#endif

            SC_METHOD(host_rdata_mux);
            sensitive << i_host_addr << s_host_rdata_sram << s_host_rdata_cfg;

            SC_METHOD(start_reset_logic);
            sensitive << i_start << s_cfg_start << i_soft_reset << s_cfg_soft_reset << s_ctrl_done;

#ifdef FX1_A3_SEAM_ORDER
            // ONE process steps Control and then the feeders in order, instead of three independent processes sensitive to
            // i_clk.pos() (each writing an sc_signal the others only see in the NEXT cycle => one beat late each way).
            SC_METHOD(seam_ordered_step);
            sensitive << i_clk.pos();
#endif

            SC_METHOD(done_latch_logic);
            sensitive << i_clk.pos();
            dont_initialize();

            SC_METHOD(debug_stream_reference_monitor);
            sensitive << i_clk.pos();
            dont_initialize();

            SC_METHOD(debug_sa_input_stream_dump);
            sensitive << i_clk.pos();
            dont_initialize();

            // debug_ref_stream_mux is not registered: it is not on the act/wei data path into the array.
            dont_initialize();
            // ----------------------------------------------------
            // Signal Interconnections
            // ----------------------------------------------------

            // 1. Clock and Reset routing
            sram_inst->i_clk(i_clk);
            sram_inst->i_rstn(i_rstn);
            sram_inst->i_deepsleep(s_false);
            sram_inst->i_powergate(s_false);
            sram_inst->i_select(i_select);

            ctrl_inst->i_clk(i_clk);
            ctrl_inst->i_rstn(i_rstn);
            ctrl_inst->i_soft_reset(s_ctrl_reset_internal);

            act_feeder->i_clk(i_clk);
            act_feeder->i_rstn(i_rstn);

            wei_feeder->i_clk(i_clk);
            wei_feeder->i_rstn(i_rstn);

            array_inst->i_clk(i_clk);
            array_inst->i_rstn(i_rstn);

            psm_inst->i_clk(i_clk);
            psm_inst->i_rstn(i_rstn);

            config_regs_inst->i_clk(i_clk);
            config_regs_inst->i_rstn(i_rstn);
            config_regs_inst->i_host_addr(i_host_addr);
            config_regs_inst->i_host_wren(i_host_wren);
            config_regs_inst->i_host_rden(i_host_rden);
            config_regs_inst->i_host_wdata(i_host_wdata);
            config_regs_inst->i_host_wmask(i_host_wmask);
            config_regs_inst->o_host_rdata(s_host_rdata_cfg);
            config_regs_inst->i_done(s_ctrl_done);
            config_regs_inst->i_soft_reset_in(i_soft_reset);
            config_regs_inst->o_start(s_cfg_start);
            config_regs_inst->o_soft_reset(s_cfg_soft_reset);
            config_regs_inst->o_profile(s_cfg_profile);
            config_regs_inst->o_act_incntlim(s_act_incntlim);
            config_regs_inst->o_act_incntstep(s_act_incntstep);
            config_regs_inst->o_act_outcntlim(s_act_outcntlim);
            config_regs_inst->o_act_outcntstep(s_act_outcntstep);
            config_regs_inst->o_act_xlim(s_act_xlim);
            config_regs_inst->o_act_xstep(s_act_xstep);
            config_regs_inst->o_act_ylim(s_act_ylim);
            config_regs_inst->o_act_ystep(s_act_ystep);
            config_regs_inst->o_act_chlim(s_act_chlim);
            config_regs_inst->o_act_chstep(s_act_chstep);
            config_regs_inst->o_act_til_xlim(s_act_til_xlim);
            config_regs_inst->o_act_til_xstep(s_act_til_xstep);
            config_regs_inst->o_act_til_ylim(s_act_til_ylim);
            config_regs_inst->o_act_til_ystep(s_act_til_ystep);
            config_regs_inst->o_wei_incntlim(s_wei_incntlim);
            config_regs_inst->o_wei_incntstep(s_wei_incntstep);
            config_regs_inst->o_wei_wlim(s_wei_wlim);
            config_regs_inst->o_wei_wstep(s_wei_wstep);
            config_regs_inst->o_wei_klim(s_wei_klim);
            config_regs_inst->o_wei_kstep(s_wei_kstep);
            config_regs_inst->o_wei_til_klim(s_wei_til_klim);
            config_regs_inst->o_wei_til_kstep(s_wei_til_kstep);
            config_regs_inst->o_wei_cols_active(s_wei_cols_active);
            config_regs_inst->o_wei_waligned(s_wei_waligned);
            config_regs_inst->o_cxlim(s_cxlim);
            config_regs_inst->o_cxstep(s_cxstep);
            config_regs_inst->o_cklim(s_cklim);
            config_regs_inst->o_ckstep(s_ckstep);
            config_regs_inst->o_out_ncontexts(s_out_ncontexts);
            config_regs_inst->o_out_til_cylim(s_out_til_cylim);
            config_regs_inst->o_out_til_cystep(s_out_til_cystep);
            config_regs_inst->o_out_til_cklim(s_out_til_cklim);
            config_regs_inst->o_out_til_ckstep(s_out_til_ckstep);
            config_regs_inst->o_out_inactive_cols(s_out_inactive_cols);
            config_regs_inst->o_out_preload_en(s_out_preload_en);
            config_regs_inst->o_act_base_addr(s_act_base_addr);
            config_regs_inst->o_wei_base_addr(s_wei_base_addr);
            config_regs_inst->o_out_base_addr(s_out_base_addr);
            config_regs_inst->o_in_h(s_in_h);
            config_regs_inst->o_in_w(s_in_w);
            config_regs_inst->o_in_c(s_in_c);
            config_regs_inst->o_kernel_h(s_kernel_h);
            config_regs_inst->o_kernel_w(s_kernel_w);
            config_regs_inst->o_stride(s_stride);
            config_regs_inst->o_padding(s_padding);
            config_regs_inst->o_dilation(s_dilation);
            config_regs_inst->o_tile_x(s_tile_x);
            config_regs_inst->o_tile_y(s_tile_y);
            config_regs_inst->o_tile_k(s_tile_k);
            config_regs_inst->o_tile_c(s_tile_c);
            config_regs_inst->o_x_used(s_x_used);
            config_regs_inst->o_y_used(s_y_used);

            // 2. Host Interface Routing to SRAM
            sram_inst->i_host_addr(i_host_addr);
            sram_inst->i_host_wren(i_host_wren);
            sram_inst->i_host_rden(i_host_rden);
            sram_inst->i_host_wdata(i_host_wdata);
            sram_inst->i_host_wmask(i_host_wmask);
            sram_inst->o_host_rdata(s_host_rdata_sram);

            // 3. FSM Controller bindings
            ctrl_inst->i_start(s_start_internal);
            ctrl_inst->o_done(s_ctrl_done);
            ctrl_inst->o_feed_deadlock(o_deadlock);

            // PSM done feedbacks to Control FSM
            ctrl_inst->i_outbuf_done(s_psm_done);
            ctrl_inst->i_finalwrite(s_psm_finalwrite);
            ctrl_inst->i_shift_done(s_psm_shift_done);

            // Feeder done feedbacks to Control FSM
            ctrl_inst->i_act_done(s_act_done);
            ctrl_inst->i_act_til_done(s_act_til_done);
            ctrl_inst->i_act_fifo_empty(s_act_fifo_empty);
            ctrl_inst->i_act_fifo_full(s_act_fifo_full);
            ctrl_inst->i_act_stall(s_act_stall);

            ctrl_inst->i_wei_done(s_wei_done);
            ctrl_inst->i_wei_til_done(s_wei_til_done);
            ctrl_inst->i_wei_fifo_empty(s_wei_fifo_empty);
            ctrl_inst->i_wei_fifo_full(s_wei_fifo_full);
            ctrl_inst->i_wei_stall(s_wei_stall);

            ctrl_inst->i_mvm_k(i_mvm_k);
            ctrl_inst->i_total_contexts(i_total_contexts);

            // Control outputs routed to Feeders
            ctrl_inst->o_act_feeder_en(s_act_feeder_en);
            ctrl_inst->o_act_feeder_clear(s_act_feeder_clear);
            ctrl_inst->o_act_start(s_act_start);
            ctrl_inst->o_act_valid(s_act_valid);
            ctrl_inst->o_act_finalpush(s_act_finalpush);
            ctrl_inst->o_act_cnt_en(s_act_cnt_en);
            ctrl_inst->o_act_cnt_clear(s_act_cnt_clear);
            ctrl_inst->o_act_clearfifo(s_act_clearfifo);
            ctrl_inst->o_act_pop_en(s_act_pop_en);
            ctrl_inst->o_act_finalctx(s_act_finalctx);

            ctrl_inst->o_wei_feeder_en(s_wei_feeder_en);
            ctrl_inst->o_wei_feeder_clear(s_wei_feeder_clear);
            ctrl_inst->o_wei_start(s_wei_start);
            ctrl_inst->o_wei_valid(s_wei_valid);
            ctrl_inst->o_wei_finalpush(s_wei_finalpush);
            ctrl_inst->o_wei_cnt_en(s_wei_cnt_en);
            ctrl_inst->o_wei_cnt_clear(s_wei_cnt_clear);
            ctrl_inst->o_wei_clearfifo(s_wei_clearfifo);
            ctrl_inst->o_wei_pop_en(s_wei_pop_en);
            ctrl_inst->o_wei_cswitch(s_wei_cswitch);

            // Control outputs routed to PSM
            ctrl_inst->o_outbuf_start(s_psm_start);
            ctrl_inst->o_outbuf_reset(s_psm_reset);
            ctrl_inst->o_context_id(s_context_id);
            ctrl_inst->o_local_context_id(s_local_context_id);
            ctrl_inst->o_out_tile_id(s_out_tile_id);
            ctrl_inst->o_global_context_id(s_global_context_id);

            // Control outputs routed to Systolic Array
            ctrl_inst->o_sa_clear(s_sa_clear);
            ctrl_inst->o_softstall(s_softstall);
            ctrl_inst->o_pipeline_en(s_pipeline_en);
            ctrl_inst->o_cswitch_arr(s_cswitch_arr);

            // 4. Feeder bindings
            act_feeder->i_feeder_en(s_act_feeder_en);
            act_feeder->i_feeder_clear(s_act_feeder_clear);
            act_feeder->i_start(s_act_start);
            act_feeder->i_valid(s_act_valid);
            act_feeder->i_finalpush(s_act_finalpush);
            act_feeder->i_cnt_en(s_act_cnt_en);
            act_feeder->i_cnt_clear(s_act_cnt_clear);
            act_feeder->i_clearfifo(s_act_clearfifo);
            act_feeder->i_pop_en(s_act_pop_en);
            act_feeder->i_finalctx(s_act_finalctx);
            act_feeder->i_context_id(s_local_context_id);
            act_feeder->i_ncontexts(s_out_ncontexts);
            act_feeder->i_act_reps(s_act_reps);
            act_feeder->i_mvm_k(i_mvm_k);

            act_feeder->o_act_done(s_act_done);
            act_feeder->o_act_til_done(s_act_til_done);
            act_feeder->o_fifo_empty(s_act_fifo_empty);
            act_feeder->o_fifo_full(s_act_fifo_full);
            act_feeder->o_stall(s_act_stall);

            act_feeder->o_srama_addr(s_srama_addr);
            act_feeder->o_srama_rden(s_srama_rden);
            act_feeder->i_srama_data(s_srama_data);
            act_feeder->o_act_arr(s_act_arr);

            act_feeder->i_act_base_addr(s_act_base_addr);

            act_feeder->i_act_incntlim(s_act_incntlim);
            act_feeder->i_act_incntstep(s_act_incntstep);
            act_feeder->i_act_outcntlim(s_act_outcntlim);
            act_feeder->i_act_outcntstep(s_act_outcntstep);
            act_feeder->i_act_dil_pat(s_dil_pat);
            // same signal psm already receives (npu_top.h:477); RTL
            // sauria_logic.sv feeds it to both consumers, this model fed it to
            // only one.
            act_feeder->i_rows_active(s_rows_active);
            act_feeder->i_loc_woffs(s_loc_woffs);
            // Full SAURIA IFMAP runtime config
            act_feeder->i_act_xlim(s_act_xlim);
            act_feeder->i_act_xstep(s_act_xstep);
            act_feeder->i_act_ylim(s_act_ylim);
            act_feeder->i_act_ystep(s_act_ystep);
            act_feeder->i_act_chlim(s_act_chlim);
            act_feeder->i_act_chstep(s_act_chstep);

            act_feeder->i_act_til_xlim(s_act_til_xlim);
            act_feeder->i_act_til_xstep(s_act_til_xstep);
            act_feeder->i_act_til_ylim(s_act_til_ylim);
            act_feeder->i_act_til_ystep(s_act_til_ystep);

            wei_feeder->i_wei_incntlim(s_wei_incntlim);
            wei_feeder->i_wei_incntstep(s_wei_incntstep);
            // Full SAURIA WEIGHT runtime config
            wei_feeder->i_wei_wlim(s_wei_wlim);
            wei_feeder->i_wei_wstep(s_wei_wstep);
            wei_feeder->i_wei_klim(s_wei_klim);
            wei_feeder->i_wei_kstep(s_wei_kstep);
            wei_feeder->i_wei_til_klim(s_wei_til_klim);
            wei_feeder->i_wei_til_kstep(s_wei_til_kstep);
            wei_feeder->i_wei_cols_active(s_wei_cols_active);
            wei_feeder->i_wei_waligned(s_wei_waligned);
            wei_feeder->i_feeder_en(s_wei_feeder_en);
            wei_feeder->i_feeder_clear(s_wei_feeder_clear);
            wei_feeder->i_start(s_wei_start);
            wei_feeder->i_valid(s_wei_valid);
            wei_feeder->i_finalpush(s_wei_finalpush);
            wei_feeder->i_cnt_en(s_wei_cnt_en);
            wei_feeder->i_cnt_clear(s_wei_cnt_clear);
            wei_feeder->i_clearfifo(s_wei_clearfifo);
            wei_feeder->i_pop_en(s_wei_pop_en);
            wei_feeder->i_cswitch(s_wei_cswitch);

            wei_feeder->o_wei_done(s_wei_done);
            wei_feeder->o_wei_til_done(s_wei_til_done);
            wei_feeder->o_fifo_empty(s_wei_fifo_empty);
            wei_feeder->o_fifo_full(s_wei_fifo_full);
            wei_feeder->o_stall(s_wei_stall);

            wei_feeder->o_sramb_addr(s_sramb_addr);
            wei_feeder->o_sramb_rden(s_sramb_rden);
            wei_feeder->i_sramb_data(s_sramb_data);
            wei_feeder->o_wei_arr(s_wei_arr);
            wei_feeder->i_context_id(s_local_context_id);
            wei_feeder->i_out_tile_id(s_out_tile_id);
            wei_feeder->i_ncontexts(s_out_ncontexts);
            wei_feeder->i_mvm_k(i_mvm_k);

            wei_feeder->i_wei_base_addr(s_wei_base_addr);

            // 5. SRAM Core Interface bindings (Accelerator-side)
            sram_inst->i_srama_addr(s_srama_addr);
            sram_inst->i_srama_rden(s_srama_rden);
            sram_inst->o_srama_data(s_srama_data);

            sram_inst->i_sramb_addr(s_sramb_addr);
            sram_inst->i_sramb_rden(s_sramb_rden);
            sram_inst->o_sramb_data(s_sramb_data);

            sram_inst->i_sramc_wdata(s_sramc_wdata);
            sram_inst->i_sramc_addr(s_sramc_addr);
            sram_inst->i_sramc_wren(s_sramc_wren);
            sram_inst->i_sramc_rden(s_sramc_rden);
            sram_inst->i_sramc_wmask(s_sramc_wmask);
            sram_inst->o_sramc_rdata(s_sramc_rdata);

            // 6. Systolic Array bindings
            array_inst->i_threshold(i_threshold);
            // Feeders are wired DIRECTLY to the array, as in the RTL (combinational, no extra stage). A debug mux process
            // on this path would add a spurious 1-cycle delay.
            array_inst->i_act_arr(s_act_arr);
            array_inst->i_wei_arr(s_wei_arr);
            array_inst->i_c_arr(s_psm_to_sa_c);
            array_inst->o_c_arr(s_sa_to_psm_c);
            array_inst->i_pipeline_en(s_pipeline_en);
            // RTL feeds the same wire to both feeders (ifmap_feeder.sv:73,
            // wei_feeder.sv); this model routed it only to the array and psm.
            act_feeder->i_pipeline_en(s_pipeline_en);
            wei_feeder->i_pipeline_en(s_pipeline_en);
            array_inst->i_cscan_en(s_cscan_en);
            array_inst->i_cswitch_arr(s_cswitch_arr);
            array_inst->i_sa_clear(s_sa_clear);
            array_inst->i_softstall(s_softstall);
            array_inst->i_pop_en_dbg(s_act_pop_en);
            array_inst->i_context_id(s_context_id);

            // 7. PSM bindings
            psm_inst->i_c_arr(s_sa_to_psm_c);
            psm_inst->o_c_arr(s_psm_to_sa_c);

            psm_inst->i_sramc_rdata(s_sramc_rdata);
            psm_inst->o_sramc_addr(s_sramc_addr);
            psm_inst->o_sramc_wren(s_sramc_wren);
            psm_inst->o_sramc_rden(s_sramc_rden);
            psm_inst->o_sramc_wmask(s_sramc_wmask);
            psm_inst->o_sramc_wdata(s_sramc_wdata);

            psm_inst->i_fsm_start(s_psm_start);
            psm_inst->i_fsm_reset(s_psm_reset);
            psm_inst->i_pipeline_en(s_pipeline_en);

            psm_inst->o_done(s_psm_done);
            psm_inst->o_finalwrite(s_psm_finalwrite);
            psm_inst->o_shift_done(s_psm_shift_done);
            psm_inst->o_cscan_en(s_cscan_en);

            psm_inst->i_out_base_addr(s_out_base_addr);

            // Connect static configurations from Config Registers
            ctrl_inst->i_incntlim(s_incntlim);
            ctrl_inst->i_act_reps(s_act_reps);
            ctrl_inst->i_wei_reps(s_wei_reps);
            ctrl_inst->i_ncontexts(s_out_ncontexts);

            psm_inst->i_cxlim(s_cxlim);
            psm_inst->i_cxstep(s_cxstep);
            psm_inst->i_cklim(s_cklim);
            psm_inst->i_ckstep(s_ckstep);
            // Full SAURIA OUTPUT / PSM runtime
            psm_inst->i_til_cylim(s_out_til_cylim);
            psm_inst->i_til_cystep(s_out_til_cystep);
            psm_inst->i_til_cklim(s_out_til_cklim);
            psm_inst->i_til_ckstep(s_out_til_ckstep);
            psm_inst->i_ncontexts(s_out_ncontexts);
            psm_inst->i_preload_en(s_out_preload_en);
            psm_inst->i_rows_active(s_rows_active);
            psm_inst->i_context_id(s_global_context_id);
            psm_inst->i_total_contexts(i_total_contexts);
#ifdef FX1_A3_PSM_INACTIVE_COLS
            psm_inst->i_inactive_cols(s_out_inactive_cols);
#else
            // psm_inst->i_inactive_cols(s_out_inactive_cols);
#endif

            // Config registers module bindings
            config_regs_inst->o_incntlim(s_incntlim);
            config_regs_inst->o_act_reps(s_act_reps);
            config_regs_inst->o_wei_reps(s_wei_reps);
            config_regs_inst->o_dil_pat(s_dil_pat);
            config_regs_inst->o_rows_active(s_rows_active);
            config_regs_inst->o_loc_woffs(s_loc_woffs);
        }

        void host_rdata_mux()
        {
            uint32_t addr = i_host_addr.read();
            uint32_t mem_region = addr & SAURIA_MEM_ADDR_MASK;
            if (mem_region == CFG_REGS_OFFSET)
            {
                o_host_rdata.write(s_host_rdata_cfg.read());
            }
            else
            {
                o_host_rdata.write(s_host_rdata_sram.read());
            }
        }

#ifdef FX1_A3_SEAM_ORDER
        void seam_ordered_step()
        {
            // Order = RTL combinational resolution order: Control first (outputs from the
            // registered state), then the feeders consume them.
            ctrl_inst->seam_step();
            act_feeder->seam_step();
            wei_feeder->seam_step();
#ifdef FX1_A3_PSM_SEAM_ORDER
            psm_inst->seam_step();   // Psm runs AFTER Control
#endif
        }
#endif

        void done_latch_logic()
        {
            bool reset_active =
                !i_rstn.read() ||
                i_soft_reset.read() ||
                s_cfg_soft_reset.read();

            bool new_start =
                s_start_internal.read();

            if (reset_active || new_start)
            {
                done_latched_reg = false;
            }
            else if (s_ctrl_done.read())
            {
                done_latched_reg = true;

                DBG_COUT << "[NPU_TOP] DONE latched from controller"
                         << std::endl;
            }

            o_done.write(done_latched_reg || s_ctrl_done.read());
        }

        void clear_debug_stream_ref()
        {
            for (int y = 0; y < Y_DIM; y++)
            {
                for (int x = 0; x < X_DIM; x++)
                {
                    dbg_ref_mat[y][x] = 0.0;
                }
            }

            dbg_ref_cycle = 0;
            dbg_compare_count = 0;
        }

        void debug_stream_reference_monitor()
        {
            if (!i_rstn.read())
            {
                clear_debug_stream_ref();
                dbg_ref_ctx = 0;
                dbg_ref_cycle = 0;
                dbg_ref_active = false;
                dbg_ref_initialized = false;
                return;
            }

            // -----------------------------------------------------
            // Start new reference accumulation at each context clear
            // -----------------------------------------------------
            bool new_context_pulse =
                s_act_cnt_clear.read() &&
                s_wei_cnt_clear.read();

            if (new_context_pulse)
            {
                dbg_ref_ctx = s_context_id.read();

                clear_debug_stream_ref();

                dbg_ref_active = true;
                dbg_ref_initialized = true;

                DBG_COUT << "\n[STREAM REF START]"
                         << " context=" << dbg_ref_ctx
                         << " cxlim=" << s_cxlim.read()
                         << " cxstep=" << s_cxstep.read()
                         << std::endl;
            }

            // -----------------------------------------------------
            // Accumulate software reference from actual SA inputs
            // C[y][x] += act_arr[y] * wei_arr[x]
            // -----------------------------------------------------
            if (FX1_DUMPS && dbg_ref_active &&
                s_pipeline_en.read() &&
                s_act_pop_en.read() &&
                s_wei_pop_en.read())
            {
                act_vector_t<Y_DIM, T_ACT> act_vec = s_act_arr.read();
                wei_vector_t<X_DIM, T_WEI> wei_vec = s_wei_arr.read();

                static std::ofstream sa_stream_trace("trace_sysc/sa_input_stream.csv");
                static bool sa_stream_header = false;
                static uint32_t sa_stream_count = 0;

                if (!sa_stream_header)
                {
                    sa_stream_trace << "count,context";
                    for (int y = 0; y < Y_DIM; y++)
                    {
                        sa_stream_trace << ",act" << y;
                    }
                    for (int x = 0; x < X_DIM; x++)
                    {
                        sa_stream_trace << ",wei" << x;
                    }
                    sa_stream_trace << "\n";
                    sa_stream_header = true;
                }

                if (sa_stream_count < 4096)
                {
                    sa_stream_trace << sa_stream_count << "," << s_context_id.read();

                    for (int y = 0; y < Y_DIM; y++)
                    {
                        sa_stream_trace << "," << static_cast<double>(act_vec[y]);
                    }

                    for (int x = 0; x < X_DIM; x++)
                    {
                        sa_stream_trace << "," << static_cast<double>(wei_vec[x]);
                    }

                    sa_stream_trace << "\n";
                    sa_stream_count++;
                }

                for (int y = 0; y < Y_DIM; y++)
                {
                    for (int x = 0; x < X_DIM; x++)
                    {
                        dbg_ref_mat[y][x] +=
                            static_cast<double>(act_vec[y]) *
                            static_cast<double>(wei_vec[x]);
                    }
                }

                if (dbg_ref_cycle < 8)
                {
                    DBG_COUT << "[STREAM REF ACC]"
                             << " ctx=" << dbg_ref_ctx
                             << " cycle=" << dbg_ref_cycle
                             << " act0=" << act_vec[0]
                             << " act1=" << act_vec[1]
                             << " wei0=" << wei_vec[0]
                             << " wei1=" << wei_vec[1]
                             << " ref00=" << dbg_ref_mat[0][0]
                             << " ref10=" << dbg_ref_mat[1][0]
                             << std::endl;
                }

                dbg_ref_cycle++;
            }

            // -----------------------------------------------------
            // Compare PSM/SRAMC write data with stream reference
            // Assumption: each SRAMC vector write corresponds to one
            // output column, containing Y_DIM rows.
            // -----------------------------------------------------
            if (dbg_ref_initialized && s_sramc_wren.read())
            {
                uint32_t addr = s_sramc_addr.read();

                uint32_t context_stride =
                    s_cxlim.read() * s_cxstep.read();

                uint32_t ctx_base =
                    dbg_ref_ctx * context_stride;

                if (addr >= ctx_base && s_cxstep.read() != 0)
                {
                    uint32_t col =
                        (addr - ctx_base) / s_cxstep.read();

                    if (col < X_DIM && dbg_compare_count < 64)
                    {
                        psum_vector_t<Y_DIM, T_PSUM> actual =
                            s_sramc_wdata.read();

                        // DBG_COUT << "\n[STREAM REF CMP]"
                        //           << " ctx=" << dbg_ref_ctx
                        //           << " addr=" << addr
                        //           << " col=" << col
                        //           << " ref_vs_actual=[\n";

                        for (int y = 0; y < Y_DIM; y++)
                        {
                            double ref_val = dbg_ref_mat[y][col];
                            double act_val = static_cast<double>(actual[y]);

                            DBG_COUT << "  y=" << y
                                     << " ref=" << ref_val
                                     << " actual=" << act_val
                                     << " diff=" << (act_val - ref_val)
                                     << "\n";
                        }

                        DBG_COUT << "]" << std::endl;

                        dbg_compare_count++;
                    }
                }
            }
        }

        void debug_sa_input_stream_dump()
        {
            if (!FX1_DUMPS) return;
            if (!i_rstn.read())
            {
                return;
            }

            std::string inst_name = this->name();
            if (inst_name.find("NpuTop_std") == std::string::npos)
            {
                return;
            }

            static std::ofstream sa_stream_trace("trace_sysc/sa_input_stream_physical_std.csv");
            static bool header_written = false;

            static bool prev_clear = false;
            static bool active_dump = false;
            static uint32_t current_ctx = 0;

            if (!header_written)
            {
                sa_stream_trace << "context,t";
                for (int y = 0; y < Y_DIM; y++)
                {
                    sa_stream_trace << ",act" << y;
                }
                for (int x = 0; x < X_DIM; x++)
                {
                    sa_stream_trace << ",wei" << x;
                }
                sa_stream_trace << "\n";
                header_written = true;
            }

            bool clear_now =
                s_act_cnt_clear.read() &&
                s_wei_cnt_clear.read();

            bool clear_pulse =
                clear_now && !prev_clear;

            prev_clear = clear_now;

            if (clear_pulse)
            {
                current_ctx = s_context_id.read();
                active_dump = true;

                DBG_COUT << "[REAL FEEDER STREAM DUMP START]"
                         << " ctx=" << current_ctx
                         << " incntlim=" << s_incntlim.read()
                         << std::endl;
            }

            if (!active_dump)
            {
                return;
            }
        }

        void debug_execution_monitor()
        {
            bool running = false;
            int cycle = 0;

            while (true)
            {
                wait(i_clk.posedge_event());

                if (s_start_internal.read() && !running)
                {
                    running = true;
                    cycle = 0;

                    DBG_COUT << "\n[NPU_TOP DEBUG] Execution monitor started"
                             << std::endl;
                }

                if (running)
                {
                    cycle++;

                    if ((cycle % 100) == 0)
                    {
                        DBG_COUT << "[NPU_TOP DEBUG] cycle=" << cycle
                                 << " start=" << s_start_internal.read()
                                 << " done=" << o_done.read()
                                 << std::endl;
                    }

                    if (o_done.read())
                    {
                        DBG_COUT << "[NPU_TOP DEBUG] DONE at cycle "
                                 << cycle << std::endl;
                        running = false;
                    }

                    if (cycle == 20000)
                    {
                        DBG_COUT << "[NPU_TOP DEBUG] TIMEOUT monitor reached 20000 cycles"
                                 << std::endl;
                    }
                }
            }
        }

        void start_reset_logic()
        {
            bool start_val = i_start.read() || s_cfg_start.read();

            static bool prev_start = false;

            if (start_val && !prev_start)
            {
                DBG_COUT << "\n=========================================\n";
                DBG_COUT << "NPU TOP RUNTIME CONFIGURATION AT START\n";
                DBG_COUT << "=========================================\n";

                DBG_COUT << "\nSTART SOURCE\n";
                DBG_COUT << "i_start     : " << i_start.read() << "\n";
                DBG_COUT << "s_cfg_start : " << s_cfg_start.read() << "\n";

                DBG_COUT << "\nCONTROL\n";
                DBG_COUT << "INCNTLIM    : " << s_incntlim.read() << "\n";
                DBG_COUT << "ACT_REPS    : " << s_act_reps.read() << "\n";
                DBG_COUT << "WEI_REPS    : " << s_wei_reps.read() << "\n";

                DBG_COUT << "\nACTIVATION\n";
                DBG_COUT << "ACT_INCNTLIM   : " << s_act_incntlim.read() << "\n";
                DBG_COUT << "ACT_INCNTSTEP  : " << s_act_incntstep.read() << "\n";
                DBG_COUT << "ACT_OUTCNTLIM  : " << s_act_outcntlim.read() << "\n";
                DBG_COUT << "ACT_OUTCNTSTEP : " << s_act_outcntstep.read() << "\n";
                DBG_COUT << "DIL_PAT        : 0x"
                         << std::hex << s_dil_pat.read().to_uint64()
                         << std::dec << "\n";
                DBG_COUT << "ROWS_ACTIVE    : " << s_rows_active.read() << "\n";
                DBG_COUT << "ACT_XLIM       : " << s_act_xlim.read() << "\n";
                DBG_COUT << "ACT_XSTEP      : " << s_act_xstep.read() << "\n";
                DBG_COUT << "ACT_YLIM       : " << s_act_ylim.read() << "\n";
                DBG_COUT << "ACT_YSTEP      : " << s_act_ystep.read() << "\n";
                DBG_COUT << "ACT_CHLIM      : " << s_act_chlim.read() << "\n";
                DBG_COUT << "ACT_CHSTEP     : " << s_act_chstep.read() << "\n";
                DBG_COUT << "ACT_TIL_XLIM   : " << s_act_til_xlim.read() << "\n";
                DBG_COUT << "ACT_TIL_XSTEP  : " << s_act_til_xstep.read() << "\n";
                DBG_COUT << "ACT_TIL_YLIM   : " << s_act_til_ylim.read() << "\n";
                DBG_COUT << "ACT_TIL_YSTEP  : " << s_act_til_ystep.read() << "\n";

                DBG_COUT << "\nWEIGHT\n";
                DBG_COUT << "WEI_INCNTLIM   : " << s_wei_incntlim.read() << "\n";
                DBG_COUT << "WEI_INCNTSTEP  : " << s_wei_incntstep.read() << "\n";
                DBG_COUT << "WEI_WLIM        : " << s_wei_wlim.read() << "\n";
                DBG_COUT << "WEI_WSTEP       : " << s_wei_wstep.read() << "\n";
                DBG_COUT << "WEI_KLIM        : " << s_wei_klim.read() << "\n";
                DBG_COUT << "WEI_KSTEP       : " << s_wei_kstep.read() << "\n";
                DBG_COUT << "WEI_TIL_KLIM    : " << s_wei_til_klim.read() << "\n";
                DBG_COUT << "WEI_TIL_KSTEP   : " << s_wei_til_kstep.read() << "\n";
                DBG_COUT << "WEI_COLS_ACTIVE : 0x"
                         << std::hex << s_wei_cols_active.read()
                         << std::dec << "\n";
                DBG_COUT << "WEI_WALIGNED    : " << s_wei_waligned.read() << "\n";

                DBG_COUT << "\nPSUM\n";
                DBG_COUT << "CXLIM          : " << s_cxlim.read() << "\n";
                DBG_COUT << "CXSTEP         : " << s_cxstep.read() << "\n";
                DBG_COUT << "CKLIM          : " << s_cklim.read() << "\n";
                DBG_COUT << "CKSTEP         : " << s_ckstep.read() << "\n";
                DBG_COUT << "NCONTEXTS      : " << s_out_ncontexts.read() << "\n";
                DBG_COUT << "TIL_CYLIM      : " << s_out_til_cylim.read() << "\n";
                DBG_COUT << "TIL_CYSTEP     : " << s_out_til_cystep.read() << "\n";
                DBG_COUT << "TIL_CKLIM      : " << s_out_til_cklim.read() << "\n";
                DBG_COUT << "TIL_CKSTEP     : " << s_out_til_ckstep.read() << "\n";
                DBG_COUT << "INACTIVE_COLS  : " << s_out_inactive_cols.read() << "\n";
                DBG_COUT << "PRELOAD_EN     : " << s_out_preload_en.read() << "\n";

                DBG_COUT << "\nMEMORY MAP\n";
                DBG_COUT << "ACT_BASE_ADDR  : " << s_act_base_addr.read() << "\n";
                DBG_COUT << "WEI_BASE_ADDR  : " << s_wei_base_addr.read() << "\n";
                DBG_COUT << "OUT_BASE_ADDR  : " << s_out_base_addr.read() << "\n";

                DBG_COUT << "=========================================\n\n";
            }

            prev_start = start_val;

            s_start_internal.write(start_val);
            s_ctrl_reset_internal.write(i_soft_reset.read() || s_cfg_soft_reset.read());
        }

        ~NpuTop()
        {
            delete sram_inst;
            delete ctrl_inst;
            delete act_feeder;
            delete wei_feeder;
            delete array_inst;
            delete psm_inst;
            delete config_regs_inst;
        }

        void trace(sc_trace_file *tf)
        {
            if (!tf)
                return;
            // Trace configurations
            sc_trace(tf, i_start, std::string(name()) + ".i_start");
            sc_trace(tf, o_done, std::string(name()) + ".o_done");
            sc_trace(tf, o_deadlock, std::string(name()) + ".o_deadlock");
            sc_trace(tf, i_threshold, std::string(name()) + ".i_threshold");
            sc_trace(tf, i_select, std::string(name()) + ".i_select");

            // Trace internal signals
            sc_trace(tf, s_act_feeder_en, std::string(name()) + ".s_act_feeder_en");
            sc_trace(tf, s_act_start, std::string(name()) + ".s_act_start");
            sc_trace(tf, s_act_valid, std::string(name()) + ".s_act_valid");
            sc_trace(tf, s_act_pop_en, std::string(name()) + ".s_act_pop_en");
            sc_trace(tf, s_wei_feeder_en, std::string(name()) + ".s_wei_feeder_en");
            sc_trace(tf, s_wei_start, std::string(name()) + ".s_wei_start");
            sc_trace(tf, s_wei_valid, std::string(name()) + ".s_wei_valid");
            sc_trace(tf, s_wei_pop_en, std::string(name()) + ".s_wei_pop_en");
            sc_trace(tf, s_pipeline_en, std::string(name()) + ".s_pipeline_en");
            sc_trace(tf, s_cscan_en, std::string(name()) + ".s_cscan_en");
            sc_trace(tf, s_psm_start, std::string(name()) + ".s_psm_start");
            sc_trace(tf, s_psm_done, std::string(name()) + ".s_psm_done");
            sc_trace(tf, s_psm_shift_done, std::string(name()) + ".s_psm_shift_done");
            sc_trace(tf, s_psm_finalwrite, std::string(name()) + ".s_psm_finalwrite");

            // Trace memory controls
            sc_trace(tf, s_srama_addr, std::string(name()) + ".s_srama_addr");
            sc_trace(tf, s_srama_rden, std::string(name()) + ".s_srama_rden");
            sc_trace(tf, s_sramb_addr, std::string(name()) + ".s_sramb_addr");
            sc_trace(tf, s_sramb_rden, std::string(name()) + ".s_sramb_rden");
            sc_trace(tf, s_sramc_addr, std::string(name()) + ".s_sramc_addr");
            sc_trace(tf, s_sramc_wren, std::string(name()) + ".s_sramc_wren");
            sc_trace(tf, s_sramc_rden, std::string(name()) + ".s_sramc_rden");

            // Trace data vectors
            sc_trace(tf, s_act_arr, std::string(name()) + ".s_act_arr");
            sc_trace(tf, s_wei_arr, std::string(name()) + ".s_wei_arr");
            sc_trace(tf, s_sa_to_psm_c, std::string(name()) + ".s_sa_to_psm_c");
        }

    private:
        // Submodules instances
        Sram<X_DIM, Y_DIM, T_ACT, T_WEI, T_PSUM, SRAMA_CAP, SRAMB_CAP, SRAMC_CAP> *sram_inst{nullptr};
        Control<X_DIM, Y_DIM, PE_LAT, EXTRA_CSREG> *ctrl_inst{nullptr};
        IfmapFeeder<Y_DIM, T_ACT, SRAMA_CAP, ACT_FIFO_POS> *act_feeder{nullptr};
        WeightFeeder<X_DIM, T_WEI, SRAMB_CAP, WEI_FIFO_POS, 0> *wei_feeder{nullptr};
        SystolicArray<X_DIM, Y_DIM, T_ACT, T_WEI, T_PSUM> *array_inst{nullptr};
        Psm<X_DIM, Y_DIM, T_PSUM, SRAMC_CAP> *psm_inst{nullptr};
        ConfigRegs<32, 32, X_DIM, Y_DIM, 2, 15, 15, 15, DILP_W, 8> *config_regs_inst{nullptr};

        // ---------------------------------------------------------
        // Debug: software reference from actual act_arr/wei_arr stream
        // ---------------------------------------------------------
        double dbg_ref_mat[Y_DIM][X_DIM];
        uint32_t dbg_ref_ctx{0};
        uint32_t dbg_ref_cycle{0};
        bool dbg_ref_active{false};
        bool dbg_ref_initialized{false};

        uint32_t dbg_compare_count{0};

        // Internal signals for inter-module communication (memory)
        sc_signal<uint32_t> s_act_base_addr;
        sc_signal<uint32_t> s_wei_base_addr;
        sc_signal<uint32_t> s_out_base_addr;

        sc_signal<uint32_t> s_global_context_id;
        sc_signal<uint32_t> s_local_context_id;
        sc_signal<uint32_t> s_out_tile_id;

        // Host mux and FSM handshake internal signals
        sc_signal<host_data_t> s_host_rdata_sram{"s_host_rdata_sram"};
        sc_signal<host_data_t> s_host_rdata_cfg{"s_host_rdata_cfg"};
        sc_signal<bool> s_ctrl_done{"s_ctrl_done"};
        bool done_latched_reg{false};
        sc_signal<bool> s_cfg_start{"s_cfg_start"};
        sc_signal<bool> s_cfg_soft_reset{"s_cfg_soft_reset"};
        // PROFILE broadcast from unified config_regs. Modules auto-select LINEAR vs
        // SAURIA from the (profile-routed) config they receive, so this is currently a
        // debug/observation tap; expose downstream later if explicit wiring is wanted.
        sc_signal<uint32_t> s_cfg_profile{"s_cfg_profile"};
        sc_signal<bool> s_start_internal{"s_start_internal"};
        sc_signal<bool> s_ctrl_reset_internal{"s_ctrl_reset_internal"};

        // Static parameter simulation ports/signals
        sc_signal<bool> s_false{"s_false", false};
        sc_signal<uint32_t> s_incntlim{"s_incntlim"};
        sc_signal<uint32_t> s_act_reps{"s_act_reps"};
        sc_signal<uint32_t> s_wei_reps{"s_wei_reps"};
        sc_signal<uint32_t> s_one{"s_one", 1};
        sc_signal<sc_bv<DILP_W>> s_dil_pat{"s_dil_pat"};
        sc_signal<sramc_mask_t<Y_DIM>> s_rows_active{"s_rows_active"};
        sc_signal<act_vector_t<Y_DIM, uint32_t>> s_loc_woffs{"s_loc_woffs"};

        // Internal Signals: Controller <-> Feeders
        sc_signal<bool> s_act_feeder_en{"s_act_feeder_en"};
        sc_signal<bool> s_act_feeder_clear{"s_act_feeder_clear"};
        sc_signal<bool> s_act_start{"s_act_start"};
        sc_signal<bool> s_act_valid{"s_act_valid"};
        sc_signal<bool> s_act_finalpush{"s_act_finalpush"};
        sc_signal<bool> s_act_cnt_en{"s_act_cnt_en"};
        sc_signal<bool> s_act_cnt_clear{"s_act_cnt_clear"};
        sc_signal<bool> s_act_clearfifo{"s_act_clearfifo"};
        sc_signal<bool> s_act_pop_en{"s_act_pop_en"};
        sc_signal<bool> s_act_finalctx{"s_act_finalctx"};

        // Full SAURIA IFMAP runtime config
        sc_signal<uint32_t> s_act_xlim{"s_act_xlim"};
        sc_signal<uint32_t> s_act_xstep{"s_act_xstep"};
        sc_signal<uint32_t> s_act_ylim{"s_act_ylim"};
        sc_signal<uint32_t> s_act_ystep{"s_act_ystep"};
        sc_signal<uint32_t> s_act_chlim{"s_act_chlim"};
        sc_signal<uint32_t> s_act_chstep{"s_act_chstep"};
        sc_signal<uint32_t> s_act_til_xlim{"s_act_til_xlim"};
        sc_signal<uint32_t> s_act_til_xstep{"s_act_til_xstep"};
        sc_signal<uint32_t> s_act_til_ylim{"s_act_til_ylim"};
        sc_signal<uint32_t> s_act_til_ystep{"s_act_til_ystep"};
        sc_signal<uint32_t> s_act_incntlim{"s_act_incntlim"};
        sc_signal<uint32_t> s_act_incntstep{"s_act_incntstep"};
        sc_signal<uint32_t> s_act_outcntlim{"s_act_outcntlim"};
        sc_signal<uint32_t> s_act_outcntstep{"s_act_outcntstep"};
        sc_signal<uint32_t> s_context_id{"s_context_id"};

        sc_signal<bool> s_wei_feeder_en{"s_wei_feeder_en"};
        sc_signal<bool> s_wei_feeder_clear{"s_wei_feeder_clear"};
        sc_signal<bool> s_wei_start{"s_wei_start"};
        sc_signal<bool> s_wei_valid{"s_wei_valid"};
        sc_signal<bool> s_wei_finalpush{"s_wei_finalpush"};
        sc_signal<bool> s_wei_cnt_en{"s_wei_cnt_en"};
        sc_signal<bool> s_wei_cnt_clear{"s_wei_cnt_clear"};
        sc_signal<bool> s_wei_clearfifo{"s_wei_clearfifo"};
        sc_signal<bool> s_wei_pop_en{"s_wei_pop_en"};
        sc_signal<bool> s_wei_cswitch{"s_wei_cswitch"};
        sc_signal<uint32_t> s_wei_incntlim{"s_wei_incntlim"};
        sc_signal<uint32_t> s_wei_incntstep{"s_wei_incntstep"};
        // Full SAURIA WEIGHT runtime config
        sc_signal<uint32_t> s_wei_wlim{"s_wei_wlim"};
        sc_signal<uint32_t> s_wei_wstep{"s_wei_wstep"};
        sc_signal<uint32_t> s_wei_klim{"s_wei_klim"};
        sc_signal<uint32_t> s_wei_kstep{"s_wei_kstep"};
        sc_signal<uint32_t> s_wei_til_klim{"s_wei_til_klim"};
        sc_signal<uint32_t> s_wei_til_kstep{"s_wei_til_kstep"};
        sc_signal<uint64_t> s_wei_cols_active{"s_wei_cols_active"};
        sc_signal<uint32_t> s_wei_waligned{"s_wei_waligned"};

        // Internal Signals: Controller <-> PSM
        sc_signal<bool> s_psm_start{"s_psm_start"};
        sc_signal<bool> s_psm_reset{"s_psm_reset"};

        // Internal Signals: Controller <-> Systolic Array
        sc_signal<bool> s_sa_clear{"s_sa_clear"};
        sc_signal<bool> s_softstall{"s_softstall"};
        sc_signal<bool> s_pipeline_en{"s_pipeline_en"};
        sc_signal<sc_bv<X_DIM>> s_cswitch_arr{"s_cswitch_arr"};

        // Internal Signals: Feeders <-> Control (Feedbacks)
        sc_signal<bool> s_act_done{"s_act_done"};
        sc_signal<bool> s_act_til_done{"s_act_til_done"};
        sc_signal<bool> s_act_fifo_empty{"s_act_fifo_empty"};
        sc_signal<bool> s_act_fifo_full{"s_act_fifo_full"};
        sc_signal<bool> s_act_stall{"s_act_stall"};

        sc_signal<bool> s_wei_done{"s_wei_done"};
        sc_signal<bool> s_wei_til_done{"s_wei_til_done"};
        sc_signal<bool> s_wei_fifo_empty{"s_wei_fifo_empty"};
        sc_signal<bool> s_wei_fifo_full{"s_wei_fifo_full"};
        sc_signal<bool> s_wei_stall{"s_wei_stall"};

        // Internal Signals: PSM <-> Control (Feedbacks)
        sc_signal<bool> s_psm_done{"s_psm_done"};
        sc_signal<bool> s_psm_finalwrite{"s_psm_finalwrite"};
        sc_signal<bool> s_psm_shift_done{"s_psm_shift_done"};
        sc_signal<bool> s_cscan_en{"s_cscan_en"};

        sc_signal<uint32_t> s_cxlim{"s_cxlim"};
        sc_signal<uint32_t> s_cxstep{"s_cxstep"};
        sc_signal<uint32_t> s_cklim{"s_cklim"};
        sc_signal<uint32_t> s_ckstep{"s_ckstep"};
        // Full SAURIA output/PSM runtime config
        sc_signal<uint32_t> s_out_ncontexts{"s_out_ncontexts"};
        sc_signal<uint32_t> s_out_til_cylim{"s_out_til_cylim"};
        sc_signal<uint32_t> s_out_til_cystep{"s_out_til_cystep"};
        sc_signal<uint32_t> s_out_til_cklim{"s_out_til_cklim"};
        sc_signal<uint32_t> s_out_til_ckstep{"s_out_til_ckstep"};
        sc_signal<uint32_t> s_out_inactive_cols{"s_out_inactive_cols"};
        sc_signal<bool> s_out_preload_en{"s_out_preload_en"};

        // Internal Signals: Feeders <-> SRAM
        sc_signal<uint32_t> s_srama_addr{"s_srama_addr"};
        sc_signal<bool> s_srama_rden{"s_srama_rden"};
        sc_signal<act_vector_t<Y_DIM, T_ACT>> s_srama_data{"s_srama_data"};

        sc_signal<uint32_t> s_sramb_addr{"s_sramb_addr"};
        sc_signal<bool> s_sramb_rden{"s_sramb_rden"};
        sc_signal<wei_vector_t<X_DIM, T_WEI>> s_sramb_data{"s_sramb_data"};

        // Internal Signals: PSM <-> SRAM
        sc_signal<psum_vector_t<Y_DIM, T_PSUM>> s_sramc_wdata{"s_sramc_wdata"};
        sc_signal<uint32_t> s_sramc_addr{"s_sramc_addr"};
        sc_signal<bool> s_sramc_wren{"s_sramc_wren"};
        sc_signal<bool> s_sramc_rden{"s_sramc_rden"};
        sc_signal<sramc_mask_t<Y_DIM>> s_sramc_wmask{"s_sramc_wmask"};
        sc_signal<psum_vector_t<Y_DIM, T_PSUM>> s_sramc_rdata{"s_sramc_rdata"};

        // Internal Signals: Feeders <-> Systolic Array
        sc_signal<act_vector_t<Y_DIM, T_ACT>> s_act_arr{"s_act_arr"};
        sc_signal<wei_vector_t<X_DIM, T_WEI>> s_wei_arr{"s_wei_arr"};

        // Internal Signals: PSM <-> Systolic Array
        sc_signal<psum_vector_t<Y_DIM, T_PSUM>> s_psm_to_sa_c{"s_psm_to_sa_c"};
        sc_signal<psum_vector_t<Y_DIM, T_PSUM>> s_sa_to_psm_c{"s_sa_to_psm_c"};

        // [Run - time(Layer config)]: internal signals
        sc_signal<uint32_t> s_in_h{"s_in_h"};
        sc_signal<uint32_t> s_in_w{"s_in_w"};
        sc_signal<uint32_t> s_in_c{"s_in_c"};

        sc_signal<uint32_t> s_kernel_h{"s_kernel_h"};
        sc_signal<uint32_t> s_kernel_w{"s_kernel_w"};

        sc_signal<uint32_t> s_stride{"s_stride"};
        sc_signal<uint32_t> s_padding{"s_padding"};
        sc_signal<uint32_t> s_dilation{"s_dilation"};

        sc_signal<uint32_t> s_tile_x{"s_tile_x"};
        sc_signal<uint32_t> s_tile_y{"s_tile_y"};
        sc_signal<uint32_t> s_tile_k{"s_tile_k"};
        sc_signal<uint32_t> s_tile_c{"s_tile_c"};

        sc_signal<uint32_t> s_x_used{"s_x_used"};
        sc_signal<uint32_t> s_y_used{"s_y_used"};
    };

} // namespace sauria_rtl

#endif // SAURIA_RTL_NPU_TOP_H
