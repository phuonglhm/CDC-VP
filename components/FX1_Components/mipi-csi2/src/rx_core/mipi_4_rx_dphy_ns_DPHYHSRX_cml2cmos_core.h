//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_cml2cmos_core
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_CML2CMOS_CORE_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_CML2CMOS_CORE_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"

class mipi_4_rx_dphy_ns_DPHYHSRX_cml2cmos_core : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> clk_in_m{"clk_in_m"};
    sc_core::sc_in<sc_dt::sc_logic> clk_in_p{"clk_in_p"};
    sc_core::sc_out<sc_dt::sc_logic> clk_m_cmos{"clk_m_cmos"};
    sc_core::sc_out<sc_dt::sc_logic> clk_p_cmos{"clk_p_cmos"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> ibias{"ibias"};
    sc_core::sc_in<sc_dt::sc_logic> pon{"pon"};
    sc_core::sc_in<sc_dt::sc_logic> ponz{"ponz"};
    sc_core::sc_in<sc_dt::sc_logic> tie_high{"tie_high"};
    sc_core::sc_in<sc_dt::sc_logic> tie_low{"tie_low"};
    sc_core::sc_in<sc_dt::sc_logic> tie_low_cdm{"tie_low_cdm"};
    sc_core::sc_out<sc_dt::sc_logic> to_buff_calm{"to_buff_calm"};
    sc_core::sc_out<sc_dt::sc_logic> to_buff_calp{"to_buff_calp"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_cml2cmos_core);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_cml2cmos_core(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal<sc_dt::sc_logic> ibias_cgm{"ibias_cgm", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_logic> b3{"b3", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_logic> a3{"a3", vl::logic<1>().sl()};
    sc_core::sc_signal_resolved supply_ok{"supply_ok", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved enable{"enable", sc_dt::SC_LOGIC_Z};

    void assign_supply_ok();
    void assign_enable();
    void gate_to_buff_calm_buf();
    void gate_to_buff_calp_buf();
    void gate_clk_m_cmos_buf();
    void gate_clk_p_cmos_buf();
    void proc_always_153741();
    void proc_always_153752();
    void proc_always_153769();
};

#endif
