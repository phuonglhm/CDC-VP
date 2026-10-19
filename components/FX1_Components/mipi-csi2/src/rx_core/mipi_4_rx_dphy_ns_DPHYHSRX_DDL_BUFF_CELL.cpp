//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_DDL_BUFF_CELL
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYHSRX_DDL_BUFF_CELL.h"

mipi_4_rx_dphy_ns_DPHYHSRX_DDL_BUFF_CELL::mipi_4_rx_dphy_ns_DPHYHSRX_DDL_BUFF_CELL(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    INV_BACK.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_DDL_INV("INV_BACK"));
    INV_BACK->bn(bn);
    INV_BACK->bp(bp);
    INV_BACK->i(in_back);
    INV_BACK->on(forward);
    INV_BACK->onz(back);
    INV_BACK->pd(tie_low);
    INV_BACK->pu(tie_high);
    INV_BACK->gd(gd);
    INV_BACK->o(out_back);
    INV_BACK->vp(vp);
    INV_FORWARD.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_DDL_INV("INV_FORWARD"));
    INV_FORWARD->bn(bn);
    INV_FORWARD->bp(bp);
    INV_FORWARD->i(in_forward);
    INV_FORWARD->on(forward);
    INV_FORWARD->onz(back);
    INV_FORWARD->pd(pd);
    INV_FORWARD->pu(pu);
    INV_FORWARD->gd(gd);
    INV_FORWARD->o(out_forward);
    INV_FORWARD->vp(vp);
    INV_TRISTATE.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_DDL_inv_en("INV_TRISTATE"));
    INV_TRISTATE->i(in_forward);
    INV_TRISTATE->on(back);
    INV_TRISTATE->onz(forward);
    INV_TRISTATE->gd(gd);
    INV_TRISTATE->o(out_back);
    INV_TRISTATE->vp(vp);
}
