//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_mux_ddl_sync
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_mux_ddl_sync.h"

mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_mux_ddl_sync::mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_mux_ddl_sync(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    DATA_BUF.reset(new mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_mux("DATA_BUF"));
    DATA_BUF->clk_long_n(clk_long_n);
    DATA_BUF->clk_long_p(clk_long_p);
    DATA_BUF->clk_short_n(clk_short_n);
    DATA_BUF->clk_short_p(clk_short_p);
    DATA_BUF->en_long(en_long);
    DATA_BUF->on(on);
    DATA_BUF->tie_high(tie_high);
    DATA_BUF->tie_low(tie_low);
    DATA_BUF->clk_out_n(hs_clkn);
    DATA_BUF->clk_out_p(hs_clkp);
    DATA_BUF->gd(gd);
    DATA_BUF->vp(vp);
    PHASE_CHANGE_LOGIC.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_DDL_PHASE_CHANGE_LOGIC("PHASE_CHANGE_LOGIC"));
    PHASE_CHANGE_LOGIC->ddl_phase_change(ddl_phase_change);
    PHASE_CHANGE_LOGIC->hs_clkn(hs_clkp);
    PHASE_CHANGE_LOGIC->hs_clkp(hs_clkn);
    PHASE_CHANGE_LOGIC->reset(reset);
    PHASE_CHANGE_LOGIC->flag_sync(flag_sync);
    PHASE_CHANGE_LOGIC->gate_in_pulse(gate_pulse);
    PHASE_CHANGE_LOGIC->gate_in_pulsez(gate_pulsez);
    PHASE_CHANGE_LOGIC->gate_out_pulse(gate);
    PHASE_CHANGE_LOGIC->gate_out_pulsez(gatez);
    PHASE_CHANGE_LOGIC->gd(gd);
    PHASE_CHANGE_LOGIC->vp(vp);
    GATEN.reset(new mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_mux_buf("GATEN"));
    GATEN->i(hs_clkn);
    GATEN->on(gate_pulsez);
    GATEN->onz(gate_pulse);
    GATEN->o(clk_out_n);
    GATEN->gd(gd);
    GATEN->vp(vp);
    GATEP.reset(new mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_mux_buf("GATEP"));
    GATEP->i(hs_clkp);
    GATEP->on(gate_pulsez);
    GATEP->onz(gate_pulse);
    GATEP->o(clk_out_p);
    GATEP->gd(gd);
    GATEP->vp(vp);
}
