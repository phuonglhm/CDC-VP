//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_slicer_latch_ptype_nand
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYHSRX_slicer_latch_ptype_nand.h"

mipi_4_rx_dphy_ns_DPHYHSRX_slicer_latch_ptype_nand::mipi_4_rx_dphy_ns_DPHYHSRX_slicer_latch_ptype_nand(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(assign_supply_ok);
    sensitive << vp << gd;
    SC_METHOD(assign_clk_int);
    sensitive << clk << clk_n;
    SC_METHOD(gate_vi_m_inv_buf);
    sensitive << vi_m << supply_ok;
    SC_METHOD(gate_vi_p_inv_buf);
    sensitive << vi_p << supply_ok;
    invc.reset(new mipi_4_rx_dphy_ns_sc_invx24_lvt("invc"));
    invc->a(clk);
    invc->z(clk_n);
    invc->gd(gd);
    invc->vp(vp);
    invq.reset(new mipi_4_rx_dphy_ns_sc_invx48_lvt("invq"));
    invq->a(vm5_storage);
    invq->z(q);
    invq->gd(gd);
    invq->vp(vp);
    SC_METHOD(proc_initial_158279);
    SC_METHOD(proc_initial_158280);
    SC_METHOD(proc_always_158286);
    sensitive << supply_ok << vi_m_inv << vi_p_inv << vp4;
    SC_METHOD(proc_always_158317);
    sensitive << supply_ok << clk_int << vp4 << vm5_storage;
}

// pfinlvt  M0( .B(vp), .D(vp4), .G(clk_n), .S(vm5_storage) );
// pfinlvt  M14( .B(vp), .D(kp), .G(vm5_storage), .S(vp) );
// pfinlvt  M15( .B(vp), .D(vm5_storage), .G(clk), .S(sp) );
// pfinlvt  M16( .B(vp), .D(sp), .G(kp), .S(vp) );
// pfinlvt  P30( .B(vp), .D(vp4), .G(vm4), .S(vp) );
// pfinlvt  P31( .B(vp), .D(vp4), .G(vi_m_inv), .S(vp) );
// pfinlvt  P32( .B(vp), .D(vm4), .G(vp4), .S(vp) );
// pfinlvt  P33( .B(vp), .D(vm4), .G(vi_p_inv), .S(vp) );
// pfinlvt  P55( .B(vp), .D(vi_m_inv), .G(vi_m), .S(vp) );
// pfinlvt  P59( .B(vp), .D(vi_p_inv), .G(vi_p), .S(vp) );
// wire_cap  I10( .w(vp4) );
// wire_cap  I11( .w(vm4) );
// wire_cap  I12( .w(vm5_storage) );
// wire_cap  I13( .w(kp) );
// wire_cap  I48( .w(vi_m_inv) );
// wire_cap  I49( .w(vi_p_inv) );
// wire_cap  I50( .w(net424) );
// wire_cap  I51( .w(net426) );
// wire_cap  I177( .w(clk_n) );
// nfinlvt  M2( .B(gd), .D(vp4), .G(clk), .S(vm5_storage) );
// nfinlvt  M17( .B(gd), .D(vm5_storage), .G(clk_n), .S(sn) );
// nfinlvt  M18( .B(gd), .D(kp), .G(vm5_storage), .S(gd) );
// nfinlvt  M19( .B(gd), .D(sn), .G(kp), .S(gd) );
// nfinlvt  N30( .B(gd), .D(net424), .G(vi_m_inv), .S(gd) );
// nfinlvt  N31( .B(gd), .D(vp4), .G(vm4), .S(net424) );
// nfinlvt  N32( .B(gd), .D(net426), .G(vi_p_inv), .S(gd) );
// nfinlvt  N33( .B(gd), .D(vm4), .G(vp4), .S(net426) );
// nfinlvt  N55( .B(gd), .D(vi_m_inv), .G(vi_m), .S(gd) );
// nfinlvt  N59( .B(gd), .D(vi_p_inv), .G(vi_p), .S(gd) );
// // Instances of basic/opin/symbol
void mipi_4_rx_dphy_ns_DPHYHSRX_slicer_latch_ptype_nand::assign_supply_ok() {
    vl::wr(supply_ok, vl::land(vl::ceq(vl::rd(vp), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))));
}

void mipi_4_rx_dphy_ns_DPHYHSRX_slicer_latch_ptype_nand::assign_clk_int() {
    vl::wr(clk_int, vl::land(vl::ceq(vl::rd(clk), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(clk_n), vl::logic<1>(0x0ULL))));
}

void mipi_4_rx_dphy_ns_DPHYHSRX_slicer_latch_ptype_nand::gate_vi_m_inv_buf() {
    const vl::logic<1> v = vl::bufif1_(~vl::rd(vi_m), vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL)));
    vl::wr(vi_m_inv, v);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_slicer_latch_ptype_nand::gate_vi_p_inv_buf() {
    const vl::logic<1> v = vl::bufif1_(~vl::rd(vi_p), vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL)));
    vl::wr(vi_p_inv, v);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_slicer_latch_ptype_nand::proc_initial_158279() {
    vl::logic<1> vp4_sh = vl::rd(vp4);
    vp4_sh = vl::logic<1>(0x1ULL);
    vl::wr(vp4, vp4_sh);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_slicer_latch_ptype_nand::proc_initial_158280() {
    vm4 = vl::logic<1>(0x0ULL);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_slicer_latch_ptype_nand::proc_always_158286() {
    if (vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL)).test()) {
        if (vl::ceq(vl::rd(vi_m_inv), vl::rd(vi_p_inv)).test()) {
            vl::wr(vp4, vl::rd(vp4));
        } else {
            if (vl::ceq(vl::rd(vi_m_inv), vl::logic<1>(0x0ULL)).test()) {
                vl::wr(vp4, vl::logic<1>(0x1ULL));
            } else {
                vl::wr(vp4, vl::logic<1>(0x0ULL));
            }
        }
    } else {
        vl::wr(vp4, vl::logic<1>::all_z());
    }
}

// if(supply_ok===1'b1) begin
// if(vi_p_inv===1'b0 || vp4===1'b0)
// vm4 <= 1'b1;
// else if(vp4===1'b1 && vi_p_inv===1'b1)
// vm4 <= 1'b0;
// else
// vm4 <= 1'b0;
// end
// else begin
// vm4 <= 1'bz;
// end
// end
void mipi_4_rx_dphy_ns_DPHYHSRX_slicer_latch_ptype_nand::proc_always_158317() {
    if (vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL)).test()) {
        if (vl::ceq(vl::rd(clk_int), vl::logic<1>(0x1ULL)).test()) {
            vl::wr(vm5_storage, vl::rd(vp4));
        } else {
            vl::wr(vm5_storage, vl::rd(vm5_storage));
        }
    } else {
        vl::wr(vm5_storage, vl::logic<1>::all_z());
    }
}
