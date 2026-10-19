//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_DDL_4bit
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYHSRX_DDL_4bit.h"

mipi_4_rx_dphy_ns_DPHYHSRX_DDL_4bit::mipi_4_rx_dphy_ns_DPHYHSRX_DDL_4bit(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(split_phase);
    sensitive << phase;
    SC_METHOD(gate_ddl_clk_outn01);
    sensitive << ddl_clk_outn_int << vp << gd << tie_low;
    SC_METHOD(gate_ddl_clk_outp01);
    sensitive << ddl_clk_outp_int << vp << gd << tie_low;
    SC_METHOD(gate_ddl_clk_outn02);
    sensitive << vp << gd << tie_low;
    SC_METHOD(gate_ddl_clk_outp02);
    sensitive << vp << gd << tie_low;
    FF_3.reset(new mipi_4_rx_dphy_ns_sc_syncx10_as_lvt("FF_3"));
    FF_3->ck(phase_sync);
    FF_3->d(phase_b[3]);
    FF_3->set(onz);
    FF_3->qb(net2075[3]);
    FF_3->gd(gd);
    FF_3->vp(vp);
    FF_2.reset(new mipi_4_rx_dphy_ns_sc_syncx10_as_lvt("FF_2"));
    FF_2->ck(phase_sync);
    FF_2->d(phase_b[2]);
    FF_2->set(onz);
    FF_2->qb(net2075[2]);
    FF_2->gd(gd);
    FF_2->vp(vp);
    FF_1.reset(new mipi_4_rx_dphy_ns_sc_syncx10_as_lvt("FF_1"));
    FF_1->ck(phase_sync);
    FF_1->d(phase_b[1]);
    FF_1->set(onz);
    FF_1->qb(net2075[1]);
    FF_1->gd(gd);
    FF_1->vp(vp);
    FF_0.reset(new mipi_4_rx_dphy_ns_sc_syncx10_as_lvt("FF_0"));
    FF_0->ck(phase_sync);
    FF_0->d(phase_b[0]);
    FF_0->set(onz);
    FF_0->qb(net2075[0]);
    FF_0->gd(gd);
    FF_0->vp(vp);
    I393.reset(new mipi_4_rx_dphy_ns_sc_tie_hilow_lvt("I393"));
    I393->gd(gd);
    I393->tie_high(tie_high);
    I393->tie_low(tie_low);
    I393->vp(vp);
    I405.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_DDL_MUX_2_1_inv_en("I405"));
    I405->i(nextp);
    I405->on(tie_low);
    I405->onz(tune_enz);
    I405->gd(gd);
    I405->o(ddl_clk_outp_int);
    I405->vp(vp);
    I406.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_DDL_MUX_2_1_inv_en("I406"));
    I406->i(nextn);
    I406->on(tie_low);
    I406->onz(tune_enz);
    I406->gd(gd);
    I406->o(ddl_clk_outn_int);
    I406->vp(vp);
    SC_METHOD(glue_LOGIC_phase);
    sensitive << syncd_phase[0] << syncd_phase[1] << syncd_phase[2] << syncd_phase[3];
    SC_METHOD(split_LOGIC_y);
    sensitive << c_LOGIC_y;
    dont_initialize();
    SC_METHOD(split_LOGIC_yz);
    sensitive << c_LOGIC_yz;
    dont_initialize();
    LOGIC.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_DDL_LOGIC_4bit("LOGIC"));
    LOGIC->phase(c_LOGIC_phase);
    LOGIC->pon(on);
    LOGIC->tune_en(tie_low);
    LOGIC->tune_enz(tune_enz);
    LOGIC->y(c_LOGIC_y);
    LOGIC->yz(c_LOGIC_yz);
    LOGIC->gd(gd);
    LOGIC->vp(vp);
    I394.reset(new mipi_4_rx_dphy_ns_sc_invx24_lvt("I394"));
    I394->a(pi_outp);
    I394->z(pi_out2p);
    I394->gd(gd);
    I394->vp(vp);
    I399.reset(new mipi_4_rx_dphy_ns_sc_invx24_lvt("I399"));
    I399->a(pi_outn);
    I399->z(pi_out2n);
    I399->gd(gd);
    I399->vp(vp);
    BUF_CELL_0.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_DDL_DIFF_BUFF_CELL("BUF_CELL_0"));
    BUF_CELL_0->back(yz[0]);
    BUF_CELL_0->bn(bn);
    BUF_CELL_0->bp(bp);
    BUF_CELL_0->forward(y[0]);
    BUF_CELL_0->in_back_n(out_bufn_b[0]);
    BUF_CELL_0->in_back_p(out_bufp_b[0]);
    BUF_CELL_0->in_forward_n(in_bufn);
    BUF_CELL_0->in_forward_p(in_bufp);
    BUF_CELL_0->pdn(yz[0]);
    BUF_CELL_0->pdp(tie_low);
    BUF_CELL_0->pun(tie_high);
    BUF_CELL_0->pup(y[0]);
    BUF_CELL_0->tie_high(tie_high);
    BUF_CELL_0->tie_low(tie_low);
    BUF_CELL_0->out_back_n(out_bufn);
    BUF_CELL_0->out_back_p(out_bufp);
    BUF_CELL_0->out_forward_n(out_bufn_f[0]);
    BUF_CELL_0->out_forward_p(out_bufp_f[0]);
    BUF_CELL_0->gd(gd);
    BUF_CELL_0->vp(vp);
    BUF_CELL_1.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_DDL_DIFF_BUFF_CELL("BUF_CELL_1"));
    BUF_CELL_1->back(yz[1]);
    BUF_CELL_1->bn(bn);
    BUF_CELL_1->bp(bp);
    BUF_CELL_1->forward(y[1]);
    BUF_CELL_1->in_back_n(out_bufn_b[1]);
    BUF_CELL_1->in_back_p(out_bufp_b[1]);
    BUF_CELL_1->in_forward_n(out_bufn_f[0]);
    BUF_CELL_1->in_forward_p(out_bufp_f[0]);
    BUF_CELL_1->pdn(tie_low);
    BUF_CELL_1->pdp(yz[1]);
    BUF_CELL_1->pun(y[1]);
    BUF_CELL_1->pup(tie_high);
    BUF_CELL_1->tie_high(tie_high);
    BUF_CELL_1->tie_low(tie_low);
    BUF_CELL_1->out_back_n(out_bufn_b[0]);
    BUF_CELL_1->out_back_p(out_bufp_b[0]);
    BUF_CELL_1->out_forward_n(out_bufn_f[1]);
    BUF_CELL_1->out_forward_p(out_bufp_f[1]);
    BUF_CELL_1->gd(gd);
    BUF_CELL_1->vp(vp);
    BUF_CELL_2.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_DDL_DIFF_BUFF_CELL("BUF_CELL_2"));
    BUF_CELL_2->back(yz[2]);
    BUF_CELL_2->bn(bn);
    BUF_CELL_2->bp(bp);
    BUF_CELL_2->forward(y[2]);
    BUF_CELL_2->in_back_n(out_bufn_b[2]);
    BUF_CELL_2->in_back_p(out_bufp_b[2]);
    BUF_CELL_2->in_forward_n(out_bufn_f[1]);
    BUF_CELL_2->in_forward_p(out_bufp_f[1]);
    BUF_CELL_2->pdn(yz[2]);
    BUF_CELL_2->pdp(tie_low);
    BUF_CELL_2->pun(tie_high);
    BUF_CELL_2->pup(y[2]);
    BUF_CELL_2->tie_high(tie_high);
    BUF_CELL_2->tie_low(tie_low);
    BUF_CELL_2->out_back_n(out_bufn_b[1]);
    BUF_CELL_2->out_back_p(out_bufp_b[1]);
    BUF_CELL_2->out_forward_n(out_bufn_f[2]);
    BUF_CELL_2->out_forward_p(out_bufp_f[2]);
    BUF_CELL_2->gd(gd);
    BUF_CELL_2->vp(vp);
    BUF_CELL_3.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_DDL_DIFF_BUFF_CELL("BUF_CELL_3"));
    BUF_CELL_3->back(yz[3]);
    BUF_CELL_3->bn(bn);
    BUF_CELL_3->bp(bp);
    BUF_CELL_3->forward(y[3]);
    BUF_CELL_3->in_back_n(lbn);
    BUF_CELL_3->in_back_p(lbp);
    BUF_CELL_3->in_forward_n(out_bufn_f[2]);
    BUF_CELL_3->in_forward_p(out_bufp_f[2]);
    BUF_CELL_3->pdn(tie_low);
    BUF_CELL_3->pdp(yz[3]);
    BUF_CELL_3->pun(y[3]);
    BUF_CELL_3->pup(tie_high);
    BUF_CELL_3->tie_high(tie_high);
    BUF_CELL_3->tie_low(tie_low);
    BUF_CELL_3->out_back_n(out_bufn_b[2]);
    BUF_CELL_3->out_back_p(out_bufp_b[2]);
    BUF_CELL_3->out_forward_n(lbn);
    BUF_CELL_3->out_forward_p(lbp);
    BUF_CELL_3->gd(gd);
    BUF_CELL_3->vp(vp);
    I462_3.reset(new mipi_4_rx_dphy_ns_sc_invx4_lvt("I462_3"));
    I462_3->a(net2075[3]);
    I462_3->z(syncd_phase[3]);
    I462_3->gd(gd);
    I462_3->vp(vp);
    I462_2.reset(new mipi_4_rx_dphy_ns_sc_invx4_lvt("I462_2"));
    I462_2->a(net2075[2]);
    I462_2->z(syncd_phase[2]);
    I462_2->gd(gd);
    I462_2->vp(vp);
    I462_1.reset(new mipi_4_rx_dphy_ns_sc_invx4_lvt("I462_1"));
    I462_1->a(net2075[1]);
    I462_1->z(syncd_phase[1]);
    I462_1->gd(gd);
    I462_1->vp(vp);
    I462_0.reset(new mipi_4_rx_dphy_ns_sc_invx4_lvt("I462_0"));
    I462_0->a(net2075[0]);
    I462_0->z(syncd_phase[0]);
    I462_0->gd(gd);
    I462_0->vp(vp);
    GATEN.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_DDL_gate("GATEN"));
    GATEN->i(pi_out2n);
    GATEN->on(on);
    GATEN->onz(onz);
    GATEN->pd(tie_low);
    GATEN->pu(on);
    GATEN->o(clk_outn);
    GATEN->gd(gd);
    GATEN->vp(vp);
    GATEP.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_DDL_gate("GATEP"));
    GATEP->i(pi_out2p);
    GATEP->on(on);
    GATEP->onz(onz);
    GATEP->pd(tie_low);
    GATEP->pu(on);
    GATEP->o(clk_outp);
    GATEP->gd(gd);
    GATEP->vp(vp);
    SC_METHOD(glue_PI_N_0_word);
    sensitive << syncd_phase[0] << syncd_phase[1] << syncd_phase[2] << syncd_phase[3];
    PI_N_0.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_DDL_PI_BIASED("PI_N_0"));
    PI_N_0->bn(bn);
    PI_N_0->bp(bp);
    PI_N_0->inA(out_bufn);
    PI_N_0->inB(nextn);
    PI_N_0->pd(onz);
    PI_N_0->pu(tie_high);
    PI_N_0->tie_high(tie_high);
    PI_N_0->tie_low(tie_low);
    PI_N_0->word(c_PI_N_0_word);
    PI_N_0->o(pi_outn);
    PI_N_0->gd(gd);
    PI_N_0->vp(vp);
    SC_METHOD(glue_PI_P_0_word);
    sensitive << syncd_phase[0] << syncd_phase[1] << syncd_phase[2] << syncd_phase[3];
    PI_P_0.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_DDL_PI_BIASED("PI_P_0"));
    PI_P_0->bn(bn);
    PI_P_0->bp(bp);
    PI_P_0->inA(out_bufp);
    PI_P_0->inB(nextp);
    PI_P_0->pd(tie_low);
    PI_P_0->pu(on);
    PI_P_0->tie_high(tie_high);
    PI_P_0->tie_low(tie_low);
    PI_P_0->word(c_PI_P_0_word);
    PI_P_0->o(pi_outp);
    PI_P_0->gd(gd);
    PI_P_0->vp(vp);
    CLK_MUX_P.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_DDL_MUX_2_1("CLK_MUX_P"));
    CLK_MUX_P->clk_in(clk_inp);
    CLK_MUX_P->ddl_clk(nextp);
    CLK_MUX_P->tune_en(tie_low);
    CLK_MUX_P->z(in_bufp);
    CLK_MUX_P->gd(gd);
    CLK_MUX_P->vp(vp);
    I388.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_DDL_MUX_2_1("I388"));
    I388->clk_in(clk_inn);
    I388->ddl_clk(nextn);
    I388->tune_en(tie_low);
    I388->z(in_bufn);
    I388->gd(gd);
    I388->vp(vp);
    BUF_CELL_NEXT.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_DDL_DIFF_BUFF_CELL_REP("BUF_CELL_NEXT"));
    BUF_CELL_NEXT->bn(bn);
    BUF_CELL_NEXT->bp(bp);
    BUF_CELL_NEXT->in_n(out_bufn);
    BUF_CELL_NEXT->in_p(out_bufp);
    BUF_CELL_NEXT->on(on);
    BUF_CELL_NEXT->onz(onz);
    BUF_CELL_NEXT->tie_high(tie_high);
    BUF_CELL_NEXT->tie_low(tie_low);
    BUF_CELL_NEXT->out_n(nextn);
    BUF_CELL_NEXT->out_p(nextp);
    BUF_CELL_NEXT->gd(gd);
    BUF_CELL_NEXT->vp(vp);
    SC_THREAD(proc_initial_154668);
    set_stack_size(0x20000);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_DDL_4bit::split_phase() {
    const vl::logic<4> v = vl::rd(phase);
    phase_b[0].write(v.bit(0).sl());
    phase_b[1].write(v.bit(1).sl());
    phase_b[2].write(v.bit(2).sl());
    phase_b[3].write(v.bit(3).sl());
}

