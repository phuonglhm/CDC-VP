//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_CMOS_DELAY
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYHSRX_CMOS_DELAY.h"

mipi_4_rx_dphy_ns_DPHYHSRX_CMOS_DELAY::mipi_4_rx_dphy_ns_DPHYHSRX_CMOS_DELAY(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    I0.reset(new mipi_4_rx_dphy_ns_sc_invx24_lvt("I0"));
    I0->a(in_p);
    I0->z(a1);
    I0->gd(gd);
    I0->vp(vp);
    I7.reset(new mipi_4_rx_dphy_ns_sc_invx24_lvt("I7"));
    I7->a(in_n);
    I7->z(b1);
    I7->gd(gd);
    I7->vp(vp);
    I21.reset(new mipi_4_rx_dphy_ns_sc_invx16_lvt("I21"));
    I21->a(b2);
    I21->z(b3);
    I21->gd(gd);
    I21->vp(vp);
    I22.reset(new mipi_4_rx_dphy_ns_sc_invx16_lvt("I22"));
    I22->a(a2);
    I22->z(a3);
    I22->gd(gd);
    I22->vp(vp);
    I19.reset(new mipi_4_rx_dphy_ns_sc_invx8_lvt("I19"));
    I19->a(a1);
    I19->z(a2);
    I19->gd(gd);
    I19->vp(vp);
    I20.reset(new mipi_4_rx_dphy_ns_sc_invx8_lvt("I20"));
    I20->a(b1);
    I20->z(b2);
    I20->gd(gd);
    I20->vp(vp);
    I37.reset(new mipi_4_rx_dphy_ns_sc_invx32_lvt("I37"));
    I37->a(a3);
    I37->z(out_p);
    I37->gd(gd);
    I37->vp(vp);
    I38.reset(new mipi_4_rx_dphy_ns_sc_invx32_lvt("I38"));
    I38->a(b3);
    I38->z(out_n);
    I38->gd(gd);
    I38->vp(vp);
}
