//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_dphytermcal
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYTERMCAL_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYTERMCAL_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_4_rx_dphy_ns_gen_sync__reset_edge0.h"

// Parameters: WIDTH=4, CAL_SEL=0, IDLE=0, STARTCAL=1, RAMPUP=2, RAMPDWN=3, SAME_CMPOUT=4, ERROR=5, ROUND_MAX=6, ENDCAL=7
class mipi_4_rx_dphy_ns_dphytermcal : public sc_core::sc_module {
public:
    // CAL_SEL = 1'b1 -> Upper termination calibration - Just rump down ; CAL_SEL = 1'b0 -> Default bottom termination calibration - Rump up and rum down
    sc_core::sc_in<sc_dt::sc_logic> cfg_clk{"cfg_clk"};
    // System clock that generates the 1MHz clock
    sc_core::sc_in<sc_dt::sc_logic> clock1m{"clock1m"};
    // tx_atb_comp_clk  - 1 MHZ clock
    sc_core::sc_in<sc_dt::sc_logic> resetz{"resetz"};
    // FSM reset - Should be synchronous with clock1m
    sc_core::sc_in<sc_dt::sc_logic> cal_en{"cal_en"};
    // Calibration enable signal - Should be synchronous with clock1m
    sc_core::sc_in<sc_dt::sc_logic> cal_round_max{"cal_round_max"};
    // cal_round_max = 1'b1 -> Round to max; cal_round_max = 1'b0 -> Round to min
    sc_core::sc_in<sc_dt::sc_lv<3>> timertermcal{"timertermcal"};
    // Threshold to 1MHz counter
    sc_core::sc_in<sc_dt::sc_logic> cmpout{"cmpout"};
    // Comparator output signal
    sc_core::sc_out<sc_dt::sc_lv<4>> setrd{"setrd"};
    // Termination calibration word
    sc_core::sc_out<sc_dt::sc_logic> cal_error{"cal_error"};
    // Calibration error
    sc_core::sc_out<sc_dt::sc_logic> cal_done{"cal_done"};
    // Calibration done signal
    sc_core::sc_out<sc_dt::sc_logic> rtunpon{"rtunpon"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_dphytermcal);
    explicit mipi_4_rx_dphy_ns_dphytermcal(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000.0;  // time precision (fs)
    sc_core::sc_signal<sc_dt::sc_logic> aux_tripu{"aux_tripu", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_logic> aux_tripd{"aux_tripd", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_lv<4>> aux_a{"aux_a", vl::logic<4>().lv()};
    sc_core::sc_signal<sc_dt::sc_lv<4>> aux_b{"aux_b", vl::logic<4>().lv()};
    sc_core::sc_signal<sc_dt::sc_logic> not_same_cmpout{"not_same_cmpout", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_lv<3>> counter{"counter", vl::logic<3>().lv()};
    sc_core::sc_signal<sc_dt::sc_lv<3>> state{"state", vl::logic<3>().lv()};
    sc_core::sc_signal_resolved cmpout_sync{"cmpout_sync", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal<sc_dt::sc_logic> cmpout_tripu_toggles{"cmpout_tripu_toggles", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_logic> cmpout_tripd_toggles{"cmpout_tripd_toggles", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_lv<2>> edge_detect{"edge_detect", vl::logic<2>().lv()};
    sc_core::sc_signal<sc_dt::sc_logic> cal_en_det{"cal_en_det", vl::logic<1>().sl()};
    sc_core::sc_signal_rv<5> aux_sum{"aux_sum", vl::logic<5>::all_z().lv()};
    sc_core::sc_signal_rv<5> even_odd{"even_odd", vl::logic<5>::all_z().lv()};
    sc_core::sc_signal_rv<4> round_max_calc{"round_max_calc", vl::logic<4>::all_z().lv()};
    sc_core::sc_signal_resolved cal_error_int{"cal_error_int", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal<sc_dt::sc_logic> setrd_auxa_not_seq{"setrd_auxa_not_seq", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_logic> setrd_auxb_not_seq{"setrd_auxb_not_seq", vl::logic<1>().sl()};
    // CMPOUT Synchronization
    std::unique_ptr<mipi_4_rx_dphy_ns_gen_sync__reset_edge0> compout_sync;
    sc_core::sc_signal_resolved tie0_{"tie0_", sc_dt::SC_LOGIC_0};
    vl::tie tie0__drv{"tie0__drv", sc_dt::SC_LOGIC_0};
    sc_core::sc_signal_resolved nc_compout_sync_data_out_dly{"nc_compout_sync_data_out_dly", sc_dt::SC_LOGIC_Z};

    void assign_rtunpon();
    void assign_aux_sum();
    void assign_even_odd();
    void assign_round_max_calc();
    void assign_cal_error_int();
    void proc_always_98();
    void proc_always_115();
    void proc_always_126();
    void proc_always_142();
    void proc_always_184();
    void proc_always_219();
    void proc_always_236();
    void proc_always_248();
    void proc_always_260();
    void proc_always_272();
    void proc_always_296();
    void proc_always_308();
    void proc_always_320();
    void proc_always_334();
    void proc_always_349();
    void proc_always_369();
};

#endif