vl::logic<4> mipi_4_rx_dphy_ns_DPHYHSRX_DDL_4bit::rd_net2075() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(net2075[0]));
    v.set_bit(1, vl::rd(net2075[1]));
    v.set_bit(2, vl::rd(net2075[2]));
    v.set_bit(3, vl::rd(net2075[3]));
    return v;
}

vl::logic<3> mipi_4_rx_dphy_ns_DPHYHSRX_DDL_4bit::rd_out_bufn_b() const {
    vl::logic<3> v(0);
    v.set_bit(0, vl::rd(out_bufn_b[0]));
    v.set_bit(1, vl::rd(out_bufn_b[1]));
    v.set_bit(2, vl::rd(out_bufn_b[2]));
    return v;
}

vl::logic<3> mipi_4_rx_dphy_ns_DPHYHSRX_DDL_4bit::rd_out_bufn_f() const {
    vl::logic<3> v(0);
    v.set_bit(0, vl::rd(out_bufn_f[0]));
    v.set_bit(1, vl::rd(out_bufn_f[1]));
    v.set_bit(2, vl::rd(out_bufn_f[2]));
    return v;
}

vl::logic<3> mipi_4_rx_dphy_ns_DPHYHSRX_DDL_4bit::rd_out_bufp_b() const {
    vl::logic<3> v(0);
    v.set_bit(0, vl::rd(out_bufp_b[0]));
    v.set_bit(1, vl::rd(out_bufp_b[1]));
    v.set_bit(2, vl::rd(out_bufp_b[2]));
    return v;
}

