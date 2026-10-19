//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_sar_guard_band
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_SAR_GUARD_BAND_H
#define VL_MIPI_4_RX_DPHY_NS_SAR_GUARD_BAND_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_4_rx_dphy_ns_gen_sync__reset_edge0.h"

// Parameters: CALIDLE=0, CALSAR1=1, CALSAR2=2, CALSAR3=3, CALSAR4=4, CALSAR5=5, CALSAR6=6, CALERROR=7, CALDONE=8, WAITCAL=9, SAMP_DATA_READY=10
class mipi_4_rx_dphy_ns_sar_guard_band : public sc_core::sc_module {
public:
    // add rcal done and no convergence
    sc_core::sc_in<sc_dt::sc_logic> clock{"clock"};
    sc_core::sc_in<sc_dt::sc_logic> resetz{"resetz"};
    sc_core::sc_in<sc_dt::sc_lv<8>> total_ones{"total_ones"};
    sc_core::sc_in<sc_dt::sc_logic> data_ready{"data_ready"};
    sc_core::sc_out<sc_dt::sc_logic> cal_done{"cal_done"};
    sc_core::sc_out<sc_dt::sc_logic> no_convergence{"no_convergence"};
    sc_core::sc_out<sc_dt::sc_lv<6>> control_current_word{"control_current_word"};
    // output of the calibration
    sc_core::sc_in<sc_dt::sc_lv<8>> cfgclkfreq{"cfgclkfreq"};
    sc_core::sc_in<sc_dt::sc_logic> cmp_polarity{"cmp_polarity"};
    sc_core::sc_in<sc_dt::sc_logic> start_calibration{"start_calibration"};
    sc_core::sc_in<sc_dt::sc_lv<8>> offset_comp_threshold{"offset_comp_threshold"};
    sc_core::sc_in<sc_dt::sc_lv<8>> tolerance{"tolerance"};
    sc_core::sc_out<sc_dt::sc_logic> enable_counter{"enable_counter"};
    sc_core::sc_in<sc_dt::sc_lv<3>> number_of_ready_count{"number_of_ready_count"};
    sc_core::sc_in<sc_dt::sc_logic> repeatcal{"repeatcal"};
    sc_core::sc_out<sc_dt::sc_logic> afe_offset_cal_en{"afe_offset_cal_en"};
    sc_core::sc_out<sc_dt::sc_logic> data_ready_at_clock_in{"data_ready_at_clock_in"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_sar_guard_band);
    explicit mipi_4_rx_dphy_ns_sar_guard_band(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_resolved cmpstateout{"cmpstateout", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal<sc_dt::sc_lv<10>> guard_counter{"guard_counter", vl::logic<10>().lv()};
    sc_core::sc_signal<sc_dt::sc_logic> guard_time_finished{"guard_time_finished", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_lv<8>> total_ones_sync{"total_ones_sync", vl::logic<8>().lv()};
    sc_core::sc_signal<sc_dt::sc_lv<2>> edge_detect{"edge_detect", vl::logic<2>().lv()};
    sc_core::sc_signal<sc_dt::sc_logic> start_calibration_syn{"start_calibration_syn", vl::logic<1>().sl()};
    // output of the calibration
    // Comparator Polarity
    sc_core::sc_signal_resolved comparator_polarity{"comparator_polarity", sc_dt::SC_LOGIC_Z};
    // Threshold limit hit flg
    sc_core::sc_signal_resolved hit_threshold{"hit_threshold", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal<sc_dt::sc_lv<6>> bcal{"bcal", vl::logic<6>().lv()};
    // Offset calibration control bits for PWM receiver
    sc_core::sc_signal<sc_dt::sc_logic> caldone{"caldone", vl::logic<1>().sl()};
    // Calibration done flag
    // reg             calerror                ; // Calibration error flag - "No convergence condition"
    sc_core::sc_signal<sc_dt::sc_lv<4>> state{"state", vl::logic<4>().lv()};
    // PWM comparator offset calibration state
    sc_core::sc_signal<sc_dt::sc_lv<4>> save_state{"save_state", vl::logic<4>().lv()};
    // PWM comparator offset calibration save sate ( to use comon states)
    sc_core::sc_signal<sc_dt::sc_logic> calsardone{"calsardone", vl::logic<1>().sl()};
    // PWM comparator offset calibration done flag
    sc_core::sc_signal<sc_dt::sc_logic> calsarerror{"calsarerror", vl::logic<1>().sl()};
    // PWM comparator offset calibration error flag - "No convergence condition"
    sc_core::sc_signal<sc_dt::sc_logic> bit_cnt_ena{"bit_cnt_ena", vl::logic<1>().sl()};
    // bit counter enable
    sc_core::sc_signal<sc_dt::sc_lv<3>> bit_cnt{"bit_cnt", vl::logic<3>().lv()};
    // bit counter
    sc_core::sc_signal_rv<6> onehotbit{"onehotbit", vl::logic<6>::all_z().lv()};
    // One-Hot bit
    sc_core::sc_signal_resolved cmpstateout_syn{"cmpstateout_syn", sc_dt::SC_LOGIC_Z};
    // reg             grant_guard_time_count  ;
    sc_core::sc_signal<sc_dt::sc_lv<3>> ready_cnt{"ready_cnt", vl::logic<3>().lv()};
    sc_core::sc_signal<sc_dt::sc_logic> sample_done{"sample_done", vl::logic<1>().sl()};
    // dynamic_pulse_catcher i01_data_ready_strobe (
    // .pulse_in(data_ready),
    // .clk(clock),
    // .rst(resetz),
    // .pulse_out(data_ready_at_clock_in)
    // );
    std::unique_ptr<mipi_4_rx_dphy_ns_gen_sync__reset_edge0> data_ready_sync;
    sc_core::sc_signal_resolved tie0_{"tie0_", sc_dt::SC_LOGIC_0};
    vl::tie tie0__drv{"tie0__drv", sc_dt::SC_LOGIC_0};
    sc_core::sc_signal_resolved nc_data_ready_sync_data_out_dly{"nc_data_ready_sync_data_out_dly", sc_dt::SC_LOGIC_Z};

    void assign_cmpstateout();
    void assign_hit_threshold();
    void assign_control_current_word();
    void assign_no_convergence();
    void assign_cal_done();
    void assign_comparator_polarity();
    void assign_cmpstateout_syn();
    void assign_onehotbit();
    void proc_always_154();
    void proc_always_203();
    void proc_always_241();
    void proc_always_252();
    void proc_always_269();
    void proc_always_316();
    void proc_always_345();
    void proc_always_358();
    void proc_always_368();
    void proc_always_391();
    void proc_always_404();
    void proc_always_421();
    void proc_always_449();
    void proc_always_492();
    void proc_always_510();
    void proc_always_540();
};

#endif
