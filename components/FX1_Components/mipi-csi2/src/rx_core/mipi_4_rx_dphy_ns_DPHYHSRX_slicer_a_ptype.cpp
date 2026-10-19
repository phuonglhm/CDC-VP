//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_slicer_a_ptype
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYHSRX_slicer_a_ptype.h"

mipi_4_rx_dphy_ns_DPHYHSRX_slicer_a_ptype::mipi_4_rx_dphy_ns_DPHYHSRX_slicer_a_ptype(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    stg1.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_slicer_comp1a_ptype("stg1"));
    stg1->clk(phi1_gated);
    stg1->gd(gd);
    stg1->vi_m(vi_m);
    stg1->vi_p(vi_p);
    stg1->vp(vp);
    stg1->vom(v1_m);
    stg1->vop(v1_p);
    stg3.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_slicer_latch_ptype_nand("stg3"));
    stg3->clk(phi1_gated);
    stg3->gd(gd);
    stg3->vi_m(v1_m);
    stg3->vi_p(v1_p);
    stg3->vp(vp);
    stg3->q(q);
    // wire_cap  I23( .w(phi1_gated) );
    // wire_cap  I27( .w(v1_m) );
    // wire_cap  I28( .w(v1_p) );
    // wire_cap  I29( .w(vi_p) );
    // wire_cap  I30( .w(vi_m) );
    I38.reset(new mipi_4_rx_dphy_ns_sc_na2x24_lvt("I38"));
    I38->a(phi1);
    I38->b(en);
    I38->z(phi1_gated);
    I38->gd(gd);
    I38->vp(vp);
}
