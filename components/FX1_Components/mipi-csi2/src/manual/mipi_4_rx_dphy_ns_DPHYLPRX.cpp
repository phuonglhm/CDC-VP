#include "mipi_4_rx_dphy_ns_DPHYLPRX.h"

using vl::logic;

namespace {
inline bool is1(const logic<1>& x) { return x.get_bit_val(0) == 1; }
inline bool is0(const logic<1>& x) { return x.get_bit_val(0) == 0; }
}  // namespace

mipi_4_rx_dphy_ns_DPHYLPRX::mipi_4_rx_dphy_ns_DPHYLPRX(sc_core::sc_module_name nm)
    : sc_core::sc_module(nm) {
    // level converters for the comparator enables
    I107.reset(new mipi_4_rx_dphy_ns_LEVELCONV_vp_det_soc("I107"));
    I107->A_in(ponlp); I107->Z(ponlph); I107->Z_neg(ponlphz); I107->gd(gd); I107->vph(vph); I107->vss(vss);
    I140.reset(new mipi_4_rx_dphy_ns_LEVELCONV_vp_det_soc("I140"));
    I140->A_in(ponulp); I140->Z(ponulph); I140->Z_neg(ponulphz); I140->gd(gd); I140->vph(vph); I140->vss(vss);
    I141.reset(new mipi_4_rx_dphy_ns_LEVELCONV_vp_det_soc("I141"));
    I141->A_in(poncd); I141->Z(poncdh); I141->Z_neg(poncdhz); I141->gd(gd); I141->vph(vph); I141->vss(vss);
    // common comparator input enable
    sctg_I133.reset(new mipi_4_rx_dphy_ns_sctg_nand2x("sctg_I133"));
    sctg_I133->a0(ponlphz); sctg_I133->a1(poncdhz); sctg_I133->z(poncmph); sctg_I133->gd(gd); sctg_I133->vph(vph);
    sctg_I134.reset(new mipi_4_rx_dphy_ns_sctg_inv4x("sctg_I134"));
    sctg_I134->a(poncmph); sctg_I134->z(poncmphz); sctg_I134->gd(gd); sctg_I134->vph(vph);
    sctg_I135.reset(new mipi_4_rx_dphy_ns_sctg_inv4x("sctg_I135"));
    sctg_I135->a(prg_on); sctg_I135->z(prg_onz); sctg_I135->gd(gd); sctg_I135->vph(vph);
    // ULP receivers
    ULP_1.reset(new mipi_4_rx_dphy_ns_DPHYLPRX_ULP("ULP_1"));
    ULP_1->ULPout(ulp_1); ULP_1->gd(gd); ULP_1->vdd(vdd); ULP_1->ving(dp_intl); ULP_1->vph(vph);
    ULP_1->vss(vss); ULP_1->pon(ponulp); ULP_1->ponhv(ponulph);
    ULP_0.reset(new mipi_4_rx_dphy_ns_DPHYLPRX_ULP("ULP_0"));
    ULP_0->ULPout(ulp_0); ULP_0->gd(gd); ULP_0->vdd(vdd); ULP_0->ving(dn_intl); ULP_0->vph(vph);
    ULP_0->vss(vss); ULP_0->pon(ponulp); ULP_0->ponhv(ponulph);

    SC_METHOD(eval);                       // dynamic sensitivity, see end_of_elaboration()
    SC_METHOD(stage1);
    sensitive << z_int_[0].ev << z_int_[1].ev << z_int_[2].ev << z_int_[3].ev;
    dont_initialize();
    SC_METHOD(stage2);
    sensitive << z_out_[0].ev << z_out_[1].ev << z_out_[2].ev << z_out_[3].ev;
    dont_initialize();
    SC_METHOD(merge_ulp);
    sensitive << ulp_1 << ulp_0;
}

void mipi_4_rx_dphy_ns_DPHYLPRX::end_of_elaboration() {
    trig_ = dp.value_changed_event() | dn.value_changed_event();
    trig_ |= gd.value_changed_event();
    trig_ |= vdd.value_changed_event();
    trig_ |= vph.value_changed_event();
    trig_ |= vss.value_changed_event();
    trig_ |= ibiascd.value_changed_event();
    trig_ |= ibiaslp.value_changed_event();
    trig_ |= poncd.value_changed_event();
    trig_ |= ponlp.value_changed_event();
    trig_ |= vrefcd.value_changed_event();
    trig_ |= vreflp.value_changed_event();
    trig_ |= poncdh.value_changed_event();
    trig_ |= ponlph.value_changed_event();
    trig_ |= poncmph.value_changed_event();
    trig_ |= poncmphz.value_changed_event();
    if (const vl::sres* s = vl::sres_of(dp)) trig_ |= s->strength_changed_event();
    if (const vl::sres* s = vl::sres_of(dn)) trig_ |= s->strength_changed_event();
}

// nmos pass transistor with strength: supply reduces to strong, gate low isolates
mipi_4_rx_dphy_ns_DPHYLPRX::sv mipi_4_rx_dphy_ns_DPHYLPRX::nmos_s(const sv& d, const logic<1>& g) {
    const int gv = g.get_bit_val(0);
    const int dv = d.v.get_bit_val(0);
    const int s = d.s == vl::ST_SUPPLY ? vl::ST_STRONG : d.s;
    if (dv == 2) return sv{vl::LZ(), vl::ST_HIGHZ};
    if (gv == 1) return sv{d.v, s};
    if (gv == 0) return sv{vl::LZ(), vl::ST_HIGHZ};
    return sv{vl::LX(), s};
}

