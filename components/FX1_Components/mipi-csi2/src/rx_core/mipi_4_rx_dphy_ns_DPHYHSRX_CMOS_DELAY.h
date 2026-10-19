//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_CMOS_DELAY
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_CMOS_DELAY_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_CMOS_DELAY_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "rx_cells.h"

class mipi_4_rx_dphy_ns_DPHYHSRX_CMOS_DELAY : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> in_n{"in_n"};
    sc_core::sc_in<sc_dt::sc_logic> in_p{"in_p"};
    sc_core::sc_in<sc_dt::sc_logic> pon{"pon"};
    sc_core::sc_in<sc_dt::sc_logic> ponz{"ponz"};
    sc_core::sc_out<sc_dt::sc_logic> out_n{"out_n"};
    sc_core::sc_out<sc_dt::sc_logic> out_p{"out_p"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_CMOS_DELAY);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_CMOS_DELAY(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_resolved b1{"b1", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved b2{"b2", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved b3{"b3", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved a1{"a1", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved a2{"a2", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved a3{"a3", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx24_lvt> I0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx24_lvt> I7;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx16_lvt> I21;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx16_lvt> I22;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx8_lvt> I19;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx8_lvt> I20;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx32_lvt> I37;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx32_lvt> I38;
};

#endif
