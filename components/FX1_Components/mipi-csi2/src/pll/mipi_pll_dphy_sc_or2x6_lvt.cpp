//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_sc_or2x6_lvt
//--------------------------------------------------------------------------------------------------
#include "mipi_pll_dphy_sc_or2x6_lvt.h"

mipi_pll_dphy_sc_or2x6_lvt::mipi_pll_dphy_sc_or2x6_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    inv.reset(new mipi_pll_dphy_sc_invx8_lvt("inv"));
    inv->a(no);
    inv->z(z);
    inv->gd(gd);
    inv->vp(vp);
    I0.reset(new mipi_pll_dphy_wire_cap("I0"));
    I0->w(no);
    nor1.reset(new mipi_pll_dphy_sc_no2x6_lvt("nor1"));
    nor1->a(a);
    nor1->b(b);
    nor1->z(no);
    nor1->gd(gd);
    nor1->vp(vp);
}
