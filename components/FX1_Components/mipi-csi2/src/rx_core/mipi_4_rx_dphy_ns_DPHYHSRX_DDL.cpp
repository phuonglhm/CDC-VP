//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_DDL
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYHSRX_DDL.h"

mipi_4_rx_dphy_ns_DPHYHSRX_DDL::mipi_4_rx_dphy_ns_DPHYHSRX_DDL(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(split_atb_sw_hv);
    sensitive << atb_sw_hv;
    SC_METHOD(split_phase);
    sensitive << phase;
    SC_METHOD(force_mux_nextn);
    sensitive << nextn__drv << nextn_fev_;
    SC_METHOD(force_mux_nextp);
    sensitive << nextp__drv << nextp_fev_;
    SC_METHOD(gate_buf_ddl_clk_outp);
    sensitive << tune_en << vp;
    SC_METHOD(gate_buf_ddl_clk_outn);
    sensitive << tune_en << vp;
    BUF_CELL_NEXT.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_DDL_DIFF_BUFF_CELL_REP("BUF_CELL_NEXT"));
    BUF_CELL_NEXT->bn(bn);
    BUF_CELL_NEXT->bp(bp);
    BUF_CELL_NEXT->in_n(out_bufn);
    BUF_CELL_NEXT->in_p(out_bufp);
    BUF_CELL_NEXT->on(on);
    BUF_CELL_NEXT->onz(onz);
    BUF_CELL_NEXT->tie_high(tie_high);
    BUF_CELL_NEXT->tie_low(tie_low);
    BUF_CELL_NEXT->out_n(nextn__drv);
    BUF_CELL_NEXT->out_p(nextp__drv);
    BUF_CELL_NEXT->gd(gd);
    BUF_CELL_NEXT->vp(vp);
    GATEN.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_DDL_gate("GATEN"));
    GATEN->i(pi_out2n);
    GATEN->on(gatez);
    GATEN->onz(gate);
    GATEN->pd(gate);
    GATEN->pu(tie_high);
    GATEN->o(clk_outn);
    GATEN->gd(gd);
    GATEN->vp(vp);
    GATEP.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_DDL_gate("GATEP"));
    GATEP->i(pi_out2p);
    GATEP->on(gatez);
    GATEP->onz(gate);
    GATEP->pd(tie_low);
    GATEP->pu(gatez);
    GATEP->o(clk_outp);
    GATEP->gd(gd);
    GATEP->vp(vp);
    sctg_ATB_NV_3.reset(new mipi_4_rx_dphy_ns_sctg_inv1x("sctg_ATB_NV_3"));
    sctg_ATB_NV_3->a(atb_sw_hv_b[3]);
    sctg_ATB_NV_3->z(atb_sw_hvz[3]);
    sctg_ATB_NV_3->gd(gd);
    sctg_ATB_NV_3->vph(vph);
    sctg_ATB_NV_2.reset(new mipi_4_rx_dphy_ns_sctg_inv1x("sctg_ATB_NV_2"));
    sctg_ATB_NV_2->a(atb_sw_hv_b[2]);
    sctg_ATB_NV_2->z(atb_sw_hvz[2]);
    sctg_ATB_NV_2->gd(gd);
    sctg_ATB_NV_2->vph(vph);
    sctg_ATB_NV_1.reset(new mipi_4_rx_dphy_ns_sctg_inv1x("sctg_ATB_NV_1"));
    sctg_ATB_NV_1->a(atb_sw_hv_b[1]);
    sctg_ATB_NV_1->z(atb_sw_hvz[1]);
    sctg_ATB_NV_1->gd(gd);
    sctg_ATB_NV_1->vph(vph);
    sctg_ATB_NV_0.reset(new mipi_4_rx_dphy_ns_sctg_inv1x("sctg_ATB_NV_0"));
    sctg_ATB_NV_0->a(atb_sw_hv_b[0]);
    sctg_ATB_NV_0->z(atb_sw_hvz[0]);
    sctg_ATB_NV_0->gd(gd);
    sctg_ATB_NV_0->vph(vph);
    sc_lvt_I394.reset(new mipi_4_rx_dphy_ns_sc_invx24_lvt("sc_lvt_I394"));
    sc_lvt_I394->a(pi_outp);
    sc_lvt_I394->z(pi_out2p);
    sc_lvt_I394->gd(gd);
    sc_lvt_I394->vp(vp);
    sc_lvt_I399.reset(new mipi_4_rx_dphy_ns_sc_invx24_lvt("sc_lvt_I399"));
    sc_lvt_I399->a(pi_outn);
    sc_lvt_I399->z(pi_out2n);
    sc_lvt_I399->gd(gd);
    sc_lvt_I399->vp(vp);
    I405.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_DDL_MUX_2_1_inv_en("I405"));
    I405->i(nextp);
    I405->on(tune_en);
    I405->onz(tune_enz);
    I405->gd(gd);
    I405->o(ddl_clk_outp);
    I405->vp(vp);
    I406.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_DDL_MUX_2_1_inv_en("I406"));
    I406->i(nextn);
    I406->on(tune_en);
    I406->onz(tune_enz);
    I406->gd(gd);
    I406->o(ddl_clk_outn);
    I406->vp(vp);
    sc_lvt_I393.reset(new mipi_4_rx_dphy_ns_sc_tie_hilow_lvt("sc_lvt_I393"));
    sc_lvt_I393->gd(gd);
    sc_lvt_I393->tie_high(tie_high);
    sc_lvt_I393->tie_low(tie_low);
    sc_lvt_I393->vp(vp);
    SC_METHOD(glue_LOGIC_phase);
    sensitive << syncd_phase[0] << syncd_phase[1] << syncd_phase[2] << syncd_phase[3] << syncd_phase[4];
    SC_METHOD(split_LOGIC_y);
    sensitive << c_LOGIC_y;
    dont_initialize();
    SC_METHOD(split_LOGIC_yz);
    sensitive << c_LOGIC_yz;
    dont_initialize();
    LOGIC.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_DDL_LOGIC("LOGIC"));
    LOGIC->phase(c_LOGIC_phase);
    LOGIC->pon(on);
    LOGIC->tune_en(tune_en);
    LOGIC->tune_enz(tune_enz);
    LOGIC->y(c_LOGIC_y);
    LOGIC->yz(c_LOGIC_yz);
    LOGIC->gd(gd);
    LOGIC->vp(vp);
    SC_METHOD(glue_PI_N_0_word);
    sensitive << syncd_phase[0] << syncd_phase[1] << syncd_phase[2] << syncd_phase[3] << syncd_phase[4];
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
    sensitive << syncd_phase[0] << syncd_phase[1] << syncd_phase[2] << syncd_phase[3] << syncd_phase[4];
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
    sc_lvt_FF_4.reset(new mipi_4_rx_dphy_ns_sc_syncx10_as_lvt("sc_lvt_FF_4"));
    sc_lvt_FF_4->ck(phase_sync);
    sc_lvt_FF_4->d(phase_b[4]);
    sc_lvt_FF_4->set(onz);
    sc_lvt_FF_4->qb(net2193[4]);
    sc_lvt_FF_4->gd(gd);
    sc_lvt_FF_4->vp(vp);
    sc_lvt_FF_3.reset(new mipi_4_rx_dphy_ns_sc_syncx10_as_lvt("sc_lvt_FF_3"));
    sc_lvt_FF_3->ck(phase_sync);
    sc_lvt_FF_3->d(phase_b[3]);
    sc_lvt_FF_3->set(onz);
    sc_lvt_FF_3->qb(net2193[3]);
    sc_lvt_FF_3->gd(gd);
    sc_lvt_FF_3->vp(vp);
    sc_lvt_FF_2.reset(new mipi_4_rx_dphy_ns_sc_syncx10_as_lvt("sc_lvt_FF_2"));
    sc_lvt_FF_2->ck(phase_sync);
    sc_lvt_FF_2->d(phase_b[2]);
    sc_lvt_FF_2->set(onz);
    sc_lvt_FF_2->qb(net2193[2]);
    sc_lvt_FF_2->gd(gd);
    sc_lvt_FF_2->vp(vp);
    sc_lvt_FF_1.reset(new mipi_4_rx_dphy_ns_sc_syncx10_as_lvt("sc_lvt_FF_1"));
    sc_lvt_FF_1->ck(phase_sync);
    sc_lvt_FF_1->d(phase_b[1]);
    sc_lvt_FF_1->set(onz);
    sc_lvt_FF_1->qb(net2193[1]);
    sc_lvt_FF_1->gd(gd);
    sc_lvt_FF_1->vp(vp);
    sc_lvt_FF_0.reset(new mipi_4_rx_dphy_ns_sc_syncx10_as_lvt("sc_lvt_FF_0"));
    sc_lvt_FF_0->ck(phase_sync);
    sc_lvt_FF_0->d(phase_b[0]);
    sc_lvt_FF_0->set(onz);
    sc_lvt_FF_0->qb(net2193[0]);
    sc_lvt_FF_0->gd(gd);
    sc_lvt_FF_0->vp(vp);
    sc_lvt_I462_4.reset(new mipi_4_rx_dphy_ns_sc_invx4_lvt("sc_lvt_I462_4"));
    sc_lvt_I462_4->a(net2193[4]);
    sc_lvt_I462_4->z(syncd_phase[4]);
    sc_lvt_I462_4->gd(gd);
    sc_lvt_I462_4->vp(vp);
    sc_lvt_I462_3.reset(new mipi_4_rx_dphy_ns_sc_invx4_lvt("sc_lvt_I462_3"));
    sc_lvt_I462_3->a(net2193[3]);
    sc_lvt_I462_3->z(syncd_phase[3]);
    sc_lvt_I462_3->gd(gd);
    sc_lvt_I462_3->vp(vp);
    sc_lvt_I462_2.reset(new mipi_4_rx_dphy_ns_sc_invx4_lvt("sc_lvt_I462_2"));
    sc_lvt_I462_2->a(net2193[2]);
    sc_lvt_I462_2->z(syncd_phase[2]);
    sc_lvt_I462_2->gd(gd);
    sc_lvt_I462_2->vp(vp);
    sc_lvt_I462_1.reset(new mipi_4_rx_dphy_ns_sc_invx4_lvt("sc_lvt_I462_1"));
    sc_lvt_I462_1->a(net2193[1]);
    sc_lvt_I462_1->z(syncd_phase[1]);
    sc_lvt_I462_1->gd(gd);
    sc_lvt_I462_1->vp(vp);
    sc_lvt_I462_0.reset(new mipi_4_rx_dphy_ns_sc_invx4_lvt("sc_lvt_I462_0"));
    sc_lvt_I462_0->a(net2193[0]);
    sc_lvt_I462_0->z(syncd_phase[0]);
    sc_lvt_I462_0->gd(gd);
    sc_lvt_I462_0->vp(vp);
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
    BUF_CELL_3->in_back_n(out_bufn_b[3]);
    BUF_CELL_3->in_back_p(out_bufp_b[3]);
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
    BUF_CELL_3->out_forward_n(out_bufn_f[3]);
    BUF_CELL_3->out_forward_p(out_bufp_f[3]);
    BUF_CELL_3->gd(gd);
    BUF_CELL_3->vp(vp);
    BUF_CELL_4.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_DDL_DIFF_BUFF_CELL("BUF_CELL_4"));
    BUF_CELL_4->back(yz[4]);
    BUF_CELL_4->bn(bn);
    BUF_CELL_4->bp(bp);
    BUF_CELL_4->forward(y[4]);
    BUF_CELL_4->in_back_n(out_bufn_b[4]);
    BUF_CELL_4->in_back_p(out_bufp_b[4]);
    BUF_CELL_4->in_forward_n(out_bufn_f[3]);
    BUF_CELL_4->in_forward_p(out_bufp_f[3]);
    BUF_CELL_4->pdn(yz[4]);
    BUF_CELL_4->pdp(tie_low);
    BUF_CELL_4->pun(tie_high);
    BUF_CELL_4->pup(y[4]);
    BUF_CELL_4->tie_high(tie_high);
    BUF_CELL_4->tie_low(tie_low);
    BUF_CELL_4->out_back_n(out_bufn_b[3]);
    BUF_CELL_4->out_back_p(out_bufp_b[3]);
    BUF_CELL_4->out_forward_n(out_bufn_f[4]);
    BUF_CELL_4->out_forward_p(out_bufp_f[4]);
    BUF_CELL_4->gd(gd);
    BUF_CELL_4->vp(vp);
    BUF_CELL_5.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_DDL_DIFF_BUFF_CELL("BUF_CELL_5"));
    BUF_CELL_5->back(yz[5]);
    BUF_CELL_5->bn(bn);
    BUF_CELL_5->bp(bp);
    BUF_CELL_5->forward(y[5]);
    BUF_CELL_5->in_back_n(out_bufn_b[5]);
    BUF_CELL_5->in_back_p(out_bufp_b[5]);
    BUF_CELL_5->in_forward_n(out_bufn_f[4]);
    BUF_CELL_5->in_forward_p(out_bufp_f[4]);
    BUF_CELL_5->pdn(tie_low);
    BUF_CELL_5->pdp(yz[5]);
    BUF_CELL_5->pun(y[5]);
    BUF_CELL_5->pup(tie_high);
    BUF_CELL_5->tie_high(tie_high);
    BUF_CELL_5->tie_low(tie_low);
    BUF_CELL_5->out_back_n(out_bufn_b[4]);
    BUF_CELL_5->out_back_p(out_bufp_b[4]);
    BUF_CELL_5->out_forward_n(out_bufn_f[5]);
    BUF_CELL_5->out_forward_p(out_bufp_f[5]);
    BUF_CELL_5->gd(gd);
    BUF_CELL_5->vp(vp);
    BUF_CELL_6.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_DDL_DIFF_BUFF_CELL("BUF_CELL_6"));
    BUF_CELL_6->back(yz[6]);
    BUF_CELL_6->bn(bn);
    BUF_CELL_6->bp(bp);
    BUF_CELL_6->forward(y[6]);
    BUF_CELL_6->in_back_n(out_bufn_b[6]);
    BUF_CELL_6->in_back_p(out_bufp_b[6]);
    BUF_CELL_6->in_forward_n(out_bufn_f[5]);
    BUF_CELL_6->in_forward_p(out_bufp_f[5]);
    BUF_CELL_6->pdn(yz[6]);
    BUF_CELL_6->pdp(tie_low);
    BUF_CELL_6->pun(tie_high);
    BUF_CELL_6->pup(y[6]);
    BUF_CELL_6->tie_high(tie_high);
    BUF_CELL_6->tie_low(tie_low);
    BUF_CELL_6->out_back_n(out_bufn_b[5]);
    BUF_CELL_6->out_back_p(out_bufp_b[5]);
    BUF_CELL_6->out_forward_n(out_bufn_f[6]);
    BUF_CELL_6->out_forward_p(out_bufp_f[6]);
    BUF_CELL_6->gd(gd);
    BUF_CELL_6->vp(vp);
    BUF_CELL_7.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_DDL_DIFF_BUFF_CELL("BUF_CELL_7"));
    BUF_CELL_7->back(yz[7]);
    BUF_CELL_7->bn(bn);
    BUF_CELL_7->bp(bp);
    BUF_CELL_7->forward(y[7]);
    BUF_CELL_7->in_back_n(lbn);
    BUF_CELL_7->in_back_p(lbp);
    BUF_CELL_7->in_forward_n(out_bufn_f[6]);
    BUF_CELL_7->in_forward_p(out_bufp_f[6]);
    BUF_CELL_7->pdn(tie_low);
    BUF_CELL_7->pdp(yz[7]);
    BUF_CELL_7->pun(y[7]);
    BUF_CELL_7->pup(tie_high);
    BUF_CELL_7->tie_high(tie_high);
    BUF_CELL_7->tie_low(tie_low);
    BUF_CELL_7->out_back_n(out_bufn_b[6]);
    BUF_CELL_7->out_back_p(out_bufp_b[6]);
    BUF_CELL_7->out_forward_n(lbn);
    BUF_CELL_7->out_forward_p(lbp);
    BUF_CELL_7->gd(gd);
    BUF_CELL_7->vp(vp);
    CLK_MUX_P.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_DDL_MUX_2_1("CLK_MUX_P"));
    CLK_MUX_P->clk_in(clk_inp);
    CLK_MUX_P->ddl_clk(nextp);
    CLK_MUX_P->tune_en(tune_en);
    CLK_MUX_P->z(in_bufp);
    CLK_MUX_P->gd(gd);
    CLK_MUX_P->vp(vp);
    I388.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_DDL_MUX_2_1("I388"));
    I388->clk_in(clk_inn);
    I388->ddl_clk(nextn);
    I388->tune_en(tune_en);
    I388->z(in_bufn);
    I388->gd(gd);
    I388->vp(vp);
    // wire_cap  I369( .w(nextp) );
    // wire_cap  I391( .w(nextn) );
    // wire_cap  I419( .w(out_bufp) );
    // wire_cap  I420( .w(out_bufn) );
    // wire_cap  I421( .w(out_bufp_b[0]) );
    // wire_cap  I422( .w(out_bufn_b[0]) );
    // wire_cap  I423( .w(out_bufp_b[1]) );
    // wire_cap  I424( .w(out_bufn_b[1]) );
    // wire_cap  I425( .w(out_bufp_b[2]) );
    // wire_cap  I426( .w(out_bufn_b[2]) );
    // wire_cap  I427( .w(out_bufp_b[3]) );
    // wire_cap  I428( .w(out_bufn_b[3]) );
    // wire_cap  I429( .w(out_bufn_b[4]) );
    // wire_cap  I430( .w(out_bufp_b[4]) );
    // wire_cap  I431( .w(out_bufn_b[5]) );f
    // wire_cap  I432( .w(out_bufp_b[5]) );
    // wire_cap  I433( .w(out_bufn_b[6]) );
    // wire_cap  I434( .w(out_bufp_b[6]) );
    // wire_cap  I435( .w(lbn) );
    // wire_cap  I436( .w(lbp) );
    // wire_cap  I440( .w(in_bufn) );
    // wire_cap  I441( .w(out_bufn_f[4]) );
    // wire_cap  I442( .w(out_bufn_f[5]) );
    // wire_cap  I443( .w(out_bufn_f[6]) );
    // wire_cap  I444( .w(out_bufn_f[3]) );
    // wire_cap  I445( .w(out_bufn_f[1]) );
    // wire_cap  I446( .w(out_bufn_f[2]) );
    // wire_cap  I447( .w(out_bufn_f[0]) );
    // wire_cap  I448( .w(out_bufp_f[4]) );
    // wire_cap  I449( .w(out_bufp_f[5]) );
    // wire_cap  I450( .w(out_bufp_f[6]) );
    // wire_cap  I451( .w(out_bufp_f[3]) );
    // wire_cap  I452( .w(out_bufp_f[1]) );
    // wire_cap  I453( .w(out_bufp_f[2]) );
    // wire_cap  I454( .w(out_bufp_f[0]) );
    // wire_cap  I455( .w(in_bufp) );
    BIAS.reset(new mipi_4_rx_dphy_ns_DPHYHSRX_DDL_BIAS("BIAS"));
    BIAS->bias_ddl(iref);
    BIAS->pon(on);
    BIAS->pon_hv(on_hv);
    BIAS->ponz(onz);
    BIAS->tie_high(tie_high);
    BIAS->tie_low(tie_low);
    BIAS->tune(tune);
    BIAS->bn(bn);
    BIAS->bp(bp);
    BIAS->gd(gd);
    BIAS->vp(vp);
    ATB_GD_BUFF.reset(new mipi_4_rx_dphy_ns_DPHYATB_testswitch_vp("ATB_GD_BUFF"));
    ATB_GD_BUFF->comm(gd);
    ATB_GD_BUFF->pon(atb_sw_hv_b[1]);
    ATB_GD_BUFF->ponz(atb_sw_hvz[1]);
    ATB_GD_BUFF->A(atb_line);
    ATB_GD_BUFF->gd(gd);
    ATB_GD_PI.reset(new mipi_4_rx_dphy_ns_DPHYATB_testswitch_vp("ATB_GD_PI"));
    ATB_GD_PI->comm(gd);
    ATB_GD_PI->pon(atb_sw_hv_b[3]);
    ATB_GD_PI->ponz(atb_sw_hvz[3]);
    ATB_GD_PI->A(atb_line);
    ATB_GD_PI->gd(gd);
    ATB_VDD_BUFF.reset(new mipi_4_rx_dphy_ns_DPHYATB_testswitch_vp("ATB_VDD_BUFF"));
    ATB_VDD_BUFF->comm(vp);
    ATB_VDD_BUFF->pon(atb_sw_hv_b[0]);
    ATB_VDD_BUFF->ponz(atb_sw_hvz[0]);
    ATB_VDD_BUFF->A(atb_line);
    ATB_VDD_BUFF->gd(gd);
    ATB_VDD_PI.reset(new mipi_4_rx_dphy_ns_DPHYATB_testswitch_vp("ATB_VDD_PI"));
    ATB_VDD_PI->comm(vp);
    ATB_VDD_PI->pon(atb_sw_hv_b[2]);
    ATB_VDD_PI->ponz(atb_sw_hvz[2]);
    ATB_VDD_PI->A(atb_line);
    ATB_VDD_PI->gd(gd);
    SC_THREAD(proc_initial_155700);
    set_stack_size(0x20000);
    SC_THREAD(proc_initial_155712);
    set_stack_size(0x20000);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_DDL::split_atb_sw_hv() {
    const vl::logic<4> v = vl::rd(atb_sw_hv);
    atb_sw_hv_b[0].write(v.bit(0).sl());
    atb_sw_hv_b[1].write(v.bit(1).sl());
    atb_sw_hv_b[2].write(v.bit(2).sl());
    atb_sw_hv_b[3].write(v.bit(3).sl());
}

