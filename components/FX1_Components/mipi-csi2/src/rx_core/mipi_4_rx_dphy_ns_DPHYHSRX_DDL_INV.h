//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_DDL_INV
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_DDL_INV_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_DDL_INV_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"

class mipi_4_rx_dphy_ns_DPHYHSRX_DDL_INV : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> bn{"bn"};
    sc_core::sc_in<sc_dt::sc_logic> bp{"bp"};
    sc_core::sc_in<sc_dt::sc_logic> i{"i"};
    sc_core::sc_in<sc_dt::sc_logic> on{"on"};
    sc_core::sc_in<sc_dt::sc_logic> onz{"onz"};
    sc_core::sc_in<sc_dt::sc_logic> pd{"pd"};
    sc_core::sc_in<sc_dt::sc_logic> pu{"pu"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_out<sc_dt::sc_logic> o{"o"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_DDL_INV);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_DDL_INV(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000.0;  // time precision (fs)
    sc_core::sc_signal_resolved supply_ok{"supply_ok", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved enable{"enable", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal<sc_dt::sc_logic> o_int{"o_int", vl::logic<1>().sl()};
    sc_core::sc_signal<double, sc_core::SC_UNCHECKED_WRITERS> ddl_delay{"ddl_delay", 0.0};
    bool ddl_delay_forced_ = false;
    sc_core::sc_event gate_ddl_delay_output_ev;
    vl::logic<1> gate_ddl_delay_output_pv = vl::logic<1>::all_z();
    bool gate_ddl_delay_output_ev_pend = false;

    void force_ddl_delay(double v) { ddl_delay_forced_ = true; ddl_delay.write(v); }
    void release_ddl_delay() { ddl_delay_forced_ = false; }
    void assign_supply_ok();
    void assign_enable();
    void gate_ddl_delay_output();
    void gate_ddl_delay_output_upd();
    void proc_always_155031();
    void proc_initial_155045();
};

#endif
