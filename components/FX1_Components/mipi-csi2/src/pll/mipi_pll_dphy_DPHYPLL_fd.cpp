//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_DPHYPLL_fd
//--------------------------------------------------------------------------------------------------
#include "mipi_pll_dphy_DPHYPLL_fd.h"

mipi_pll_dphy_DPHYPLL_fd::mipi_pll_dphy_DPHYPLL_fd(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(assign_supply_ok);
    sensitive << dvdd << dgnd;
    SC_METHOD(assign_net234_initial);
    sensitive << supply_ok << net234;
    SC_METHOD(assign_m0ff);
    sensitive << supply_ok << m0ff_initial;
    SC_METHOD(assign_pass_m0_initial);
    sensitive << supply_ok << pass_m0;
    SC_METHOD(assign_net236);
    sensitive << supply_ok << net236_out;
    SC_METHOD(gate_buf_m0ff);
    sensitive << pass_m0_initial << dvdd << dgnd;
    pg.reset(new mipi_pll_dphy_DPHYPLL_fd_pg("pg"));
    pg->dgnd(dgnd);
    pg->dvdd(dvdd);
    pg->in(hs_clk_int);
    pg->nout(hs_mux_p1);
    pg->on(pg_off);
    I79.reset(new mipi_pll_dphy_DPHYPLL_sc_syncx10_ar_lvt_speedup("I79"));
    I79->ck(hs_mux_p2);
    I79->d(fbk_mux_syn);
    I79->reset_b(m0ff);
    I79->qb(net294);
    I79->gd(dgnd);
    I79->vp(dvdd);
    I80.reset(new mipi_pll_dphy_DPHYPLL_sc_syncx10_ar_lvt_speedup("I80"));
    I80->ck(hs_mux_p2);
    I80->d(fbk_mux_deglitch);
    I80->reset_b(m0ff);
    I80->qb(trigger_dump);
    I80->gd(dgnd);
    I80->vp(dvdd);
    pgbig.reset(new mipi_pll_dphy_DPHYPLL_fd_pg_big("pgbig"));
    pgbig->on(m0ff);
    pgbig->nout(hs_mux_p2);
    pgbig->dgnd(dgnd);
    pgbig->dvdd(dvdd);
    pgbig->in(hs_clk_int);
    I76.reset(new mipi_pll_dphy_sc_syncx10_as_lvt("I76"));
    I76->ck(hs_div2b);
    I76->d(net281);
    I76->set(tie_low);
    I76->qb(net281);
    I76->gd(dgnd);
    I76->vp(dvdd);
    I78.reset(new mipi_pll_dphy_sc_syncx10_as_lvt("I78"));
    I78->ck(resyn);
    I78->d(fbk_mux);
    I78->set(tie_low);
    I78->qb(net144);
    I78->gd(dgnd);
    I78->vp(dvdd);
    I81.reset(new mipi_pll_dphy_sc_syncx10_as_lvt("I81"));
    I81->ck(hs_mux_p1);
    I81->d(hs_div2b);
    I81->set(tie_low);
    I81->qb(hs_div2b);
    I81->gd(dgnd);
    I81->vp(dvdd);
    INV_CDM.reset(new mipi_pll_dphy_sc_invx32_lvt("INV_CDM"));
    INV_CDM->a(hs_mux);
    INV_CDM->z(net307);
    INV_CDM->gd(gd);
    INV_CDM->vp(vpl);
    I38.reset(new mipi_pll_dphy_sc_na2x24_lvt("I38"));
    I38->a(trigger_dump);
    I38->b(fbk_mux_deglitch);
    I38->z(pg_off);
    I38->gd(dgnd);
    I38->vp(dvdd);
    I39.reset(new mipi_pll_dphy_sc_na2x24_lvt("I39"));
    I39->a(fbk_mux_deglitch);
    I39->b(trigger_dump);
    I39->z(pass_m0);
    I39->gd(dgnd);
    I39->vp(dvdd);
    CDM_OUT_3.reset(new mipi_pll_dphy_sc_no2x12_lvt("CDM_OUT_3"));
    CDM_OUT_3->a(tie_low_cdm);
    CDM_OUT_3->b(net307);
    CDM_OUT_3->z(hs_clk_int);
    CDM_OUT_3->gd(gd);
    CDM_OUT_3->vp(vpl);
    CDM_OUT_2.reset(new mipi_pll_dphy_sc_no2x12_lvt("CDM_OUT_2"));
    CDM_OUT_2->a(tie_low_cdm);
    CDM_OUT_2->b(net307);
    CDM_OUT_2->z(hs_clk_int);
    CDM_OUT_2->gd(gd);
    CDM_OUT_2->vp(vpl);
    CDM_OUT_1.reset(new mipi_pll_dphy_sc_no2x12_lvt("CDM_OUT_1"));
    CDM_OUT_1->a(tie_low_cdm);
    CDM_OUT_1->b(net307);
    CDM_OUT_1->z(hs_clk_int);
    CDM_OUT_1->gd(gd);
    CDM_OUT_1->vp(vpl);
    CDM_OUT_0.reset(new mipi_pll_dphy_sc_no2x12_lvt("CDM_OUT_0"));
    CDM_OUT_0->a(tie_low_cdm);
    CDM_OUT_0->b(net307);
    CDM_OUT_0->z(hs_clk_int);
    CDM_OUT_0->gd(gd);
    CDM_OUT_0->vp(vpl);
    I44.reset(new mipi_pll_dphy_sc_invx16_lvt("I44"));
    I44->a(net144);
    I44->z(fbk_mux_syn);
    I44->gd(dgnd);
    I44->vp(dvdd);
    I48.reset(new mipi_pll_dphy_sc_invx16_lvt("I48"));
    I48->a(net294);
    I48->z(fbk_mux_deglitch);
    I48->gd(dgnd);
    I48->vp(dvdd);
    I58.reset(new mipi_pll_dphy_sc_invx16_lvt("I58"));
    I58->a(net281);
    I58->z(resyn);
    I58->gd(dgnd);
    I58->vp(dvdd);
    I61.reset(new mipi_pll_dphy_sc_invx16_lvt("I61"));
    I61->a(hs_div2b);
    I61->z(hs_div2);
    I61->gd(dgnd);
    I61->vp(dvdd);
    I71.reset(new mipi_pll_dphy_sc_invx16_lvt("I71"));
    I71->a(net234_initial);
    I71->z(m0ff);
    I71->gd(dgnd);
    I71->vp(dvdd);
    I73.reset(new mipi_pll_dphy_sc_invx16_lvt("I73"));
    I73->a(pass_m0_initial);
    I73->z(net236_out);
    I73->gd(dgnd);
    I73->vp(dvdd);
    I53.reset(new mipi_pll_dphy_sc_tie_hilow_lvt("I53"));
    I53->gd(dgnd);
    I53->tie_high(tie_high);
    I53->tie_low(tie_low);
    I53->vp(dvdd);
    I69.reset(new mipi_pll_dphy_sc_dlax23_ar_lvt("I69"));
    I69->ck_m(net236);
    I69->ck_p(pass_m0);
    I69->d(m0);
    I69->pwr_on(tie_high);
    I69->qb(net234);
    I69->gd(dgnd);
    I69->vp(dvdd);
}

