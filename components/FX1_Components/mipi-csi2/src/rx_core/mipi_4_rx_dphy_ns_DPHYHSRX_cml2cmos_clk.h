//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_cml2cmos_clk
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_CML2CMOS_CLK_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_CML2CMOS_CLK_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_cml2cmos_core.h"

class mipi_4_rx_dphy_ns_DPHYHSRX_cml2cmos_clk : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> clk_in_m{"clk_in_m"};
    sc_core::sc_in<sc_dt::sc_logic> clk_in_p{"clk_in_p"};
    sc_core::sc_out<sc_dt::sc_logic> clk_m_cal{"clk_m_cal"};
    sc_core::sc_out<sc_dt::sc_logic> clk_m_cmos{"clk_m_cmos"};
    sc_core::sc_out<sc_dt::sc_logic> clk_p_cal{"clk_p_cal"};
    sc_core::sc_out<sc_dt::sc_logic> clk_p_cmos{"clk_p_cmos"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> ibias{"ibias"};
    sc_core::sc_in<sc_dt::sc_logic> pon{"pon"};
    sc_core::sc_in<sc_dt::sc_logic> ponz{"ponz"};
    sc_core::sc_in<sc_dt::sc_logic> tie_high{"tie_high"};
    sc_core::sc_in<sc_dt::sc_logic> tie_low{"tie_low"};
    sc_core::sc_in<sc_dt::sc_logic> tie_low_cdm{"tie_low_cdm"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    sc_core::sc_in<sc_dt::sc_logic> cal_en{"cal_en"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_cml2cmos_clk);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_cml2cmos_clk(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_resolved net1904{"net1904", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net1894{"net1894", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<2> net1839{"net1839", vl::logic<2>::all_z().lv()};
    sc_core::sc_signal_resolved net1899{"net1899", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved m_cal{"m_cal", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net1886{"net1886", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved p_cal{"p_cal", sc_dt::SC_LOGIC_Z};
    // // Instances of devices/nfinlvt/symbol
    // nfinlvt  N391( .B(gd), .D(gd), .G(gd), .S(gd) );
    // nfinlvt  N434( .B(gd), .D(net1899), .G(cal_en), .S(gd) );
    // nfinlvt  N436( .B(gd), .D(m_cal), .G(net1886), .S(net1899) );
    // nfinlvt  N439( .B(gd), .D(net1904), .G(cal_en), .S(gd) );
    // nfinlvt  N440( .B(gd), .D(p_cal), .G(net1894), .S(net1904) );
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_cml2cmos_core> I433;

    void gate_m_cal_buf01();
    void gate_p_cal_buf01();
    void gate_m_cal_buf02();
    void gate_p_cal_buf02();
};

#endif