void mipi_4_rx_dphy_ns_DPHYHSRX_DDL::split_phase() {
    const vl::logic<5> v = vl::rd(phase);
    phase_b[0].write(v.bit(0).sl());
    phase_b[1].write(v.bit(1).sl());
    phase_b[2].write(v.bit(2).sl());
    phase_b[3].write(v.bit(3).sl());
    phase_b[4].write(v.bit(4).sl());
}

vl::logic<4> mipi_4_rx_dphy_ns_DPHYHSRX_DDL::rd_atb_sw_hv() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(atb_sw_hv_b[0]));
    v.set_bit(1, vl::rd(atb_sw_hv_b[1]));
    v.set_bit(2, vl::rd(atb_sw_hv_b[2]));
    v.set_bit(3, vl::rd(atb_sw_hv_b[3]));
    return v;
}

vl::logic<4> mipi_4_rx_dphy_ns_DPHYHSRX_DDL::rd_atb_sw_hvz() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(atb_sw_hvz[0]));
    v.set_bit(1, vl::rd(atb_sw_hvz[1]));
    v.set_bit(2, vl::rd(atb_sw_hvz[2]));
    v.set_bit(3, vl::rd(atb_sw_hvz[3]));
    return v;
}

vl::logic<5> mipi_4_rx_dphy_ns_DPHYHSRX_DDL::rd_net2193() const {
    vl::logic<5> v(0);
    v.set_bit(0, vl::rd(net2193[0]));
    v.set_bit(1, vl::rd(net2193[1]));
    v.set_bit(2, vl::rd(net2193[2]));
    v.set_bit(3, vl::rd(net2193[3]));
    v.set_bit(4, vl::rd(net2193[4]));
    return v;
}

