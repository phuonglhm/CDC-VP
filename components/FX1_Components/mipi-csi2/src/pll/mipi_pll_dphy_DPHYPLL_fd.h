//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_DPHYPLL_fd
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_PLL_DPHY_DPHYPLL_FD_H
#define VL_MIPI_PLL_DPHY_DPHYPLL_FD_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_pll_dphy_DPHYPLL_sc_syncx10_ar_lvt_speedup.h"
#include "mipi_pll_dphy_sc_dlax23_ar_lvt.h"
#include "mipi_pll_dphy_sc_syncx10_as_lvt.h"
#include "mipi_pll_dphy_sc_tie_hilow_lvt.h"
#include "pll_cells.h"

class mipi_pll_dphy_DPHYPLL_fd : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> fbk_mux{"fbk_mux"};
    sc_core::sc_in<sc_dt::sc_logic> hs_mux{"hs_mux"};
    sc_core::sc_in<sc_dt::sc_logic> m0{"m0"};
    sc_core::sc_in<sc_dt::sc_logic> tie_low_cdm{"tie_low_cdm"};
    sc_core::sc_out<sc_dt::sc_logic> hs_div2{"hs_div2"};
    sc_core::sc_in<sc_dt::sc_logic> dgnd{"dgnd"};
    sc_core::sc_in<sc_dt::sc_logic> dvdd{"dvdd"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vpl{"vpl"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};

    SC_HAS_PROCESS(mipi_pll_dphy_DPHYPLL_fd);
    explicit mipi_pll_dphy_DPHYPLL_fd(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_resolved net144{"net144", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved trigger_dump{"trigger_dump", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved tie_high{"tie_high", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved resyn{"resyn", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net307{"net307", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved m0ff{"m0ff", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved hs_div2b{"hs_div2b", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net281{"net281", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved hs_mux_p1{"hs_mux_p1", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved fbk_mux_syn{"fbk_mux_syn", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved hs_mux_p2{"hs_mux_p2", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved fbk_mux_deglitch{"fbk_mux_deglitch", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net234{"net234", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved pg_off{"pg_off", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net236{"net236", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved tie_low{"tie_low", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved pass_m0{"pass_m0", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net294{"net294", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved hs_clk_int{"hs_clk_int", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net305{"net305", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net234_initial{"net234_initial", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved m0ff_initial{"m0ff_initial", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved pass_m0_initial{"pass_m0_initial", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net236_out{"net236_out", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved supply_ok{"supply_ok", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_pll_dphy_DPHYPLL_fd_pg> pg;
    std::unique_ptr<mipi_pll_dphy_DPHYPLL_sc_syncx10_ar_lvt_speedup> I79;
    std::unique_ptr<mipi_pll_dphy_DPHYPLL_sc_syncx10_ar_lvt_speedup> I80;
    std::unique_ptr<mipi_pll_dphy_DPHYPLL_fd_pg_big> pgbig;
    std::unique_ptr<mipi_pll_dphy_sc_syncx10_as_lvt> I76;
    std::unique_ptr<mipi_pll_dphy_sc_syncx10_as_lvt> I78;
    std::unique_ptr<mipi_pll_dphy_sc_syncx10_as_lvt> I81;
    std::unique_ptr<mipi_pll_dphy_sc_invx32_lvt> INV_CDM;
    std::unique_ptr<mipi_pll_dphy_sc_na2x24_lvt> I38;
    std::unique_ptr<mipi_pll_dphy_sc_na2x24_lvt> I39;
    std::unique_ptr<mipi_pll_dphy_sc_no2x12_lvt> CDM_OUT_3;
    std::unique_ptr<mipi_pll_dphy_sc_no2x12_lvt> CDM_OUT_2;
    std::unique_ptr<mipi_pll_dphy_sc_no2x12_lvt> CDM_OUT_1;
    std::unique_ptr<mipi_pll_dphy_sc_no2x12_lvt> CDM_OUT_0;
    std::unique_ptr<mipi_pll_dphy_sc_invx16_lvt> I44;
    std::unique_ptr<mipi_pll_dphy_sc_invx16_lvt> I48;
    std::unique_ptr<mipi_pll_dphy_sc_invx16_lvt> I58;
    std::unique_ptr<mipi_pll_dphy_sc_invx16_lvt> I61;
    std::unique_ptr<mipi_pll_dphy_sc_invx16_lvt> I71;
    std::unique_ptr<mipi_pll_dphy_sc_invx16_lvt> I73;
    std::unique_ptr<mipi_pll_dphy_sc_tie_hilow_lvt> I53;
    std::unique_ptr<mipi_pll_dphy_sc_dlax23_ar_lvt> I69;

    void assign_supply_ok();
    void assign_net234_initial();
    void assign_m0ff();
    void assign_pass_m0_initial();
    void assign_net236();
    void gate_buf_m0ff();
};

#endif
