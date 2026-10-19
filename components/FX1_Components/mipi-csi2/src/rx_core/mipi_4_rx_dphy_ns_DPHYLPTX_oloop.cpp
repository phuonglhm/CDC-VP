//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYLPTX_oloop
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYLPTX_oloop.h"

mipi_4_rx_dphy_ns_DPHYLPTX_oloop::mipi_4_rx_dphy_ns_DPHYLPTX_oloop(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(merge_iocont);
    sensitive << iocont_b[0] << iocont_b[1];
    SC_METHOD(split_lconvs_dinhv);
    sensitive << c_lconvs_dinhv;
    dont_initialize();
    SC_METHOD(split_lconvs_dinhvz);
    sensitive << c_lconvs_dinhvz;
    dont_initialize();
    SC_METHOD(split_lconvs_enioconthv);
    sensitive << c_lconvs_enioconthv;
    dont_initialize();
    lconvs.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_levelconvs("lconvs"));
    lconvs->atb_lptx1200_on(atb_lptx1200_on);
    lconvs->din(din);
    lconvs->ena(ena);
    lconvs->eniocont(eniocont);
    lconvs->enlogic(enlogic);
    lconvs->enpulldwn(enpulldwn);
    lconvs->enulp(enulp);
    lconvs->pon(pon);
    lconvs->sr_bypass(sr_bypass);
    lconvs->atb_lptx1200_on_hv(atb_lptx1200_on_hv);
    lconvs->atb_lptx1200_on_hvz(atb_lptx1200_on_hvz);
    lconvs->dinhv(c_lconvs_dinhv);
    lconvs->dinhvz(c_lconvs_dinhvz);
    lconvs->enahv(enahv);
    lconvs->enahvz(enahvz);
    lconvs->enioconthv(c_lconvs_enioconthv);
    lconvs->enioconthvz(enioconthvz);
    lconvs->enpulldwnhv(enpulldwnhv);
    lconvs->enpulldwnhvz(enpulldwnhvz);
    lconvs->enulphv(enulphv);
    lconvs->enulphvz(enulphvz);
    lconvs->ponhv(ponhv);
    lconvs->ponhvz(ponhvz);
    lconvs->sr_bypasshv(sr_bypasshv);
    lconvs->gd(gd);
    lconvs->vdd(vdd);
    lconvs->vph(vph);
    lconvs->vss(vss);
    ATBsw.reset(new mipi_4_rx_dphy_ns_DPHYATB_testswitch("ATBsw"));
    ATBsw->comm(atb_line);
    ATBsw->pon(atb_lptx1200_on_hv);
    ATBsw->ponz(atb_lptx1200_on_hvz);
    ATBsw->A(vreg);
    ATBsw->gd(gd);
    ATBsw->vph(vph);
    LOG.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_log("LOG"));
    LOG->en(enahv);
    LOG->enulp(enulphv);
    LOG->pon(ponhv);
    LOG->proghv(proghv);
    LOG->setrhv(setrhv);
    LOG->enable(enablehv);
    LOG->enablez(enablehvz);
    LOG->onb(onbhv);
    LOG->onbz(onbhvz);
    LOG->proghvz(proghvz);
    LOG->setrz(setrz);
    LOG->gd(gd);
    LOG->vph(vph);
    drvn.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_core("drvn"));
    drvn->biasn(biasn);
    drvn->biasn_fb(biasn_fb);
    drvn->biasp(biasp);
    drvn->cascn(cascn);
    drvn->cascp(cascp);
    drvn->din(dinhv[0]);
    drvn->dinz(dinhvz[0]);
    drvn->enable(enablehv);
    drvn->enablez(enablehvz);
    drvn->enpulldwn(enpulldwnhv);
    drvn->enulp(enulphv);
    drvn->loopin(loopinn);
    drvn->onb(onbhv);
    drvn->onbz(onbhvz);
    drvn->setr(setrhv);
    drvn->setrz(setrz);
    drvn->sr_bypasshv(sr_bypasshv);
    drvn->tie_high(tie_high);
    drvn->tie_low(tie_low);
    drvn->loopout(loopoutn);
    drvn->dout(dn);
    drvn->gd(gd);
    drvn->vph(vph);
    drvp.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_core("drvp"));
    drvp->biasn(biasn);
    drvp->biasn_fb(biasn_fb);
    drvp->biasp(biasp);
    drvp->cascn(cascn);
    drvp->cascp(cascp);
    drvp->din(dinhv[1]);
    drvp->dinz(dinhvz[1]);
    drvp->enable(enablehv);
    drvp->enablez(enablehvz);
    drvp->enpulldwn(enpulldwnhv);
    drvp->enulp(enulphv);
    drvp->loopin(loopinp);
    drvp->onb(onbhv);
    drvp->onbz(onbhvz);
    drvp->setr(setrhv);
    drvp->setrz(setrz);
    drvp->sr_bypasshv(sr_bypasshv);
    drvp->tie_high(tie_high);
    drvp->tie_low(tie_low);
    drvp->loopout(loopoutp);
    drvp->dout(dp);
    drvp->gd(gd);
    drvp->vph(vph);
    SC_METHOD(glue_BIAS_cur_setz);
    sensitive << proghvz << proghv;
    BIAS.reset(new mipi_4_rx_dphy_ns_DPHYLPTX_bias("BIAS"));
    BIAS->cur_setz(c_BIAS_cur_setz);
    BIAS->pon(ponhv);
    BIAS->ponz(ponhvz);
    BIAS->tie_high(tie_high);
    BIAS->tie_low(tie_low);
    BIAS->vref1200m(vreg);
    BIAS->biasn(biasn);
    BIAS->biasn_fb(biasn_fb);
    BIAS->biasp(biasp);
    BIAS->cascn(cascn);
    BIAS->cascp(cascp);
    BIAS->gd(gd);
    BIAS->ibias(ibias);
    BIAS->vph(vph);
    LowHigh.reset(new mipi_4_rx_dphy_ns_tieLowHigh_HV("LowHigh"));
    LowHigh->high(net652);
    LowHigh->low(tie_lowHV);
    LowHigh->gd(gd);
    LowHigh->vph(vph);
    IOcont_1.reset(new mipi_4_rx_dphy_ns_DPHY_iocont_buffer("IOcont_1"));
    IOcont_1->a(dp);
    IOcont_1->enhv(enioconthv[1]);
    IOcont_1->tielow_hv(tie_lowHV);
    IOcont_1->y(iocont_b[1]);
    IOcont_1->gd(gd);
    IOcont_1->vdd(vdd);
    IOcont_1->vp(vp);
    IOcont_1->vph(vph);
    IOcont_1->vss(vss);
    IOcont_0.reset(new mipi_4_rx_dphy_ns_DPHY_iocont_buffer("IOcont_0"));
    IOcont_0->a(dn);
    IOcont_0->enhv(enioconthv[0]);
    IOcont_0->tielow_hv(tie_lowHV);
    IOcont_0->y(iocont_b[0]);
    IOcont_0->gd(gd);
    IOcont_0->vdd(vdd);
    IOcont_0->vp(vp);
    IOcont_0->vph(vph);
    IOcont_0->vss(vss);
}

