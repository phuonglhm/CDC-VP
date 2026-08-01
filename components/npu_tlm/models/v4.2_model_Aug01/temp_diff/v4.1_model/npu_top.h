// SystemC Model for SAURIA NPU Core
// Top-Level NPU Wrapper (Connecting Sram, Control, Feeders, Array, PSM, ConfigRegs)

#ifndef SAURIA_UNIFIED_NPU_TOP_H
#define SAURIA_UNIFIED_NPU_TOP_H

#include <fstream>
#include <sstream>
#include <vector>
#include "sauria_types.h"
#include "debug.h"
// Unified profile-aware config registers (in unified/). Every other module is the
// UNCHANGED v1 implementation, pulled from the parent dirs via the -I. include path.
#include "config_regs.h"
#include "control/main_controller.h"
#include "data_feeder/ifmap_feeder.h"
#include "data_feeder/wei_feeder.h"
#include "systolic_array/sa_array.h"
#include "psm/psm_top.h"
#include "sram/sram_top.h"

#ifndef FX1_NO_PERF
#include "instrumentation/perf_counters.h"
#endif

namespace sauria
{

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
        int PE_LAT = X_DIM + Y_DIM,
        int EXTRA_CSREG = 1>
    class NpuTop : public sc_module
    {
    public:
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
        fx1::PerfCounters *perf{nullptr};
        void attach_perf(fx1::PerfCounters *p)
        {
            perf = p;
            if (array_inst)
                array_inst->perf = p;
        }
#endif
        // Constructor
        SC_HAS_PROCESS(NpuTop);
        NpuTop(sc_module_name nm, const PeConfig &pe_cfg = PeConfig()) : sc_module(nm)
        {
            // Instantiate submodules
            sram_inst = new Sram<X_DIM, Y_DIM, T_ACT, T_WEI, T_PSUM, SRAMA_CAP, SRAMB_CAP, SRAMC_CAP>("sram_inst");
            ctrl_inst = new Control<X_DIM, Y_DIM, PE_LAT, EXTRA_CSREG>("ctrl_inst");
            act_feeder = new IfmapFeeder<Y_DIM, T_ACT, SRAMA_CAP, FIFO_DEPTH>("act_feeder");
            wei_feeder = new WeightFeeder<X_DIM, T_WEI, SRAMB_CAP, FIFO_DEPTH>("wei_feeder");
            array_inst = new SystolicArray<X_DIM, Y_DIM, T_ACT, T_WEI, T_PSUM>("array_inst", pe_cfg);
            psm_inst = new Psm<X_DIM, Y_DIM, T_PSUM, SRAMC_CAP>("psm_inst");
            config_regs_inst = new ConfigRegs<32, 32, X_DIM, Y_DIM, 2, 15, 15, 15, DILP_W, 8>("config_regs_inst");

#ifndef FX1_NO_PERF
            // Forward the externally-attached perf struct down to the array.
            // attach_perf() must be called before sc_start() if used.
            array_inst->perf = perf;
#endif

            SC_METHOD(host_rdata_mux);
            sensitive << i_host_addr << s_host_rdata_sram << s_host_rdata_cfg;

            SC_METHOD(start_reset_logic);
            sensitive << i_start << s_cfg_start << i_soft_reset << s_cfg_soft_reset << s_ctrl_done;

            SC_METHOD(done_latch_logic);
            sensitive << i_clk.pos();
            dont_initialize();

            SC_METHOD(debug_stream_reference_monitor);
            sensitive << i_clk.pos();
            dont_initialize();

            SC_METHOD(debug_sa_input_stream_dump);
            sensitive << i_clk.pos();
            dont_initialize();

            SC_METHOD(debug_ref_stream_mux);
            sensitive << i_clk.pos();
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
            array_inst->i_act_arr(s_act_arr_to_array);
            array_inst->i_wei_arr(s_wei_arr_to_array);
            array_inst->i_c_arr(s_psm_to_sa_c);
            array_inst->o_c_arr(s_sa_to_psm_c);
            array_inst->i_pipeline_en(s_pipeline_en);
            array_inst->i_cscan_en(s_cscan_en);
            array_inst->i_cswitch_arr(s_cswitch_arr);
            array_inst->i_sa_clear(s_sa_clear);
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
            // psm_inst->i_inactive_cols(s_out_inactive_cols);

            // Config registers module bindings
            config_regs_inst->o_incntlim(s_incntlim);
            config_regs_inst->o_act_reps(s_act_reps);
            config_regs_inst->o_wei_reps(s_wei_reps);
            config_regs_inst->o_dil_pat(s_dil_pat);
            config_regs_inst->o_rows_active(s_rows_active);
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
            if (dbg_ref_active &&
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

            // uint32_t k_len = s_incntlim.read();
            // if (k_len == 0)
            //     k_len = s_incntlim.read();
            // uint32_t max_delay = (X_DIM > Y_DIM) ? (X_DIM - 1) : (Y_DIM - 1);
            // uint32_t total_t = k_len + max_delay;

            // DBG_COUT << "[REAL FEEDER STREAM DUMP START]"
            //           << " ctx = " << current_ctx
            //           << " incntlim = " << s_incntlim.read()
            //           << std::endl;

            // if (!s_pipeline_en.read())
            // {
            //     return;
            // }

            // if (local_t >= total_t)
            // {
            //     active_dump = false;
            //     return;
            // }

            // act_vector_t<Y_DIM, T_ACT> act_vec = s_act_arr.read();
            // wei_vector_t<X_DIM, T_WEI> wei_vec = s_wei_arr.read();

            // sa_stream_trace << current_ctx << "," << local_t;

            // for (int y = 0; y < Y_DIM; y++)
            // {
            //     sa_stream_trace << "," << static_cast<int32_t>(act_vec[y]);
            // }

            // for (int x = 0; x < X_DIM; x++)
            // {
            //     sa_stream_trace << "," << static_cast<int32_t>(wei_vec[x]);
            // }

            // sa_stream_trace << "\n";
            // sa_stream_trace.flush();

            // local_t++;

            // if (local_t >= total_t)
            // {
            //     active_dump = false;
            //     DBG_COUT << "[REAL FEEDER STREAM DUMP DONE]"
            //               << " ctx=" << current_ctx
            //               << " dumped_t=" << local_t
            //               << std::endl;
            // }
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

    public:
        // Submodules instances
        Sram<X_DIM, Y_DIM, T_ACT, T_WEI, T_PSUM, SRAMA_CAP, SRAMB_CAP, SRAMC_CAP> *sram_inst{nullptr};
        Control<X_DIM, Y_DIM, PE_LAT, EXTRA_CSREG> *ctrl_inst{nullptr};
        IfmapFeeder<Y_DIM, T_ACT, SRAMA_CAP, FIFO_DEPTH> *act_feeder{nullptr};
        WeightFeeder<X_DIM, T_WEI, SRAMB_CAP, FIFO_DEPTH> *wei_feeder{nullptr};
        SystolicArray<X_DIM, Y_DIM, T_ACT, T_WEI, T_PSUM> *array_inst{nullptr};
        Psm<X_DIM, Y_DIM, T_PSUM, SRAMC_CAP> *psm_inst{nullptr};
        ConfigRegs<32, 32, X_DIM, Y_DIM, 2, 15, 15, 15, DILP_W, 8> *config_regs_inst{nullptr};

    private:

        // ---------------------------------------------------------
        // Debug golden stream injection from SAURIA Python
        // A shape: [4, 8, 576]
        // B shape: [4, 576, 16]
        // ---------------------------------------------------------
        bool dbg_ref_stream_loaded{false};
        bool dbg_ref_stream_en{false}; // tạm bật để debug

        std::vector<int32_t> dbg_A_flat;
        std::vector<int32_t> dbg_B_flat;

        uint32_t dbg_stream_ctx{0};
        uint32_t dbg_stream_k{0};
        bool dbg_stream_active{false};
        bool dbg_prev_cnt_clear{false};

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

        // sc_signal<uint32_t> s_total_contexts;

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
        sc_signal<uint32_t> s_wei_cols_active{"s_wei_cols_active"};
        sc_signal<uint32_t> s_wei_waligned{"s_wei_waligned"};

        // Internal Signals: Controller <-> PSM
        sc_signal<bool> s_psm_start{"s_psm_start"};
        sc_signal<bool> s_psm_reset{"s_psm_reset"};

        // Internal Signals: Controller <-> Systolic Array
        sc_signal<bool> s_sa_clear{"s_sa_clear"};
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
        sc_signal<act_vector_t<Y_DIM, T_ACT>> s_act_arr_to_array{"s_act_arr_to_array"};
        sc_signal<wei_vector_t<X_DIM, T_WEI>> s_wei_arr_to_array{"s_wei_arr_to_array"};

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

        void debug_load_ref_stream_once()
        {
            if (dbg_ref_stream_loaded)
            {
                return;
            }

            auto load_txt_i32 = [](const std::string &path)
            {
                std::vector<int32_t> v;
                std::ifstream ifs(path);
                std::string line;

                while (std::getline(ifs, line))
                {
                    if (line.empty())
                        continue;
                    v.push_back(static_cast<int32_t>(std::stol(line)));
                }

                return v;
            };

            dbg_A_flat = load_txt_i32("/tmp/sauria_A_Mat_mvm_flat.txt");
            dbg_B_flat = load_txt_i32("/tmp/sauria_B_Mat_mvm_flat.txt");

            DBG_COUT << "[DBG REF STREAM] loaded A elements=" << dbg_A_flat.size()
                     << " B elements=" << dbg_B_flat.size()
                     << std::endl;

            dbg_ref_stream_loaded = true;
        }

        void debug_ref_stream_mux()
        {
            if (!i_rstn.read())
            {
                s_act_arr_to_array.write(act_vector_t<Y_DIM, T_ACT>(static_cast<T_ACT>(0)));
                s_wei_arr_to_array.write(wei_vector_t<X_DIM, T_WEI>(static_cast<T_WEI>(0)));

                dbg_stream_ctx = 0;
                dbg_stream_k = 0;
                dbg_stream_active = false;
                dbg_prev_cnt_clear = false;
                return;
            }

            if (!dbg_ref_stream_en)
            {
                s_act_arr_to_array.write(s_act_arr.read());
                s_wei_arr_to_array.write(s_wei_arr.read());
                return;
            }

            debug_load_ref_stream_once();

            bool clear_now =
                s_act_cnt_clear.read() &&
                s_wei_cnt_clear.read();

            bool clear_pulse =
                clear_now && !dbg_prev_cnt_clear;

            dbg_prev_cnt_clear = clear_now;

            if (clear_pulse)
            {
                dbg_stream_ctx = s_context_id.read();
                dbg_stream_k = 0;
                dbg_stream_active = true;

                DBG_COUT << "[DBG REF STREAM START]"
                         << " ctx=" << dbg_stream_ctx
                         << " incntlim=" << s_incntlim.read()
                         << std::endl;
            }

            act_vector_t<Y_DIM, T_ACT> act_out(static_cast<T_ACT>(0));
            wei_vector_t<X_DIM, T_WEI> wei_out(static_cast<T_WEI>(0));

            bool valid_compute_cycle =
                dbg_stream_active &&
                s_pipeline_en.read() &&
                s_act_pop_en.read() &&
                s_wei_pop_en.read();

            // ---------------------------------------------------------
            // Physical skewed injection into systolic array boundary.
            // A_Mat_mvm/B_Mat_mvm are logical MVM streams.
            // The array boundary needs feeder-register skew:
            //
            //   act lane y  gets A[ctx][y][k] at time t = k + y
            //   wei lane x  gets B[ctx][k][x] at time t = k + x
            //
            // Therefore:
            //   act[y] = A[ctx][y][t-y]
            //   wei[x] = B[ctx][t-x][x]
            // ---------------------------------------------------------

            const uint32_t K_LEN = 576;

            // Need extra cycles to feed delayed lanes.
            // max lane delay = max(Y_DIM-1, X_DIM-1)
            const uint32_t INJECT_TOTAL =
                K_LEN + ((X_DIM > Y_DIM) ? (X_DIM - 1) : (Y_DIM - 1));

            bool valid_inject_cycle =
                dbg_stream_active &&
                s_pipeline_en.read();

            if (valid_inject_cycle && dbg_stream_k < INJECT_TOTAL)
            {
                uint32_t t = dbg_stream_k;

                for (int y = 0; y < Y_DIM; y++)
                {
                    int kk = static_cast<int>(t) - y;

                    if (kk >= 0 && kk < static_cast<int>(K_LEN))
                    {
                        uint32_t idx =
                            dbg_stream_ctx * (Y_DIM * K_LEN) + y * K_LEN + static_cast<uint32_t>(kk);

                        if (idx < dbg_A_flat.size())
                        {
                            act_out[y] = static_cast<T_ACT>(dbg_A_flat[idx]);
                        }
                    }
                    else
                    {
                        act_out[y] = static_cast<T_ACT>(0);
                    }
                }

                for (int x = 0; x < X_DIM; x++)
                {
                    int kk = static_cast<int>(t) - x;

                    if (kk >= 0 && kk < static_cast<int>(K_LEN))
                    {
                        uint32_t idx =
                            dbg_stream_ctx * (K_LEN * X_DIM) + static_cast<uint32_t>(kk) * X_DIM + x;

                        if (idx < dbg_B_flat.size())
                        {
                            wei_out[x] = static_cast<T_WEI>(dbg_B_flat[idx]);
                        }
                    }
                    else
                    {
                        wei_out[x] = static_cast<T_WEI>(0);
                    }
                }

                if (dbg_stream_k < 24)
                {
                    DBG_COUT << "[DBG REF STREAM SKEWED]"
                             << " ctx=" << dbg_stream_ctx
                             << " t=" << dbg_stream_k
                             << " act=" << act_out
                             << " wei=" << wei_out
                             << std::endl;
                }

                dbg_stream_k++;

                if (dbg_stream_k >= INJECT_TOTAL)
                {
                    dbg_stream_active = false;
                    DBG_COUT << "[DBG REF STREAM SKEWED DONE]"
                             << " ctx=" << dbg_stream_ctx
                             << " injected_cycles=" << dbg_stream_k
                             << std::endl;
                }
            }

            s_act_arr_to_array.write(act_out);
            s_wei_arr_to_array.write(wei_out);
        }
    };

} // namespace sauria

#endif // SAURIA_UNIFIED_NPU_TOP_H