vl::logic<7> mipi_4_rx_dphy_ns_DPHYHSRX_DDL::rd_out_bufn_b() const {
    vl::logic<7> v(0);
    v.set_bit(0, vl::rd(out_bufn_b[0]));
    v.set_bit(1, vl::rd(out_bufn_b[1]));
    v.set_bit(2, vl::rd(out_bufn_b[2]));
    v.set_bit(3, vl::rd(out_bufn_b[3]));
    v.set_bit(4, vl::rd(out_bufn_b[4]));
    v.set_bit(5, vl::rd(out_bufn_b[5]));
    v.set_bit(6, vl::rd(out_bufn_b[6]));
    return v;
}

vl::logic<7> mipi_4_rx_dphy_ns_DPHYHSRX_DDL::rd_out_bufn_f() const {
    vl::logic<7> v(0);
    v.set_bit(0, vl::rd(out_bufn_f[0]));
    v.set_bit(1, vl::rd(out_bufn_f[1]));
    v.set_bit(2, vl::rd(out_bufn_f[2]));
    v.set_bit(3, vl::rd(out_bufn_f[3]));
    v.set_bit(4, vl::rd(out_bufn_f[4]));
    v.set_bit(5, vl::rd(out_bufn_f[5]));
    v.set_bit(6, vl::rd(out_bufn_f[6]));
    return v;
}

