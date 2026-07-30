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
#include "control/instruction_decoder.h"
#include "control/sauria_dma.h"
#include "data_feeder/ifmap_feeder.h"
#include "data_feeder/wei_feeder.h"
#include "systolic_array/sa_array.h"
#include "psm/psm_top.h"
#include "psm/obp_top.h"
#include "psm/re_rce.h"
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
        // re-routes to the array and OBP instances in case it is set after construction.
        fx1::PerfCounters *perf{nullptr};
        void attach_perf(fx1::PerfCounters *p)
        {
            perf = p;
            if (array_inst)
                array_inst->perf = p;
            if (obp_inst_a)
                obp_inst_a->perf = p;
            if (obp_inst_b)
                obp_inst_b->perf = p;
        }
#endif
        void set_dram(std::vector<uint8_t> *dram)
        {
            m_dram = dram;
            if (dma_inst) dma_inst->set_dram(dram);
            if (decoder_inst) decoder_inst->set_dram(dram);
        }

        // Constructor
        SC_HAS_PROCESS(NpuTop);
        NpuTop(sc_module_name nm, const PeConfig &pe_cfg = PeConfig()) : sc_module(nm)
        {
            // Instantiate submodules
            sram_inst = new Sram<X_DIM, Y_DIM, T_ACT, T_WEI, T_PSUM, SRAMA_CAP, SRAMB_CAP, SRAMC_CAP>("sram_inst");
            ctrl_inst_a = new Control<X_DIM, Y_DIM, PE_LAT, EXTRA_CSREG>("ctrl_inst_a");
            ctrl_inst_b = new Control<X_DIM, Y_DIM, PE_LAT, EXTRA_CSREG>("ctrl_inst_b");
            act_feeder_a = new IfmapFeeder<Y_DIM, T_ACT, SRAMA_CAP, FIFO_DEPTH, false>("act_feeder_a");
            act_feeder_b = new IfmapFeeder<Y_DIM, T_ACT, SRAMA_CAP, FIFO_DEPTH, true>("act_feeder_b");
            wei_feeder_a = new WeightFeeder<X_DIM, T_WEI, SRAMB_CAP, FIFO_DEPTH, 0, false>("wei_feeder_a");
            wei_feeder_b = new WeightFeeder<X_DIM, T_WEI, SRAMB_CAP, FIFO_DEPTH, 0, true>("wei_feeder_b");
            array_inst = new SystolicArray<X_DIM, Y_DIM, T_ACT, T_WEI, T_PSUM>("array_inst", pe_cfg);
            psm_inst_a = new Psm<X_DIM, Y_DIM, T_PSUM, SRAMC_CAP>("psm_inst_a");
            psm_inst_b = new Psm<X_DIM, Y_DIM, T_PSUM, SRAMC_CAP>("psm_inst_b");
            obp_inst_a = new Obp<Y_DIM, 0x00140000, 0x00150000, T_PSUM, T_ACT>("obp_inst_a");
            obp_inst_b = new Obp<Y_DIM, 0x00160000, 0x00170000, T_PSUM, T_ACT>("obp_inst_b");
            rce_inst_a = new ReconfigurableEngine<0x00200000, 0x00210000, 0x00220000>("rce_inst_a");
            rce_inst_b = new ReconfigurableEngine<0x00230000, 0x00240000, 0x00250000>("rce_inst_b");
            re_inst_a = new ReductionEngine<Y_DIM, T_PSUM, T_ACT>("re_inst_a");
            re_inst_b = new ReductionEngine<Y_DIM, T_PSUM, T_ACT>("re_inst_b");
            config_regs_inst = new ConfigRegs<32, 32, X_DIM, Y_DIM, 2, 15, 15, 15, DILP_W, 8>("config_regs_inst");
            dma_inst = new SauriaDma<X_DIM, Y_DIM, T_ACT, T_WEI, T_PSUM, SRAMA_CAP, SRAMB_CAP, SRAMC_CAP>("dma_inst");
            decoder_inst = new InstructionDecoder<X_DIM, Y_DIM, T_ACT, T_WEI, T_PSUM, SRAMA_CAP, SRAMB_CAP, SRAMC_CAP>("decoder_inst");
            dma_inst->set_sram(sram_inst);
            decoder_inst->set_dma(dma_inst);
            decoder_inst->set_sram(sram_inst);

#ifndef FX1_NO_PERF
            // Forward the externally-attached perf struct down to array and OBP modules.
            // attach_perf() must be called before sc_start() if used.
            array_inst->perf = perf;
            if (obp_inst_a) obp_inst_a->perf = perf;
            if (obp_inst_b) obp_inst_b->perf = perf;
#endif

            SC_METHOD(host_rdata_mux);
            sensitive << i_host_addr << s_host_rdata_sram << s_host_rdata_cfg << s_host_rdata_obp_a << s_host_rdata_obp_b << s_host_rdata_rce_a << s_host_rdata_rce_b;

            SC_METHOD(start_reset_logic);
            sensitive << i_start << s_cfg_start << i_soft_reset << s_cfg_soft_reset << s_ctrl_done_a << s_ctrl_done_b << s_trigger_start_a << s_trigger_start_b;

            SC_METHOD(nsplit_mux_logic);
            sensitive << s_use_instr_mode << s_instr_nsplit << s_config_nsplit;

            SC_METHOD(base_addr_mux_logic);
            sensitive << s_use_instr_mode << s_instr_wei_base_addr_a << s_instr_wei_base_addr_b << s_instr_ifmap_base_addr_a << s_instr_ifmap_base_addr_b << s_wei_base_addr << s_act_base_addr;

            SC_METHOD(done_latch_logic);
            sensitive << i_clk.pos();
            dont_initialize();

            SC_METHOD(obp_cfg_unpack);
            sensitive << s_obp_cfg_a << s_obp_cfg_b;

            SC_METHOD(deadlock_merge_logic);
            sensitive << s_deadlock_a << s_deadlock_b;

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

            ctrl_inst_a->i_clk(i_clk);
            ctrl_inst_a->i_rstn(i_rstn);
            ctrl_inst_a->i_soft_reset(s_ctrl_reset_internal);

            ctrl_inst_b->i_clk(i_clk);
            ctrl_inst_b->i_rstn(i_rstn);
            ctrl_inst_b->i_soft_reset(s_ctrl_reset_internal);

            act_feeder_a->i_clk(i_clk);
            act_feeder_a->i_rstn(i_rstn);

            act_feeder_b->i_clk(i_clk);
            act_feeder_b->i_rstn(i_rstn);

            wei_feeder_a->i_clk(i_clk);
            wei_feeder_a->i_rstn(i_rstn);

            wei_feeder_b->i_clk(i_clk);
            wei_feeder_b->i_rstn(i_rstn);

            array_inst->i_clk(i_clk);
            array_inst->i_rstn(i_rstn);

            psm_inst_a->i_clk(i_clk);
            psm_inst_a->i_rstn(i_rstn);

            psm_inst_b->i_clk(i_clk);
            psm_inst_b->i_rstn(i_rstn);

            config_regs_inst->i_clk(i_clk);
            config_regs_inst->i_rstn(i_rstn);
            config_regs_inst->i_host_addr(i_host_addr);
            config_regs_inst->i_host_wren(i_host_wren);
            config_regs_inst->i_host_rden(i_host_rden);
            config_regs_inst->i_host_wdata(i_host_wdata);
            config_regs_inst->i_host_wmask(i_host_wmask);
            config_regs_inst->o_host_rdata(s_host_rdata_cfg);
            config_regs_inst->i_done(s_ctrl_done_a); // For status reporting compatibility, use Done A
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
            config_regs_inst->o_incntlim(s_incntlim);
            config_regs_inst->o_act_reps(s_act_reps);
            config_regs_inst->o_wei_reps(s_wei_reps);
            config_regs_inst->o_dil_pat(s_dil_pat);
            config_regs_inst->o_rows_active(s_rows_active);
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
            config_regs_inst->o_nsplit(s_config_nsplit);
            config_regs_inst->o_obp_cfg_a(s_obp_cfg_a);
            config_regs_inst->o_requant_scale_a(s_requant_scale_a);
            config_regs_inst->o_requant_shift_a(s_requant_shift_a);
            config_regs_inst->o_obp_cfg_b(s_obp_cfg_b);
            config_regs_inst->o_requant_scale_b(s_requant_scale_b);
            config_regs_inst->o_requant_shift_b(s_requant_shift_b);

            // 2. Host Interface Routing to SRAM
            sram_inst->i_host_addr(i_host_addr);
            sram_inst->i_host_wren(i_host_wren);
            sram_inst->i_host_rden(i_host_rden);
            sram_inst->i_host_wdata(i_host_wdata);
            sram_inst->i_host_wmask(i_host_wmask);
            sram_inst->o_host_rdata(s_host_rdata_sram);

            // 3. FSM Controller A bindings
            ctrl_inst_a->i_start(s_start_internal_a);
            ctrl_inst_a->o_done(s_ctrl_done_a);
            ctrl_inst_a->o_feed_deadlock(s_deadlock_a);
            ctrl_inst_a->o_active(s_ctrl_active_a);
            ctrl_inst_a->i_outbuf_done(s_psm_done_a);
            ctrl_inst_a->i_finalwrite(s_psm_finalwrite_a);
            ctrl_inst_a->i_shift_done(s_psm_shift_done_a);
            ctrl_inst_a->i_act_done(s_act_done_a);
            ctrl_inst_a->i_act_til_done(s_act_til_done_a);
            ctrl_inst_a->i_act_fifo_empty(s_act_fifo_empty_a);
            ctrl_inst_a->i_act_fifo_full(s_act_fifo_full_a);
            ctrl_inst_a->i_act_stall(s_act_stall_a);
            ctrl_inst_a->i_wei_done(s_wei_done_a);
            ctrl_inst_a->i_wei_til_done(s_wei_til_done_a);
            ctrl_inst_a->i_wei_fifo_empty(s_wei_fifo_empty_a);
            ctrl_inst_a->i_wei_fifo_full(s_wei_fifo_full_a);
            ctrl_inst_a->i_wei_stall(s_wei_stall_a);
            ctrl_inst_a->i_mvm_k(i_mvm_k);
            ctrl_inst_a->i_total_contexts(i_total_contexts);
            ctrl_inst_a->o_act_feeder_en(s_act_feeder_en_a);
            ctrl_inst_a->o_act_feeder_clear(s_act_feeder_clear_a);
            ctrl_inst_a->o_act_start(s_act_start_a);
            ctrl_inst_a->o_act_valid(s_act_valid_a);
            ctrl_inst_a->o_act_finalpush(s_act_finalpush_a);
            ctrl_inst_a->o_act_cnt_en(s_act_cnt_en_a);
            ctrl_inst_a->o_act_cnt_clear(s_act_cnt_clear_a);
            ctrl_inst_a->o_act_clearfifo(s_act_clearfifo_a);
            ctrl_inst_a->o_act_pop_en(s_act_pop_en_a);
            ctrl_inst_a->o_act_finalctx(s_act_finalctx_a);
            ctrl_inst_a->o_wei_feeder_en(s_wei_feeder_en_a);
            ctrl_inst_a->o_wei_feeder_clear(s_wei_feeder_clear_a);
            ctrl_inst_a->o_wei_start(s_wei_start_a);
            ctrl_inst_a->o_wei_valid(s_wei_valid_a);
            ctrl_inst_a->o_wei_finalpush(s_wei_finalpush_a);
            ctrl_inst_a->o_wei_cnt_en(s_wei_cnt_en_a);
            ctrl_inst_a->o_wei_cnt_clear(s_wei_cnt_clear_a);
            ctrl_inst_a->o_wei_clearfifo(s_wei_clearfifo_a);
            ctrl_inst_a->o_wei_pop_en(s_wei_pop_en_a);
            ctrl_inst_a->o_wei_cswitch(s_wei_cswitch_a);
            ctrl_inst_a->o_outbuf_start(s_psm_start_a);
            ctrl_inst_a->o_outbuf_reset(s_psm_reset_a);
            ctrl_inst_a->o_context_id(s_context_id_a);
            ctrl_inst_a->o_local_context_id(s_local_context_id_a);
            ctrl_inst_a->o_out_tile_id(s_out_tile_id_a);
            ctrl_inst_a->o_global_context_id(s_global_context_id_a);
            ctrl_inst_a->o_sa_clear(s_sa_clear_a);
            ctrl_inst_a->o_pipeline_en(s_pipeline_en_a);
            ctrl_inst_a->o_cswitch_arr(s_cswitch_arr_a);
            ctrl_inst_a->i_incntlim(s_incntlim);
            ctrl_inst_a->i_act_reps(s_act_reps);
            ctrl_inst_a->i_wei_reps(s_wei_reps);
            ctrl_inst_a->i_ncontexts(s_out_ncontexts);

            // 3. FSM Controller B bindings
            ctrl_inst_b->i_start(s_start_internal_b);
            ctrl_inst_b->o_done(s_ctrl_done_b);
            ctrl_inst_b->o_feed_deadlock(s_deadlock_b);
            ctrl_inst_b->o_active(s_ctrl_active_b);
            ctrl_inst_b->i_outbuf_done(s_psm_done_b);
            ctrl_inst_b->i_finalwrite(s_psm_finalwrite_b);
            ctrl_inst_b->i_shift_done(s_psm_shift_done_b);
            ctrl_inst_b->i_act_done(s_act_done_b);
            ctrl_inst_b->i_act_til_done(s_act_til_done_b);
            ctrl_inst_b->i_act_fifo_empty(s_act_fifo_empty_b);
            ctrl_inst_b->i_act_fifo_full(s_act_fifo_full_b);
            ctrl_inst_b->i_act_stall(s_act_stall_b);
            ctrl_inst_b->i_wei_done(s_wei_done_b);
            ctrl_inst_b->i_wei_til_done(s_wei_til_done_b);
            ctrl_inst_b->i_wei_fifo_empty(s_wei_fifo_empty_b);
            ctrl_inst_b->i_wei_fifo_full(s_wei_fifo_full_b);
            ctrl_inst_b->i_wei_stall(s_wei_stall_b);
            ctrl_inst_b->i_mvm_k(i_mvm_k);
            ctrl_inst_b->i_total_contexts(i_total_contexts);
            ctrl_inst_b->o_act_feeder_en(s_act_feeder_en_b);
            ctrl_inst_b->o_act_feeder_clear(s_act_feeder_clear_b);
            ctrl_inst_b->o_act_start(s_act_start_b);
            ctrl_inst_b->o_act_valid(s_act_valid_b);
            ctrl_inst_b->o_act_finalpush(s_act_finalpush_b);
            ctrl_inst_b->o_act_cnt_en(s_act_cnt_en_b);
            ctrl_inst_b->o_act_cnt_clear(s_act_cnt_clear_b);
            ctrl_inst_b->o_act_clearfifo(s_act_clearfifo_b);
            ctrl_inst_b->o_act_pop_en(s_act_pop_en_b);
            ctrl_inst_b->o_act_finalctx(s_act_finalctx_b);
            ctrl_inst_b->o_wei_feeder_en(s_wei_feeder_en_b);
            ctrl_inst_b->o_wei_feeder_clear(s_wei_feeder_clear_b);
            ctrl_inst_b->o_wei_start(s_wei_start_b);
            ctrl_inst_b->o_wei_valid(s_wei_valid_b);
            ctrl_inst_b->o_wei_finalpush(s_wei_finalpush_b);
            ctrl_inst_b->o_wei_cnt_en(s_wei_cnt_en_b);
            ctrl_inst_b->o_wei_cnt_clear(s_wei_cnt_clear_b);
            ctrl_inst_b->o_wei_clearfifo(s_wei_clearfifo_b);
            ctrl_inst_b->o_wei_pop_en(s_wei_pop_en_b);
            ctrl_inst_b->o_wei_cswitch(s_wei_cswitch_b);
            ctrl_inst_b->o_outbuf_start(s_psm_start_b);
            ctrl_inst_b->o_outbuf_reset(s_psm_reset_b);
            ctrl_inst_b->o_context_id(s_context_id_b);
            ctrl_inst_b->o_local_context_id(s_local_context_id_b);
            ctrl_inst_b->o_out_tile_id(s_out_tile_id_b);
            ctrl_inst_b->o_global_context_id(s_global_context_id_b);
            ctrl_inst_b->o_sa_clear(s_sa_clear_b);
            ctrl_inst_b->o_pipeline_en(s_pipeline_en_b);
            ctrl_inst_b->o_cswitch_arr(s_cswitch_arr_b);
            ctrl_inst_b->i_incntlim(s_incntlim);
            ctrl_inst_b->i_act_reps(s_act_reps);
            ctrl_inst_b->i_wei_reps(s_wei_reps);
            ctrl_inst_b->i_ncontexts(s_out_ncontexts);

            // 4. Feeder A bindings
            act_feeder_a->i_feeder_en(s_act_feeder_en_a);
            act_feeder_a->i_feeder_clear(s_act_feeder_clear_a);
            act_feeder_a->i_start(s_act_start_a);
            act_feeder_a->i_valid(s_act_valid_a);
            act_feeder_a->i_finalpush(s_act_finalpush_a);
            act_feeder_a->i_cnt_en(s_act_cnt_en_a);
            act_feeder_a->i_cnt_clear(s_act_cnt_clear_a);
            act_feeder_a->i_clearfifo(s_act_clearfifo_a);
            act_feeder_a->i_pop_en(s_act_pop_en_a);
            act_feeder_a->i_finalctx(s_act_finalctx_a);
            act_feeder_a->i_context_id(s_local_context_id_a);
            act_feeder_a->i_ncontexts(s_out_ncontexts);
            act_feeder_a->i_act_reps(s_act_reps);
            act_feeder_a->i_mvm_k(i_mvm_k);
            act_feeder_a->o_act_done(s_act_done_a);
            act_feeder_a->o_act_til_done(s_act_til_done_a);
            act_feeder_a->o_fifo_empty(s_act_fifo_empty_a);
            act_feeder_a->o_fifo_full(s_act_fifo_full_a);
            act_feeder_a->o_stall(s_act_stall_a);
            act_feeder_a->o_srama_addr(s_srama_addr_a);
            act_feeder_a->o_srama_rden(s_srama_rden_a);
            act_feeder_a->i_srama_data(s_srama_data_a);
            act_feeder_a->o_act_arr(s_act_arr_a);
            act_feeder_a->i_act_base_addr(s_act_base_addr_a);
            act_feeder_a->i_act_incntlim(s_act_incntlim);
            act_feeder_a->i_act_incntstep(s_act_incntstep);
            act_feeder_a->i_act_outcntlim(s_act_outcntlim);
            act_feeder_a->i_act_outcntstep(s_act_outcntstep);
            act_feeder_a->i_act_dil_pat(s_dil_pat);
            act_feeder_a->i_act_xlim(s_act_xlim);
            act_feeder_a->i_act_xstep(s_act_xstep);
            act_feeder_a->i_act_ylim(s_act_ylim);
            act_feeder_a->i_act_ystep(s_act_ystep);
            act_feeder_a->i_act_chlim(s_act_chlim);
            act_feeder_a->i_act_chstep(s_act_chstep);
            act_feeder_a->i_act_til_xlim(s_act_til_xlim);
            act_feeder_a->i_act_til_xstep(s_act_til_xstep);
            act_feeder_a->i_act_til_ylim(s_act_til_ylim);
            act_feeder_a->i_act_til_ystep(s_act_til_ystep);
            act_feeder_a->i_nsplit(s_nsplit);

            // 4. Feeder B bindings
            act_feeder_b->i_feeder_en(s_act_feeder_en_b);
            act_feeder_b->i_feeder_clear(s_act_feeder_clear_b);
            act_feeder_b->i_start(s_act_start_b);
            act_feeder_b->i_valid(s_act_valid_b);
            act_feeder_b->i_finalpush(s_act_finalpush_b);
            act_feeder_b->i_cnt_en(s_act_cnt_en_b);
            act_feeder_b->i_cnt_clear(s_act_cnt_clear_b);
            act_feeder_b->i_clearfifo(s_act_clearfifo_b);
            act_feeder_b->i_pop_en(s_act_pop_en_b);
            act_feeder_b->i_finalctx(s_act_finalctx_b);
            act_feeder_b->i_context_id(s_local_context_id_b);
            act_feeder_b->i_ncontexts(s_out_ncontexts);
            act_feeder_b->i_act_reps(s_act_reps);
            act_feeder_b->i_mvm_k(i_mvm_k);
            act_feeder_b->o_act_done(s_act_done_b);
            act_feeder_b->o_act_til_done(s_act_til_done_b);
            act_feeder_b->o_fifo_empty(s_act_fifo_empty_b);
            act_feeder_b->o_fifo_full(s_act_fifo_full_b);
            act_feeder_b->o_stall(s_act_stall_b);
            act_feeder_b->o_srama_addr(s_srama_addr_b);
            act_feeder_b->o_srama_rden(s_srama_rden_b);
            act_feeder_b->i_srama_data(s_srama_data_b);
            act_feeder_b->o_act_arr(s_act_arr_b);
            act_feeder_b->i_act_base_addr(s_act_base_addr_b);
            act_feeder_b->i_act_incntlim(s_act_incntlim);
            act_feeder_b->i_act_incntstep(s_act_incntstep);
            act_feeder_b->i_act_outcntlim(s_act_outcntlim);
            act_feeder_b->i_act_outcntstep(s_act_outcntstep);
            act_feeder_b->i_act_dil_pat(s_dil_pat);
            act_feeder_b->i_act_xlim(s_act_xlim);
            act_feeder_b->i_act_xstep(s_act_xstep);
            act_feeder_b->i_act_ylim(s_act_ylim);
            act_feeder_b->i_act_ystep(s_act_ystep);
            act_feeder_b->i_act_chlim(s_act_chlim);
            act_feeder_b->i_act_chstep(s_act_chstep);
            act_feeder_b->i_act_til_xlim(s_act_til_xlim);
            act_feeder_b->i_act_til_xstep(s_act_til_xstep);
            act_feeder_b->i_act_til_ylim(s_act_til_ylim);
            act_feeder_b->i_act_til_ystep(s_act_til_ystep);
            act_feeder_b->i_nsplit(s_nsplit);

            // Weight Feeder A bindings
            wei_feeder_a->i_wei_incntlim(s_wei_incntlim);
            wei_feeder_a->i_wei_incntstep(s_wei_incntstep);
            wei_feeder_a->i_wei_wlim(s_wei_wlim);
            wei_feeder_a->i_wei_wstep(s_wei_wstep);
            wei_feeder_a->i_wei_klim(s_wei_klim);
            wei_feeder_a->i_wei_kstep(s_wei_kstep);
            wei_feeder_a->i_wei_til_klim(s_wei_til_klim);
            wei_feeder_a->i_wei_til_kstep(s_wei_til_kstep);
            wei_feeder_a->i_wei_cols_active(s_wei_cols_active);
            wei_feeder_a->i_wei_waligned(s_wei_waligned);
            wei_feeder_a->i_feeder_en(s_wei_feeder_en_a);
            wei_feeder_a->i_feeder_clear(s_wei_feeder_clear_a);
            wei_feeder_a->i_start(s_wei_start_a);
            wei_feeder_a->i_valid(s_wei_valid_a);
            wei_feeder_a->i_finalpush(s_wei_finalpush_a);
            wei_feeder_a->i_cnt_en(s_wei_cnt_en_a);
            wei_feeder_a->i_cnt_clear(s_wei_cnt_clear_a);
            wei_feeder_a->i_clearfifo(s_wei_clearfifo_a);
            wei_feeder_a->i_pop_en(s_wei_pop_en_a);
            wei_feeder_a->i_cswitch(s_wei_cswitch_a);
            wei_feeder_a->o_wei_done(s_wei_done_a);
            wei_feeder_a->o_wei_til_done(s_wei_til_done_a);
            wei_feeder_a->o_fifo_empty(s_wei_fifo_empty_a);
            wei_feeder_a->o_fifo_full(s_wei_fifo_full_a);
            wei_feeder_a->o_stall(s_wei_stall_a);
            wei_feeder_a->o_sramb_addr(s_sramb_addr_a);
            wei_feeder_a->o_sramb_rden(s_sramb_rden_a);
            wei_feeder_a->i_sramb_data(s_sramb_data_a);
            wei_feeder_a->o_wei_arr(s_wei_arr_a);
            wei_feeder_a->i_context_id(s_local_context_id_a);
            wei_feeder_a->i_out_tile_id(s_out_tile_id_a);
            wei_feeder_a->i_ncontexts(s_out_ncontexts);
            wei_feeder_a->i_mvm_k(i_mvm_k);
            wei_feeder_a->i_wei_base_addr(s_wei_base_addr_a);
            wei_feeder_a->i_nsplit(s_nsplit);

            // Weight Feeder B bindings
            wei_feeder_b->i_wei_incntlim(s_wei_incntlim);
            wei_feeder_b->i_wei_incntstep(s_wei_incntstep);
            wei_feeder_b->i_wei_wlim(s_wei_wlim);
            wei_feeder_b->i_wei_wstep(s_wei_wstep);
            wei_feeder_b->i_wei_klim(s_wei_klim);
            wei_feeder_b->i_wei_kstep(s_wei_kstep);
            wei_feeder_b->i_wei_til_klim(s_wei_til_klim);
            wei_feeder_b->i_wei_til_kstep(s_wei_til_kstep);
            wei_feeder_b->i_wei_cols_active(s_wei_cols_active);
            wei_feeder_b->i_wei_waligned(s_wei_waligned);
            wei_feeder_b->i_feeder_en(s_wei_feeder_en_b);
            wei_feeder_b->i_feeder_clear(s_wei_feeder_clear_b);
            wei_feeder_b->i_start(s_wei_start_b);
            wei_feeder_b->i_valid(s_wei_valid_b);
            wei_feeder_b->i_finalpush(s_wei_finalpush_b);
            wei_feeder_b->i_cnt_en(s_wei_cnt_en_b);
            wei_feeder_b->i_cnt_clear(s_wei_cnt_clear_b);
            wei_feeder_b->i_clearfifo(s_wei_clearfifo_b);
            wei_feeder_b->i_pop_en(s_wei_pop_en_b);
            wei_feeder_b->i_cswitch(s_wei_cswitch_b);
            wei_feeder_b->o_wei_done(s_wei_done_b);
            wei_feeder_b->o_wei_til_done(s_wei_til_done_b);
            wei_feeder_b->o_fifo_empty(s_wei_fifo_empty_b);
            wei_feeder_b->o_fifo_full(s_wei_fifo_full_b);
            wei_feeder_b->o_stall(s_wei_stall_b);
            wei_feeder_b->o_sramb_addr(s_sramb_addr_b);
            wei_feeder_b->o_sramb_rden(s_sramb_rden_b);
            wei_feeder_b->i_sramb_data(s_sramb_data_b);
            wei_feeder_b->o_wei_arr(s_wei_arr_b);
            wei_feeder_b->i_context_id(s_local_context_id_b);
            wei_feeder_b->i_out_tile_id(s_out_tile_id_b);
            wei_feeder_b->i_ncontexts(s_out_ncontexts);
            wei_feeder_b->i_mvm_k(i_mvm_k);
            wei_feeder_b->i_wei_base_addr(s_wei_base_addr_b);
            wei_feeder_b->i_nsplit(s_nsplit);

            // 4.5 Instruction Decoder bindings
            decoder_inst->i_clk(i_clk);
            decoder_inst->i_rstn(i_rstn);
            decoder_inst->i_host_addr(i_host_addr);
            decoder_inst->i_host_wdata(i_host_wdata);
            decoder_inst->i_host_wren(i_host_wren);
            decoder_inst->i_ctrl_active_a(s_ctrl_active_a);
            decoder_inst->i_ctrl_active_b(s_ctrl_active_b);
            decoder_inst->o_use_instr_mode(s_use_instr_mode);
            decoder_inst->o_nsplit(s_instr_nsplit);
            decoder_inst->o_trigger_start_a(s_trigger_start_a);
            decoder_inst->o_trigger_start_b(s_trigger_start_b);
            decoder_inst->o_wei_addr_a(s_instr_wei_base_addr_a);
            decoder_inst->o_ifmap_addr_a(s_instr_ifmap_base_addr_a);
            decoder_inst->o_wei_addr_b(s_instr_wei_base_addr_b);
            decoder_inst->o_ifmap_addr_b(s_instr_ifmap_base_addr_b);

            // DMA binding
            dma_inst->i_clk(i_clk);
            dma_inst->i_rstn(i_rstn);

            // 5. SRAM Core Interface bindings (Accelerator-side)
            sram_inst->i_srama_addr_a(s_srama_addr_a);
            sram_inst->i_srama_rden_a(s_srama_rden_a);
            sram_inst->o_srama_data_a(s_srama_data_a);

            sram_inst->i_srama_addr_b(s_srama_addr_b);
            sram_inst->i_srama_rden_b(s_srama_rden_b);
            sram_inst->o_srama_data_b(s_srama_data_b);

            sram_inst->i_sramb_addr_a(s_sramb_addr_a);
            sram_inst->i_sramb_rden_a(s_sramb_rden_a);
            sram_inst->o_sramb_data_a(s_sramb_data_a);

            sram_inst->i_sramb_addr_b(s_sramb_addr_b);
            sram_inst->i_sramb_rden_b(s_sramb_rden_b);
            sram_inst->o_sramb_data_b(s_sramb_data_b);

            // Connect SRAM C to OBP outputs
            sram_inst->i_sramc_wdata_a(s_obp_sramc_wdata_a);
            sram_inst->i_sramc_addr_a(s_obp_sramc_addr_a);
            sram_inst->i_sramc_wren_a(s_obp_sramc_wren_a);
            sram_inst->i_sramc_rden_a(s_sramc_rden_a);
            sram_inst->i_sramc_wmask_a(s_obp_sramc_wmask_a);
            sram_inst->o_sramc_rdata_a(s_sramc_rdata_a);

            sram_inst->i_sramc_wdata_b(s_obp_sramc_wdata_b);
            sram_inst->i_sramc_addr_b(s_obp_sramc_addr_b);
            sram_inst->i_sramc_wren_b(s_obp_sramc_wren_b);
            sram_inst->i_sramc_rden_b(s_sramc_rden_b);
            sram_inst->i_sramc_wmask_b(s_obp_sramc_wmask_b);
            sram_inst->o_sramc_rdata_b(s_sramc_rdata_b);

            // 6. Systolic Array bindings
            array_inst->i_threshold(i_threshold);
            array_inst->i_nsplit(s_nsplit);
            array_inst->i_act_arr_a(s_act_arr_to_array_a);
            array_inst->i_act_arr_b(s_act_arr_to_array_b);
            array_inst->i_wei_arr_a(s_wei_arr_to_array_a);
            array_inst->i_wei_arr_b(s_wei_arr_to_array_b);
            array_inst->i_c_arr_a(s_psm_to_sa_c_a);
            array_inst->o_c_arr_a(s_sa_to_psm_c_a);
            array_inst->i_c_arr_b(s_psm_to_sa_c_b);
            array_inst->o_c_arr_b(s_sa_to_psm_c_b);
            array_inst->i_pipeline_en_a(s_pipeline_en_a);
            array_inst->i_pipeline_en_b(s_pipeline_en_b);
            array_inst->i_cscan_en_a(s_cscan_en_a);
            array_inst->i_cscan_en_b(s_cscan_en_b);
            array_inst->i_cswitch_arr_a(s_cswitch_arr_a);
            array_inst->i_cswitch_arr_b(s_cswitch_arr_b);
            array_inst->i_sa_clear_a(s_sa_clear_a);
            array_inst->i_sa_clear_b(s_sa_clear_b);
            array_inst->i_context_id_a(s_context_id_a);
            array_inst->i_context_id_b(s_context_id_b);

            // 7. PSM A bindings
            psm_inst_a->i_c_arr(s_sa_to_psm_c_a);
            psm_inst_a->o_c_arr(s_psm_to_sa_c_a);
            psm_inst_a->i_sramc_rdata(s_sramc_rdata_a);
            psm_inst_a->o_sramc_addr(s_sramc_addr_a);
            psm_inst_a->o_sramc_wren(s_sramc_wren_a);
            psm_inst_a->o_sramc_rden(s_sramc_rden_a);
            psm_inst_a->o_sramc_wmask(s_sramc_wmask_a);
            psm_inst_a->o_sramc_wdata(s_sramc_wdata_a);
            psm_inst_a->i_fsm_start(s_psm_start_a);
            psm_inst_a->i_fsm_reset(s_psm_reset_a);
            psm_inst_a->i_pipeline_en(s_pipeline_en_a);
            psm_inst_a->o_done(s_psm_done_a);
            psm_inst_a->o_finalwrite(s_psm_finalwrite_a);
            psm_inst_a->o_shift_done(s_psm_shift_done_a);
            psm_inst_a->o_cscan_en(s_cscan_en_a);
            psm_inst_a->i_out_base_addr(s_out_base_addr);
            psm_inst_a->i_cxlim(s_cxlim);
            psm_inst_a->i_cxstep(s_cxstep);
            psm_inst_a->i_cklim(s_cklim);
            psm_inst_a->i_ckstep(s_ckstep);
            psm_inst_a->i_til_cylim(s_out_til_cylim);
            psm_inst_a->i_til_cystep(s_out_til_cystep);
            psm_inst_a->i_til_cklim(s_out_til_cklim);
            psm_inst_a->i_til_ckstep(s_out_til_ckstep);
            psm_inst_a->i_ncontexts(s_out_ncontexts);
            psm_inst_a->i_preload_en(s_out_preload_en);
            psm_inst_a->i_rows_active(s_rows_active);
            psm_inst_a->i_context_id(s_global_context_id_a);
            psm_inst_a->i_total_contexts(i_total_contexts);

            // 7. PSM B bindings
            psm_inst_b->i_c_arr(s_sa_to_psm_c_b);
            psm_inst_b->o_c_arr(s_psm_to_sa_c_b);
            psm_inst_b->i_sramc_rdata(s_sramc_rdata_b);
            psm_inst_b->o_sramc_addr(s_sramc_addr_b);
            psm_inst_b->o_sramc_wren(s_sramc_wren_b);
            psm_inst_b->o_sramc_rden(s_sramc_rden_b);
            psm_inst_b->o_sramc_wmask(s_sramc_wmask_b);
            psm_inst_b->o_sramc_wdata(s_sramc_wdata_b);
            psm_inst_b->i_fsm_start(s_psm_start_b);
            psm_inst_b->i_fsm_reset(s_psm_reset_b);
            psm_inst_b->i_pipeline_en(s_pipeline_en_b);
            psm_inst_b->o_done(s_psm_done_b);
            psm_inst_b->o_finalwrite(s_psm_finalwrite_b);
            psm_inst_b->o_shift_done(s_psm_shift_done_b);
            psm_inst_b->o_cscan_en(s_cscan_en_b);
            psm_inst_b->i_out_base_addr(s_out_base_addr);
            psm_inst_b->i_cxlim(s_cxlim);
            psm_inst_b->i_cxstep(s_cxstep);
            psm_inst_b->i_cklim(s_cklim);
            psm_inst_b->i_ckstep(s_ckstep);
            psm_inst_b->i_til_cylim(s_out_til_cylim);
            psm_inst_b->i_til_cystep(s_out_til_cystep);
            psm_inst_b->i_til_cklim(s_out_til_cklim);
            psm_inst_b->i_til_ckstep(s_out_til_ckstep);
            psm_inst_b->i_ncontexts(s_out_ncontexts);
            psm_inst_b->i_preload_en(s_out_preload_en);
            psm_inst_b->i_rows_active(s_rows_active);
            psm_inst_b->i_context_id(s_global_context_id_b);
            psm_inst_b->i_total_contexts(i_total_contexts);

            // 8. OBP A bindings
            obp_inst_a->i_clk(i_clk);
            obp_inst_a->i_rstn(i_rstn);
            obp_inst_a->i_data(s_sramc_wdata_a);
            obp_inst_a->i_addr(s_sramc_addr_a);
            obp_inst_a->i_wmask(s_sramc_wmask_a);
            obp_inst_a->i_valid(s_sramc_wren_a);
            obp_inst_a->i_residual(s_residual_zero_a);
            obp_inst_a->o_sramc_wdata(s_obp_sramc_wdata_a);
            obp_inst_a->o_sramc_addr(s_obp_sramc_addr_a);
            obp_inst_a->o_sramc_wren(s_obp_sramc_wren_a);
            obp_inst_a->o_sramc_wmask(s_obp_sramc_wmask_a);
            obp_inst_a->o_valid(s_obp_valid_a);
            obp_inst_a->i_bias_en(s_obp_bias_en_a);
            obp_inst_a->i_requant_en(s_obp_requant_en_a);
            obp_inst_a->i_lut_en(s_obp_lut_en_a);
            obp_inst_a->i_residual_en(s_obp_residual_en_a);
            obp_inst_a->i_requant_scale(s_requant_scale_a);
            obp_inst_a->i_requant_shift(s_requant_shift_a);
            obp_inst_a->i_host_addr(i_host_addr);
            obp_inst_a->i_host_wren(i_host_wren);
            obp_inst_a->i_host_rden(i_host_rden);
            obp_inst_a->i_host_wdata(i_host_wdata);
            obp_inst_a->i_host_wmask(i_host_wmask);
            obp_inst_a->o_host_rdata(s_host_rdata_obp_a);

            // 8. OBP B bindings
            obp_inst_b->i_clk(i_clk);
            obp_inst_b->i_rstn(i_rstn);
            obp_inst_b->i_data(s_sramc_wdata_b);
            obp_inst_b->i_addr(s_sramc_addr_b);
            obp_inst_b->i_wmask(s_sramc_wmask_b);
            obp_inst_b->i_valid(s_sramc_wren_b);
            obp_inst_b->i_residual(s_residual_zero_b);
            obp_inst_b->o_sramc_wdata(s_obp_sramc_wdata_b);
            obp_inst_b->o_sramc_addr(s_obp_sramc_addr_b);
            obp_inst_b->o_sramc_wren(s_obp_sramc_wren_b);
            obp_inst_b->o_sramc_wmask(s_obp_sramc_wmask_b);
            obp_inst_b->o_valid(s_obp_valid_b);
            obp_inst_b->i_bias_en(s_obp_bias_en_b);
            obp_inst_b->i_requant_en(s_obp_requant_en_b);
            obp_inst_b->i_lut_en(s_obp_lut_en_b);
            obp_inst_b->i_residual_en(s_obp_residual_en_b);
            obp_inst_b->i_requant_scale(s_requant_scale_b);
            obp_inst_b->i_requant_shift(s_requant_shift_b);
            obp_inst_b->i_host_addr(i_host_addr);
            obp_inst_b->i_host_wren(i_host_wren);
            obp_inst_b->i_host_rden(i_host_rden);
            obp_inst_b->i_host_wdata(i_host_wdata);
            obp_inst_b->i_host_wmask(i_host_wmask);
            obp_inst_b->o_host_rdata(s_host_rdata_obp_b);

            // RCE A bindings
            rce_inst_a->i_clk(i_clk);
            rce_inst_a->i_rstn(i_rstn);
            rce_inst_a->i_host_addr(i_host_addr);
            rce_inst_a->i_host_wren(i_host_wren);
            rce_inst_a->i_host_rden(i_host_rden);
            rce_inst_a->i_host_wdata(i_host_wdata);
            rce_inst_a->i_host_wmask(i_host_wmask);
            rce_inst_a->o_host_rdata(s_host_rdata_rce_a);
            rce_inst_a->i_lut_op(s_re_lut_op_a);
            rce_inst_a->i_lut_in(s_re_lut_in_a);
            rce_inst_a->i_lut_valid(s_re_lut_valid_a);
            rce_inst_a->o_lut_out(s_re_lut_out_a);
            rce_inst_a->o_lut_valid(s_re_lut_valid_out_a);

            // RCE B bindings
            rce_inst_b->i_clk(i_clk);
            rce_inst_b->i_rstn(i_rstn);
            rce_inst_b->i_host_addr(i_host_addr);
            rce_inst_b->i_host_wren(i_host_wren);
            rce_inst_b->i_host_rden(i_host_rden);
            rce_inst_b->i_host_wdata(i_host_wdata);
            rce_inst_b->i_host_wmask(i_host_wmask);
            rce_inst_b->o_host_rdata(s_host_rdata_rce_b);
            rce_inst_b->i_lut_op(s_re_lut_op_b);
            rce_inst_b->i_lut_in(s_re_lut_in_b);
            rce_inst_b->i_lut_valid(s_re_lut_valid_b);
            rce_inst_b->o_lut_out(s_re_lut_out_b);
            rce_inst_b->o_lut_valid(s_re_lut_valid_out_b);

            // RE A bindings
            re_inst_a->i_clk(i_clk);
            re_inst_a->i_rstn(i_rstn);
            re_inst_a->i_mode(s_re_mode_a);
            re_inst_a->i_start(s_re_start_a);
            re_inst_a->i_valid(s_re_valid_a);
            re_inst_a->i_vector_data(s_re_vector_data_a);
            re_inst_a->i_skip_data(s_re_skip_data_a);
            re_inst_a->i_requant_scale(s_requant_scale_a);
            re_inst_a->i_requant_shift(s_requant_shift_a);
            re_inst_a->o_lut_op(s_re_lut_op_a);
            re_inst_a->o_lut_in(s_re_lut_in_a);
            re_inst_a->o_lut_valid(s_re_lut_valid_a);
            re_inst_a->i_lut_out(s_re_lut_out_a);
            re_inst_a->i_lut_valid(s_re_lut_valid_out_a);
            re_inst_a->o_vector_out(s_re_vector_out_a);
            re_inst_a->o_valid(s_re_valid_out_a);
            re_inst_a->o_done(s_re_done_a);

            // RE B bindings
            re_inst_b->i_clk(i_clk);
            re_inst_b->i_rstn(i_rstn);
            re_inst_b->i_mode(s_re_mode_b);
            re_inst_b->i_start(s_re_start_b);
            re_inst_b->i_valid(s_re_valid_b);
            re_inst_b->i_vector_data(s_re_vector_data_b);
            re_inst_b->i_skip_data(s_re_skip_data_b);
            re_inst_b->i_requant_scale(s_requant_scale_b);
            re_inst_b->i_requant_shift(s_requant_shift_b);
            re_inst_b->o_lut_op(s_re_lut_op_b);
            re_inst_b->o_lut_in(s_re_lut_in_b);
            re_inst_b->o_lut_valid(s_re_lut_valid_b);
            re_inst_b->i_lut_out(s_re_lut_out_b);
            re_inst_b->i_lut_valid(s_re_lut_valid_out_b);
            re_inst_b->o_vector_out(s_re_vector_out_b);
            re_inst_b->o_valid(s_re_valid_out_b);
            re_inst_b->o_done(s_re_done_b);
        }

        ~NpuTop()
        {
            delete sram_inst;
            delete ctrl_inst_a;
            delete ctrl_inst_b;
            delete act_feeder_a;
            delete act_feeder_b;
            delete wei_feeder_a;
            delete wei_feeder_b;
            delete array_inst;
            delete psm_inst_a;
            delete psm_inst_b;
            delete obp_inst_a;
            delete obp_inst_b;
            delete rce_inst_a;
            delete rce_inst_b;
            delete re_inst_a;
            delete re_inst_b;
            delete config_regs_inst;
            delete decoder_inst;
            delete dma_inst;
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

            // Trace internal signals (Lane A)
            sc_trace(tf, s_act_feeder_en_a, std::string(name()) + ".s_act_feeder_en_a");
            sc_trace(tf, s_act_start_a, std::string(name()) + ".s_act_start_a");
            sc_trace(tf, s_act_valid_a, std::string(name()) + ".s_act_valid_a");
            sc_trace(tf, s_act_pop_en_a, std::string(name()) + ".s_act_pop_en_a");
            sc_trace(tf, s_wei_feeder_en_a, std::string(name()) + ".s_wei_feeder_en_a");
            sc_trace(tf, s_wei_start_a, std::string(name()) + ".s_wei_start_a");
            sc_trace(tf, s_wei_valid_a, std::string(name()) + ".s_wei_valid_a");
            sc_trace(tf, s_wei_pop_en_a, std::string(name()) + ".s_wei_pop_en_a");
            sc_trace(tf, s_pipeline_en_a, std::string(name()) + ".s_pipeline_en_a");
            sc_trace(tf, s_cscan_en_a, std::string(name()) + ".s_cscan_en_a");
            sc_trace(tf, s_psm_start_a, std::string(name()) + ".s_psm_start_a");
            sc_trace(tf, s_psm_done_a, std::string(name()) + ".s_psm_done_a");
            sc_trace(tf, s_psm_shift_done_a, std::string(name()) + ".s_psm_shift_done_a");
            sc_trace(tf, s_psm_finalwrite_a, std::string(name()) + ".s_psm_finalwrite_a");

            // Trace internal signals (Lane B)
            sc_trace(tf, s_act_feeder_en_b, std::string(name()) + ".s_act_feeder_en_b");
            sc_trace(tf, s_act_start_b, std::string(name()) + ".s_act_start_b");
            sc_trace(tf, s_act_valid_b, std::string(name()) + ".s_act_valid_b");
            sc_trace(tf, s_act_pop_en_b, std::string(name()) + ".s_act_pop_en_b");
            sc_trace(tf, s_wei_feeder_en_b, std::string(name()) + ".s_wei_feeder_en_b");
            sc_trace(tf, s_wei_start_b, std::string(name()) + ".s_wei_start_b");
            sc_trace(tf, s_wei_valid_b, std::string(name()) + ".s_wei_valid_b");
            sc_trace(tf, s_wei_pop_en_b, std::string(name()) + ".s_wei_pop_en_b");
            sc_trace(tf, s_pipeline_en_b, std::string(name()) + ".s_pipeline_en_b");
            sc_trace(tf, s_cscan_en_b, std::string(name()) + ".s_cscan_en_b");
            sc_trace(tf, s_psm_start_b, std::string(name()) + ".s_psm_start_b");
            sc_trace(tf, s_psm_done_b, std::string(name()) + ".s_psm_done_b");
            sc_trace(tf, s_psm_shift_done_b, std::string(name()) + ".s_psm_shift_done_b");
            sc_trace(tf, s_psm_finalwrite_b, std::string(name()) + ".s_psm_finalwrite_b");
        }

    public:
        // Submodules instances
        Sram<X_DIM, Y_DIM, T_ACT, T_WEI, T_PSUM, SRAMA_CAP, SRAMB_CAP, SRAMC_CAP> *sram_inst{nullptr};
        Control<X_DIM, Y_DIM, PE_LAT, EXTRA_CSREG> *ctrl_inst_a{nullptr};
        Control<X_DIM, Y_DIM, PE_LAT, EXTRA_CSREG> *ctrl_inst_b{nullptr};
        IfmapFeeder<Y_DIM, T_ACT, SRAMA_CAP, FIFO_DEPTH, false> *act_feeder_a{nullptr};
        IfmapFeeder<Y_DIM, T_ACT, SRAMA_CAP, FIFO_DEPTH, true> *act_feeder_b{nullptr};
        WeightFeeder<X_DIM, T_WEI, SRAMB_CAP, FIFO_DEPTH, 0, false> *wei_feeder_a{nullptr};
        WeightFeeder<X_DIM, T_WEI, SRAMB_CAP, FIFO_DEPTH, 0, true> *wei_feeder_b{nullptr};
        SystolicArray<X_DIM, Y_DIM, T_ACT, T_WEI, T_PSUM> *array_inst{nullptr};
        Psm<X_DIM, Y_DIM, T_PSUM, SRAMC_CAP> *psm_inst_a{nullptr};
        Psm<X_DIM, Y_DIM, T_PSUM, SRAMC_CAP> *psm_inst_b{nullptr};
        Obp<Y_DIM, 0x00140000, 0x00150000, T_PSUM, T_ACT> *obp_inst_a{nullptr};
        Obp<Y_DIM, 0x00160000, 0x00170000, T_PSUM, T_ACT> *obp_inst_b{nullptr};
        ConfigRegs<32, 32, X_DIM, Y_DIM, 2, 15, 15, 15, DILP_W, 8> *config_regs_inst{nullptr};
        ReconfigurableEngine<0x00200000, 0x00210000, 0x00220000> *rce_inst_a{nullptr};
        ReconfigurableEngine<0x00230000, 0x00240000, 0x00250000> *rce_inst_b{nullptr};
        ReductionEngine<Y_DIM, T_PSUM, T_ACT> *re_inst_a{nullptr};
        ReductionEngine<Y_DIM, T_PSUM, T_ACT> *re_inst_b{nullptr};
        InstructionDecoder<X_DIM, Y_DIM, T_ACT, T_WEI, T_PSUM, SRAMA_CAP, SRAMB_CAP, SRAMC_CAP> *decoder_inst{nullptr};
        SauriaDma<X_DIM, Y_DIM, T_ACT, T_WEI, T_PSUM, SRAMA_CAP, SRAMB_CAP, SRAMC_CAP> *dma_inst{nullptr};
        std::vector<uint8_t> *m_dram{nullptr};

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

        // Host mux and FSM handshake internal signals
        sc_signal<host_data_t> s_host_rdata_sram{"s_host_rdata_sram"};
        sc_signal<host_data_t> s_host_rdata_cfg{"s_host_rdata_cfg"};
        sc_signal<bool> s_ctrl_done_a{"s_ctrl_done_a"};
        sc_signal<bool> s_ctrl_done_b{"s_ctrl_done_b"};
        bool done_latched_reg_a{false};
        bool done_latched_reg_b{false};
        sc_signal<bool> s_cfg_start{"s_cfg_start"};
        sc_signal<bool> s_cfg_soft_reset{"s_cfg_soft_reset"};
        // PROFILE broadcast from unified config_regs.
        sc_signal<uint32_t> s_cfg_profile{"s_cfg_profile"};
        sc_signal<bool> s_start_internal{"s_start_internal"};
        sc_signal<bool> s_start_internal_a{"s_start_internal_a"};
        sc_signal<bool> s_start_internal_b{"s_start_internal_b"};
        sc_signal<bool> s_trigger_start_a{"s_trigger_start_a"};
        sc_signal<bool> s_trigger_start_b{"s_trigger_start_b"};
        sc_signal<bool> s_ctrl_active_a{"s_ctrl_active_a"};
        sc_signal<bool> s_ctrl_active_b{"s_ctrl_active_b"};
        sc_signal<bool> s_use_instr_mode{"s_use_instr_mode"};
        sc_signal<uint32_t> s_instr_nsplit{"s_instr_nsplit"};
        sc_signal<uint32_t> s_config_nsplit{"s_config_nsplit"};
        sc_signal<uint32_t> s_instr_wei_base_addr_a{"s_instr_wei_base_addr_a"};
        sc_signal<uint32_t> s_instr_ifmap_base_addr_a{"s_instr_ifmap_base_addr_a"};
        sc_signal<uint32_t> s_instr_wei_base_addr_b{"s_instr_wei_base_addr_b"};
        sc_signal<uint32_t> s_instr_ifmap_base_addr_b{"s_instr_ifmap_base_addr_b"};
        sc_signal<uint32_t> s_wei_base_addr_a{"s_wei_base_addr_a"};
        sc_signal<uint32_t> s_wei_base_addr_b{"s_wei_base_addr_b"};
        sc_signal<uint32_t> s_act_base_addr_a{"s_act_base_addr_a"};
        sc_signal<uint32_t> s_act_base_addr_b{"s_act_base_addr_b"};
        sc_signal<bool> s_ctrl_reset_internal{"s_ctrl_reset_internal"};

        // Static parameter simulation ports/signals
        sc_signal<bool> s_false{"s_false", false};
        sc_signal<uint32_t> s_incntlim{"s_incntlim"};
        sc_signal<uint32_t> s_act_reps{"s_act_reps"};
        sc_signal<uint32_t> s_wei_reps{"s_wei_reps"};
        sc_signal<uint32_t> s_one{"s_one", 1};
        sc_signal<sc_bv<DILP_W>> s_dil_pat{"s_dil_pat"};
        sc_signal<sramc_mask_t<Y_DIM>> s_rows_active{"s_rows_active"};

        // ---------------------------------------------------------
        // Dual Lane Internal Signals
        // ---------------------------------------------------------
        // Lane A signals
        sc_signal<bool> s_act_feeder_en_a{"s_act_feeder_en_a"};
        sc_signal<bool> s_act_feeder_clear_a{"s_act_feeder_clear_a"};
        sc_signal<bool> s_act_start_a{"s_act_start_a"};
        sc_signal<bool> s_act_valid_a{"s_act_valid_a"};
        sc_signal<bool> s_act_finalpush_a{"s_act_finalpush_a"};
        sc_signal<bool> s_act_cnt_en_a{"s_act_cnt_en_a"};
        sc_signal<bool> s_act_cnt_clear_a{"s_act_cnt_clear_a"};
        sc_signal<bool> s_act_clearfifo_a{"s_act_clearfifo_a"};
        sc_signal<bool> s_act_pop_en_a{"s_act_pop_en_a"};
        sc_signal<bool> s_act_finalctx_a{"s_act_finalctx_a"};

        sc_signal<bool> s_wei_feeder_en_a{"s_wei_feeder_en_a"};
        sc_signal<bool> s_wei_feeder_clear_a{"s_wei_feeder_clear_a"};
        sc_signal<bool> s_wei_start_a{"s_wei_start_a"};
        sc_signal<bool> s_wei_valid_a{"s_wei_valid_a"};
        sc_signal<bool> s_wei_finalpush_a{"s_wei_finalpush_a"};
        sc_signal<bool> s_wei_cnt_en_a{"s_wei_cnt_en_a"};
        sc_signal<bool> s_wei_cnt_clear_a{"s_wei_cnt_clear_a"};
        sc_signal<bool> s_wei_clearfifo_a{"s_wei_clearfifo_a"};
        sc_signal<bool> s_wei_pop_en_a{"s_wei_pop_en_a"};
        sc_signal<bool> s_wei_cswitch_a{"s_wei_cswitch_a"};

        sc_signal<bool> s_act_done_a{"s_act_done_a"};
        sc_signal<bool> s_act_til_done_a{"s_act_til_done_a"};
        sc_signal<bool> s_act_fifo_empty_a{"s_act_fifo_empty_a"};
        sc_signal<bool> s_act_fifo_full_a{"s_act_fifo_full_a"};
        sc_signal<bool> s_act_stall_a{"s_act_stall_a"};

        sc_signal<bool> s_wei_done_a{"s_wei_done_a"};
        sc_signal<bool> s_wei_til_done_a{"s_wei_til_done_a"};
        sc_signal<bool> s_wei_fifo_empty_a{"s_wei_fifo_empty_a"};
        sc_signal<bool> s_wei_fifo_full_a{"s_wei_fifo_full_a"};
        sc_signal<bool> s_wei_stall_a{"s_wei_stall_a"};

        sc_signal<bool> s_psm_start_a{"s_psm_start_a"};
        sc_signal<bool> s_psm_reset_a{"s_psm_reset_a"};
        sc_signal<bool> s_psm_done_a{"s_psm_done_a"};
        sc_signal<bool> s_psm_finalwrite_a{"s_psm_finalwrite_a"};
        sc_signal<bool> s_psm_shift_done_a{"s_psm_shift_done_a"};
        sc_signal<bool> s_cscan_en_a{"s_cscan_en_a"};

        sc_signal<bool> s_sa_clear_a{"s_sa_clear_a"};
        sc_signal<bool> s_pipeline_en_a{"s_pipeline_en_a"};
        sc_signal<sc_bv<X_DIM>> s_cswitch_arr_a{"s_cswitch_arr_a"};

        sc_signal<uint32_t> s_context_id_a{"s_context_id_a"};
        sc_signal<uint32_t> s_global_context_id_a{"s_global_context_id_a"};
        sc_signal<uint32_t> s_local_context_id_a{"s_local_context_id_a"};
        sc_signal<uint32_t> s_out_tile_id_a{"s_out_tile_id_a"};

        sc_signal<uint32_t> s_srama_addr_a{"s_srama_addr_a"};
        sc_signal<bool> s_srama_rden_a{"s_srama_rden_a"};
        sc_signal<act_vector_t<Y_DIM, T_ACT>> s_srama_data_a{"s_srama_data_a"};

        sc_signal<uint32_t> s_sramb_addr_a{"s_sramb_addr_a"};
        sc_signal<bool> s_sramb_rden_a{"s_sramb_rden_a"};
        sc_signal<wei_vector_t<X_DIM, T_WEI>> s_sramb_data_a{"s_sramb_data_a"};

        sc_signal<psum_vector_t<Y_DIM, T_PSUM>> s_sramc_wdata_a{"s_sramc_wdata_a"};
        sc_signal<uint32_t> s_sramc_addr_a{"s_sramc_addr_a"};
        sc_signal<bool> s_sramc_wren_a{"s_sramc_wren_a"};
        sc_signal<bool> s_sramc_rden_a{"s_sramc_rden_a"};
        sc_signal<sramc_mask_t<Y_DIM>> s_sramc_wmask_a{"s_sramc_wmask_a"};
        sc_signal<psum_vector_t<Y_DIM, T_PSUM>> s_sramc_rdata_a{"s_sramc_rdata_a"};

        sc_signal<psum_vector_t<Y_DIM, T_PSUM>> s_psm_to_sa_c_a{"s_psm_to_sa_c_a"};
        sc_signal<psum_vector_t<Y_DIM, T_PSUM>> s_sa_to_psm_c_a{"s_sa_to_psm_c_a"};

        sc_signal<act_vector_t<Y_DIM, T_ACT>> s_act_arr_a{"s_act_arr_a"};
        sc_signal<wei_vector_t<X_DIM, T_WEI>> s_wei_arr_a{"s_wei_arr_a"};
        sc_signal<act_vector_t<Y_DIM, T_ACT>> s_act_arr_to_array_a{"s_act_arr_to_array_a"};
        sc_signal<wei_vector_t<X_DIM, T_WEI>> s_wei_arr_to_array_a{"s_wei_arr_to_array_a"};

        // OBP A signals
        sc_signal<psum_vector_t<Y_DIM, T_PSUM>> s_obp_sramc_wdata_a{"s_obp_sramc_wdata_a"};
        sc_signal<uint32_t> s_obp_sramc_addr_a{"s_obp_sramc_addr_a"};
        sc_signal<bool> s_obp_sramc_wren_a{"s_obp_sramc_wren_a"};
        sc_signal<sramc_mask_t<Y_DIM>> s_obp_sramc_wmask_a{"s_obp_sramc_wmask_a"};
        sc_signal<bool> s_obp_valid_a{"s_obp_valid_a"};
        sc_signal<host_data_t> s_host_rdata_obp_a{"s_host_rdata_obp_a"};
        sc_signal<host_data_t> s_host_rdata_rce_a{"s_host_rdata_rce_a"};
        sc_signal<uint32_t> s_re_lut_op_a{"s_re_lut_op_a"};
        sc_signal<float> s_re_lut_in_a{"s_re_lut_in_a"};
        sc_signal<bool> s_re_lut_valid_a{"s_re_lut_valid_a"};
        sc_signal<float> s_re_lut_out_a{"s_re_lut_out_a"};
        sc_signal<bool> s_re_lut_valid_out_a{"s_re_lut_valid_out_a"};
        sc_signal<uint32_t> s_re_mode_a{"s_re_mode_a"};
        sc_signal<bool> s_re_start_a{"s_re_start_a"};
        sc_signal<bool> s_re_valid_a{"s_re_valid_a"};
        sc_signal<psum_vector_t<Y_DIM, T_PSUM>> s_re_vector_data_a{"s_re_vector_data_a"};
        sc_signal<act_vector_t<Y_DIM, T_ACT>> s_re_skip_data_a{"s_re_skip_data_a"};
        sc_signal<psum_vector_t<Y_DIM, T_PSUM>> s_re_vector_out_a{"s_re_vector_out_a"};
        sc_signal<bool> s_re_valid_out_a{"s_re_valid_out_a"};
        sc_signal<bool> s_re_done_a{"s_re_done_a"};

        // Unpacked OBP Config signals
        sc_signal<bool> s_obp_bias_en_a{"s_obp_bias_en_a"};
        sc_signal<bool> s_obp_requant_en_a{"s_obp_requant_en_a"};
        sc_signal<bool> s_obp_lut_en_a{"s_obp_lut_en_a"};
        sc_signal<bool> s_obp_residual_en_a{"s_obp_residual_en_a"};
        sc_signal<act_vector_t<Y_DIM, T_ACT>> s_residual_zero_a{"s_residual_zero_a"};

        // Lane B signals
        sc_signal<bool> s_act_feeder_en_b{"s_act_feeder_en_b"};
        sc_signal<bool> s_act_feeder_clear_b{"s_act_feeder_clear_b"};
        sc_signal<bool> s_act_start_b{"s_act_start_b"};
        sc_signal<bool> s_act_valid_b{"s_act_valid_b"};
        sc_signal<bool> s_act_finalpush_b{"s_act_finalpush_b"};
        sc_signal<bool> s_act_cnt_en_b{"s_act_cnt_en_b"};
        sc_signal<bool> s_act_cnt_clear_b{"s_act_cnt_clear_b"};
        sc_signal<bool> s_act_clearfifo_b{"s_act_clearfifo_b"};
        sc_signal<bool> s_act_pop_en_b{"s_act_pop_en_b"};
        sc_signal<bool> s_act_finalctx_b{"s_act_finalctx_b"};

        sc_signal<bool> s_wei_feeder_en_b{"s_wei_feeder_en_b"};
        sc_signal<bool> s_wei_feeder_clear_b{"s_wei_feeder_clear_b"};
        sc_signal<bool> s_wei_start_b{"s_wei_start_b"};
        sc_signal<bool> s_wei_valid_b{"s_wei_valid_b"};
        sc_signal<bool> s_wei_finalpush_b{"s_wei_finalpush_b"};
        sc_signal<bool> s_wei_cnt_en_b{"s_wei_cnt_en_b"};
        sc_signal<bool> s_wei_cnt_clear_b{"s_wei_cnt_clear_b"};
        sc_signal<bool> s_wei_clearfifo_b{"s_wei_clearfifo_b"};
        sc_signal<bool> s_wei_pop_en_b{"s_wei_pop_en_b"};
        sc_signal<bool> s_wei_cswitch_b{"s_wei_cswitch_b"};

        sc_signal<bool> s_act_done_b{"s_act_done_b"};
        sc_signal<bool> s_act_til_done_b{"s_act_til_done_b"};
        sc_signal<bool> s_act_fifo_empty_b{"s_act_fifo_empty_b"};
        sc_signal<bool> s_act_fifo_full_b{"s_act_fifo_full_b"};
        sc_signal<bool> s_act_stall_b{"s_act_stall_b"};

        sc_signal<bool> s_wei_done_b{"s_wei_done_b"};
        sc_signal<bool> s_wei_til_done_b{"s_wei_til_done_b"};
        sc_signal<bool> s_wei_fifo_empty_b{"s_wei_fifo_empty_b"};
        sc_signal<bool> s_wei_fifo_full_b{"s_wei_fifo_full_b"};
        sc_signal<bool> s_wei_stall_b{"s_wei_stall_b"};

        sc_signal<bool> s_psm_start_b{"s_psm_start_b"};
        sc_signal<bool> s_psm_reset_b{"s_psm_reset_b"};
        sc_signal<bool> s_psm_done_b{"s_psm_done_b"};
        sc_signal<bool> s_psm_finalwrite_b{"s_psm_finalwrite_b"};
        sc_signal<bool> s_psm_shift_done_b{"s_psm_shift_done_b"};
        sc_signal<bool> s_cscan_en_b{"s_cscan_en_b"};

        sc_signal<bool> s_sa_clear_b{"s_sa_clear_b"};
        sc_signal<bool> s_pipeline_en_b{"s_pipeline_en_b"};
        sc_signal<sc_bv<X_DIM>> s_cswitch_arr_b{"s_cswitch_arr_b"};

        sc_signal<uint32_t> s_context_id_b{"s_context_id_b"};
        sc_signal<uint32_t> s_global_context_id_b{"s_global_context_id_b"};
        sc_signal<uint32_t> s_local_context_id_b{"s_local_context_id_b"};
        sc_signal<uint32_t> s_out_tile_id_b{"s_out_tile_id_b"};

        sc_signal<uint32_t> s_srama_addr_b{"s_srama_addr_b"};
        sc_signal<bool> s_srama_rden_b{"s_srama_rden_b"};
        sc_signal<act_vector_t<Y_DIM, T_ACT>> s_srama_data_b{"s_srama_data_b"};

        sc_signal<uint32_t> s_sramb_addr_b{"s_sramb_addr_b"};
        sc_signal<bool> s_sramb_rden_b{"s_sramb_rden_b"};
        sc_signal<wei_vector_t<X_DIM, T_WEI>> s_sramb_data_b{"s_sramb_data_b"};

        sc_signal<psum_vector_t<Y_DIM, T_PSUM>> s_sramc_wdata_b{"s_sramc_wdata_b"};
        sc_signal<uint32_t> s_sramc_addr_b{"s_sramc_addr_b"};
        sc_signal<bool> s_sramc_wren_b{"s_sramc_wren_b"};
        sc_signal<bool> s_sramc_rden_b{"s_sramc_rden_b"};
        sc_signal<sramc_mask_t<Y_DIM>> s_sramc_wmask_b{"s_sramc_wmask_b"};
        sc_signal<psum_vector_t<Y_DIM, T_PSUM>> s_sramc_rdata_b{"s_sramc_rdata_b"};

        sc_signal<psum_vector_t<Y_DIM, T_PSUM>> s_psm_to_sa_c_b{"s_psm_to_sa_c_b"};
        sc_signal<psum_vector_t<Y_DIM, T_PSUM>> s_sa_to_psm_c_b{"s_sa_to_psm_c_b"};

        sc_signal<act_vector_t<Y_DIM, T_ACT>> s_act_arr_b{"s_act_arr_b"};
        sc_signal<wei_vector_t<X_DIM, T_WEI>> s_wei_arr_b{"s_wei_arr_b"};
        sc_signal<act_vector_t<Y_DIM, T_ACT>> s_act_arr_to_array_b{"s_act_arr_to_array_b"};
        sc_signal<wei_vector_t<X_DIM, T_WEI>> s_wei_arr_to_array_b{"s_wei_arr_to_array_b"};

        // OBP B signals
        sc_signal<psum_vector_t<Y_DIM, T_PSUM>> s_obp_sramc_wdata_b{"s_obp_sramc_wdata_b"};
        sc_signal<uint32_t> s_obp_sramc_addr_b{"s_obp_sramc_addr_b"};
        sc_signal<bool> s_obp_sramc_wren_b{"s_obp_sramc_wren_b"};
        sc_signal<sramc_mask_t<Y_DIM>> s_obp_sramc_wmask_b{"s_obp_sramc_wmask_b"};
        sc_signal<bool> s_obp_valid_b{"s_obp_valid_b"};
        sc_signal<host_data_t> s_host_rdata_obp_b{"s_host_rdata_obp_b"};
        sc_signal<host_data_t> s_host_rdata_rce_b{"s_host_rdata_rce_b"};
        sc_signal<uint32_t> s_re_lut_op_b{"s_re_lut_op_b"};
        sc_signal<float> s_re_lut_in_b{"s_re_lut_in_b"};
        sc_signal<bool> s_re_lut_valid_b{"s_re_lut_valid_b"};
        sc_signal<float> s_re_lut_out_b{"s_re_lut_out_b"};
        sc_signal<bool> s_re_lut_valid_out_b{"s_re_lut_valid_out_b"};
        sc_signal<uint32_t> s_re_mode_b{"s_re_mode_b"};
        sc_signal<bool> s_re_start_b{"s_re_start_b"};
        sc_signal<bool> s_re_valid_b{"s_re_valid_b"};
        sc_signal<psum_vector_t<Y_DIM, T_PSUM>> s_re_vector_data_b{"s_re_vector_data_b"};
        sc_signal<act_vector_t<Y_DIM, T_ACT>> s_re_skip_data_b{"s_re_skip_data_b"};
        sc_signal<psum_vector_t<Y_DIM, T_PSUM>> s_re_vector_out_b{"s_re_vector_out_b"};
        sc_signal<bool> s_re_valid_out_b{"s_re_valid_out_b"};
        sc_signal<bool> s_re_done_b{"s_re_done_b"};

        // Unpacked OBP Config signals
        sc_signal<bool> s_obp_bias_en_b{"s_obp_bias_en_b"};
        sc_signal<bool> s_obp_requant_en_b{"s_obp_requant_en_b"};
        sc_signal<bool> s_obp_lut_en_b{"s_obp_lut_en_b"};
        sc_signal<bool> s_obp_residual_en_b{"s_obp_residual_en_b"};
        sc_signal<act_vector_t<Y_DIM, T_ACT>> s_residual_zero_b{"s_residual_zero_b"};

        // ConfigRegs OBP signals
        sc_signal<uint32_t> s_obp_cfg_a{"s_obp_cfg_a"};
        sc_signal<uint32_t> s_requant_scale_a{"s_requant_scale_a"};
        sc_signal<uint32_t> s_requant_shift_a{"s_requant_shift_a"};
        sc_signal<uint32_t> s_obp_cfg_b{"s_obp_cfg_b"};
        sc_signal<uint32_t> s_requant_scale_b{"s_requant_scale_b"};
        sc_signal<uint32_t> s_requant_shift_b{"s_requant_shift_b"};
        sc_signal<uint32_t> s_nsplit{"s_nsplit"};

        // Deadlock outputs from A and B controllers
        sc_signal<bool> s_deadlock_a{"s_deadlock_a"};
        sc_signal<bool> s_deadlock_b{"s_deadlock_b"};

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

        void host_rdata_mux()
        {
            uint32_t addr = i_host_addr.read();
            uint32_t region = addr & 0x00FF0000;
            uint32_t mem_region = addr & SAURIA_MEM_ADDR_MASK;
            if (mem_region == CFG_REGS_OFFSET)
            {
                o_host_rdata.write(s_host_rdata_cfg.read());
            }
            else if (region == 0x00140000 || region == 0x00150000)
            {
                o_host_rdata.write(s_host_rdata_obp_a.read());
            }
            else if (region == 0x00160000 || region == 0x00170000)
            {
                o_host_rdata.write(s_host_rdata_obp_b.read());
            }
            else if (region == 0x00200000 || region == 0x00210000 || region == 0x00220000)
            {
                o_host_rdata.write(s_host_rdata_rce_a.read());
            }
            else if (region == 0x00230000 || region == 0x00240000 || region == 0x00250000)
            {
                o_host_rdata.write(s_host_rdata_rce_b.read());
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
                i_start.read() ||
                s_cfg_start.read();

            if (reset_active || new_start)
            {
                done_latched_reg_a = false;
                done_latched_reg_b = false;
                o_done.write(false);
            }
            else
            {
                if (s_ctrl_done_a.read())
                {
                    done_latched_reg_a = true;
                }
                if (s_ctrl_done_b.read())
                {
                    done_latched_reg_b = true;
                }

                uint32_t nsplit = s_nsplit.read();
                if (nsplit < Y_DIM)
                {
                    o_done.write(done_latched_reg_a && done_latched_reg_b);
                }
                else
                {
                    o_done.write(done_latched_reg_a);
                }
            }
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
            dbg_compare_count = 0;
            dbg_ref_cycle = 0;
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

            bool new_context_pulse =
                s_act_cnt_clear_a.read() &&
                s_wei_cnt_clear_a.read();

            if (new_context_pulse)
            {
                dbg_ref_ctx = s_context_id_a.read();
                clear_debug_stream_ref();
                dbg_ref_active = true;
                dbg_ref_initialized = true;

                DBG_COUT << "\n[STREAM REF START]"
                         << " context=" << dbg_ref_ctx
                         << " cxlim=" << s_cxlim.read()
                         << " cxstep=" << s_cxstep.read()
                         << std::endl;
            }

            if (dbg_ref_active &&
                s_pipeline_en_a.read() &&
                s_act_pop_en_a.read() &&
                s_wei_pop_en_a.read())
            {
                act_vector_t<Y_DIM, T_ACT> act_vec = s_act_arr_to_array_a.read();
                wei_vector_t<X_DIM, T_WEI> wei_vec = s_wei_arr_to_array_a.read();

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
                    sa_stream_trace << sa_stream_count << "," << s_context_id_a.read();

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

                dbg_ref_cycle++;
            }

            if (dbg_ref_initialized && s_obp_sramc_wren_a.read())
            {
                uint32_t addr = s_obp_sramc_addr_a.read();
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
                            s_obp_sramc_wdata_a.read();

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
                s_act_cnt_clear_a.read() &&
                s_wei_cnt_clear_a.read();

            bool clear_pulse =
                clear_now && !prev_clear;

            prev_clear = clear_now;

            if (clear_pulse)
            {
                current_ctx = s_context_id_a.read();
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
            bool start_a = start_val || s_trigger_start_a.read();
            bool start_b = start_val || s_trigger_start_b.read();
            static bool prev_start = false;

            if (start_val && !prev_start)
            {
                DBG_COUT << "\n=========================================\n";
                DBG_COUT << "NPU TOP RUNTIME CONFIGURATION AT START\n";
                DBG_COUT << "=========================================\n";
                DBG_COUT << "=========================================\n\n";
            }

            prev_start = start_val;
            s_start_internal.write(start_val);
            s_start_internal_a.write(start_a);
            s_start_internal_b.write(start_b);
            s_ctrl_reset_internal.write(i_soft_reset.read() || s_cfg_soft_reset.read());
        }

        void nsplit_mux_logic()
        {
            if (s_use_instr_mode.read())
            {
                s_nsplit.write(s_instr_nsplit.read());
            }
            else
            {
                s_nsplit.write(s_config_nsplit.read());
            }
        }

        void base_addr_mux_logic()
        {
            if (s_use_instr_mode.read())
            {
                s_wei_base_addr_a.write(s_instr_wei_base_addr_a.read());
                s_wei_base_addr_b.write(s_instr_wei_base_addr_b.read());
                s_act_base_addr_a.write(s_instr_ifmap_base_addr_a.read());
                s_act_base_addr_b.write(s_instr_ifmap_base_addr_b.read());
            }
            else
            {
                s_wei_base_addr_a.write(s_wei_base_addr.read());
                s_wei_base_addr_b.write(s_wei_base_addr.read());
                s_act_base_addr_a.write(s_act_base_addr.read());
                s_act_base_addr_b.write(s_act_base_addr.read());
            }
        }

        void debug_ref_stream_mux()
        {
            if (!i_rstn.read())
            {
                s_act_arr_to_array_a.write(act_vector_t<Y_DIM, T_ACT>(static_cast<T_ACT>(0)));
                s_wei_arr_to_array_a.write(wei_vector_t<X_DIM, T_WEI>(static_cast<T_WEI>(0)));
                s_act_arr_to_array_b.write(act_vector_t<Y_DIM, T_ACT>(static_cast<T_ACT>(0)));
                s_wei_arr_to_array_b.write(wei_vector_t<X_DIM, T_WEI>(static_cast<T_WEI>(0)));

                dbg_stream_ctx = 0;
                dbg_stream_k = 0;
                dbg_stream_active = false;
                dbg_prev_cnt_clear = false;
                return;
            }

            if (!dbg_ref_stream_en)
            {
                s_act_arr_to_array_a.write(s_act_arr_a.read());
                s_wei_arr_to_array_a.write(s_wei_arr_a.read());
                s_act_arr_to_array_b.write(s_act_arr_b.read());
                s_wei_arr_to_array_b.write(s_wei_arr_b.read());
                return;
            }

            debug_load_ref_stream_once();

            bool clear_now =
                s_act_cnt_clear_a.read() &&
                s_wei_cnt_clear_a.read();

            bool clear_pulse =
                clear_now && !dbg_prev_cnt_clear;

            dbg_prev_cnt_clear = clear_now;

            if (clear_pulse)
            {
                dbg_stream_ctx = s_context_id_a.read();
                dbg_stream_k = 0;
                dbg_stream_active = true;

                DBG_COUT << "[DBG REF STREAM START]"
                         << " ctx=" << dbg_stream_ctx
                         << " incntlim=" << s_incntlim.read()
                         << std::endl;
            }

            act_vector_t<Y_DIM, T_ACT> act_out_a(static_cast<T_ACT>(0));
            wei_vector_t<X_DIM, T_WEI> wei_out_a(static_cast<T_WEI>(0));
            act_vector_t<Y_DIM, T_ACT> act_out_b(static_cast<T_ACT>(0));
            wei_vector_t<X_DIM, T_WEI> wei_out_b(static_cast<T_WEI>(0));

            const uint32_t K_LEN = 576;
            const uint32_t INJECT_TOTAL =
                K_LEN + ((X_DIM > Y_DIM) ? (X_DIM - 1) : (Y_DIM - 1));

            bool valid_inject_cycle =
                dbg_stream_active &&
                s_pipeline_en_a.read();

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
                            act_out_a[y] = static_cast<T_ACT>(dbg_A_flat[idx]);
                        }
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
                            wei_out_a[x] = static_cast<T_WEI>(dbg_B_flat[idx]);
                        }
                    }
                }

                if (dbg_stream_k < 24)
                {
                    DBG_COUT << "[DBG REF STREAM SKEWED]"
                             << " ctx=" << dbg_stream_ctx
                             << " t=" << dbg_stream_k
                             << " act=" << act_out_a
                             << " wei=" << wei_out_a
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

            s_act_arr_to_array_a.write(act_out_a);
            s_wei_arr_to_array_a.write(wei_out_a);
            s_act_arr_to_array_b.write(act_out_b);
            s_wei_arr_to_array_b.write(wei_out_b);
        }

        void obp_cfg_unpack()
        {
            uint32_t cfg_a = s_obp_cfg_a.read();
            s_obp_bias_en_a.write((cfg_a & 0x1) != 0);
            s_obp_requant_en_a.write((cfg_a & 0x2) != 0);
            s_obp_lut_en_a.write((cfg_a & 0x4) != 0);
            s_obp_residual_en_a.write((cfg_a & 0x8) != 0);

            uint32_t cfg_b = s_obp_cfg_b.read();
            s_obp_bias_en_b.write((cfg_b & 0x1) != 0);
            s_obp_requant_en_b.write((cfg_b & 0x2) != 0);
            s_obp_lut_en_b.write((cfg_b & 0x4) != 0);
            s_obp_residual_en_b.write((cfg_b & 0x8) != 0);
        }

        void deadlock_merge_logic()
        {
            o_deadlock.write(s_deadlock_a.read() || s_deadlock_b.read());
        }
    };

} // namespace sauria

#endif // SAURIA_UNIFIED_NPU_TOP_H
