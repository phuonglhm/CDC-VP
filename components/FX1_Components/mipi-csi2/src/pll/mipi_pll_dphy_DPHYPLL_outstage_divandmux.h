//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_DPHYPLL_outstage_divandmux
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_PLL_DPHY_DPHYPLL_OUTSTAGE_DIVANDMUX_H
#define VL_MIPI_PLL_DPHY_DPHYPLL_OUTSTAGE_DIVANDMUX_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_pll_dphy_DPHYPLL_outstage_4out.h"
#include "mipi_pll_dphy_DPHYPLL_outstage_divandmux_logic.h"
#include "mipi_pll_dphy_DPHYPLL_quaddiv_2_4_8.h"
#include "mipi_pll_dphy_sc_dlax23_ar_lvt.h"
#include "pll_cells.h"

class mipi_pll_dphy_DPHYPLL_outstage_divandmux : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> clkdcc0{"clkdcc0"};
    sc_core::sc_in<sc_dt::sc_logic> clkdcc180{"clkdcc180"};
    sc_core::sc_in<sc_dt::sc_logic> clkdcc270{"clkdcc270"};
    sc_core::sc_in<sc_dt::sc_logic> clkdcc90{"clkdcc90"};
    sc_core::sc_in<sc_dt::sc_logic> clkext{"clkext"};
    sc_core::sc_in<sc_dt::sc_lv<2>> clksel{"clksel"};
    sc_core::sc_in<sc_dt::sc_logic> clkvco_lf{"clkvco_lf"};
    sc_core::sc_in<sc_dt::sc_logic> onlvz{"onlvz"};
    sc_core::sc_in<sc_dt::sc_lv<2>> p_int{"p_int"};
    sc_core::sc_out<sc_dt::sc_logic> clkout0{"clkout0"};
    sc_core::sc_out<sc_dt::sc_logic> clkout180{"clkout180"};
    sc_core::sc_out<sc_dt::sc_logic> clkout270{"clkout270"};
    sc_core::sc_out<sc_dt::sc_logic> clkout90{"clkout90"};
    sc_core::sc_out<sc_dt::sc_logic> clkout_stop{"clkout_stop"};
    sc_core::sc_in<sc_dt::sc_logic> agnd{"agnd"};
    sc_core::sc_in<sc_dt::sc_logic> avddlv{"avddlv"};

    SC_HAS_PROCESS(mipi_pll_dphy_DPHYPLL_outstage_divandmux);
    explicit mipi_pll_dphy_DPHYPLL_outstage_divandmux(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_vector<sc_core::sc_signal_resolved> clksel_b{"clksel_b", 2, vl::make_z1};
    sc_core::sc_signal_resolved net261{"net261", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved onlv_buf{"onlv_buf", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clkext0{"clkext0", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved mux_off{"mux_off", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clkext1{"clkext1", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved onlvz_buf{"onlvz_buf", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved mux_off_z{"mux_off_z", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<2> clksel_int{"clksel_int", vl::logic<2>::all_z().lv()};
    sc_core::sc_signal_resolved clkext2{"clkext2", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clkext3{"clkext3", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clksel1_del{"clksel1_del", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net355{"net355", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_pll_dphy_sc_mux2x8_lvt> premux;
    // nfinlvt  N123( .B(agnd), .D(agnd), .G(agnd), .S(agnd) );
    // pfinlvt  P124( .B(avddlv), .D(avddlv), .G(avddlv), .S(avddlv) );
    std::unique_ptr<mipi_pll_dphy_DPHYPLL_quaddiv_2_4_8> quaddiv;
    std::unique_ptr<mipi_pll_dphy_sc_dlax23_ar_lvt> I120;
    std::unique_ptr<mipi_pll_dphy_DPHYPLL_outstage_divandmux_logic> logic;
    sc_core::sc_signal_rv<2> c_logic_clksel{"c_logic_clksel", vl::logic<2>::all_z().lv()};
    std::unique_ptr<mipi_pll_dphy_DPHYPLL_outstage_4out> muxglitch;
    std::unique_ptr<mipi_pll_dphy_sc_invx32_lvt> I87;
    std::unique_ptr<mipi_pll_dphy_sc_invx32_lvt> I119;
    std::unique_ptr<mipi_pll_dphy_sc_invx32_lvt> I122;

    void split_clksel();
    vl::logic<2> rd_clksel() const;
    void glue_logic_clksel();
};

#endif
