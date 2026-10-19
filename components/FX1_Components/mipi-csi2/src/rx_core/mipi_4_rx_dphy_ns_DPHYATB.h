//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYATB
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYATB_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYATB_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_4_rx_dphy_ns_LEVELCONV_h2l_soc.h"
#include "mipi_4_rx_dphy_ns_rtune_comp.h"
#include "mipi_4_rx_dphy_ns_sctg_tg3x.h"
#include "rx_cells.h"

class mipi_4_rx_dphy_ns_DPHYATB : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> atb_en{"atb_en"};
    sc_core::sc_in<sc_dt::sc_logic> atb_ext_con_hv{"atb_ext_con_hv"};
    sc_core::sc_in<sc_dt::sc_logic> atb_ext_con_hvz{"atb_ext_con_hvz"};
    sc_core::sc_in<sc_dt::sc_logic> atb_line{"atb_line"};
    sc_core::sc_in<sc_dt::sc_logic> cb_en_up_1st_hv{"cb_en_up_1st_hv"};
    sc_core::sc_in<sc_dt::sc_logic> cb_en_up_1st_hvz{"cb_en_up_1st_hvz"};
    sc_core::sc_in<sc_dt::sc_logic> clkhn{"clkhn"};
    sc_core::sc_in<sc_dt::sc_logic> clkhp{"clkhp"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> ponh{"ponh"};
    sc_core::sc_in<sc_dt::sc_logic> ponhz{"ponhz"};
    sc_core::sc_in<sc_dt::sc_logic> repl{"repl"};
    sc_core::sc_in<sc_dt::sc_logic> rext{"rext"};
    sc_core::sc_in<sc_dt::sc_lv<10>> sarh{"sarh"};
    sc_core::sc_in<sc_dt::sc_lv<10>> sarhz{"sarhz"};
    sc_core::sc_in<sc_dt::sc_lv<2>> selh{"selh"};
    sc_core::sc_in<sc_dt::sc_lv<2>> selhz{"selhz"};
    sc_core::sc_in<sc_dt::sc_logic> tie_low_hv{"tie_low_hv"};
    sc_core::sc_in<sc_dt::sc_lv<10>> vctrl_replica{"vctrl_replica"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};
    sc_core::sc_in<sc_dt::sc_logic> vref_1000{"vref_1000"};
    sc_core::sc_out<sc_dt::sc_logic> atb{"atb"};
    sc_core::sc_out<sc_dt::sc_logic> cb_comp_out{"cb_comp_out"};
    sc_core::sc_out<sc_dt::sc_logic> nbias{"nbias"};
    sc_core::sc_in<sc_dt::sc_logic> vdd{"vdd"};
    sc_core::sc_in<sc_dt::sc_logic> vss{"vss"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYATB);
    explicit mipi_4_rx_dphy_ns_DPHYATB(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_resolved supply_ok{"supply_ok", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved comp{"comp", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved outh{"outh", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved atb_int{"atb_int", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved tie_high_lv_soc{"tie_high_lv_soc", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved tie_low_lv_soc{"tie_low_lv_soc", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved atb_en_z{"atb_en_z", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved cb_comp_out_z{"cb_comp_out_z", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal<sc_dt::sc_lv<10>> comp_in{"comp_in", vl::logic<10>().lv()};
    // sctg_tg3x  I101( .en(ph_d), .en_n(ph_c), .gd(gd), .i(a), .o(comp),
    // .vph(vph) );
    // mipi_4_rx_dphy_ns_sctg_tg3x  I102( .en(ph_b), .en_n(ph_a), .gd(gd), .i(b), .o(comp),
    // .vph(vph) );
    sc_core::sc_signal<sc_dt::sc_logic> atb_line_x{"atb_line_x", vl::logic<1>().sl()};
    sc_core::sc_signal_resolved atb_line_wire{"atb_line_wire", sc_dt::SC_LOGIC_Z};
    // DPHYATB_inv1_hv  I63( .enh(ponh), .enhz(ponhz), .gd(gd), .A(ph[3]),
    // .vph(vph), .Z(ph_c) );
    // DPHYATB_inv1_hv  I64( .enh(ponh), .enhz(ponhz), .gd(gd), .A(ph[2]),
    // .vph(vph), .Z(ph_a) );
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_h2l_soc> I235;
    // DPHYATB_clk  I69( .clkhn(clkhn), .clkhp(clkhp), .gd(gd), .vph(vph),
    // .ph(ph) );
    std::unique_ptr<mipi_4_rx_dphy_ns_rtune_comp> I11;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx8_lvt> I234;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_tg3x> I433;
    // cb_rtune_r2r_dac  I51( .dac_hn(sarhz[7:0]), .gd(gd), .th_hn(thz),
    // .vphreg(vref), .vdac(dac) );
    // DPHYATB_amux2  I94( .enh(ponh), .enhz(ponhz), .gd(gd),
    // .in({a1,vref_1000}), .selh(cb_en_up_1st_hv),
    // .selhz(cb_en_up_1st_hvz), .vph(vph), .out(a) );
    // DPHYATB_amux2  I95( .enh(ponh), .enhz(ponhz), .gd(gd),
    // .in({b1,vctrl_replica}), .selh(cb_en_up_1st_hv),
    // .selhz(cb_en_up_1st_hvz), .vph(vph), .out(b) );
    // DPHYATB_amux2  I103( .enh(ponh), .enhz(ponhz), .gd(gd),
    // .in({atb_line,rext}), .selh(selh[1]), .selhz(selhz[1]), .vph(vph),
    // .out(a1) );
    // DPHYATB_amux2  I104( .enh(ponh), .enhz(ponhz), .gd(gd), .in({dac,repl})
    // , .selh(selh[0]), .selhz(selhz[0]), .vph(vph), .out(b1) );
    // nfinhv  N427( .B(gd), .D(atb_int), .G(tie_low_hv), .S(gd) );
    // DPHYATB_bin2therm  I52( .binh({sarh[8],sarh[9]}), .gd(gd), .vph(vph),
    // .thz(thz) );
    // pfinhv  P57( .B(vph), .D(vref), .G(ponhz), .S(vph) );
    // DPHYATB_inv0_hv  I61( .enh(ponh), .enhz(ponhz), .gd(gd), .A(ph[4]),
    // .vph(vph), .Z(ph_b) );
    // DPHYATB_inv0_hv  I77( .enh(ponh), .enhz(ponhz), .gd(gd), .A(ph[5]),
    // .vph(vph), .Z(ph_d) );
    std::unique_ptr<mipi_4_rx_dphy_ns_tieLowHigh_LV> I236;

    void assign_atb_line_wire();
    void assign_supply_ok();
    void proc_initial_151468();
    void proc_always_151475();
    void proc_always_151509();
};

#endif
