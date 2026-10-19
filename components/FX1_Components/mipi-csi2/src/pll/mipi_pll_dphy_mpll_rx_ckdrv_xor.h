//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_mpll_rx_ckdrv_xor
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_PLL_DPHY_MPLL_RX_CKDRV_XOR_H
#define VL_MIPI_PLL_DPHY_MPLL_RX_CKDRV_XOR_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "pll_cells.h"

class mipi_pll_dphy_mpll_rx_ckdrv_xor : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> clk_i_m{"clk_i_m"};
    sc_core::sc_in<sc_dt::sc_logic> clk_i_p{"clk_i_p"};
    sc_core::sc_in<sc_dt::sc_logic> clk_q_m{"clk_q_m"};
    sc_core::sc_in<sc_dt::sc_logic> clk_q_p{"clk_q_p"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    sc_core::sc_out<sc_dt::sc_logic> xor_out{"xor_out"};

    SC_HAS_PROCESS(mipi_pll_dphy_mpll_rx_ckdrv_xor);
    explicit mipi_pll_dphy_mpll_rx_ckdrv_xor(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_resolved xorp{"xorp", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved xr{"xr", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved xorm{"xorm", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_pll_dphy_mpll_rx_ckdrv_dccres> xor_r;
    std::unique_ptr<mipi_pll_dphy_wire_cap> I47;
    std::unique_ptr<mipi_pll_dphy_wire_cap> I48;
    std::unique_ptr<mipi_pll_dphy_wire_cap> I49;
    std::unique_ptr<mipi_pll_dphy_mpll_rx_ckdrv_na2x31> nam_2;
    std::unique_ptr<mipi_pll_dphy_mpll_rx_ckdrv_na2x31> nam_1;
    std::unique_ptr<mipi_pll_dphy_mpll_rx_ckdrv_na2x31> nam_0;
    std::unique_ptr<mipi_pll_dphy_mpll_rx_ckdrv_na2x31> nap_2;
    std::unique_ptr<mipi_pll_dphy_mpll_rx_ckdrv_na2x31> nap_1;
    std::unique_ptr<mipi_pll_dphy_mpll_rx_ckdrv_na2x31> nap_0;
    std::unique_ptr<mipi_pll_dphy_mpll_rx_ckdrv_na2x31_thresh> napm;
};

#endif
