//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_mpll_mux2
//--------------------------------------------------------------------------------------------------
#include "mipi_pll_dphy_mpll_mux2.h"

mipi_pll_dphy_mpll_mux2::mipi_pll_dphy_mpll_mux2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(split_in);
    sensitive << in;
    SC_METHOD(split_s);
    sensitive << s;
    SC_METHOD(gate_buf_si0);
    sensitive << in_b[0] << si[0] << siz[0];
    SC_METHOD(gate_buf_si1);
    sensitive << in_b[1] << si[1] << siz[1];
    SC_METHOD(gate_buf_si2);
    sensitive << in_b[2] << si[2] << siz[2];
    SC_METHOD(gate_buf_si3);
    sensitive << in_b[3] << si[3] << siz[3];
    SC_METHOD(gate_buf_power_off);
    sensitive << pwr_off;
    I13.reset(new mipi_pll_dphy_wire_cap("I13"));
    I13->w(outb);
    I12.reset(new mipi_pll_dphy_wire_cap("I12"));
    I12->w(in_b[1]);
    I11.reset(new mipi_pll_dphy_wire_cap("I11"));
    I11->w(outb);
    I14.reset(new mipi_pll_dphy_wire_cap("I14"));
    I14->w(in_b[2]);
    wire_cap2.reset(new mipi_pll_dphy_wire_cap("wire_cap2"));
    wire_cap2->w(in_b[0]);
    I17.reset(new mipi_pll_dphy_wire_cap("I17"));
    I17->w(in_b[3]);
    I15.reset(new mipi_pll_dphy_wire_cap("I15"));
    I15->w(outb);
    I16.reset(new mipi_pll_dphy_wire_cap("I16"));
    I16->w(outb);
    I31.reset(new mipi_pll_dphy_sc_invx16_lvt("I31"));
    I31->a(outb);
    I31->z(net214);
    I31->gd(gd);
    I31->vp(vp);
    I32.reset(new mipi_pll_dphy_sc_invx16_lvt("I32"));
    I32->a(net214);
    I32->z(out);
    I32->gd(gd);
    I32->vp(vp);
    I29.reset(new mipi_pll_dphy_sc_no2x12_lvt("I29"));
    I29->a(net208);
    I29->b(pwr_off);
    I29->z(si[3]);
    I29->gd(gd);
    I29->vp(vp);
    I28.reset(new mipi_pll_dphy_sc_no2x12_lvt("I28"));
    I28->a(net206);
    I28->b(pwr_off);
    I28->z(si[2]);
    I28->gd(gd);
    I28->vp(vp);
    I25.reset(new mipi_pll_dphy_sc_no2x12_lvt("I25"));
    I25->a(net204);
    I25->b(pwr_off);
    I25->z(si[1]);
    I25->gd(gd);
    I25->vp(vp);
    I23.reset(new mipi_pll_dphy_sc_no2x12_lvt("I23"));
    I23->a(net202);
    I23->b(pwr_off);
    I23->z(si[0]);
    I23->gd(gd);
    I23->vp(vp);
    I27.reset(new mipi_pll_dphy_sc_invx8_lvt("I27"));
    I27->a(s_b[3]);
    I27->z(net208);
    I27->gd(gd);
    I27->vp(vp);
    I26.reset(new mipi_pll_dphy_sc_invx8_lvt("I26"));
    I26->a(s_b[2]);
    I26->z(net206);
    I26->gd(gd);
    I26->vp(vp);
    I24.reset(new mipi_pll_dphy_sc_invx8_lvt("I24"));
    I24->a(s_b[1]);
    I24->z(net204);
    I24->gd(gd);
    I24->vp(vp);
    I21.reset(new mipi_pll_dphy_sc_invx8_lvt("I21"));
    I21->a(s_b[0]);
    I21->z(net202);
    I21->gd(gd);
    I21->vp(vp);
    inv7.reset(new mipi_pll_dphy_sc_invx8_lvt("inv7"));
    inv7->a(si[3]);
    inv7->z(siz[3]);
    inv7->gd(gd);
    inv7->vp(vp);
    I9.reset(new mipi_pll_dphy_sc_invx8_lvt("I9"));
    I9->a(si[0]);
    I9->z(siz[0]);
    I9->gd(gd);
    I9->vp(vp);
    I8.reset(new mipi_pll_dphy_sc_invx8_lvt("I8"));
    I8->a(si[2]);
    I8->z(siz[2]);
    I8->gd(gd);
    I8->vp(vp);
    I7.reset(new mipi_pll_dphy_sc_invx8_lvt("I7"));
    I7->a(si[1]);
    I7->z(siz[1]);
    I7->gd(gd);
    I7->vp(vp);
}

