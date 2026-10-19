//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_cml2cmos_data
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYHSRX_cml2cmos_data.h"

mipi_4_rx_dphy_ns_DPHYHSRX_cml2cmos_data::mipi_4_rx_dphy_ns_DPHYHSRX_cml2cmos_data(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    I433.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_cml2cmos_core("I433"));
    I433->clk_in_m(clk_in_m);
    I433->clk_in_p(clk_in_p);
    I433->clk_m_cmos(clk_m_cmos);
    I433->clk_p_cmos(clk_p_cmos);
    I433->gd(gd);
    I433->ibias(ibias);
    I433->pon(pon);
    I433->ponz(ponz);
    I433->tie_high(tie_high);
    I433->tie_low(tie_low);
    I433->tie_low_cdm(tie_low_cdm);
    I433->to_buff_calm(net1851);
    I433->to_buff_calp(net1848);
    I433->vp(vp);
}
