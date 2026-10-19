//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYLPTX
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYLPTX.h"

mipi_4_rx_dphy_ns_DPHYLPTX::mipi_4_rx_dphy_ns_DPHYLPTX(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    DRVOLOOP.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_oloop("DRVOLOOP"));
    DRVOLOOP->atb_line(atb_line);
    DRVOLOOP->atb_lptx1200_on(atb_lptx1200_on);
    DRVOLOOP->din(din);
    DRVOLOOP->ena(ena);
    DRVOLOOP->eniocont(eniocont);
    DRVOLOOP->enlogic(enlogic);
    DRVOLOOP->enpulldwn(enpulldwn);
    DRVOLOOP->enulp(enulp);
    DRVOLOOP->ibias(ibias);
    DRVOLOOP->loopinn(net112);
    DRVOLOOP->loopinp(net111);
    DRVOLOOP->pon(pon);
    DRVOLOOP->proghv(proghv);
    DRVOLOOP->setrhv(setrhv);
    DRVOLOOP->sr_bypass(sr_bypass);
    DRVOLOOP->dn(dn);
    DRVOLOOP->dp(dp);
    DRVOLOOP->iocont(iocont);
    DRVOLOOP->loopoutn(net112);
    DRVOLOOP->loopoutp(net111);
    DRVOLOOP->gd(gd);
    DRVOLOOP->vdd(vdd);
    DRVOLOOP->vp(vp);
    DRVOLOOP->vph(vph);
    DRVOLOOP->vreg(vreg);
    DRVOLOOP->vss(vss);
}
