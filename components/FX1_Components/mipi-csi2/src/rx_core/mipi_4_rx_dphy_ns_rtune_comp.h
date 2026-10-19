//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_rtune_comp
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_RTUNE_COMP_H
#define VL_MIPI_4_RX_DPHY_NS_RTUNE_COMP_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"

class mipi_4_rx_dphy_ns_rtune_comp : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> r_h{"r_h"};
    sc_core::sc_in<sc_dt::sc_logic> rt_pwron_h{"rt_pwron_h"};
    sc_core::sc_in<sc_dt::sc_lv<10>> vin{"vin"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};
    sc_core::sc_out<sc_dt::sc_logic> comp_result_h{"comp_result_h"};
    sc_core::sc_out<sc_dt::sc_logic> nbias{"nbias"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_rtune_comp);
    explicit mipi_4_rx_dphy_ns_rtune_comp(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal<sc_dt::sc_lv<10>> latch_reg{"latch_reg", vl::logic<10>().lv()};
    sc_core::sc_signal<sc_dt::sc_logic, sc_core::SC_UNCHECKED_WRITERS> latch_enable{"latch_enable", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_logic, sc_core::SC_UNCHECKED_WRITERS> comp_result_h_int{"comp_result_h_int", vl::logic<1>().sl()};
    sc_core::sc_signal_resolved supply_ok{"supply_ok", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved rt_pwron_h_dly{"rt_pwron_h_dly", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<10> ref_dac{"ref_dac", vl::logic<10>::all_z().lv()};
    sc_core::sc_signal_resolved disable_message{"disable_message", sc_dt::SC_LOGIC_Z};
    double t1 = 0.0;
    double t2 = 0.0;
    sc_core::sc_event assign_rt_pwron_h_dly_ev;
    vl::logic<1> assign_rt_pwron_h_dly_pv = vl::logic<1>::all_z();
    bool assign_rt_pwron_h_dly_ev_pend = false;

    void assign_ref_dac();
    void assign_supply_ok();
    void assign_rt_pwron_h_dly();
    void assign_rt_pwron_h_dly_upd();
    void assign_disable_message();
    void gate_buf_out();
    void proc_initial_161362();
    void proc_always_161367();
    void proc_always_161410();
    void proc_always_161432();
    void proc_always_161459();
};

#endif