void mipi_4_rx_dphy_ns_DPHYLPTX_oloop::merge_iocont() {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(iocont_b[0]));
    v.set_bit(1, vl::rd(iocont_b[1]));
    vl::wr(iocont, v);
}

vl::logic<2> mipi_4_rx_dphy_ns_DPHYLPTX_oloop::rd_dinhv() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(dinhv[0]));
    v.set_bit(1, vl::rd(dinhv[1]));
    return v;
}

vl::logic<2> mipi_4_rx_dphy_ns_DPHYLPTX_oloop::rd_dinhvz() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(dinhvz[0]));
    v.set_bit(1, vl::rd(dinhvz[1]));
    return v;
}

vl::logic<2> mipi_4_rx_dphy_ns_DPHYLPTX_oloop::rd_enioconthv() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(enioconthv[0]));
    v.set_bit(1, vl::rd(enioconthv[1]));
    return v;
}

vl::logic<2> mipi_4_rx_dphy_ns_DPHYLPTX_oloop::rd_iocont() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(iocont_b[0]));
    v.set_bit(1, vl::rd(iocont_b[1]));
    return v;
}

void mipi_4_rx_dphy_ns_DPHYLPTX_oloop::split_lconvs_dinhv() {
    const vl::logic<2> v = vl::rd(c_lconvs_dinhv);
    dinhv[0].write(v.bit(0).sl());
    dinhv[1].write(v.bit(1).sl());
}

void mipi_4_rx_dphy_ns_DPHYLPTX_oloop::split_lconvs_dinhvz() {
    const vl::logic<2> v = vl::rd(c_lconvs_dinhvz);
    dinhvz[0].write(v.bit(0).sl());
    dinhvz[1].write(v.bit(1).sl());
}

void mipi_4_rx_dphy_ns_DPHYLPTX_oloop::split_lconvs_enioconthv() {
    const vl::logic<2> v = vl::rd(c_lconvs_enioconthv);
    enioconthv[0].write(v.bit(0).sl());
    enioconthv[1].write(v.bit(1).sl());
}

void mipi_4_rx_dphy_ns_DPHYLPTX_oloop::glue_BIAS_cur_setz() { vl::wr(c_BIAS_cur_setz, vl::cat(vl::rd(proghvz).bit(2), vl::rd(proghv).bit(1), vl::rd(proghv).bit(0))); }
