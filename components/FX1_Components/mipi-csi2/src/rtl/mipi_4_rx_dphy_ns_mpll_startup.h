//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_mpll_startup
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_MPLL_STARTUP_H
#define VL_MIPI_4_RX_DPHY_NS_MPLL_STARTUP_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_4_rx_dphy_ns_gen_sync__reset_edge0.h"
#include "mipi_4_rx_dphy_ns_gen_sync_bus__reset_edge0_bus_width4.h"

// Parameters: IDLE=0, PLLEN=1, PLLRST=2, PLLRST_EXIT=3, PLLGSHIFT=4, PLLGSHIFT_EXIT=5, DET_LOCK_EN=6, DET_LOCK_EN_EXIT=7, FSM_DONE=8, COUNT_SIZE=9
class mipi_4_rx_dphy_ns_mpll_startup : public sc_core::sc_module {
public:
    // -- INPUT AND OUTPUT PORTS  --------------------------------------------------
    sc_core::sc_in<sc_dt::sc_logic> rstz{"rstz"};
    sc_core::sc_in<sc_dt::sc_logic> cfgclk{"cfgclk"};
    sc_core::sc_in<sc_dt::sc_logic> onpll{"onpll"};
    sc_core::sc_in<sc_dt::sc_logic> force_state{"force_state"};
    // system reset
    sc_core::sc_in<sc_dt::sc_lv<4>> main_fsm_state{"main_fsm_state"};
    // to override the PLL machine
    sc_core::sc_in<sc_dt::sc_lv<9>> mpll_rst_time{"mpll_rst_time"};
    sc_core::sc_in<sc_dt::sc_lv<9>> mpll_gear_shift_time{"mpll_gear_shift_time"};
    sc_core::sc_in<sc_dt::sc_lv<9>> mpll_lock_det_time{"mpll_lock_det_time"};
    // outputs
    sc_core::sc_out<sc_dt::sc_logic> mpll_pwron{"mpll_pwron"};
    sc_core::sc_out<sc_dt::sc_logic> mpll_reset{"mpll_reset"};
    sc_core::sc_out<sc_dt::sc_logic> mpll_gear_shift{"mpll_gear_shift"};
    sc_core::sc_out<sc_dt::sc_logic> lock_det_on{"lock_det_on"};
    sc_core::sc_out<sc_dt::sc_logic> fsm_done{"fsm_done"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_mpll_startup);
    explicit mipi_4_rx_dphy_ns_mpll_startup(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000.0;  // time precision (fs)
    // -- WIRES DECLARATION --------------------------------------------------------
    sc_core::sc_signal<sc_dt::sc_lv<4>> main_state{"main_state", vl::logic<4>().lv()};
    sc_core::sc_signal<sc_dt::sc_logic> change_state{"change_state", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_lv<9>> target{"target", vl::logic<9>().lv()};
    sc_core::sc_signal<sc_dt::sc_logic> counter_run{"counter_run", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_lv<9>> counter{"counter", vl::logic<9>().lv()};
    sc_core::sc_signal_resolved force_state_sync{"force_state_sync", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<4> main_fsm_state_int{"main_fsm_state_int", vl::logic<4>::all_z().lv()};
    sc_core::sc_signal_resolved onpll_sync{"onpll_sync", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clk_scanable{"clk_scanable", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved rstz_scanable{"rstz_scanable", sc_dt::SC_LOGIC_Z};
    // synchronization ( assumes onPLL is completly asynchronous of cfgclk)
    std::unique_ptr<mipi_4_rx_dphy_ns_gen_sync__reset_edge0> gen_onpll_sync;
    sc_core::sc_signal_resolved tie0_{"tie0_", sc_dt::SC_LOGIC_0};
    vl::tie tie0__drv{"tie0__drv", sc_dt::SC_LOGIC_0};
    sc_core::sc_signal_resolved nc_gen_onpll_sync_data_out_dly{"nc_gen_onpll_sync_data_out_dly", sc_dt::SC_LOGIC_Z};
    // Data coming from  testability (other clock domain : ensure good bus synchonization)
    std::unique_ptr<mipi_4_rx_dphy_ns_gen_sync__reset_edge0> sync_force_state_sync;
    sc_core::sc_signal_resolved nc_sync_force_state_sync_data_out_dly{"nc_sync_force_state_sync_data_out_dly", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_gen_sync_bus__reset_edge0_bus_width4> main_fsm_sync_bus;

    void assign_clk_scanable();
    void assign_rstz_scanable();
    void proc_always_135();
    void proc_always_197();
    void proc_always_225();
    void proc_always_238();
    void proc_always_249();
    void proc_always_257();
    void proc_always_267();
    void proc_always_280();
    void proc_always_289();
};

#endif
