//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_cml2cmos_data
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_CML2CMOS_DATA_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_CML2CMOS_DATA_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_cml2cmos_core.h"

class mipi_4_rx_dphy_ns_DPHYHSRX_cml2cmos_data : public sc_core::sc_module {
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
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_cml2cmos_data);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_cml2cmos_data(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_resolved net1848{"net1848", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net1851{"net1851", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_cml2cmos_core> I433;
};

#endif