vl::logic<3> mipi_4_rx_dphy_ns_DPHYHSRX_DDL_4bit::rd_out_bufp_f() const {
    vl::logic<3> v(0);
    v.set_bit(0, vl::rd(out_bufp_f[0]));
    v.set_bit(1, vl::rd(out_bufp_f[1]));
    v.set_bit(2, vl::rd(out_bufp_f[2]));
    return v;
}

vl::logic<4> mipi_4_rx_dphy_ns_DPHYHSRX_DDL_4bit::rd_phase() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(phase_b[0]));
    v.set_bit(1, vl::rd(phase_b[1]));
    v.set_bit(2, vl::rd(phase_b[2]));
    v.set_bit(3, vl::rd(phase_b[3]));
    return v;
}

vl::logic<4> mipi_4_rx_dphy_ns_DPHYHSRX_DDL_4bit::rd_syncd_phase() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(syncd_phase[0]));
    v.set_bit(1, vl::rd(syncd_phase[1]));
    v.set_bit(2, vl::rd(syncd_phase[2]));
    v.set_bit(3, vl::rd(syncd_phase[3]));
    return v;
}

vl::logic<4> mipi_4_rx_dphy_ns_DPHYHSRX_DDL_4bit::rd_y() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(y[0]));
    v.set_bit(1, vl::rd(y[1]));
    v.set_bit(2, vl::rd(y[2]));
    v.set_bit(3, vl::rd(y[3]));
    return v;
}

