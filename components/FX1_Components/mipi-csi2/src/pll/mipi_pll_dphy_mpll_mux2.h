//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_mpll_mux2
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_PLL_DPHY_MPLL_MUX2_H
#define VL_MIPI_PLL_DPHY_MPLL_MUX2_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "pll_cells.h"

class mipi_pll_dphy_mpll_mux2 : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_lv<4>> in{"in"};
    sc_core::sc_in<sc_dt::sc_logic> pwr_off{"pwr_off"};
    sc_core::sc_in<sc_dt::sc_lv<4>> s{"s"};
    sc_core::sc_out<sc_dt::sc_logic> out{"out"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};

    SC_HAS_PROCESS(mipi_pll_dphy_mpll_mux2);
    explicit mipi_pll_dphy_mpll_mux2(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_vector<sc_core::sc_signal_resolved> in_b{"in_b", 4, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> s_b{"s_b", 4, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> si{"si", 4, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> siz{"siz", 4, vl::make_z1};
    sc_core::sc_signal_resolved outb{"outb", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net214{"net214", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net208{"net208", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net206{"net206", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net204{"net204", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net202{"net202", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_pll_dphy_wire_cap> I13;
    std::unique_ptr<mipi_pll_dphy_wire_cap> I12;
    std::unique_ptr<mipi_pll_dphy_wire_cap> I11;
    std::unique_ptr<mipi_pll_dphy_wire_cap> I14;
    std::unique_ptr<mipi_pll_dphy_wire_cap> wire_cap2;
    std::unique_ptr<mipi_pll_dphy_wire_cap> I17;
    std::unique_ptr<mipi_pll_dphy_wire_cap> I15;
    std::unique_ptr<mipi_pll_dphy_wire_cap> I16;
    std::unique_ptr<mipi_pll_dphy_sc_invx16_lvt> I31;
    std::unique_ptr<mipi_pll_dphy_sc_invx16_lvt> I32;
    std::unique_ptr<mipi_pll_dphy_sc_no2x12_lvt> I29;
    std::unique_ptr<mipi_pll_dphy_sc_no2x12_lvt> I28;
    std::unique_ptr<mipi_pll_dphy_sc_no2x12_lvt> I25;
    std::unique_ptr<mipi_pll_dphy_sc_no2x12_lvt> I23;
    std::unique_ptr<mipi_pll_dphy_sc_invx8_lvt> I27;
    std::unique_ptr<mipi_pll_dphy_sc_invx8_lvt> I26;
    std::unique_ptr<mipi_pll_dphy_sc_invx8_lvt> I24;
    std::unique_ptr<mipi_pll_dphy_sc_invx8_lvt> I21;
    std::unique_ptr<mipi_pll_dphy_sc_invx8_lvt> inv7;
    std::unique_ptr<mipi_pll_dphy_sc_invx8_lvt> I9;
    std::unique_ptr<mipi_pll_dphy_sc_invx8_lvt> I8;
    std::unique_ptr<mipi_pll_dphy_sc_invx8_lvt> I7;

    void split_in();
    void split_s();
    vl::logic<4> rd_in() const;
    vl::logic<4> rd_s() const;
    vl::logic<4> rd_si() const;
    vl::logic<4> rd_siz() const;
    void gate_buf_si0();
    void gate_buf_si1();
    void gate_buf_si2();
    void gate_buf_si3();
    void gate_buf_power_off();
};

#endif
