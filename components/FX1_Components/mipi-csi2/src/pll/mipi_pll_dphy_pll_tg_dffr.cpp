//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_pll_tg_dffr
//--------------------------------------------------------------------------------------------------
#include "mipi_pll_dphy_pll_tg_dffr.h"

mipi_pll_dphy_pll_tg_dffr::mipi_pll_dphy_pll_tg_dffr(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(assign_supply_ok);
    sensitive << vddlv << vssa;
    SC_METHOD(assign_clkp_I12);
    sensitive << clkn;
    SC_METHOD(assign_clkn_I12);
    sensitive << clkp;
    SC_METHOD(assign_clkp_I15);
    sensitive << clkn;
    SC_METHOD(assign_clkn_I15);
    sensitive << clkp;
    SC_METHOD(proc_always_4763);
    sensitive << d << rst << supply_ok << clkp << clkn;
    SC_METHOD(proc_always_4775);
    sensitive << clkp << clkn << a << rst_n;
    SC_METHOD(proc_always_4787);
    sensitive << db << rst_n << supply_ok << clkp << clkn;
    SC_METHOD(proc_always_4801);
    sensitive << clkn << clkp << rst << an;
}

void mipi_pll_dphy_pll_tg_dffr::assign_supply_ok() {
    vl::wr(supply_ok, vl::land(vl::ceq(vl::rd(vddlv), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(vssa), vl::logic<1>(0x0ULL))));
}

// initial a   = $random; // No Initial defined value, the reset can in some cases move from undefined to zero
// initial an = ~a;
// initial q    = $random;
// initial qb   = ~q; // No Initial defined value, the reset can in some cases move from undefined to zero
void mipi_pll_dphy_pll_tg_dffr::assign_clkp_I12() {
    vl::wr(clkp_I12, vl::rd(clkn));
}

void mipi_pll_dphy_pll_tg_dffr::assign_clkn_I12() {
    vl::wr(clkn_I12, vl::rd(clkp));
}

void mipi_pll_dphy_pll_tg_dffr::assign_clkp_I15() {
    vl::wr(clkp_I15, vl::rd(clkn));
}

void mipi_pll_dphy_pll_tg_dffr::assign_clkn_I15() {
    vl::wr(clkn_I15, vl::rd(clkp));
}

void mipi_pll_dphy_pll_tg_dffr::proc_always_4763() {
    if (vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x0ULL)).test()) {
        vl::wr(a, vl::logic<1>(0x1ULL));
    } else {
        if (vl::land(vl::rd(rst), vl::rd(clkp)).test()) {
            vl::wr(a, vl::logic<1>(0x1ULL));
        } else if (vl::land(vl::rd(clkp_I12), vl::lnot(vl::rd(clkn_I12))).test()) {
            vl::wr(a, ~vl::rd(d));
        }
    }
}

void mipi_pll_dphy_pll_tg_dffr::proc_always_4775() {
    if (vl::rd(rst_n).truth_val() == 0) {
        vl::wr(q, vl::logic<1>(0x0ULL));
    } else if (vl::land(vl::rd(clkp), vl::lnot(vl::rd(clkn))).test()) {
        vl::wr(q, ~vl::rd(a));
    }
}

void mipi_pll_dphy_pll_tg_dffr::proc_always_4787() {
    if (vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x0ULL)).test()) {
        vl::wr(an, vl::logic<1>(0x0ULL));
    } else {
        if (vl::land(vl::lnot(vl::rd(rst_n)), vl::lnot(vl::rd(clkn))).test()) {
            vl::wr(an, vl::logic<1>(0x0ULL));
        } else if (vl::land(vl::lnot(vl::rd(clkn_I15)), vl::rd(clkp_I15)).test()) {
            vl::wr(an, ~vl::rd(db));
        }
    }
}

void mipi_pll_dphy_pll_tg_dffr::proc_always_4801() {
    if (vl::rd(rst).test()) {
        vl::wr(qb, vl::logic<1>(0x1ULL));
    } else if (vl::land(vl::lnot(vl::rd(clkn)), vl::rd(clkp)).test()) {
        vl::wr(qb, ~vl::rd(an));
    }
}
