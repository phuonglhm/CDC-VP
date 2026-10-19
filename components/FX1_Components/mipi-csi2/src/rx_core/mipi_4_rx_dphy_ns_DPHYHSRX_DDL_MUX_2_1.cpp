//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_DDL_MUX_2_1
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYHSRX_DDL_MUX_2_1.h"

mipi_4_rx_dphy_ns_DPHYHSRX_DDL_MUX_2_1::mipi_4_rx_dphy_ns_DPHYHSRX_DDL_MUX_2_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    I8.reset(new mipi_4_rx_dphy_ns_sc_invx4_lvt("I8"));
    I8->a(tune_en);
    I8->z(tune_enz);
    I8->gd(gd);
    I8->vp(vp);
    I4.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_DDL_MUX_2_1_inv_en("I4"));
    I4->i(ddl_clk);
    I4->on(tune_en);
    I4->onz(tune_enz);
    I4->gd(gd);
    I4->o(z);
    I4->vp(vp);
    I46.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_DDL_MUX_2_1_inv_en("I46"));
    I46->i(clk_in);
    I46->on(tune_enz);
    I46->onz(tune_en);
    I46->gd(gd);
    I46->o(z);
    I46->vp(vp);
}