vl::logic<4> mipi_4_rx_dphy_ns_DPHYHSRX_DDL_4bit::rd_yz() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(yz[0]));
    v.set_bit(1, vl::rd(yz[1]));
    v.set_bit(2, vl::rd(yz[2]));
    v.set_bit(3, vl::rd(yz[3]));
    return v;
}

void mipi_4_rx_dphy_ns_DPHYHSRX_DDL_4bit::gate_ddl_clk_outn01() {
    const vl::logic<1> v = vl::bufif1_(vl::rd(ddl_clk_outn_int), vl::land(vl::land(vl::ceq(vl::rd(vp), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(tie_low), vl::logic<1>(0x1ULL))));
    vl::wr(ddl_clk_outn, v);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_DDL_4bit::gate_ddl_clk_outp01() {
    const vl::logic<1> v = vl::bufif1_(vl::rd(ddl_clk_outp_int), vl::land(vl::land(vl::ceq(vl::rd(vp), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(tie_low), vl::logic<1>(0x1ULL))));
    vl::wr(ddl_clk_outp, v);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_DDL_4bit::gate_ddl_clk_outn02() {
    const vl::logic<1> v = vl::bufif1_(vl::logic<1>(0x1ULL), vl::land(vl::land(vl::ceq(vl::rd(vp), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(tie_low), vl::logic<1>(0x0ULL))));
    vl::wr(ddl_clk_outn, v);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_DDL_4bit::gate_ddl_clk_outp02() {
    const vl::logic<1> v = vl::bufif1_(vl::logic<1>(0x1ULL), vl::land(vl::land(vl::ceq(vl::rd(vp), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(tie_low), vl::logic<1>(0x0ULL))));
    vl::wr(ddl_clk_outp, v);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_DDL_4bit::glue_LOGIC_phase() { vl::wr(c_LOGIC_phase, rd_syncd_phase().slice<2>(2)); }

void mipi_4_rx_dphy_ns_DPHYHSRX_DDL_4bit::split_LOGIC_y() {
    const vl::logic<4> v = vl::rd(c_LOGIC_y);
    y[0].write(v.bit(0).sl());
    y[1].write(v.bit(1).sl());
    y[2].write(v.bit(2).sl());
    y[3].write(v.bit(3).sl());
}

void mipi_4_rx_dphy_ns_DPHYHSRX_DDL_4bit::split_LOGIC_yz() {
    const vl::logic<4> v = vl::rd(c_LOGIC_yz);
    yz[0].write(v.bit(0).sl());
    yz[1].write(v.bit(1).sl());
    yz[2].write(v.bit(2).sl());
    yz[3].write(v.bit(3).sl());
}

void mipi_4_rx_dphy_ns_DPHYHSRX_DDL_4bit::glue_PI_N_0_word() { vl::wr(c_PI_N_0_word, rd_syncd_phase().slice<2>(0)); }

void mipi_4_rx_dphy_ns_DPHYHSRX_DDL_4bit::glue_PI_P_0_word() { vl::wr(c_PI_P_0_word, rd_syncd_phase().slice<2>(0)); }

void mipi_4_rx_dphy_ns_DPHYHSRX_DDL_4bit::proc_initial_154668() {
    double ddl_delay_sh = ddl_delay.read();
    ddl_delay_sh = 0.75;
    if (!ddl_delay_forced_) ddl_delay.write(ddl_delay_sh);
    inv_tristate_delay = 0.0;
    for (;;) {
        wait(ddl_delay.value_changed_event());
        ddl_delay_sh = ddl_delay.read();
        BUF_CELL_3->BUFF_CELLP->INV_FORWARD->force_ddl_delay(ddl_delay_sh);
        BUF_CELL_2->BUFF_CELLP->INV_FORWARD->force_ddl_delay(ddl_delay_sh);
        BUF_CELL_1->BUFF_CELLP->INV_FORWARD->force_ddl_delay(ddl_delay_sh);
        BUF_CELL_0->BUFF_CELLP->INV_FORWARD->force_ddl_delay(ddl_delay_sh);
        BUF_CELL_3->BUFF_CELLP->INV_BACK->force_ddl_delay(ddl_delay_sh);
        BUF_CELL_2->BUFF_CELLP->INV_BACK->force_ddl_delay(ddl_delay_sh);
        BUF_CELL_1->BUFF_CELLP->INV_BACK->force_ddl_delay(ddl_delay_sh);
        BUF_CELL_0->BUFF_CELLP->INV_BACK->force_ddl_delay(ddl_delay_sh);
        BUF_CELL_3->BUFF_CELLN->INV_FORWARD->force_ddl_delay(ddl_delay_sh);
        BUF_CELL_2->BUFF_CELLN->INV_FORWARD->force_ddl_delay(ddl_delay_sh);
        BUF_CELL_1->BUFF_CELLN->INV_FORWARD->force_ddl_delay(ddl_delay_sh);
        BUF_CELL_0->BUFF_CELLN->INV_FORWARD->force_ddl_delay(ddl_delay_sh);
        BUF_CELL_3->BUFF_CELLN->INV_BACK->force_ddl_delay(ddl_delay_sh);
        BUF_CELL_2->BUFF_CELLN->INV_BACK->force_ddl_delay(ddl_delay_sh);
        BUF_CELL_1->BUFF_CELLN->INV_BACK->force_ddl_delay(ddl_delay_sh);
        BUF_CELL_0->BUFF_CELLN->INV_BACK->force_ddl_delay(ddl_delay_sh);
        BUF_CELL_NEXT->FEED_BACK_P->force_ddl_delay(ddl_delay_sh);
        BUF_CELL_NEXT->I402->force_ddl_delay(ddl_delay_sh);
        BUF_CELL_NEXT->I387->force_ddl_delay(ddl_delay_sh);
        BUF_CELL_NEXT->I403->force_ddl_delay(ddl_delay_sh);
        PI_P_0->force_pi_delay((ddl_delay_sh / 2.0));
        PI_N_0->force_pi_delay((ddl_delay_sh / 2.0));
        BUF_CELL_3->BUFF_CELLP->INV_TRISTATE->force_ddl_delay(inv_tristate_delay);
        BUF_CELL_2->BUFF_CELLP->INV_TRISTATE->force_ddl_delay(inv_tristate_delay);
        BUF_CELL_1->BUFF_CELLP->INV_TRISTATE->force_ddl_delay(inv_tristate_delay);
        BUF_CELL_0->BUFF_CELLP->INV_TRISTATE->force_ddl_delay(inv_tristate_delay);
        BUF_CELL_3->BUFF_CELLN->INV_TRISTATE->force_ddl_delay(inv_tristate_delay);
        BUF_CELL_2->BUFF_CELLN->INV_TRISTATE->force_ddl_delay(inv_tristate_delay);
        BUF_CELL_1->BUFF_CELLN->INV_TRISTATE->force_ddl_delay(inv_tristate_delay);
        BUF_CELL_0->BUFF_CELLN->INV_TRISTATE->force_ddl_delay(inv_tristate_delay);
    }
}
