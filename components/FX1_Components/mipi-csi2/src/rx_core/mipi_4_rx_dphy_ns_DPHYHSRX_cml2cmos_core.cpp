//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_cml2cmos_core
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYHSRX_cml2cmos_core.h"

mipi_4_rx_dphy_ns_DPHYHSRX_cml2cmos_core::mipi_4_rx_dphy_ns_DPHYHSRX_cml2cmos_core(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(assign_supply_ok);
    sensitive << vp << gd;
    SC_METHOD(assign_enable);
    sensitive << pon << ponz;
    SC_METHOD(gate_to_buff_calm_buf);
    sensitive << a3 << supply_ok;
    SC_METHOD(gate_to_buff_calp_buf);
    sensitive << b3 << supply_ok;
    SC_METHOD(gate_clk_m_cmos_buf);
    sensitive << to_buff_calm << supply_ok;
    SC_METHOD(gate_clk_p_cmos_buf);
    sensitive << to_buff_calp << supply_ok;
    SC_METHOD(proc_always_153741);
    sensitive << supply_ok << enable << ibias;
    SC_METHOD(proc_always_153752);
    sensitive << supply_ok << enable << clk_in_m << clk_in_p << ibias_cgm;
    SC_METHOD(proc_always_153769);
    sensitive << supply_ok << enable << clk_in_m << clk_in_p << ibias_cgm;
}

// wire  net1820;
// wire  net1821;
// wire  b1;
// wire  b2;
// wire  net1823;
// wire  b3;
// wire  net1824;
// wire  ibias_cgm;
// wire  a1;
// wire  a2;
// wire  vcmn;
// wire  a3;
// wire  vcmp;
// nfinlvt  N1( .B(gd), .D(a2), .G(a1), .S(gd) );
// nfinlvt  N2( .B(gd), .D(b2), .G(b1), .S(gd) );
// nfinlvt  N3( .B(gd), .D(a3), .G(b1), .S(gd) );
// nfinlvt  N4( .B(gd), .D(b3), .G(a1), .S(gd) );
// nfinlvt  N373( .B(gd), .D(clk_p_cmos), .G(to_buff_calp), .S(gd) );
// nfinlvt  N374( .B(gd), .D(clk_m_cmos), .G(to_buff_calm), .S(gd) );
// nfinlvt  N381( .B(gd), .D(to_buff_calp), .G(b3), .S(gd) );
// nfinlvt  N382( .B(gd), .D(to_buff_calm), .G(a3), .S(gd) );
// nfinlvt  N425( .B(gd), .D(gd), .G(gd), .S(gd) );
// nfinlvt  NFA( .B(gd), .D(a1), .G(vcmn), .S(gd) );
// nfinlvt  NFB( .B(gd), .D(b1), .G(vcmn), .S(gd) );
// nfinlvt  NNUL0( .B(gd), .D(a2), .G(b1), .S(a2) );
// nfinlvt  NNUL2( .B(gd), .D(b2), .G(a1), .S(b2) );
// nfinlvt  NNUL3( .B(gd), .D(a3), .G(a1), .S(a3) );
// nfinlvt  NNUL4( .B(gd), .D(b3), .G(b1), .S(b3) );
// nfinlvt  NSW1( .B(gd), .D(vcmn), .G(ponz), .S(gd) );
// nfinlvt  NSW2( .B(gd), .D(a1), .G(ponz), .S(gd) );
// nfinlvt  NSW3( .B(gd), .D(b1), .G(ponz), .S(gd) );
// nfinlvt  NSW5( .B(gd), .D(a3), .G(tie_low), .S(gd) );
// nfinlvt  NSW6( .B(gd), .D(b3), .G(ponz), .S(gd) );
// pfinlvt  P0( .B(vp), .D(a1), .G(clk_in_p), .S(ibias_cgm) );
// pfinlvt  P1( .B(vp), .D(b1), .G(clk_in_m), .S(ibias_cgm) );
// pfinlvt  P2( .B(vp), .D(a3), .G(a2), .S(vp) );
// pfinlvt  P3( .B(vp), .D(b3), .G(b2), .S(vp) );
// pfinlvt  P373( .B(vp), .D(clk_p_cmos), .G(to_buff_calp), .S(vp) );
// pfinlvt  P374( .B(vp), .D(clk_m_cmos), .G(to_buff_calm), .S(vp) );
// pfinlvt  P381( .B(vp), .D(to_buff_calp), .G(b3), .S(vp) );
void mipi_4_rx_dphy_ns_DPHYHSRX_cml2cmos_core::assign_supply_ok() {
    vl::wr(supply_ok, vl::land(vl::ceq(vl::rd(vp), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))));
}