void mipi_pll_dphy_DPHYPLL_fd::assign_supply_ok() {
    vl::wr(supply_ok, vl::land(vl::ceq(vl::rd(dvdd), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(dgnd), vl::logic<1>(0x0ULL))));
}

void mipi_pll_dphy_DPHYPLL_fd::assign_net234_initial() {
    vl::wr(net234_initial, vl::mux(vl::rd(supply_ok), vl::rd(net234), vl::logic<1>(0x0ULL)));
}

void mipi_pll_dphy_DPHYPLL_fd::assign_m0ff() {
    vl::wr(m0ff, vl::mux(vl::rd(supply_ok), vl::rd(m0ff_initial), vl::logic<1>(0x1ULL)));
}

void mipi_pll_dphy_DPHYPLL_fd::assign_pass_m0_initial() {
    vl::wr(pass_m0_initial, vl::mux(vl::rd(supply_ok), vl::rd(pass_m0), vl::logic<1>(0x0ULL)));
}

void mipi_pll_dphy_DPHYPLL_fd::assign_net236() {
    vl::wr(net236, vl::mux(vl::rd(supply_ok), vl::rd(net236_out), vl::logic<1>(0x1ULL)));
}

void mipi_pll_dphy_DPHYPLL_fd::gate_buf_m0ff() {
    const vl::logic<1> v = vl::bufif1_(vl::logic<1>(0x1ULL), vl::land(vl::land(vl::ceq(vl::rd(pass_m0_initial), vl::logic<1>(0x0ULL)), vl::ceq(vl::rd(dvdd), vl::logic<1>(0x1ULL))), vl::ceq(vl::rd(dgnd), vl::logic<1>(0x0ULL))));
    vl::wr(m0ff_initial, v);
}
