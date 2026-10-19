//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_preamp_clk
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYHSRX_preamp_clk.h"

mipi_4_rx_dphy_ns_DPHYHSRX_preamp_clk::mipi_4_rx_dphy_ns_DPHYHSRX_preamp_clk(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(assign_supply_ok_offset_path);
    sensitive << vp << vph << gd << bias_ihv;
    SC_METHOD(assign_vp_bias);
    sensitive << vp;
    SC_METHOD(gate_voutp0_buf);
    sensitive << vcalp << supply_ok_offset_path << calsw_h;
    SC_METHOD(gate_voutn0_buf);
    sensitive << vcaln << supply_ok_offset_path << calsw_h;
    SC_METHOD(gate_bufvoutp0);
    sensitive << voutp0_int << calsw_h;
    SC_METHOD(gate_bufvoutn0);
    sensitive << voutn0_int << calsw_h;
    OFFSET_CAL.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_preamp_offset_cal("OFFSET_CAL"));
    OFFSET_CAL->atb_sw_hv(atb_sw_hv);
    OFFSET_CAL->atb_sw_hvz(atb_sw_hvz);
    OFFSET_CAL->rx_hs_bcalz(rx_hs_bcalz);
    OFFSET_CAL->rx_pre_ponz(rx_pre_ponz);
    OFFSET_CAL->vbplv_ccm(bias_ilv_cc);
    OFFSET_CAL->vcaln(vcaln);
    OFFSET_CAL->vcalp(vcalp);
    OFFSET_CAL->atb_line(atb_line);
    OFFSET_CAL->gd(gd);
    OFFSET_CAL->vp(vp);
    CS_LV.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_preamp_current_sources_lv("CS_LV"));
    CS_LV->ilv_cc(bias_ilv_cc);
    CS_LV->ilv_cgm(bias_ilv_cgm);
    CS_LV->key(key);
    CS_LV->keyz(keyz);
    CS_LV->on(rx_pre_pon);
    CS_LV->onz(rx_pre_ponz);
    CS_LV->ibias1(ibias1);
    CS_LV->ibias2(ibias2);
    CS_LV->vp(vp);
    LB_DRV.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_lb_dirver("LB_DRV"));
    LB_DRV->en_clklb(en_clklb);
    LB_DRV->en_lb(en_lb);
    LB_DRV->en_lb_h(en_lb_h);
    LB_DRV->ibias_lb(ibias_lb);
    LB_DRV->lb_clkmarg_m(lb_clkmarg_m);
    LB_DRV->lb_clkmarg_p(lb_clkmarg_p);
    LB_DRV->lb_marg_m(lb_marg_m);
    LB_DRV->lb_marg_p(lb_marg_p);
    LB_DRV->lbcal(lbcalsw);
    LB_DRV->lb_outm(vi_m_prot);
    LB_DRV->lb_outp(vi_p_prot);
    LB_DRV->gd(gd);
    LB_DRV->vp(vp);
    LB_DRV->vph(vph);
    SC_METHOD(glue_I127_g);
    sensitive << keyz << tie_high;
    I127.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_preamp_unit2("I127"));
    I127->g(c_I127_g);
    I127->ibias(ibias2);
    I127->vim(voutn1);
    I127->vip(voutp1);
    I127->gd(gd);
    I127->voutm(vpre_m);
    I127->voutp(vpre_p);
    I127->vp(vp);
    SC_METHOD(split_LOGIC_keyhv);
    sensitive << c_LOGIC_keyhv;
    dont_initialize();
    SC_METHOD(split_LOGIC_keyhvz);
    sensitive << c_LOGIC_keyhvz;
    dont_initialize();
    LOGIC.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_logic("LOGIC"));
    LOGIC->atb_sw_hv(atb_sw_hv);
    LOGIC->en_lb(en_lb);
    LOGIC->en_lb_h(en_lb_h);
    LOGIC->gear(gmode);
    LOGIC->gearhv(gmode_h);
    LOGIC->lb_current_h(lb_current_h);
    LOGIC->on(rx_pre_pon);
    LOGIC->onhv(rx_pre_pon_h);
    LOGIC->rx_eq_bcal_h(rx_eq_bcal_h);
    LOGIC->rx_hs_bcal(rx_hs_bcal);
    LOGIC->rx_hs_encal(rx_hs_encal);
    LOGIC->rx_hs_encal_h(rx_hs_encal_h);
    LOGIC->rx_pre_burst(rx_pre_burst);
    LOGIC->rx_pre_bursthv(rx_pre_burst_h);
    LOGIC->atb_sw_hvz(atb_sw_hvz);
    LOGIC->calsw_h(calsw_h);
    LOGIC->calswn_h(calswn_h);
    LOGIC->en_lb_hz(en_lb_hz);
    LOGIC->en_lbz(en_lbz);
    LOGIC->key(key);
    LOGIC->keyhv(c_LOGIC_keyhv);
    LOGIC->keyhvz(c_LOGIC_keyhvz);
    LOGIC->keyz(keyz);
    LOGIC->lbcalsw(lbcalsw);
    LOGIC->lbkeyhv(lbkeyhv);
    LOGIC->onhvz(rx_pre_pon_hz);
    LOGIC->onz(rx_pre_ponz);
    LOGIC->rx_eq_bcal_bypass_h(rx_eq_bcal_bypass_h);
    LOGIC->rx_eq_bcal_hz(rx_eq_bcal_hz);
    LOGIC->rx_hs_bcalz(rx_hs_bcalz);
    LOGIC->gd(gd);
    LOGIC->vp(vp);
    LOGIC->vph(vph);
    SC_METHOD(glue_UNIT1_g);
    sensitive << keyz << tie_high;
    UNIT1.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_preamp_unit1("UNIT1"));
    UNIT1->g(c_UNIT1_g);
    UNIT1->ibias(ibias1);
    UNIT1->vim(voutn0);
    UNIT1->vip(voutp0);
    UNIT1->gd(gd);
    UNIT1->voutm(voutn1);
    UNIT1->voutp(voutp1);
    UNIT1->vp(vp);
    PROTSW.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_preamp_protsw("PROTSW"));
    PROTSW->calsw_h(calsw_h);
    PROTSW->calswn_h(calswn_h);
    PROTSW->vi_m(vi_m);
    PROTSW->vi_p(vi_p);
    PROTSW->gd(gd);
    PROTSW->vi_m_prot(vi_m_prot);
    PROTSW->vi_n_prot(vi_p_prot);
    PROTSW->vph(vph);
    CS_HV.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_preamp_clk_current_sources_hv("CS_HV"));
    CS_HV->ihv(bias_ihv);
    CS_HV->keyhv(keyhv[0]);
    CS_HV->keyhvz(keyhvz[0]);
    CS_HV->lbkeyhv(lbkeyhv);
    CS_HV->onhv(rx_pre_pon_h);
    CS_HV->onhvz(rx_pre_pon_hz);
    CS_HV->ibias(ibias_noeq);
    CS_HV->ibias_cal(ibias_cal);
    CS_HV->ibias_lb(ibias_lb);
    CS_HV->vph(vph);
    EQUALIZER.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_preamp_unit_noeq("EQUALIZER"));
    EQUALIZER->ibias(ibias_noeq);
    EQUALIZER->vim(vi_m_prot);
    EQUALIZER->vip(vi_p_prot);
    EQUALIZER->gd(gd);
    EQUALIZER->voutm(voutn0_int);
    EQUALIZER->voutp(voutp0_int);
    EQUALIZER->vph(vph);
}

