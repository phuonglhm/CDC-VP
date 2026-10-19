//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_DDL_DIFF_BUFF_CELL_REP
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYHSRX_DDL_DIFF_BUFF_CELL_REP.h"

mipi_4_rx_dphy_ns_DPHYHSRX_DDL_DIFF_BUFF_CELL_REP::mipi_4_rx_dphy_ns_DPHYHSRX_DDL_DIFF_BUFF_CELL_REP(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    FEED_BACK_P.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_DDL_INV("FEED_BACK_P"));
    FEED_BACK_P->bn(bn);
    FEED_BACK_P->bp(bp);
    FEED_BACK_P->i(in_p);
    FEED_BACK_P->on(on);
    FEED_BACK_P->onz(onz);
    FEED_BACK_P->pd(onz);
    FEED_BACK_P->pu(tie_high);
    FEED_BACK_P->gd(gd);
    FEED_BACK_P->o(middlep);
    FEED_BACK_P->vp(vp);
    I387.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_DDL_INV("I387"));
    I387->bn(bn);
    I387->bp(bp);
    I387->i(in_n);
    I387->on(on);
    I387->onz(onz);
    I387->pd(tie_low);
    I387->pu(on);
    I387->gd(gd);
    I387->o(middlen);
    I387->vp(vp);
    I402.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_DDL_INV("I402"));
    I402->bn(bn);
    I402->bp(bp);
    I402->i(middlep);
    I402->on(on);
    I402->onz(onz);
    I402->pd(onz);
    I402->pu(tie_high);
    I402->gd(gd);
    I402->o(out_p);
    I402->vp(vp);
    I403.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_DDL_INV("I403"));
    I403->bn(bn);
    I403->bp(bp);
    I403->i(middlen);
    I403->on(on);
    I403->onz(onz);
    I403->pd(tie_low);
    I403->pu(on);
    I403->gd(gd);
    I403->o(out_n);
    I403->vp(vp);
}
