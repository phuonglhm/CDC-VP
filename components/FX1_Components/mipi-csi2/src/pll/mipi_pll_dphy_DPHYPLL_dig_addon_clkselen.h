//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_DPHYPLL_dig_addon_clkselen
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_PLL_DPHY_DPHYPLL_DIG_ADDON_CLKSELEN_H
#define VL_MIPI_PLL_DPHY_DPHYPLL_DIG_ADDON_CLKSELEN_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "pll_cells.h"

class mipi_pll_dphy_DPHYPLL_dig_addon_clkselen : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> clksel_en_avdd{"clksel_en_avdd"};
    sc_core::sc_in<sc_dt::sc_lv<2>> clksel_int_avdd{"clksel_int_avdd"};
    sc_core::sc_in<sc_dt::sc_logic> onpll_avdd{"onpll_avdd"};
    sc_core::sc_out<sc_dt::sc_lv<2>> clksel_int_output{"clksel_int_output"};
    sc_core::sc_out<sc_dt::sc_logic> onpllz_avdd{"onpllz_avdd"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vpl{"vpl"};

    SC_HAS_PROCESS(mipi_pll_dphy_DPHYPLL_dig_addon_clkselen);
    explicit mipi_pll_dphy_DPHYPLL_dig_addon_clkselen(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_vector<sc_core::sc_signal_resolved> clksel_int_avdd_b{"clksel_int_avdd_b", 2, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> clksel_int_output_b{"clksel_int_output_b", 2, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> net2{"net2", 2, vl::make_z1};
    std::unique_ptr<mipi_pll_dphy_sc_na2x6_lvt> I99_1;
    std::unique_ptr<mipi_pll_dphy_sc_na2x6_lvt> I99_0;
    std::unique_ptr<mipi_pll_dphy_sc_invx12_lvt> I100_1;
    std::unique_ptr<mipi_pll_dphy_sc_invx12_lvt> I100_0;
    std::unique_ptr<mipi_pll_dphy_sc_invx24_lvt> I116;

    void split_clksel_int_avdd();
    void merge_clksel_int_output();
    vl::logic<2> rd_clksel_int_avdd() const;
    vl::logic<2> rd_clksel_int_output() const;
    vl::logic<2> rd_net2() const;
};

#endif