void mipi_4_rx_dphy_ns_DPHYHSRX_cml2cmos_core::assign_enable() {
    vl::wr(enable, vl::land(vl::ceq(vl::rd(pon), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(ponz), vl::logic<1>(0x0ULL))));
}

void mipi_4_rx_dphy_ns_DPHYHSRX_cml2cmos_core::gate_to_buff_calm_buf() {
    const vl::logic<1> v = vl::bufif1_(~vl::rd(a3), vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL)));
    vl::wr(to_buff_calm, v);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_cml2cmos_core::gate_to_buff_calp_buf() {
    const vl::logic<1> v = vl::bufif1_(~vl::rd(b3), vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL)));
    vl::wr(to_buff_calp, v);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_cml2cmos_core::gate_clk_m_cmos_buf() {
    const vl::logic<1> v = vl::bufif1_(~vl::rd(to_buff_calm), vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL)));
    vl::wr(clk_m_cmos, v);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_cml2cmos_core::gate_clk_p_cmos_buf() {
    const vl::logic<1> v = vl::bufif1_(~vl::rd(to_buff_calp), vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL)));
    vl::wr(clk_p_cmos, v);
}

void mipi_4_rx_dphy_ns_DPHYHSRX_cml2cmos_core::proc_always_153741() {
    if (vl::rd(supply_ok).test()) {
        if (vl::land(vl::ceq(vl::rd(enable), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(ibias), vl::logic<1>(0x1ULL))).test()) {
            vl::wr(ibias_cgm, vl::logic<1>(0x1ULL));
        } else {
            vl::wr(ibias_cgm, vl::logic<1>(0x0ULL));
        }
    } else {
        vl::wr(ibias_cgm, vl::logic<1>::all_z());
    }
}

void mipi_4_rx_dphy_ns_DPHYHSRX_cml2cmos_core::proc_always_153752() {
    if (vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL)).test()) {
        if (vl::ceq(vl::rd(enable), vl::logic<1>(0x0ULL)).test()) {
            vl::wr(a3, vl::logic<1>(0x0ULL));
        } else {
            if (vl::land(vl::land(vl::ceq(vl::rd(clk_in_m), vl::logic<1>(0x0ULL)), vl::ceq(vl::rd(clk_in_p), vl::logic<1>(0x1ULL))), vl::ceq(vl::rd(ibias_cgm), vl::logic<1>(0x1ULL))).test()) {
                vl::wr(a3, vl::logic<1>(0x0ULL));
            } else if (vl::land(vl::land(vl::ceq(vl::rd(clk_in_m), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(clk_in_p), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(ibias_cgm), vl::logic<1>(0x1ULL))).test()) {
                vl::wr(a3, vl::logic<1>(0x1ULL));
            } else {
                vl::wr(a3, vl::logic<1>());
            }
        }
    } else {
        vl::wr(a3, vl::logic<1>::all_z());
    }
}

void mipi_4_rx_dphy_ns_DPHYHSRX_cml2cmos_core::proc_always_153769() {
    if (vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL)).test()) {
        if (vl::ceq(vl::rd(enable), vl::logic<1>(0x0ULL)).test()) {
            vl::wr(b3, vl::logic<1>(0x0ULL));
        } else {
            if (vl::land(vl::land(vl::ceq(vl::rd(clk_in_m), vl::logic<1>(0x0ULL)), vl::ceq(vl::rd(clk_in_p), vl::logic<1>(0x1ULL))), vl::ceq(vl::rd(ibias_cgm), vl::logic<1>(0x1ULL))).test()) {
                vl::wr(b3, vl::logic<1>(0x1ULL));
            } else if (vl::land(vl::land(vl::ceq(vl::rd(clk_in_m), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(clk_in_p), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(ibias_cgm), vl::logic<1>(0x1ULL))).test()) {
                vl::wr(b3, vl::logic<1>(0x0ULL));
            } else {
                vl::wr(b3, vl::logic<1>());
            }
        }
    } else {
        vl::wr(b3, vl::logic<1>::all_z());
    }
}