mipi_4_rx_dphy_ns_DPHYLPRX::sv mipi_4_rx_dphy_ns_DPHYLPRX::resolve_s(const sv& a, const sv& b) {
    if (a.v.get_bit_val(0) == 2 || a.s == vl::ST_HIGHZ) return b;
    if (b.v.get_bit_val(0) == 2 || b.s == vl::ST_HIGHZ) return a;
    if (a.s > b.s) return a;
    if (b.s > a.s) return b;
    if (a.v == b.v) return a;
    return sv{vl::LX(), a.s};
}

void mipi_4_rx_dphy_ns_DPHYLPRX::schedule(stage& st, const logic<1>& v) {
    const sc_core::sc_time d = vl::dly(LPRX_IO_DELAY_RISE / 2.0, TU_, TP_);   // rise == fall
    if (st.pending) {
        if (v == st.pend) return;
        st.pending = false;
        st.ev.cancel();
    }
    if (v == st.out) return;
    st.pend = v;
    st.pending = true;
    st.due = sc_core::sc_time_stamp() + d;
    st.ev.notify(d);
}

void mipi_4_rx_dphy_ns_DPHYLPRX::eval() {
    const logic<1> gd_v = vl::rd(gd), vph_v = vl::rd(vph), vdd_v = vl::rd(vdd), vss_v = vl::rd(vss);
    const bool supply_ok = is1(vph_v) && is0(gd_v);

    // ESD: pads pass to the internal nodes while the ground is valid
    const logic<1> gd_ok = vl::lbool(is0(gd_v));
    const sv dp_inth = nmos_s(sv{vl::rd(dp), vl::strength_of(dp)}, gd_ok);
    const sv dn_inth = nmos_s(sv{vl::rd(dn), vl::strength_of(dn)}, gd_ok);
    vl::wr(dp_intl, dp_inth.v);
    vl::wr(dn_intl, dn_inth.v);

    // comparator input selection: pad when enabled, ground otherwise
    const logic<1> pcm = vl::rd(poncmph), pcmz = vl::rd(poncmphz), sok = vl::lbool(supply_ok);
    const sv gnd{gd_v, vl::strength_of(gd)};
    const sv dp_in = nmos_s(resolve_s(nmos_s(gnd, pcmz), nmos_s(dp_inth, pcm)), sok);
    const sv dn_in = nmos_s(resolve_s(nmos_s(gnd, pcmz), nmos_s(dn_inth, pcm)), sok);
    const sv vrefcd_l = nmos_s(resolve_s(nmos_s(gnd, pcmz), nmos_s(sv{vl::rd(vrefcd), vl::strength_of(vrefcd)}, pcm)), sok);
    const sv vreflp_l = nmos_s(resolve_s(nmos_s(gnd, pcmz), nmos_s(sv{vl::rd(vreflp), vl::strength_of(vreflp)}, pcm)), sok);

    // comparator bias generators (DPHYLPRX_bias_CD / DPHYLPRX_bias_LP)
    const logic<1> bias_cd = is1(vph_v) ? vl::lbool(!(is1(vl::rd(poncdh)) && is1(vl::rd(ibiascd)))) : vl::LZ();
    const logic<1> bias_lp = is1(vph_v) ? vl::lbool(!(is1(vl::rd(ponlph)) && is1(vl::rd(ibiaslp)))) : vl::LZ();

    const bool rails_ok = is1(vph_v) && is1(vdd_v) && is0(vss_v) && is0(gd_v);
    const bool en_cd = rails_ok && is0(bias_cd) && is1(vrefcd_l.v) && is1(vl::rd(poncd));
    const bool en_lp = rails_ok && is0(bias_lp) && is1(vreflp_l.v) && is1(vl::rd(ponlp));

    // DPHYLPRX_comp_*: the internal trireg (weak X) keeps X unless the input,
    // reduced by the resistive rtran, is stronger than weak - i.e. the pad is
    // driven with strong/supply strength.  Then the comparator output is 0.
    auto comp = [](bool en, const sv& vin) {
        const int v = vin.v.get_bit_val(0);
        const bool intwire_x = !((vin.s >= vl::ST_STRONG) && (v == 0 || v == 1));
        return vl::buf_((en && intwire_x) ? vin.v : vl::L0());
    };
    schedule(z_int_[0], comp(en_cd, dn_in));
    schedule(z_int_[1], comp(en_cd, dp_in));
    schedule(z_int_[2], comp(en_lp, dn_in));
    schedule(z_int_[3], comp(en_lp, dp_in));

    next_trigger(trig_);
}

void mipi_4_rx_dphy_ns_DPHYLPRX::stage1() {
    const sc_core::sc_time now = sc_core::sc_time_stamp();
    for (int k = 0; k < 4; k++) {
        stage& st = z_int_[k];
        if (st.pending && st.due <= now) {
            st.pending = false;
            st.out = st.pend;
            schedule(z_out_[k], vl::buf_(st.out));
        }
    }
}

void mipi_4_rx_dphy_ns_DPHYLPRX::stage2() {
    const sc_core::sc_time now = sc_core::sc_time_stamp();
    for (int k = 0; k < 4; k++) {
        stage& st = z_out_[k];
        if (st.pending && st.due <= now) {
            st.pending = false;
            st.out = st.pend;
        }
    }
    vl::wr(doutcd, vl::cat(z_out_[1].out, z_out_[0].out));
    vl::wr(doutlp, vl::cat(z_out_[3].out, z_out_[2].out));
}

void mipi_4_rx_dphy_ns_DPHYLPRX::merge_ulp() {
    vl::wr(doutulp, vl::cat(vl::rd(ulp_1), vl::rd(ulp_0)));
}
