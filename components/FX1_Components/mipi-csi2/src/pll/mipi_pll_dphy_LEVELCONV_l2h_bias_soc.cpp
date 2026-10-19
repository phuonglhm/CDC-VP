//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_LEVELCONV_l2h_bias_soc
//--------------------------------------------------------------------------------------------------
#include "mipi_pll_dphy_LEVELCONV_l2h_bias_soc.h"

mipi_pll_dphy_LEVELCONV_l2h_bias_soc::mipi_pll_dphy_LEVELCONV_l2h_bias_soc(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(assign_supply_ok);
    sensitive << vph << vdd << gd << vss;
    SC_METHOD(assign_inz);
    sensitive << A_in;
    SC_METHOD(assign_Z);
    sensitive << b;
    SC_METHOD(assign_Z_neg);
    sensitive << a;
    SC_METHOD(proc_always_2672);
    sensitive << supply_ok << onz_hv << vblvhv << A_in << tie_low_hv << inz;
}

void mipi_pll_dphy_LEVELCONV_l2h_bias_soc::assign_supply_ok() {
    vl::wr(supply_ok, vl::land(vl::land(vl::land(vl::ceq(vl::rd(vph), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(vdd), vl::logic<1>(0x1ULL))), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))), vl::ceq(vl::rd(vss), vl::logic<1>(0x0ULL))));
}

void mipi_pll_dphy_LEVELCONV_l2h_bias_soc::assign_inz() {
    vl::wr(inz, ~vl::rd(A_in));
}

void mipi_pll_dphy_LEVELCONV_l2h_bias_soc::assign_Z() {
    vl::wr(Z, ~vl::rd(b));
}

void mipi_pll_dphy_LEVELCONV_l2h_bias_soc::assign_Z_neg() {
    vl::wr(Z_neg, ~vl::rd(a));
}

void mipi_pll_dphy_LEVELCONV_l2h_bias_soc::proc_always_2672() {
    if (vl::rd(supply_ok).test()) {
        if (vl::ceq(vl::rd(onz_hv), vl::logic<1>(0x1ULL)).test()) {
            vl::wr(a, vl::logic<1>(0x0ULL));
        } else {
            if (vl::rd(vblvhv).test()) {
                vl::wr(a, vl::rd(A_in));
            } else {
                vl::wr(a, vl::logic<1>::all_z());
            }
        }
        if (vl::rd(tie_low_hv).test()) {
            vl::wr(b, vl::logic<1>(0x0ULL));
        } else if (vl::rd(vblvhv).test()) {
            vl::wr(b, vl::rd(inz));
        } else {
            vl::wr(b, vl::logic<1>::all_z());
        }
    } else {
        vl::wr(a, vl::logic<1>::all_z());
        vl::wr(b, vl::logic<1>::all_z());
    }
}
