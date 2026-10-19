//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_io
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_io.h"

mipi_4_rx_io::mipi_4_rx_io(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    IOPADS.reset(new mipi_4_rx_io_DPHY3RAFE4L_bump("IOPADS"));
    IOPADS->ATB(atb);
    IOPADS->REXT(rext);
    IOPADS->dn0(datan0);
    IOPADS->dn1(datan1);
    IOPADS->dn2(datan2);
    IOPADS->dn3(datan3);
    IOPADS->dnclk(clkn);
    IOPADS->dp0(datap0);
    IOPADS->dp1(datap1);
    IOPADS->dp2(datap2);
    IOPADS->dp3(datap3);
    IOPADS->dpclk(clkp);
    IOPADS->gd(gd);
    IOPADS->vp(vp);
    IOPADS->vph(vph);
}
