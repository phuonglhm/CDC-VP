//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_rcal_adc
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_RCAL_ADC_H
#define VL_MIPI_4_RX_DPHY_NS_RCAL_ADC_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_4_rx_dphy_ns_gen_sync__reset_edge0.h"

// Parameters: stRCAL_IDLE=0, stCOMP_PU=1, stRCAL_ADC=2, stTX_RESCAL=3
class mipi_4_rx_dphy_ns_rcal_adc : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> async_reset{"async_reset"};
    // input        sync_reset;
    sc_core::sc_in<sc_dt::sc_logic> ck_ref_s{"ck_ref_s"};
    sc_core::sc_in<sc_dt::sc_logic> rcal_enb{"rcal_enb"};
    sc_core::sc_in<sc_dt::sc_logic> adc_enb{"adc_enb"};
    sc_core::sc_in<sc_dt::sc_lv<3>> d_tx_term{"d_tx_term"};
    sc_core::sc_in<sc_dt::sc_logic> sup_comp_rt_result{"sup_comp_rt_result"};
    // input       scan_mode;
    sc_core::sc_out<sc_dt::sc_logic> rcal_adc_done{"rcal_adc_done"};
    sc_core::sc_out<sc_dt::sc_logic> sup_comp_mode_i{"sup_comp_mode_i"};
    sc_core::sc_out<sc_dt::sc_logic> sup_comp_rt_pwron_i{"sup_comp_rt_pwron_i"};
    sc_core::sc_out<sc_dt::sc_logic> sup_comp_rt_r_i{"sup_comp_rt_r_i"};
    // output [7:0] sup_dac;
    sc_core::sc_out<sc_dt::sc_lv<10>> sup_dac{"sup_dac"};
    sc_core::sc_out<sc_dt::sc_lv<7>> rescal_rep_i{"rescal_rep_i"};
    sc_core::sc_out<sc_dt::sc_lv<7>> tx_rescal_i{"tx_rescal_i"};
    sc_core::sc_out<sc_dt::sc_lv<10>> adc_val{"adc_val"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_rcal_adc);
    explicit mipi_4_rx_dphy_ns_rcal_adc(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000.0;  // time precision (fs)
    // Regs/Wires
    sc_core::sc_signal<sc_dt::sc_lv<4>> clk_cnt{"clk_cnt", vl::logic<4>().lv()};
    sc_core::sc_signal<sc_dt::sc_lv<2>> stage_cnt{"stage_cnt", vl::logic<2>().lv()};
    sc_core::sc_signal<sc_dt::sc_lv<4>> bit_cnt{"bit_cnt", vl::logic<4>().lv()};
    sc_core::sc_signal<sc_dt::sc_lv<2>> next_rcal_state{"next_rcal_state", vl::logic<2>().lv()};
    sc_core::sc_signal<sc_dt::sc_logic> tx_rescal_enb{"tx_rescal_enb", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_logic> load_tx_rescal{"load_tx_rescal", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_logic> sup_comp_rt_pwron_int{"sup_comp_rt_pwron_int", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_logic> rcal_adc_done_int{"rcal_adc_done_int", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_logic> initialize{"initialize", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_logic> bit_cnt_enb{"bit_cnt_enb", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_logic> stage_cnt_enb{"stage_cnt_enb", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_logic> clk_cnt_enb{"clk_cnt_enb", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_lv<2>> rcal_state{"rcal_state", vl::logic<2>().lv()};
    sc_core::sc_signal<sc_dt::sc_logic> rt_result_latch{"rt_result_latch", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_lv<10>> tx_rescal_acc{"tx_rescal_acc", vl::logic<10>().lv()};
    sc_core::sc_signal<sc_dt::sc_logic> sup_comp_mode_int{"sup_comp_mode_int", vl::logic<1>().sl()};
    sc_core::sc_signal_resolved ck_ref_g{"ck_ref_g", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clk_cnt_done{"clk_cnt_done", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved stage_cnt_done{"stage_cnt_done", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved bit_cnt_done{"bit_cnt_done", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<10> sar_bit{"sar_bit", vl::logic<10>::all_z().lv()};
    sc_core::sc_signal<sc_dt::sc_logic> rcal_enb_d2{"rcal_enb_d2", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_logic> rcal_enb_d3{"rcal_enb_d3", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_logic> adc_enb_d2{"adc_enb_d2", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_logic> adc_enb_d3{"adc_enb_d3", vl::logic<1>().sl()};
    sc_core::sc_signal_resolved rcal_trigger{"rcal_trigger", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved adc_trigger{"adc_trigger", sc_dt::SC_LOGIC_Z};
    // wire [7:0] sup_dac_n_i;
    // wire [2:0] sup_dac_th_n_i;
    sc_core::sc_signal_resolved tx_rescal_done{"tx_rescal_done", sc_dt::SC_LOGIC_Z};
    // Double FF synchronization of "rcal_enb" with "ck_ref_s"
    // if(async_reset)
    // rcal_enb_async <= 2'b00;
    // else
    // rcal_enb_async <= {rcal_enb_async[0], rcal_enb};
    // //Double FF synchronization of "adc_enb" with "ck_ref_s"
    // if(async_reset)
    // adc_enb_async <= 2'b00;
    // else
    // adc_enb_async <= {adc_enb_async[0], adc_enb};
    // wire            cfg_srst                    ;
    // wire            cfg_srst_scanable           ;
    // wire            cfgclk_rcal_gated           ;
    sc_core::sc_signal_resolved cfgclk_rcal_int_scanable{"cfgclk_rcal_int_scanable", sc_dt::SC_LOGIC_Z};
    // reg     [1:0]   cfgclk_div4                 ;
    sc_core::sc_signal_resolved rcal_enb_sync{"rcal_enb_sync", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved adc_enb_sync{"adc_enb_sync", sc_dt::SC_LOGIC_Z};
    // synchronous reset gating
    // gen_sync_rst #(.reset_value(1), .reset_edge(1)) hand_cfg_srst_sync(.rst(async_reset), .clk(cfgclk_rcal_int_scanable), .srst(cfg_srst));
    // gen_mux hand_cfg_srst_mux(.z(cfg_srst_scanable), .sel(scanmode), .d0(cfg_srst), .d1(scanrst));
    // gen_sync #(.reset_edge(1)) sync_rcal_enb_sync(.data_out(rcal_enb_sync), .data_out_dly(), .scanmode(1'b0), .clk(cfgclk_rcal_int_scanable), .rst(cfg_srst_scanable), .data_in(rcal_enb));
    // gen_sync #(.reset_edge(1)) sync_adc_enb_sync(.data_out(adc_enb_sync), .data_out_dly(), .scanmode(1'b0), .clk(cfgclk_rcal_int_scanable), .rst(cfg_srst_scanable), .data_in(adc_enb));
    std::unique_ptr<mipi_4_rx_dphy_ns_gen_sync__reset_edge0> sync_rcal_enb_sync;
    sc_core::sc_signal_resolved c_sync_rcal_enb_sync_rst{"c_sync_rcal_enb_sync_rst", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved tie0_{"tie0_", sc_dt::SC_LOGIC_0};
    vl::tie tie0__drv{"tie0__drv", sc_dt::SC_LOGIC_0};
    sc_core::sc_signal_resolved nc_sync_rcal_enb_sync_data_out_dly{"nc_sync_rcal_enb_sync_data_out_dly", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_gen_sync__reset_edge0> sync_adc_enb_sync;
    sc_core::sc_signal_resolved c_sync_adc_enb_sync_rst{"c_sync_adc_enb_sync_rst", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved nc_sync_adc_enb_sync_data_out_dly{"nc_sync_adc_enb_sync_data_out_dly", sc_dt::SC_LOGIC_Z};

    void assign_cfgclk_rcal_int_scanable();
    void assign_ck_ref_g();
    void assign_clk_cnt_done();
    void assign_stage_cnt_done();
    void assign_bit_cnt_done();
    void assign_sar_bit();
    void assign_rcal_trigger();
    void assign_adc_trigger();
    void assign_adc_val();
    void assign_tx_rescal_done();
    void glue_sync_rcal_enb_sync_rst();
    void glue_sync_adc_enb_sync_rst();
    void proc_always_210();
    void proc_always_223();
    void proc_always_256();
    void proc_always_276();
    void proc_always_296();
    void proc_always_360();
    void proc_always_438();
    void proc_always_462();
    void proc_always_471();
    void proc_always_488();
    void proc_always_507();
};

#endif