vl::logic<4> mipi_4_rx_dphy_ns_DPHYHSRX_preamp_clk::rd_keyhv() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(keyhv[0]));
    v.set_bit(1, vl::rd(keyhv[1]));
    v.set_bit(2, vl::rd(keyhv[2]));
    v.set_bit(3, vl::rd(keyhv[3]));
    return v;
}

vl::logic<4> mipi_4_rx_dphy_ns_DPHYHSRX_preamp_clk::rd_keyhvz() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(keyhvz[0]));
    v.set_bit(1, vl::rd(keyhvz[1]));
    v.set_bit(2, vl::rd(keyhvz[2]));
    v.set_bit(3, vl::rd(keyhvz[3]));
    return v;
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_clk::assign_supply_ok_offset_path() {
    vl::wr(supply_ok_offset_path, vl::land(vl::land(vl::land(vl::eq(vl::rd(vp), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(vph), vl::logic<1>(0x1ULL))), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(bias_ihv), vl::logic<1>(0x1ULL))));
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_clk::assign_vp_bias() {
    vl::wr(vp_bias, vl::rd(vp));
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_clk::gate_voutp0_buf() {
    const vl::logic<1> v = vl::bufif1_(vl::rd(vcalp), vl::land(vl::ceq(vl::rd(supply_ok_offset_path), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(calsw_h), vl::logic<1>(0x1ULL))));
    vl::wr(voutp0, v);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_clk::gate_voutn0_buf() {
    const vl::logic<1> v = vl::bufif1_(vl::rd(vcaln), vl::land(vl::ceq(vl::rd(supply_ok_offset_path), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(calsw_h), vl::logic<1>(0x1ULL))));
    vl::wr(voutn0, v);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_clk::gate_bufvoutp0() {
    const vl::logic<1> v = vl::bufif1_(vl::rd(voutp0_int), vl::ceq(vl::rd(calsw_h), vl::logic<1>(0x0ULL)));
    vl::wr(voutp0, v);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_clk::gate_bufvoutn0() {
    const vl::logic<1> v = vl::bufif1_(vl::rd(voutn0_int), vl::ceq(vl::rd(calsw_h), vl::logic<1>(0x0ULL)));
    vl::wr(voutn0, v);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_clk::glue_I127_g() { vl::wr(c_I127_g, vl::cat(vl::rd(keyz).bit(3), vl::rd(keyz).bit(2), vl::rd(keyz).bit(1), vl::rd(tie_high))); }

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_clk::split_LOGIC_keyhv() {
    const vl::logic<4> v = vl::rd(c_LOGIC_keyhv);
    keyhv[0].write(v.bit(0).sl());
    keyhv[1].write(v.bit(1).sl());
    keyhv[2].write(v.bit(2).sl());
    keyhv[3].write(v.bit(3).sl());
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_clk::split_LOGIC_keyhvz() {
    const vl::logic<4> v = vl::rd(c_LOGIC_keyhvz);
    keyhvz[0].write(v.bit(0).sl());
    keyhvz[1].write(v.bit(1).sl());
    keyhvz[2].write(v.bit(2).sl());
    keyhvz[3].write(v.bit(3).sl());
}

void mipi_4_rx_dphy_ns_DPHYHSRX_preamp_clk::glue_UNIT1_g() { vl::wr(c_UNIT1_g, vl::cat(vl::rd(keyz).bit(3), vl::rd(keyz).bit(2), vl::rd(keyz).bit(1), vl::rd(tie_high))); }
