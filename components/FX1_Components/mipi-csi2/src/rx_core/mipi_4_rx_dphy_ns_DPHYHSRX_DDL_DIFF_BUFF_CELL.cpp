//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_DDL_DIFF_BUFF_CELL
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYHSRX_DDL_DIFF_BUFF_CELL.h"

mipi_4_rx_dphy_ns_DPHYHSRX_DDL_DIFF_BUFF_CELL::mipi_4_rx_dphy_ns_DPHYHSRX_DDL_DIFF_BUFF_CELL(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    BUFF_CELLN.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_DDL_BUFF_CELL("BUFF_CELLN"));
    BUFF_CELLN->back(back);
    BUFF_CELLN->bn(bn);
    BUFF_CELLN->bp(bp);
    BUFF_CELLN->forward(forward);
    BUFF_CELLN->in_back(in_back_n);
    BUFF_CELLN->in_forward(in_forward_n);
    BUFF_CELLN->pd(pdn);
    BUFF_CELLN->pu(pun);
    BUFF_CELLN->tie_high(tie_high);
    BUFF_CELLN->tie_low(tie_low);
    BUFF_CELLN->gd(gd);
    BUFF_CELLN->out_back(out_back_n);
    BUFF_CELLN->out_forward(out_forward_n);
    BUFF_CELLN->vp(vp);
    BUFF_CELLP.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_DDL_BUFF_CELL("BUFF_CELLP"));
    BUFF_CELLP->back(back);
    BUFF_CELLP->bn(bn);
    BUFF_CELLP->bp(bp);
    BUFF_CELLP->forward(forward);
    BUFF_CELLP->in_back(in_back_p);
    BUFF_CELLP->in_forward(in_forward_p);
    BUFF_CELLP->pd(pdp);
    BUFF_CELLP->pu(pup);
    BUFF_CELLP->tie_high(tie_high);
    BUFF_CELLP->tie_low(tie_low);
    BUFF_CELLP->gd(gd);
    BUFF_CELLP->out_back(out_back_p);
    BUFF_CELLP->out_forward(out_forward_p);
    BUFF_CELLP->vp(vp);
}
