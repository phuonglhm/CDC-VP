//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYATB
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYATB.h"

mipi_4_rx_dphy_ns_DPHYATB::mipi_4_rx_dphy_ns_DPHYATB(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(assign_atb_line_wire);
    sensitive << atb_line_x;
    SC_METHOD(assign_supply_ok);
    sensitive << vph << gd;
    // DPHYATB_inv1_hv  I63( .enh(ponh), .enhz(ponhz), .gd(gd), .A(ph[3]),
    // .vph(vph), .Z(ph_c) );
    // DPHYATB_inv1_hv  I64( .enh(ponh), .enhz(ponhz), .gd(gd), .A(ph[2]),
    // .vph(vph), .Z(ph_a) );
    I235.reset(new mipi_4_rx_dphy_ns_LEVELCONV_h2l_soc("I235"));
    I235->A_in(outh);
    I235->on_hv(ponh);
    I235->onz_lv_soc(atb_en_z);
    I235->tie_low_lv_soc(tie_low_lv_soc);
    I235->Z(cb_comp_out);
    I235->Z_neg(cb_comp_out_z);
    I235->gd(gd);
    I235->vdd(vdd);
    I235->vph(vph);
    I235->vss(vss);
    // DPHYATB_clk  I69( .clkhn(clkhn), .clkhp(clkhp), .gd(gd), .vph(vph),
    // .ph(ph) );
    I11.reset(new mipi_4_rx_dphy_ns_rtune_comp("I11"));
    I11->gd(gd);
    I11->r_h(clkhp);
    I11->rt_pwron_h(ponh);
    I11->vin(comp_in);
    I11->vph(vph);
    I11->comp_result_h(outh);
    I11->nbias(nbias);
    I234.reset(new mipi_4_rx_dphy_ns_sc_invx8_lvt("I234"));
    I234->a(atb_en);
    I234->z(atb_en_z);
    I234->gd(vss);
    I234->vp(vdd);
    I433.reset(new mipi_4_rx_dphy_ns_sctg_tg3x("I433"));
    I433->en(atb_ext_con_hv);
    I433->en_n(atb_ext_con_hvz);
    I433->gd(gd);
    I433->i(atb_line_wire);
    I433->o(atb_int);
    I433->vph(vph);
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
    I236.reset(new mipi_4_rx_dphy_ns_tieLowHigh_LV("I236"));
    I236->high(tie_high_lv_soc);
    I236->low(tie_low_lv_soc);
    I236->gd(vss);
    I236->vp(vdd);
    SC_METHOD(proc_initial_151468);
    SC_METHOD(proc_always_151475);
    sensitive << selh << sarh << vctrl_replica;
    SC_METHOD(proc_always_151509);
    sensitive << supply_ok << tie_low_hv << atb_int;
}

void mipi_4_rx_dphy_ns_DPHYATB::assign_atb_line_wire() {
    vl::wr(atb_line_wire, vl::rd(atb_line_x));
}

void mipi_4_rx_dphy_ns_DPHYATB::assign_supply_ok() {
    vl::wr(supply_ok, vl::land(vl::ceq(vl::rd(vph), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))));
}

void mipi_4_rx_dphy_ns_DPHYATB::proc_initial_151468() {
    vl::logic<1> atb_line_x_sh = vl::rd(atb_line_x);
    atb_line_x_sh = vl::logic<1>();
    vl::wr(atb_line_x, atb_line_x_sh);
}

void mipi_4_rx_dphy_ns_DPHYATB::proc_always_151475() {
    vl::logic<10> comp_in_sh = vl::rd(comp_in);
    {
        const vl::logic<1> sel = vl::rd(selh).bit(0);
        if (vl::casex_eq(sel, vl::logic<1>(0x0ULL))) {
            comp_in_sh = vl::rd(sarh);
            vl::wr(comp_in, comp_in_sh);
        } else if (vl::casex_eq(sel, vl::logic<1>(0x1ULL))) {
            comp_in_sh = vl::rd(vctrl_replica);
            vl::wr(comp_in, comp_in_sh);
        }
    }
}

void mipi_4_rx_dphy_ns_DPHYATB::proc_always_151509() {
    if (vl::rd(supply_ok).test()) {
        if (vl::rd(tie_low_hv).test()) {
            vl::wr(atb, vl::logic<1>(0x0ULL));
        } else {
            vl::wr(atb, vl::rd(atb_int));
        }
    } else {
        vl::wr(atb, vl::logic<1>::all_z());
    }
}