vl::logic<7> mipi_4_rx_dphy_ns_DPHYHSRX_DDL::rd_out_bufp_b() const {
    vl::logic<7> v(0);
    v.set_bit(0, vl::rd(out_bufp_b[0]));
    v.set_bit(1, vl::rd(out_bufp_b[1]));
    v.set_bit(2, vl::rd(out_bufp_b[2]));
    v.set_bit(3, vl::rd(out_bufp_b[3]));
    v.set_bit(4, vl::rd(out_bufp_b[4]));
    v.set_bit(5, vl::rd(out_bufp_b[5]));
    v.set_bit(6, vl::rd(out_bufp_b[6]));
    return v;
}

vl::logic<7> mipi_4_rx_dphy_ns_DPHYHSRX_DDL::rd_out_bufp_f() const {
    vl::logic<7> v(0);
    v.set_bit(0, vl::rd(out_bufp_f[0]));
    v.set_bit(1, vl::rd(out_bufp_f[1]));
    v.set_bit(2, vl::rd(out_bufp_f[2]));
    v.set_bit(3, vl::rd(out_bufp_f[3]));
    v.set_bit(4, vl::rd(out_bufp_f[4]));
    v.set_bit(5, vl::rd(out_bufp_f[5]));
    v.set_bit(6, vl::rd(out_bufp_f[6]));
    return v;
}

