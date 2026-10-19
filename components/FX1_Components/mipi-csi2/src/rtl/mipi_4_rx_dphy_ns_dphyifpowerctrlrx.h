//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_dphyifpowerctrlrx
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYIFPOWERCTRLRX_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYIFPOWERCTRLRX_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_4_rx_dphy_ns_gen_sync__reset_edge0.h"
#include "mipi_4_rx_dphy_ns_gen_sync_bus__reset_edge0_bus_width4.h"

// Parameters: POWERDWN=0, BGPON=1, CAL_TYPE=2, BURNIN_CAL=3, TERMCAL=4, OFFSETCAL=5, OFFSET_LANE=6, IDLE=7, ULP=8, DDLTUNNING=9, SKEW_BACKWARD=10
class mipi_4_rx_dphy_ns_dphyifpowerctrlrx : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> cfg_clk{"cfg_clk"};
    // System clock (shall have scan mux)
    sc_core::sc_in<sc_dt::sc_logic> resetz{"resetz"};
    // Reset signal
    sc_core::sc_in<sc_dt::sc_logic> force_state{"force_state"};
    sc_core::sc_in<sc_dt::sc_lv<4>> target_state{"target_state"};
    sc_core::sc_out<sc_dt::sc_lv<4>> state_main{"state_main"};
    // timers control
    sc_core::sc_in<sc_dt::sc_lv<10>> twakeupulpm{"twakeupulpm"};
    sc_core::sc_in<sc_dt::sc_lv<10>> txstarttime{"txstarttime"};
    sc_core::sc_in<sc_dt::sc_lv<10>> txdindlytime{"txdindlytime"};
    sc_core::sc_in<sc_dt::sc_logic> txulpsexit{"txulpsexit"};
    sc_core::sc_out<sc_dt::sc_logic> lptxpon{"lptxpon"};
    sc_core::sc_out<sc_dt::sc_logic> lptxena{"lptxena"};
    sc_core::sc_out<sc_dt::sc_logic> lptxdin{"lptxdin"};
    sc_core::sc_out<sc_dt::sc_logic> lptxenpulldwn{"lptxenpulldwn"};
    // LP RX control
    // output        lprxponulp                ;
    // output        lprxponlp                 ;
    // output        lprxponcd                 ;
    sc_core::sc_out<sc_dt::sc_logic> bintpon{"bintpon"};
    sc_core::sc_in<sc_dt::sc_logic> ulpsactivenot{"ulpsactivenot"};
    sc_core::sc_out<sc_dt::sc_logic> hsrxpon_clklane{"hsrxpon_clklane"};
    sc_core::sc_out<sc_dt::sc_logic> hsrxpon_datalane{"hsrxpon_datalane"};
    sc_core::sc_in<sc_dt::sc_logic> offsetcal_clklane_done{"offsetcal_clklane_done"};
    sc_core::sc_in<sc_dt::sc_logic> offsetcal_lanes_done{"offsetcal_lanes_done"};
    sc_core::sc_out<sc_dt::sc_logic> hsrxencal{"hsrxencal"};
    sc_core::sc_out<sc_dt::sc_logic> des_en_for_datalane_offset_cal{"des_en_for_datalane_offset_cal"};
    sc_core::sc_out<sc_dt::sc_logic> rxclk_feed_int_clk{"rxclk_feed_int_clk"};
    sc_core::sc_out<sc_dt::sc_logic> rxlane_feed_int_clk{"rxlane_feed_int_clk"};
    sc_core::sc_out<sc_dt::sc_logic> hsrxencal_lane_offset{"hsrxencal_lane_offset"};
    sc_core::sc_in<sc_dt::sc_logic> perform_skew{"perform_skew"};
    sc_core::sc_in<sc_dt::sc_lv<8>> rxpontime{"rxpontime"};
    // Termination calibration signals
    sc_core::sc_out<sc_dt::sc_logic> cb_cal_en{"cb_cal_en"};
    sc_core::sc_in<sc_dt::sc_logic> rescal_done{"rescal_done"};
    sc_core::sc_out<sc_dt::sc_logic> rescal_en{"rescal_en"};
    sc_core::sc_in<sc_dt::sc_logic> cb_cal_en_ovr_en{"cb_cal_en_ovr_en"};
    sc_core::sc_in<sc_dt::sc_logic> noext_burnin_res_cal{"noext_burnin_res_cal"};
    sc_core::sc_in<sc_dt::sc_logic> burnin_cal_done{"burnin_cal_done"};
    sc_core::sc_out<sc_dt::sc_logic> burnin_cal_en{"burnin_cal_en"};
    sc_core::sc_in<sc_dt::sc_logic> burnin_cal_en_ovr_en{"burnin_cal_en_ovr_en"};
    sc_core::sc_in<sc_dt::sc_logic> bypass_offset_machine{"bypass_offset_machine"};
    sc_core::sc_in<sc_dt::sc_logic> bypass_skew_machine{"bypass_skew_machine"};
    sc_core::sc_in<sc_dt::sc_logic> bypass_ddltunning_machine{"bypass_ddltunning_machine"};
    sc_core::sc_out<sc_dt::sc_logic> ddltunning_en{"ddltunning_en"};
    // DDL tunning and one signals
    sc_core::sc_in<sc_dt::sc_logic> ddltunning_done{"ddltunning_done"};
    // skew comp flags
    sc_core::sc_out<sc_dt::sc_logic> skew_back_comp_en{"skew_back_comp_en"};
    sc_core::sc_in<sc_dt::sc_logic> skew_back_comp_done{"skew_back_comp_done"};
    // Lane control switch : core controls/IF controls
    sc_core::sc_out<sc_dt::sc_logic> enable_lanes{"enable_lanes"};
    // Configuration Signals from register bank
    sc_core::sc_in<sc_dt::sc_lv<10>> timeinitbg{"timeinitbg"};
    // Timer to have all currents/voltages generated on the bandgap
    // BIST control
    sc_core::sc_in<sc_dt::sc_logic> bist_control{"bist_control"};
    // PLL paralell boot
    sc_core::sc_out<sc_dt::sc_logic> pll_boot_fsm{"pll_boot_fsm"};
    // mux power-on control for internal feed
    sc_core::sc_out<sc_dt::sc_logic> rx_rxhs_prog_mux_pwron{"rx_rxhs_prog_mux_pwron"};
    // to perform  datalane offset and skew backwards when clklane PPI signal is off
    sc_core::sc_in<sc_dt::sc_logic> enableclk{"enableclk"};
    sc_core::sc_in<sc_dt::sc_logic> mastermacro{"mastermacro"};
    sc_core::sc_out<sc_dt::sc_logic> do_offs_and_skew_enableclk_not{"do_offs_and_skew_enableclk_not"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_dphyifpowerctrlrx);
    explicit mipi_4_rx_dphy_ns_dphyifpowerctrlrx(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000.0;  // time precision (fs)
    sc_core::sc_signal<sc_dt::sc_logic> bandgapok{"bandgapok", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_lv<10>> timer{"timer", vl::logic<10>().lv()};
    sc_core::sc_signal<sc_dt::sc_lv<10>> timeout{"timeout", vl::logic<10>().lv()};
    sc_core::sc_signal_resolved force_state_sync{"force_state_sync", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<4> target_state_sync{"target_state_sync", vl::logic<4>::all_z().lv()};
    std::unique_ptr<mipi_4_rx_dphy_ns_gen_sync__reset_edge0> gen_force_state_sync;
    sc_core::sc_signal_resolved tie0_{"tie0_", sc_dt::SC_LOGIC_0};
    vl::tie tie0__drv{"tie0__drv", sc_dt::SC_LOGIC_0};
    sc_core::sc_signal_resolved nc_gen_force_state_sync_data_out_dly{"nc_gen_force_state_sync_data_out_dly", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_gen_sync_bus__reset_edge0_bus_width4> target_state_sync_bus;

    void proc_always_271();
    void proc_always_373();
    void proc_always_389();
    void proc_always_404();
    void proc_always_418();
    void proc_always_432();
    void proc_always_449();
    void proc_always_464();
    void proc_always_480();
    void proc_always_490();
    void proc_always_502();
    void proc_always_517();
    void proc_always_534();
    void proc_always_543();
    void proc_always_555();
    void proc_always_567();
    void proc_always_581();
    void proc_always_592();
    void proc_always_605();
    void proc_always_621();
    void proc_always_636();
    void proc_always_647();
    void proc_always_668();
    void proc_always_686();
    void proc_always_705();
};

#endif
