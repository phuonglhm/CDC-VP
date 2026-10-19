//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_mux_ddl_sync
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYRXCLK_CLL_MUX_DDL_SYNC_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYRXCLK_CLL_MUX_DDL_SYNC_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_DDL_PHASE_CHANGE_LOGIC.h"
#include "mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_mux.h"
#include "rx_cells.h"

class mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_mux_ddl_sync : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> clk_long_n{"clk_long_n"};
    sc_core::sc_in<sc_dt::sc_logic> clk_long_p{"clk_long_p"};
    sc_core::sc_in<sc_dt::sc_logic> clk_short_n{"clk_short_n"};
    sc_core::sc_in<sc_dt::sc_logic> clk_short_p{"clk_short_p"};
    sc_core::sc_in<sc_dt::sc_logic> ddl_phase_change{"ddl_phase_change"};
    sc_core::sc_in<sc_dt::sc_logic> en_long{"en_long"};
    sc_core::sc_in<sc_dt::sc_logic> on{"on"};
    sc_core::sc_in<sc_dt::sc_logic> reset{"reset"};
    sc_core::sc_in<sc_dt::sc_logic> tie_high{"tie_high"};
    sc_core::sc_in<sc_dt::sc_logic> tie_low{"tie_low"};
    sc_core::sc_out<sc_dt::sc_logic> clk_out_n{"clk_out_n"};
    sc_core::sc_out<sc_dt::sc_logic> clk_out_p{"clk_out_p"};
    sc_core::sc_out<sc_dt::sc_logic> flag_sync{"flag_sync"};
    sc_core::sc_out<sc_dt::sc_logic> gate{"gate"};
    sc_core::sc_out<sc_dt::sc_logic> gatez{"gatez"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_mux_ddl_sync);
    explicit mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_mux_ddl_sync(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_resolved gate_pulse{"gate_pulse", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved hs_clkn{"hs_clkn", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved gate_pulsez{"gate_pulsez", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved hs_clkp{"hs_clkp", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_mux> DATA_BUF;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_DDL_PHASE_CHANGE_LOGIC> PHASE_CHANGE_LOGIC;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_mux_buf> GATEN;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_mux_buf> GATEP;
};

#endif