void mipi_pll_dphy_mpll_mux2::split_in() {
    const vl::logic<4> v = vl::rd(in);
    in_b[0].write(v.bit(0).sl());
    in_b[1].write(v.bit(1).sl());
    in_b[2].write(v.bit(2).sl());
    in_b[3].write(v.bit(3).sl());
}

void mipi_pll_dphy_mpll_mux2::split_s() {
    const vl::logic<4> v = vl::rd(s);
    s_b[0].write(v.bit(0).sl());
    s_b[1].write(v.bit(1).sl());
    s_b[2].write(v.bit(2).sl());
    s_b[3].write(v.bit(3).sl());
}

vl::logic<4> mipi_pll_dphy_mpll_mux2::rd_in() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(in_b[0]));
    v.set_bit(1, vl::rd(in_b[1]));
    v.set_bit(2, vl::rd(in_b[2]));
    v.set_bit(3, vl::rd(in_b[3]));
    return v;
}

vl::logic<4> mipi_pll_dphy_mpll_mux2::rd_s() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(s_b[0]));
    v.set_bit(1, vl::rd(s_b[1]));
    v.set_bit(2, vl::rd(s_b[2]));
    v.set_bit(3, vl::rd(s_b[3]));
    return v;
}

vl::logic<4> mipi_pll_dphy_mpll_mux2::rd_si() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(si[0]));
    v.set_bit(1, vl::rd(si[1]));
    v.set_bit(2, vl::rd(si[2]));
    v.set_bit(3, vl::rd(si[3]));
    return v;
}

vl::logic<4> mipi_pll_dphy_mpll_mux2::rd_siz() const {
    vl::logic<4> v(0);
    v.set_bit(0, vl::rd(siz[0]));
    v.set_bit(1, vl::rd(siz[1]));
    v.set_bit(2, vl::rd(siz[2]));
    v.set_bit(3, vl::rd(siz[3]));
    return v;
}

// nfinlvt  N12( .B(gd), .D(in[1]), .G(si[1]), .S(outb) );
// nfinlvt  M32( .B(gd), .D(in[0]), .G(si[0]), .S(outb) );
// nfinlvt  N14( .B(gd), .D(in[2]), .G(si[2]), .S(outb) );
// nfinlvt  N20( .B(gd), .D(outb), .G(pwr_off), .S(gd) );
// nfinlvt  N16( .B(gd), .D(in[3]), .G(si[3]), .S(outb) );
// pfinlvt  P12( .B(vp), .D(in[1]), .G(siz[1]), .S(outb) );
// pfinlvt  M34( .B(vp), .D(in[0]), .G(siz[0]), .S(outb) );
// pfinlvt  P16( .B(vp), .D(in[3]), .G(siz[3]), .S(outb) );
// pfinlvt  P14( .B(vp), .D(in[2]), .G(siz[2]), .S(outb) );
void mipi_pll_dphy_mpll_mux2::gate_buf_si0() {
    const vl::logic<1> v = vl::bufif1_(vl::rd(in_b[0]), vl::land(vl::ceq(vl::rd(si[0]), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(siz[0]), vl::logic<1>(0x0ULL))));
    vl::wr(outb, v);
}

void mipi_pll_dphy_mpll_mux2::gate_buf_si1() {
    const vl::logic<1> v = vl::bufif1_(vl::rd(in_b[1]), vl::land(vl::ceq(vl::rd(si[1]), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(siz[1]), vl::logic<1>(0x0ULL))));
    vl::wr(outb, v);
}

void mipi_pll_dphy_mpll_mux2::gate_buf_si2() {
    const vl::logic<1> v = vl::bufif1_(vl::rd(in_b[2]), vl::land(vl::ceq(vl::rd(si[2]), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(siz[2]), vl::logic<1>(0x0ULL))));
    vl::wr(outb, v);
}

void mipi_pll_dphy_mpll_mux2::gate_buf_si3() {
    const vl::logic<1> v = vl::bufif1_(vl::rd(in_b[3]), vl::land(vl::ceq(vl::rd(si[3]), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(siz[3]), vl::logic<1>(0x0ULL))));
    vl::wr(outb, v);
}

void mipi_pll_dphy_mpll_mux2::gate_buf_power_off() {
    const vl::logic<1> v = vl::bufif1_(vl::logic<1>(0x0ULL), vl::ceq(vl::rd(pwr_off), vl::logic<1>(0x1ULL)));
    vl::wr(outb, v);
}
