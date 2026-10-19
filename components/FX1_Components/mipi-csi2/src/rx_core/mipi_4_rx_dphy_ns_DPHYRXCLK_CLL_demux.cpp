//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_demux
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_demux.h"

mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_demux::mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_demux(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(assign_supply_ok);
    sensitive << vp << gd;
    SC_METHOD(gate_clk_out_long_p_buf00);
    sensitive << clk_out_long_p_int << vp << gd << pull_long << pull_longz << ddr_clk_sel;
    SC_METHOD(gate_clk_out_long_p_buf01);
    sensitive << vp << gd << pull_long << pull_longz << ddr_clk_sel;
    SC_METHOD(gate_clk_out_long_n_buf00);
    sensitive << clk_out_long_n_int << vp << gd << pull_long << pull_longz << ddr_clk_sel;
    SC_METHOD(gate_clk_out_long_n_buf01);
    sensitive << vp << gd << pull_long << pull_longz << ddr_clk_sel;
    I0.reset(new mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_demux_buf_long("I0"));
    I0->on(buf_on_long);
    I0->onz(buf_on_longz);
    I0->vin(clk_in_p);
    I0->gd(gd);
    I0->vp(vp);
    I0->vout(clk_out_long_n_int);
    I1.reset(new mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_demux_buf_long("I1"));
    I1->on(buf_on_long);
    I1->onz(buf_on_longz);
    I1->vin(clk_in_n);
    I1->gd(gd);
    I1->vp(vp);
    I1->vout(clk_out_long_p_int);
    I48.reset(new mipi_4_rx_dphy_ns_sc_invx24_lvt("I48"));
    I48->a(clk_en);
    I48->z(clk_enz);
    I48->gd(gd);
    I48->vp(vp);
    I52.reset(new mipi_4_rx_dphy_ns_sc_invx24_lvt("I52"));
    I52->a(buf_en_syncz);
    I52->z(buf_en_sync);
    I52->gd(gd);
    I52->vp(vp);
    I63.reset(new mipi_4_rx_dphy_ns_sc_invx24_lvt("I63"));
    I63->a(clk_mux);
    I63->z(clk_mux_z);
    I63->gd(gd);
    I63->vp(vp);
    I10.reset(new mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_demux_buf_short("I10"));
    I10->i(clk_in_p);
    I10->on(buf_on_short);
    I10->onz(buf_on_shortz);
    I10->o(clk_out_short_n_int);
    I10->gd(gd);
    I10->vp(vp);
    I11.reset(new mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_demux_buf_short("I11"));
    I11->i(clk_in_n);
    I11->on(buf_on_short);
    I11->onz(buf_on_shortz);
    I11->o(clk_out_short_p_int);
    I11->gd(gd);
    I11->vp(vp);
    I41.reset(new mipi_4_rx_dphy_ns_sc_syncx10_as_lvt("I41"));
    I41->ck(clk_mux_z);
    I41->d(a2);
    I41->set(ponz);
    I41->qb(buf_en_syncz);
    I41->gd(gd);
    I41->vp(vp);
    I28.reset(new mipi_4_rx_dphy_ns_sc_invx8_lvt("I28"));
    I28->a(en_long);
    I28->z(en_longz);
    I28->gd(gd);
    I28->vp(vp);
    I37.reset(new mipi_4_rx_dphy_ns_sc_invx8_lvt("I37"));
    I37->a(pull_longz);
    I37->z(pull_long);
    I37->gd(gd);
    I37->vp(vp);
    I69.reset(new mipi_4_rx_dphy_ns_sc_invx8_lvt("I69"));
    I69->a(pon);
    I69->z(ponz);
    I69->gd(gd);
    I69->vp(vp);
    dummy_inv_1.reset(new mipi_4_rx_dphy_ns_sc_invx8_lvt("dummy_inv_1"));
    dummy_inv_1->a(clk_in_p);
    dummy_inv_1->z(dummy_clk[1]);
    dummy_inv_1->gd(gd);
    dummy_inv_1->vp(vp);
    dummy_inv_0.reset(new mipi_4_rx_dphy_ns_sc_invx8_lvt("dummy_inv_0"));
    dummy_inv_0->a(clk_out_long_p);
    dummy_inv_0->z(dummy_clk[0]);
    dummy_inv_0->gd(gd);
    dummy_inv_0->vp(vp);
    I30.reset(new mipi_4_rx_dphy_ns_sc_invx64_lvt("I30"));
    I30->a(buf_on_longz);
    I30->z(buf_on_long);
    I30->gd(gd);
    I30->vp(vp);
    I29_0.reset(new mipi_4_rx_dphy_ns_sc_na2x24_lvt("I29_0"));
    I29_0->a(buf_en_sync);
    I29_0->b(en_long);
    I29_0->z(buf_on_longz);
    I29_0->gd(gd);
    I29_0->vp(vp);
    I29_1.reset(new mipi_4_rx_dphy_ns_sc_na2x24_lvt("I29_1"));
    I29_1->a(buf_en_sync);
    I29_1->b(en_long);
    I29_1->z(buf_on_longz);
    I29_1->gd(gd);
    I29_1->vp(vp);
    I29_2.reset(new mipi_4_rx_dphy_ns_sc_na2x24_lvt("I29_2"));
    I29_2->a(buf_en_sync);
    I29_2->b(en_long);
    I29_2->z(buf_on_longz);
    I29_2->gd(gd);
    I29_2->vp(vp);
    I29_3.reset(new mipi_4_rx_dphy_ns_sc_na2x24_lvt("I29_3"));
    I29_3->a(buf_en_sync);
    I29_3->b(en_long);
    I29_3->z(buf_on_longz);
    I29_3->gd(gd);
    I29_3->vp(vp);
    I31_0.reset(new mipi_4_rx_dphy_ns_sc_na2x24_lvt("I31_0"));
    I31_0->a(buf_en_sync);
    I31_0->b(en_longz);
    I31_0->z(buf_on_shortz);
    I31_0->gd(gd);
    I31_0->vp(vp);
    I31_1.reset(new mipi_4_rx_dphy_ns_sc_na2x24_lvt("I31_1"));
    I31_1->a(buf_en_sync);
    I31_1->b(en_longz);
    I31_1->z(buf_on_shortz);
    I31_1->gd(gd);
    I31_1->vp(vp);
    I36.reset(new mipi_4_rx_dphy_ns_sc_no2x6_lvt("I36"));
    I36->a(buf_en_sync);
    I36->b(en_longz);
    I36->z(nor_pull_long);
    I36->gd(gd);
    I36->vp(vp);
    I39.reset(new mipi_4_rx_dphy_ns_sc_no2x6_lvt("I39"));
    I39->a(nor_pull_long);
    I39->b(pull_clk_long);
    I39->z(pull_longz);
    I39->gd(gd);
    I39->vp(vp);
    I32.reset(new mipi_4_rx_dphy_ns_sc_invx32_lvt("I32"));
    I32->a(buf_on_shortz);
    I32->z(buf_on_short);
    I32->gd(gd);
    I32->vp(vp);
    I43.reset(new mipi_4_rx_dphy_ns_sc_syncx10_ar_lvt("I43"));
    I43->ck(clk_mux_z);
    I43->d(clk_enz);
    I43->reset_b(pon);
    I43->qb(a2);
    I43->gd(gd);
    I43->vp(vp);
    I54.reset(new mipi_4_rx_dphy_ns_sc_no2x12_lvt("I54"));
    I54->a(clk_mux_z);
    I54->b(ddr_clk_en);
    I54->z(clk_out_div);
    I54->gd(gd);
    I54->vp(vp);
    I64.reset(new mipi_4_rx_dphy_ns_sc_no2x12_lvt("I64"));
    I64->a(ddr_clk_sel);
    I64->b(buf_en_sync);
    I64->z(ddr_clk_en);
    I64->gd(gd);
    I64->vp(vp);
    I62.reset(new mipi_4_rx_dphy_ns_sc_mux2x8_lvt("I62"));
    I62->a0(clk_in_n);
    I62->a1(clk_out_long_p);
    I62->s(ddr_clk_sel);
    I62->z(clk_mux);
    I62->gd(gd);
    I62->vp(vp);
    SC_METHOD(proc_always_160773);
    sensitive << supply_ok << buf_on_shortz << buf_on_short << clk_out_short_p_int << clk_out_short_n_int;
}

