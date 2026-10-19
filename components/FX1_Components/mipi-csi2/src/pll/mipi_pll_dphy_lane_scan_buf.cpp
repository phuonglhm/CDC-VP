//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_lane_scan_buf
//--------------------------------------------------------------------------------------------------
#include "mipi_pll_dphy_lane_scan_buf.h"

mipi_pll_dphy_lane_scan_buf::mipi_pll_dphy_lane_scan_buf(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    I5.reset(new mipi_pll_dphy_sc_invx16_lvt("I5"));
    I5->a(a);
    I5->z(net37);
    I5->gd(gd);
    I5->vp(vp);
    I6.reset(new mipi_pll_dphy_sc_invx48_lvt("I6"));
    I6->a(net37);
    I6->z(z);
    I6->gd(gd);
    I6->vp(vp);
}