vl::logic<5> mipi_4_rx_dphy_ns_DPHYHSRX_DDL::rd_phase() const {
    vl::logic<5> v(0);
    v.set_bit(0, vl::rd(phase_b[0]));
    v.set_bit(1, vl::rd(phase_b[1]));
    v.set_bit(2, vl::rd(phase_b[2]));
    v.set_bit(3, vl::rd(phase_b[3]));
    v.set_bit(4, vl::rd(phase_b[4]));
    return v;
}

vl::logic<5> mipi_4_rx_dphy_ns_DPHYHSRX_DDL::rd_syncd_phase() const {
    vl::logic<5> v(0);
    v.set_bit(0, vl::rd(syncd_phase[0]));
    v.set_bit(1, vl::rd(syncd_phase[1]));
    v.set_bit(2, vl::rd(syncd_phase[2]));
    v.set_bit(3, vl::rd(syncd_phase[3]));
    v.set_bit(4, vl::rd(syncd_phase[4]));
    return v;
}

vl::logic<8> mipi_4_rx_dphy_ns_DPHYHSRX_DDL::rd_y() const {
    vl::logic<8> v(0);
    v.set_bit(0, vl::rd(y[0]));
    v.set_bit(1, vl::rd(y[1]));
    v.set_bit(2, vl::rd(y[2]));
    v.set_bit(3, vl::rd(y[3]));
    v.set_bit(4, vl::rd(y[4]));
    v.set_bit(5, vl::rd(y[5]));
    v.set_bit(6, vl::rd(y[6]));
    v.set_bit(7, vl::rd(y[7]));
    return v;
}

vl::logic<8> mipi_4_rx_dphy_ns_DPHYHSRX_DDL::rd_yz() const {
    vl::logic<8> v(0);
    v.set_bit(0, vl::rd(yz[0]));
    v.set_bit(1, vl::rd(yz[1]));
    v.set_bit(2, vl::rd(yz[2]));
    v.set_bit(3, vl::rd(yz[3]));
    v.set_bit(4, vl::rd(yz[4]));
    v.set_bit(5, vl::rd(yz[5]));
    v.set_bit(6, vl::rd(yz[6]));
    v.set_bit(7, vl::rd(yz[7]));
    return v;
}

void mipi_4_rx_dphy_ns_DPHYHSRX_DDL::force_mux_nextn() { vl::wr(nextn, nextn_forced_ ? nextn_fval_ : vl::rd(nextn__drv)); }

void mipi_4_rx_dphy_ns_DPHYHSRX_DDL::force_mux_nextp() { vl::wr(nextp, nextp_forced_ ? nextp_fval_ : vl::rd(nextp__drv)); }

