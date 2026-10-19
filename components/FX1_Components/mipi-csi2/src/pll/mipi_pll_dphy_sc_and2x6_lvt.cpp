//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_sc_and2x6_lvt
//--------------------------------------------------------------------------------------------------
#include "mipi_pll_dphy_sc_and2x6_lvt.h"

mipi_pll_dphy_sc_and2x6_lvt::mipi_pll_dphy_sc_and2x6_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    nand1.reset(new mipi_pll_dphy_sc_na2x6_lvt("nand1"));
    nand1->a(a);
    nand1->b(b);
    nand1->z(no);
    nand1->gd(gd);
    nand1->vp(vp);
    I0.reset(new mipi_pll_dphy_wire_cap("I0"));
    I0->w(no);
    inv.reset(new mipi_pll_dphy_sc_invx8_lvt("inv"));
    inv->a(no);
    inv->z(z);
    inv->gd(gd);
    inv->vp(vp);
}