vl::logic<2> mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_demux::rd_dummy_clk() const {
    vl::logic<2> v(0);
    v.set_bit(0, vl::rd(dummy_clk[0]));
    v.set_bit(1, vl::rd(dummy_clk[1]));
    return v;
}

void mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_demux::assign_supply_ok() {
    vl::wr(supply_ok, vl::land(vl::ceq(vl::rd(vp), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))));
}

// if (supply_ok===1'b1) begin
// if(pull_long===1'b1 && pull_longz===1'b0) begin
// clk_out_long_p  <= 1'b1;
// clk_out_long_n  <= 1'b0;
// end
// else begin
// clk_out_long_p  <= clk_out_long_p_int;
// clk_out_long_n  <= clk_out_long_n_int;
// end
// end
// else begin
// clk_out_long_n  <= 1'bz;
// clk_out_long_p  <= 1'bz;
// end
// end
void mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_demux::gate_clk_out_long_p_buf00() {
    const vl::logic<1> v = vl::bufif1_(vl::rd(clk_out_long_p_int), vl::land(vl::land(vl::land(vl::land(vl::eq(vl::rd(vp), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(pull_long), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(pull_longz), vl::logic<1>(0x1ULL))), vl::eq(vl::rd(ddr_clk_sel), vl::logic<1>(0x0ULL))));
    vl::wr(clk_out_long_p, v);
}

void mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_demux::gate_clk_out_long_p_buf01() {
    const vl::logic<1> v = vl::bufif1_(vl::logic<1>(0x1ULL), vl::land(vl::land(vl::land(vl::land(vl::eq(vl::rd(vp), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(pull_long), vl::logic<1>(0x1ULL))), vl::ceq(vl::rd(pull_longz), vl::logic<1>(0x0ULL))), vl::eq(vl::rd(ddr_clk_sel), vl::logic<1>(0x0ULL))));
    vl::wr(clk_out_long_p, v);
}

void mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_demux::gate_clk_out_long_n_buf00() {
    const vl::logic<1> v = vl::bufif1_(vl::rd(clk_out_long_n_int), vl::land(vl::land(vl::land(vl::land(vl::eq(vl::rd(vp), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(pull_long), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(pull_longz), vl::logic<1>(0x1ULL))), vl::eq(vl::rd(ddr_clk_sel), vl::logic<1>(0x0ULL))));
    vl::wr(clk_out_long_n, v);
}

void mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_demux::gate_clk_out_long_n_buf01() {
    const vl::logic<1> v = vl::bufif1_(vl::logic<1>(0x0ULL), vl::land(vl::land(vl::land(vl::land(vl::eq(vl::rd(vp), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(pull_long), vl::logic<1>(0x1ULL))), vl::ceq(vl::rd(pull_longz), vl::logic<1>(0x0ULL))), vl::eq(vl::rd(ddr_clk_sel), vl::logic<1>(0x0ULL))));
    vl::wr(clk_out_long_n, v);
}

void mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_demux::proc_always_160773() {
    if (vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL)).test()) {
        if (vl::land(vl::ceq(vl::rd(buf_on_shortz), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(buf_on_short), vl::logic<1>(0x0ULL))).test()) {
            vl::wr(clk_out_short_p, vl::logic<1>(0x1ULL));
            vl::wr(clk_out_short_n, vl::logic<1>(0x0ULL));
        } else {
            vl::wr(clk_out_short_p, vl::rd(clk_out_short_p_int));
            vl::wr(clk_out_short_n, vl::rd(clk_out_short_n_int));
        }
    } else {
        vl::wr(clk_out_short_n, vl::logic<1>::all_z());
        vl::wr(clk_out_short_p, vl::logic<1>::all_z());
    }
}