// DPHYHSRX_DDL_b2b_out  I381_1( .on(on), .onz(onz), .a(pi_outn),
// .b(pi_outp), .gd(gd), .vp(vp) );
// DPHYHSRX_DDL_b2b_out  I381_0( .on(on), .onz(onz), .a(pi_outn),
// .b(pi_outp), .gd(gd), .vp(vp) );
// DPHYHSRX_DDL_b2b_out  I401( .on(on), .onz(onz), .a(pi_out2n),
// .b(pi_out2p), .gd(gd), .vp(vp) );
void mipi_4_rx_dphy_ns_DPHYHSRX_DDL::gate_buf_ddl_clk_outp() {
    const vl::logic<1> v = vl::bufif1_(vl::logic<1>(0x1ULL), vl::land(vl::ceq(vl::rd(tune_en), vl::logic<1>(0x0ULL)), vl::ceq(vl::rd(vp), vl::logic<1>(0x1ULL))));
    vl::wr(ddl_clk_outp, v);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_DDL::gate_buf_ddl_clk_outn() {
    const vl::logic<1> v = vl::bufif1_(vl::logic<1>(0x1ULL), vl::land(vl::ceq(vl::rd(tune_en), vl::logic<1>(0x0ULL)), vl::ceq(vl::rd(vp), vl::logic<1>(0x1ULL))));
    vl::wr(ddl_clk_outn, v);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_DDL::glue_LOGIC_phase() { vl::wr(c_LOGIC_phase, rd_syncd_phase().slice<3>(2)); }

void mipi_4_rx_dphy_ns_DPHYHSRX_DDL::split_LOGIC_y() {
    const vl::logic<8> v = vl::rd(c_LOGIC_y);
    y[0].write(v.bit(0).sl());
    y[1].write(v.bit(1).sl());
    y[2].write(v.bit(2).sl());
    y[3].write(v.bit(3).sl());
    y[4].write(v.bit(4).sl());
    y[5].write(v.bit(5).sl());
    y[6].write(v.bit(6).sl());
    y[7].write(v.bit(7).sl());
}

void mipi_4_rx_dphy_ns_DPHYHSRX_DDL::split_LOGIC_yz() {
    const vl::logic<8> v = vl::rd(c_LOGIC_yz);
    yz[0].write(v.bit(0).sl());
    yz[1].write(v.bit(1).sl());
    yz[2].write(v.bit(2).sl());
    yz[3].write(v.bit(3).sl());
    yz[4].write(v.bit(4).sl());
    yz[5].write(v.bit(5).sl());
    yz[6].write(v.bit(6).sl());
    yz[7].write(v.bit(7).sl());
}

void mipi_4_rx_dphy_ns_DPHYHSRX_DDL::glue_PI_N_0_word() { vl::wr(c_PI_N_0_word, rd_syncd_phase().slice<2>(0)); }

void mipi_4_rx_dphy_ns_DPHYHSRX_DDL::glue_PI_P_0_word() { vl::wr(c_PI_P_0_word, rd_syncd_phase().slice<2>(0)); }

void mipi_4_rx_dphy_ns_DPHYHSRX_DDL::proc_initial_155700() {
    for (;;) {
        wait(tune_en.posedge_event());
        nextp_fval_ = vl::logic<1>(0x1ULL);
        nextp_forced_ = true;
        nextp_fev_.notify(sc_core::SC_ZERO_TIME);
        nextn_fval_ = vl::logic<1>(0x0ULL);
        nextn_forced_ = true;
        nextn_fev_.notify(sc_core::SC_ZERO_TIME);
        wait(vl::dly((((1000.0 / (10.0 * factor.read())) / (2.0 * vl::to_real(vl::logic<32>(0x8ULL), true))) * 20.0), TU_, TP_));
        nextp_forced_ = false;
        nextp_fev_.notify(sc_core::SC_ZERO_TIME);
        nextn_forced_ = false;
        nextn_fev_.notify(sc_core::SC_ZERO_TIME);
    }
}

void mipi_4_rx_dphy_ns_DPHYHSRX_DDL::proc_initial_155712() {
    double ddl_delay_sh = ddl_delay.read();
    double factor_sh = factor.read();
    ddl_delay_sh = 0.75;
    ddl_delay.write(ddl_delay_sh);
    inv_tristate_delay = 0.0;
    factor_sh = 2.0;
    factor.write(factor_sh);
    for (;;) {
        wait(tune_en.value_changed_event() | tune.value_changed_event());
        ddl_delay_sh = ddl_delay.read();
        factor_sh = factor.read();
        {
            const vl::logic<5> sel = vl::rd(tune);
            if (vl::case_eq(sel, vl::logic<5>(0x0ULL))) {
                ddl_delay_sh = ((1000.0 / (10.0 * factor_sh)) / (2.0 * vl::to_real(vl::logic<32>(0x8ULL), true)));
                ddl_delay.write(ddl_delay_sh);
            } else if (vl::case_eq(sel, vl::logic<5>(0x1ULL))) {
                ddl_delay_sh = ((1000.0 / (26.0 * factor_sh)) / (2.0 * vl::to_real(vl::logic<32>(0x8ULL), true)));
                ddl_delay.write(ddl_delay_sh);
            } else if (vl::case_eq(sel, vl::logic<5>(0x2ULL))) {
                ddl_delay_sh = ((1000.0 / (42.0 * factor_sh)) / (2.0 * vl::to_real(vl::logic<32>(0x8ULL), true)));
                ddl_delay.write(ddl_delay_sh);
            } else if (vl::case_eq(sel, vl::logic<5>(0x3ULL))) {
                ddl_delay_sh = ((1000.0 / (58.0 * factor_sh)) / (2.0 * vl::to_real(vl::logic<32>(0x8ULL), true)));
                ddl_delay.write(ddl_delay_sh);
            } else if (vl::case_eq(sel, vl::logic<5>(0x4ULL))) {
                ddl_delay_sh = ((1000.0 / (74.0 * factor_sh)) / (2.0 * vl::to_real(vl::logic<32>(0x8ULL), true)));
                ddl_delay.write(ddl_delay_sh);
            } else if (vl::case_eq(sel, vl::logic<5>(0x5ULL))) {
                ddl_delay_sh = ((1000.0 / (90.0 * factor_sh)) / (2.0 * vl::to_real(vl::logic<32>(0x8ULL), true)));
                ddl_delay.write(ddl_delay_sh);
            } else if (vl::case_eq(sel, vl::logic<5>(0x6ULL))) {
                ddl_delay_sh = ((1000.0 / (106.0 * factor_sh)) / (2.0 * vl::to_real(vl::logic<32>(0x8ULL), true)));
                ddl_delay.write(ddl_delay_sh);
            } else if (vl::case_eq(sel, vl::logic<5>(0x7ULL))) {
                ddl_delay_sh = ((1000.0 / (122.0 * factor_sh)) / (2.0 * vl::to_real(vl::logic<32>(0x8ULL), true)));
                ddl_delay.write(ddl_delay_sh);
            } else if (vl::case_eq(sel, vl::logic<5>(0x8ULL))) {
                ddl_delay_sh = ((1000.0 / (138.0 * factor_sh)) / (2.0 * vl::to_real(vl::logic<32>(0x8ULL), true)));
                ddl_delay.write(ddl_delay_sh);
            } else if (vl::case_eq(sel, vl::logic<5>(0x9ULL))) {
                ddl_delay_sh = ((1000.0 / (154.0 * factor_sh)) / (2.0 * vl::to_real(vl::logic<32>(0x8ULL), true)));
                ddl_delay.write(ddl_delay_sh);
            } else if (vl::case_eq(sel, vl::logic<5>(0xaULL))) {
                ddl_delay_sh = ((1000.0 / (170.0 * factor_sh)) / (2.0 * vl::to_real(vl::logic<32>(0x8ULL), true)));
                ddl_delay.write(ddl_delay_sh);
            } else if (vl::case_eq(sel, vl::logic<5>(0xbULL))) {
                ddl_delay_sh = ((1000.0 / (186.0 * factor_sh)) / (2.0 * vl::to_real(vl::logic<32>(0x8ULL), true)));
                ddl_delay.write(ddl_delay_sh);
            } else if (vl::case_eq(sel, vl::logic<5>(0xcULL))) {
                ddl_delay_sh = ((1000.0 / (202.0 * factor_sh)) / (2.0 * vl::to_real(vl::logic<32>(0x8ULL), true)));
                ddl_delay.write(ddl_delay_sh);
            } else if (vl::case_eq(sel, vl::logic<5>(0xdULL))) {
                ddl_delay_sh = ((1000.0 / (218.0 * factor_sh)) / (2.0 * vl::to_real(vl::logic<32>(0x8ULL), true)));
                ddl_delay.write(ddl_delay_sh);
            } else if (vl::case_eq(sel, vl::logic<5>(0xeULL))) {
                ddl_delay_sh = ((1000.0 / (234.0 * factor_sh)) / (2.0 * vl::to_real(vl::logic<32>(0x8ULL), true)));
                ddl_delay.write(ddl_delay_sh);
            } else if (vl::case_eq(sel, vl::logic<5>(0xfULL))) {
                ddl_delay_sh = ((1000.0 / (250.0 * factor_sh)) / (2.0 * vl::to_real(vl::logic<32>(0x8ULL), true)));
                ddl_delay.write(ddl_delay_sh);
            } else if (vl::case_eq(sel, vl::logic<5>(0x10ULL))) {
                ddl_delay_sh = ((1000.0 / (266.0 * factor_sh)) / (2.0 * vl::to_real(vl::logic<32>(0x8ULL), true)));
                ddl_delay.write(ddl_delay_sh);
            } else if (vl::case_eq(sel, vl::logic<5>(0x11ULL))) {
                ddl_delay_sh = ((1000.0 / (282.0 * factor_sh)) / (2.0 * vl::to_real(vl::logic<32>(0x8ULL), true)));
                ddl_delay.write(ddl_delay_sh);
            } else if (vl::case_eq(sel, vl::logic<5>(0x12ULL))) {
                ddl_delay_sh = ((1000.0 / (298.0 * factor_sh)) / (2.0 * vl::to_real(vl::logic<32>(0x8ULL), true)));
                ddl_delay.write(ddl_delay_sh);
            } else if (vl::case_eq(sel, vl::logic<5>(0x13ULL))) {
                ddl_delay_sh = ((1000.0 / (314.0 * factor_sh)) / (2.0 * vl::to_real(vl::logic<32>(0x8ULL), true)));
                ddl_delay.write(ddl_delay_sh);
            } else if (vl::case_eq(sel, vl::logic<5>(0x14ULL))) {
                ddl_delay_sh = ((1000.0 / (330.0 * factor_sh)) / (2.0 * vl::to_real(vl::logic<32>(0x8ULL), true)));
                ddl_delay.write(ddl_delay_sh);
            } else if (vl::case_eq(sel, vl::logic<5>(0x15ULL))) {
                ddl_delay_sh = ((1000.0 / (346.0 * factor_sh)) / (2.0 * vl::to_real(vl::logic<32>(0x8ULL), true)));
                ddl_delay.write(ddl_delay_sh);
            } else if (vl::case_eq(sel, vl::logic<5>(0x16ULL))) {
                ddl_delay_sh = ((1000.0 / (362.0 * factor_sh)) / (2.0 * vl::to_real(vl::logic<32>(0x8ULL), true)));
                ddl_delay.write(ddl_delay_sh);
            } else if (vl::case_eq(sel, vl::logic<5>(0x17ULL))) {
                ddl_delay_sh = ((1000.0 / (378.0 * factor_sh)) / (2.0 * vl::to_real(vl::logic<32>(0x8ULL), true)));
                ddl_delay.write(ddl_delay_sh);
            } else if (vl::case_eq(sel, vl::logic<5>(0x18ULL))) {
                ddl_delay_sh = ((1000.0 / (394.0 * factor_sh)) / (2.0 * vl::to_real(vl::logic<32>(0x8ULL), true)));
                ddl_delay.write(ddl_delay_sh);
            } else if (vl::case_eq(sel, vl::logic<5>(0x19ULL))) {
                ddl_delay_sh = ((1000.0 / (410.0 * factor_sh)) / (2.0 * vl::to_real(vl::logic<32>(0x8ULL), true)));
                ddl_delay.write(ddl_delay_sh);
            } else if (vl::case_eq(sel, vl::logic<5>(0x1aULL))) {
                ddl_delay_sh = ((1000.0 / (426.0 * factor_sh)) / (2.0 * vl::to_real(vl::logic<32>(0x8ULL), true)));
                ddl_delay.write(ddl_delay_sh);
            } else if (vl::case_eq(sel, vl::logic<5>(0x1bULL))) {
                ddl_delay_sh = ((1000.0 / (442.0 * factor_sh)) / (2.0 * vl::to_real(vl::logic<32>(0x8ULL), true)));
                ddl_delay.write(ddl_delay_sh);
            } else if (vl::case_eq(sel, vl::logic<5>(0x1cULL))) {
                ddl_delay_sh = ((1000.0 / (458.0 * factor_sh)) / (2.0 * vl::to_real(vl::logic<32>(0x8ULL), true)));
                ddl_delay.write(ddl_delay_sh);
            } else if (vl::case_eq(sel, vl::logic<5>(0x1dULL))) {
                ddl_delay_sh = ((1000.0 / (474.0 * factor_sh)) / (2.0 * vl::to_real(vl::logic<32>(0x8ULL), true)));
                ddl_delay.write(ddl_delay_sh);
            } else if (vl::case_eq(sel, vl::logic<5>(0x1eULL))) {
                ddl_delay_sh = ((1000.0 / (490.0 * factor_sh)) / (2.0 * vl::to_real(vl::logic<32>(0x8ULL), true)));
                ddl_delay.write(ddl_delay_sh);
            } else if (vl::case_eq(sel, vl::logic<5>(0x1fULL))) {
                ddl_delay_sh = ((1000.0 / (506.0 * factor_sh)) / (2.0 * vl::to_real(vl::logic<32>(0x8ULL), true)));
                ddl_delay.write(ddl_delay_sh);
            } else {
                ddl_delay_sh = ((1.0 * 1.0) / (2.0 * vl::to_real(vl::logic<32>(0x8ULL), true)));
                ddl_delay.write(ddl_delay_sh);
            }
        }
        BUF_CELL_7->BUFF_CELLP->INV_FORWARD->force_ddl_delay(ddl_delay_sh);
        BUF_CELL_6->BUFF_CELLP->INV_FORWARD->force_ddl_delay(ddl_delay_sh);
        BUF_CELL_5->BUFF_CELLP->INV_FORWARD->force_ddl_delay(ddl_delay_sh);
        BUF_CELL_4->BUFF_CELLP->INV_FORWARD->force_ddl_delay(ddl_delay_sh);
        BUF_CELL_3->BUFF_CELLP->INV_FORWARD->force_ddl_delay(ddl_delay_sh);
        BUF_CELL_2->BUFF_CELLP->INV_FORWARD->force_ddl_delay(ddl_delay_sh);
        BUF_CELL_1->BUFF_CELLP->INV_FORWARD->force_ddl_delay(ddl_delay_sh);
        BUF_CELL_0->BUFF_CELLP->INV_FORWARD->force_ddl_delay(ddl_delay_sh);
        BUF_CELL_7->BUFF_CELLP->INV_BACK->force_ddl_delay(ddl_delay_sh);
        BUF_CELL_6->BUFF_CELLP->INV_BACK->force_ddl_delay(ddl_delay_sh);
        BUF_CELL_5->BUFF_CELLP->INV_BACK->force_ddl_delay(ddl_delay_sh);
        BUF_CELL_4->BUFF_CELLP->INV_BACK->force_ddl_delay(ddl_delay_sh);
        BUF_CELL_3->BUFF_CELLP->INV_BACK->force_ddl_delay(ddl_delay_sh);
        BUF_CELL_2->BUFF_CELLP->INV_BACK->force_ddl_delay(ddl_delay_sh);
        BUF_CELL_1->BUFF_CELLP->INV_BACK->force_ddl_delay(ddl_delay_sh);
        BUF_CELL_0->BUFF_CELLP->INV_BACK->force_ddl_delay(ddl_delay_sh);
        BUF_CELL_7->BUFF_CELLN->INV_FORWARD->force_ddl_delay(ddl_delay_sh);
        BUF_CELL_6->BUFF_CELLN->INV_FORWARD->force_ddl_delay(ddl_delay_sh);
        BUF_CELL_5->BUFF_CELLN->INV_FORWARD->force_ddl_delay(ddl_delay_sh);
        BUF_CELL_4->BUFF_CELLN->INV_FORWARD->force_ddl_delay(ddl_delay_sh);
        BUF_CELL_3->BUFF_CELLN->INV_FORWARD->force_ddl_delay(ddl_delay_sh);
        BUF_CELL_2->BUFF_CELLN->INV_FORWARD->force_ddl_delay(ddl_delay_sh);
        BUF_CELL_1->BUFF_CELLN->INV_FORWARD->force_ddl_delay(ddl_delay_sh);
        BUF_CELL_0->BUFF_CELLN->INV_FORWARD->force_ddl_delay(ddl_delay_sh);
        BUF_CELL_7->BUFF_CELLN->INV_BACK->force_ddl_delay(ddl_delay_sh);
        BUF_CELL_6->BUFF_CELLN->INV_BACK->force_ddl_delay(ddl_delay_sh);
        BUF_CELL_5->BUFF_CELLN->INV_BACK->force_ddl_delay(ddl_delay_sh);
        BUF_CELL_4->BUFF_CELLN->INV_BACK->force_ddl_delay(ddl_delay_sh);
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
        BUF_CELL_7->BUFF_CELLP->INV_TRISTATE->force_ddl_delay(inv_tristate_delay);
        BUF_CELL_6->BUFF_CELLP->INV_TRISTATE->force_ddl_delay(inv_tristate_delay);
        BUF_CELL_5->BUFF_CELLP->INV_TRISTATE->force_ddl_delay(inv_tristate_delay);
        BUF_CELL_4->BUFF_CELLP->INV_TRISTATE->force_ddl_delay(inv_tristate_delay);
        BUF_CELL_3->BUFF_CELLP->INV_TRISTATE->force_ddl_delay(inv_tristate_delay);
        BUF_CELL_2->BUFF_CELLP->INV_TRISTATE->force_ddl_delay(inv_tristate_delay);
        BUF_CELL_1->BUFF_CELLP->INV_TRISTATE->force_ddl_delay(inv_tristate_delay);
        BUF_CELL_0->BUFF_CELLP->INV_TRISTATE->force_ddl_delay(inv_tristate_delay);
        BUF_CELL_7->BUFF_CELLN->INV_TRISTATE->force_ddl_delay(inv_tristate_delay);
        BUF_CELL_6->BUFF_CELLN->INV_TRISTATE->force_ddl_delay(inv_tristate_delay);
        BUF_CELL_5->BUFF_CELLN->INV_TRISTATE->force_ddl_delay(inv_tristate_delay);
        BUF_CELL_4->BUFF_CELLN->INV_TRISTATE->force_ddl_delay(inv_tristate_delay);
        BUF_CELL_3->BUFF_CELLN->INV_TRISTATE->force_ddl_delay(inv_tristate_delay);
        BUF_CELL_2->BUFF_CELLN->INV_TRISTATE->force_ddl_delay(inv_tristate_delay);
        BUF_CELL_1->BUFF_CELLN->INV_TRISTATE->force_ddl_delay(inv_tristate_delay);
        BUF_CELL_0->BUFF_CELLN->INV_TRISTATE->force_ddl_delay(inv_tristate_delay);
    }
}
