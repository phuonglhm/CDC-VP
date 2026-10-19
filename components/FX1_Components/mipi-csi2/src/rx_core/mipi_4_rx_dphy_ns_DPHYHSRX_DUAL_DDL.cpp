//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_DUAL_DDL
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYHSRX_DUAL_DDL.h"

mipi_4_rx_dphy_ns_DPHYHSRX_DUAL_DDL::mipi_4_rx_dphy_ns_DPHYHSRX_DUAL_DDL(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    DDL_CLK.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_DDL("DDL_CLK"));
    DDL_CLK->atb_line(atb_line);
    DDL_CLK->atb_sw_hv(atb_sw_hv);
    DDL_CLK->bn(bn);
    DDL_CLK->bp(bp);
    DDL_CLK->clk_inn(clk_inn);
    DDL_CLK->clk_inp(clk_inp);
    DDL_CLK->clk_outn(clk_outn);
    DDL_CLK->clk_outp(clk_outp);
    DDL_CLK->ddl_clk_outn(ddl_clk_outn);
    DDL_CLK->ddl_clk_outp(ddl_clk_outp);
    DDL_CLK->gd(gd);
    DDL_CLK->iref(iref);
    DDL_CLK->on(ddl_clk_on);
    DDL_CLK->onz(ddl_clk_onz);
    DDL_CLK->phase(phase_clk);
    DDL_CLK->phase_sync(phase_sync);
    DDL_CLK->tune(tune);
    DDL_CLK->tune_en(tune_en);
    DDL_CLK->vp(vp);
    DDL_CLK->vph(vph);
    DDL_CLK->gate(gate);
    DDL_CLK->on_hv(ddl_clk_on_hv);
    DDL_CLK->gatez(gatez);
    DDL_DATA.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_DDL_4bit("DDL_DATA"));
    DDL_DATA->bn(bn);
    DDL_DATA->bp(bp);
    DDL_DATA->clk_inn(data_inn);
    DDL_DATA->clk_inp(data_inp);
    DDL_DATA->clk_outn(data_outn);
    DDL_DATA->clk_outp(data_outp);
    DDL_DATA->ddl_clk_outn(net120);
    DDL_DATA->ddl_clk_outp(net119);
    DDL_DATA->gd(gd);
    DDL_DATA->on(ddl_data_on);
    DDL_DATA->onz(ddl_data_onz);
    DDL_DATA->phase(phase_data);
    DDL_DATA->phase_sync(phase_sync);
    DDL_DATA->vp(vp);
    SC_METHOD(proc_always_156553);
    sensitive << DDL_CLK->ddl_delay;
}

void mipi_4_rx_dphy_ns_DPHYHSRX_DUAL_DDL::proc_always_156553() {
    DDL_DATA->force_ddl_delay(DDL_CLK->ddl_delay.read());
}
