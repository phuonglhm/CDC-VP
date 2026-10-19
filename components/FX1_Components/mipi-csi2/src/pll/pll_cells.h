//--------------------------------------------------------------------------------------------------
// library cells (pll_cells)
//--------------------------------------------------------------------------------------------------
#ifndef VL_PLL_CELLS_H
#define VL_PLL_CELLS_H

#include "vl/vl_rt.h"
#include "udp_lib.h"

class mipi_pll_dphy_DPHYPLL_fd_pg : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> dgnd{"dgnd"};
    sc_core::sc_in<sc_dt::sc_logic> dvdd{"dvdd"};
    sc_core::sc_in<sc_dt::sc_logic> in{"in"};
    sc_core::sc_out<sc_dt::sc_logic> nout{"nout"};
    sc_core::sc_in<sc_dt::sc_logic> on{"on"};
    SC_HAS_PROCESS(mipi_pll_dphy_DPHYPLL_fd_pg);
    explicit mipi_pll_dphy_DPHYPLL_fd_pg(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << dgnd << dvdd << in << on;
    }

private:
    vl::logic<1> n_nout;
    void eval() {
        const vl::logic<1> i_dgnd = vl::rd(dgnd);
        const vl::logic<1> i_dvdd = vl::rd(dvdd);
        const vl::logic<1> i_in = vl::rd(in);
        const vl::logic<1> i_on = vl::rd(on);
    n_nout = vl::bufif1_(~i_in, vl::land(vl::land(vl::ceq(i_on, vl::logic<1>(0x1ULL)), vl::ceq(i_dvdd, vl::logic<1>(0x1ULL))), vl::ceq(i_dgnd, vl::logic<1>(0x0ULL))));
        vl::wr(nout, n_nout);
    }
};

class mipi_pll_dphy_DPHYPLL_fd_pg_big : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> on{"on"};
    sc_core::sc_out<sc_dt::sc_logic> nout{"nout"};
    sc_core::sc_in<sc_dt::sc_logic> dgnd{"dgnd"};
    sc_core::sc_in<sc_dt::sc_logic> dvdd{"dvdd"};
    sc_core::sc_in<sc_dt::sc_logic> in{"in"};
    SC_HAS_PROCESS(mipi_pll_dphy_DPHYPLL_fd_pg_big);
    explicit mipi_pll_dphy_DPHYPLL_fd_pg_big(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << on << dgnd << dvdd << in;
    }

private:
    vl::logic<1> n_nout;
    void eval() {
        const vl::logic<1> i_on = vl::rd(on);
        const vl::logic<1> i_dgnd = vl::rd(dgnd);
        const vl::logic<1> i_dvdd = vl::rd(dvdd);
        const vl::logic<1> i_in = vl::rd(in);
    n_nout = vl::bufif1_(~i_in, vl::land(vl::land(vl::ceq(i_on, vl::logic<1>(0x1ULL)), vl::ceq(i_dvdd, vl::logic<1>(0x1ULL))), vl::ceq(i_dgnd, vl::logic<1>(0x0ULL))));
        vl::wr(nout, n_nout);
    }
};

class mipi_pll_dphy_LEVELCONV_bias : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> onz_hv{"onz_hv"};
    sc_core::sc_in<sc_dt::sc_logic> tie_low_hv{"tie_low_hv"};
    sc_core::sc_out<sc_dt::sc_logic> vblvhv{"vblvhv"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};
    SC_HAS_PROCESS(mipi_pll_dphy_LEVELCONV_bias);
    explicit mipi_pll_dphy_LEVELCONV_bias(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << onz_hv << tie_low_hv << gd << vph;
    }

private:
    vl::logic<1> n_vblvhv;
    void eval() {
        const vl::logic<1> i_onz_hv = vl::rd(onz_hv);
        const vl::logic<1> i_tie_low_hv = vl::rd(tie_low_hv);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vph = vl::rd(vph);
    n_vblvhv = vl::bufif1_(~i_onz_hv, vl::land(vl::land(vl::ceq(i_gd, vl::logic<1>(0x0ULL)), vl::ceq(i_vph, vl::logic<1>(0x1ULL))), vl::ceq(i_tie_low_hv, vl::logic<1>(0x0ULL))));
        vl::wr(vblvhv, n_vblvhv);
    }
};

class mipi_pll_dphy_LEVELCONV_cdm_na3x6_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_in<sc_dt::sc_logic> tie_high_cdm{"tie_high_cdm"};
    sc_core::sc_in<sc_dt::sc_logic> tie_low_cdm{"tie_low_cdm"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_pll_dphy_LEVELCONV_cdm_na3x6_lvt);
    explicit mipi_pll_dphy_LEVELCONV_cdm_na3x6_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << a << tie_high_cdm << tie_low_cdm << gd << vp;
    }

private:
    vl::logic<1> n_z;
    void eval() {
        const vl::logic<1> i_a = vl::rd(a);
        const vl::logic<1> i_tie_high_cdm = vl::rd(tie_high_cdm);
        const vl::logic<1> i_tie_low_cdm = vl::rd(tie_low_cdm);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vp = vl::rd(vp);
    n_z = vl::bufif1_(~i_a, vl::land(vl::land(vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))), vl::ceq(i_tie_low_cdm, vl::logic<1>(0x0ULL))), vl::ceq(i_tie_high_cdm, vl::logic<1>(0x1ULL))));
        vl::wr(z, n_z);
    }
};

class mipi_pll_dphy_LEVELCONV_l2l_cdm_tie_LowHigh : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    sc_core::sc_out<sc_dt::sc_logic> tie_high{"tie_high"};
    sc_core::sc_out<sc_dt::sc_logic> tie_low{"tie_low"};
    SC_HAS_PROCESS(mipi_pll_dphy_LEVELCONV_l2l_cdm_tie_LowHigh);
    explicit mipi_pll_dphy_LEVELCONV_l2l_cdm_tie_LowHigh(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << gd << vp;
    }

private:
    vl::logic<1> n_tie_high, n_tie_low;
    void eval() {
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vp = vl::rd(vp);
    n_tie_low = vl::mux(vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))), vl::logic<1>(0x0ULL), vl::logic<1>::all_z());
    n_tie_high = vl::mux(vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))), vl::logic<1>(0x1ULL), vl::logic<1>::all_z());
        vl::wr(tie_high, n_tie_high);
        vl::wr(tie_low, n_tie_low);
    }
};

class mipi_pll_dphy_LEVELCONV_vp_det_soc : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> A_in{"A_in"};
    sc_core::sc_out<sc_dt::sc_logic> Z{"Z"};
    sc_core::sc_out<sc_dt::sc_logic> Z_neg{"Z_neg"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};
    sc_core::sc_in<sc_dt::sc_logic> vss{"vss"};
    SC_HAS_PROCESS(mipi_pll_dphy_LEVELCONV_vp_det_soc);
    explicit mipi_pll_dphy_LEVELCONV_vp_det_soc(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A_in << gd << vph << vss;
    }

private:
    vl::logic<1> n_Z, n_Z_neg;
    void eval() {
        const vl::logic<1> i_A_in = vl::rd(A_in);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vph = vl::rd(vph);
        const vl::logic<1> i_vss = vl::rd(vss);
    n_Z_neg = vl::bufif1_(~i_A_in, vl::land(vl::land(vl::ceq(i_vph, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))), vl::ceq(i_vss, vl::logic<1>(0x0ULL))));
    n_Z = vl::bufif1_(i_A_in, vl::land(vl::land(vl::ceq(i_vph, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))), vl::ceq(i_vss, vl::logic<1>(0x0ULL))));
        vl::wr(Z, n_Z);
        vl::wr(Z_neg, n_Z_neg);
    }
};

class mipi_pll_dphy_SEM_AN2_S_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_AN2_S_1);
    explicit mipi_pll_dphy_SEM_AN2_S_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A1_org, n_A2_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::and_(n_A1_org, n_A2_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEM_AN2_S_4 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_AN2_S_4);
    explicit mipi_pll_dphy_SEM_AN2_S_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A1_org, n_A2_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::and_(n_A1_org, n_A2_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEM_AN3_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> A3{"A3"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_AN3_1);
    explicit mipi_pll_dphy_SEM_AN3_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << A3 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A1_org, n_A2_org, n_A3_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_A3 = vl::rd(A3);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A3_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A3, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::and_(n_A1_org, n_A2_org, n_A3_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEM_AO21B_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> B{"B"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_AO21B_1);
    explicit mipi_pll_dphy_SEM_AO21B_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << B << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_int_res_0, n_int_res_1, n_A1_org, n_A2_org, n_B_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_B = vl::rd(B);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_B_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_1 = vl::not_(n_B_org);
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_0 = vl::and_(n_A1_org, n_A2_org);
    n_X = vl::or_(n_int_res_0, n_int_res_1);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEM_AO22_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> B1{"B1"};
    sc_core::sc_in<sc_dt::sc_logic> B2{"B2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_AO22_1);
    explicit mipi_pll_dphy_SEM_AO22_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << B1 << B2 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_int_res_0, n_int_res_1, n_A1_org, n_A2_org, n_B1_org, n_B2_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_B1 = vl::rd(B1);
        const vl::logic<1> i_B2 = vl::rd(B2);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_B2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_B1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_1 = vl::and_(n_B1_org, n_B2_org);
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_0 = vl::and_(n_A1_org, n_A2_org);
    n_X = vl::or_(n_int_res_0, n_int_res_1);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEM_AO22_2 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> B1{"B1"};
    sc_core::sc_in<sc_dt::sc_logic> B2{"B2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_AO22_2);
    explicit mipi_pll_dphy_SEM_AO22_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << B1 << B2 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_int_res_0, n_int_res_1, n_A1_org, n_A2_org, n_B1_org, n_B2_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_B1 = vl::rd(B1);
        const vl::logic<1> i_B2 = vl::rd(B2);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_B2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_B1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_1 = vl::and_(n_B1_org, n_B2_org);
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_0 = vl::and_(n_A1_org, n_A2_org);
    n_X = vl::or_(n_int_res_0, n_int_res_1);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEM_AO2BB2_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> B1{"B1"};
    sc_core::sc_in<sc_dt::sc_logic> B2{"B2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_AO2BB2_1);
    explicit mipi_pll_dphy_SEM_AO2BB2_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << B1 << B2 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_int_res_0, n_int_res_1, n_int_res_2, n_int_res_3, n_A1_org, n_A2_org, n_B1_org, n_B2_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_B1 = vl::rd(B1);
        const vl::logic<1> i_B2 = vl::rd(B2);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_B2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_B1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_3 = vl::and_(n_B1_org, n_B2_org);
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_1 = vl::not_(n_A2_org);
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_0 = vl::not_(n_A1_org);
    n_int_res_2 = vl::and_(n_int_res_0, n_int_res_1);
    n_X = vl::or_(n_int_res_2, n_int_res_3);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEM_AO2BB2_2 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> B1{"B1"};
    sc_core::sc_in<sc_dt::sc_logic> B2{"B2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_AO2BB2_2);
    explicit mipi_pll_dphy_SEM_AO2BB2_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << B1 << B2 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_int_res_0, n_int_res_1, n_int_res_2, n_int_res_3, n_A1_org, n_A2_org, n_B1_org, n_B2_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_B1 = vl::rd(B1);
        const vl::logic<1> i_B2 = vl::rd(B2);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_B2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_B1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_3 = vl::and_(n_B1_org, n_B2_org);
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_1 = vl::not_(n_A2_org);
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_0 = vl::not_(n_A1_org);
    n_int_res_2 = vl::and_(n_int_res_0, n_int_res_1);
    n_X = vl::or_(n_int_res_2, n_int_res_3);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEM_AOAI211_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> B{"B"};
    sc_core::sc_in<sc_dt::sc_logic> C{"C"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_AOAI211_1);
    explicit mipi_pll_dphy_SEM_AOAI211_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << B << C << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_int_res_0, n_int_res_1, n_A1_org, n_A2_org, n_B_org, n_C_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_B = vl::rd(B);
        const vl::logic<1> i_C = vl::rd(C);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_C_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_C, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_B_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_0 = vl::and_(n_A1_org, n_A2_org);
    n_int_res_1 = vl::or_(n_int_res_0, n_B_org);
    n_X = vl::nand_(n_int_res_1, n_C_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEM_AOI211_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> B1{"B1"};
    sc_core::sc_in<sc_dt::sc_logic> B2{"B2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_AOI211_1);
    explicit mipi_pll_dphy_SEM_AOI211_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << B1 << B2 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_int_res_0, n_A1_org, n_A2_org, n_B1_org, n_B2_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_B1 = vl::rd(B1);
        const vl::logic<1> i_B2 = vl::rd(B2);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_B2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_B1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_0 = vl::and_(n_A1_org, n_A2_org);
    n_X = vl::nor_(n_int_res_0, n_B1_org, n_B2_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEM_AOI21_S_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> B{"B"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_AOI21_S_1);
    explicit mipi_pll_dphy_SEM_AOI21_S_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << B << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_int_res_0, n_A1_org, n_A2_org, n_B_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_B = vl::rd(B);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_B_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_0 = vl::and_(n_A1_org, n_A2_org);
    n_X = vl::nor_(n_int_res_0, n_B_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEM_AOI22_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> B1{"B1"};
    sc_core::sc_in<sc_dt::sc_logic> B2{"B2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_AOI22_1);
    explicit mipi_pll_dphy_SEM_AOI22_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << B1 << B2 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_int_res_0, n_int_res_1, n_A1_org, n_A2_org, n_B1_org, n_B2_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_B1 = vl::rd(B1);
        const vl::logic<1> i_B2 = vl::rd(B2);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_B2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_B1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_1 = vl::and_(n_B1_org, n_B2_org);
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_0 = vl::and_(n_A1_org, n_A2_org);
    n_X = vl::nor_(n_int_res_0, n_int_res_1);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEM_AOI31_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> A3{"A3"};
    sc_core::sc_in<sc_dt::sc_logic> B{"B"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_AOI31_1);
    explicit mipi_pll_dphy_SEM_AOI31_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << A3 << B << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_int_res_0, n_A1_org, n_A2_org, n_A3_org, n_B_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_A3 = vl::rd(A3);
        const vl::logic<1> i_B = vl::rd(B);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_B_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A3_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A3, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_0 = vl::and_(n_A1_org, n_A2_org, n_A3_org);
    n_X = vl::nor_(n_int_res_0, n_B_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEM_AOI32_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> A3{"A3"};
    sc_core::sc_in<sc_dt::sc_logic> B1{"B1"};
    sc_core::sc_in<sc_dt::sc_logic> B2{"B2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_AOI32_1);
    explicit mipi_pll_dphy_SEM_AOI32_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << A3 << B1 << B2 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_int_res_0, n_int_res_1, n_A1_org, n_A2_org, n_A3_org, n_B1_org, n_B2_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_A3 = vl::rd(A3);
        const vl::logic<1> i_B1 = vl::rd(B1);
        const vl::logic<1> i_B2 = vl::rd(B2);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_B2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_B1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_1 = vl::and_(n_B1_org, n_B2_org);
    n_A3_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A3, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_0 = vl::and_(n_A1_org, n_A2_org, n_A3_org);
    n_X = vl::nor_(n_int_res_0, n_int_res_1);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEM_BUF_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_BUF_1);
    explicit mipi_pll_dphy_SEM_BUF_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A_org;
    void eval() {
        const vl::logic<1> i_A = vl::rd(A);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::buf_(n_A_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEM_BUF_2 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_BUF_2);
    explicit mipi_pll_dphy_SEM_BUF_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A_org;
    void eval() {
        const vl::logic<1> i_A = vl::rd(A);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::buf_(n_A_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEM_BUF_4 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_BUF_4);
    explicit mipi_pll_dphy_SEM_BUF_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A_org;
    void eval() {
        const vl::logic<1> i_A = vl::rd(A);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::buf_(n_A_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEM_BUF_8 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_BUF_8);
    explicit mipi_pll_dphy_SEM_BUF_8(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A_org;
    void eval() {
        const vl::logic<1> i_A = vl::rd(A);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::buf_(n_A_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEM_BUF_S_4 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_BUF_S_4);
    explicit mipi_pll_dphy_SEM_BUF_S_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A_org;
    void eval() {
        const vl::logic<1> i_A = vl::rd(A);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::buf_(n_A_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEM_BUF_S_8 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_BUF_S_8);
    explicit mipi_pll_dphy_SEM_BUF_S_8(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A_org;
    void eval() {
        const vl::logic<1> i_A = vl::rd(A);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::buf_(n_A_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEM_DCAP_12 : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_DCAP_12);
    explicit mipi_pll_dphy_SEM_DCAP_12(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << VDD << VSS << VBP << VBN;
    }

private:
    void eval() {
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    }
};

class mipi_pll_dphy_SEM_DCAP_16 : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_DCAP_16);
    explicit mipi_pll_dphy_SEM_DCAP_16(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << VDD << VSS << VBP << VBN;
    }

private:
    void eval() {
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    }
};

class mipi_pll_dphy_SEM_DCAP_4 : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_DCAP_4);
    explicit mipi_pll_dphy_SEM_DCAP_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << VDD << VSS << VBP << VBN;
    }

private:
    void eval() {
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    }
};

class mipi_pll_dphy_SEM_DCAP_6 : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_DCAP_6);
    explicit mipi_pll_dphy_SEM_DCAP_6(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << VDD << VSS << VBP << VBN;
    }

private:
    void eval() {
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    }
};

class mipi_pll_dphy_SEM_DCAP_8 : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_DCAP_8);
    explicit mipi_pll_dphy_SEM_DCAP_8(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << VDD << VSS << VBP << VBN;
    }

private:
    void eval() {
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    }
};

class mipi_pll_dphy_SEM_EN2_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_EN2_1);
    explicit mipi_pll_dphy_SEM_EN2_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A1_org, n_A2_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::xnor_(n_A1_org, n_A2_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEM_EN2_2 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_EN2_2);
    explicit mipi_pll_dphy_SEM_EN2_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A1_org, n_A2_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::xnor_(n_A1_org, n_A2_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEM_EO2_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_EO2_1);
    explicit mipi_pll_dphy_SEM_EO2_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A1_org, n_A2_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::xor_(n_A1_org, n_A2_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEM_EO2_2 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_EO2_2);
    explicit mipi_pll_dphy_SEM_EO2_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A1_org, n_A2_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::xor_(n_A1_org, n_A2_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEM_FSDPRBQ_D_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> Q{"Q"};
    sc_core::sc_in<sc_dt::sc_logic> CK{"CK"};
    sc_core::sc_in<sc_dt::sc_logic> D{"D"};
    sc_core::sc_in<sc_dt::sc_logic> SI{"SI"};
    sc_core::sc_in<sc_dt::sc_logic> SE{"SE"};
    sc_core::sc_in<sc_dt::sc_logic> RD{"RD"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_FSDPRBQ_D_1);
    explicit mipi_pll_dphy_SEM_FSDPRBQ_D_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << CK << D << SI << SE << RD << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_Q, n_SD, n_delayed_CK, n_delayed_D, n_delayed_SI, n_delayed_SE, n_delayed_RD, n_int_res__D, n_int_res__RD, n_xcr_0, n_CK_org, n_D_org, n_RD_org, n_SE_org, n_SI_org;
    vl::udp_state<3> s0_{3};
    vl::udp_state<6> s1_{3};
    void eval() {
        const vl::logic<1> i_CK = vl::rd(CK);
        const vl::logic<1> i_D = vl::rd(D);
        const vl::logic<1> i_SI = vl::rd(SI);
        const vl::logic<1> i_SE = vl::rd(SE);
        const vl::logic<1> i_RD = vl::rd(RD);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
        bool chg = true;
        for (int it = 0; chg && it < 16; ++it) {
            chg = false;
        n_delayed_SI = i_SI;
        n_SI_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_SI, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_delayed_SE = i_SE;
        n_SE_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_SE, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_delayed_RD = i_RD;
        n_RD_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_RD, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_int_res__RD = vl::not_(n_RD_org);
        n_delayed_D = i_D;
        n_D_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_D, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_int_res__D = vl::ucomb(udp::mipi_pll_dphy_mux2, vl::v3(n_SI_org), vl::v3(n_D_org), vl::v3(n_SE_org));
        n_delayed_CK = i_CK;
        n_CK_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_CK, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_SD = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(vl::logic<1>(0x0ULL), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        { const int in[3] = {vl::v3(n_CK_org), vl::v3(n_int_res__D), vl::v3(n_int_res__RD)}; if (s0_.update(udp::mipi_pll_dphy_dff_r_err, in)) chg = true; }
        n_xcr_0 = vl::from3(s0_.st);
        { const int in[6] = {vl::v3(vl::LX()), vl::v3(n_CK_org), vl::v3(n_int_res__D), vl::v3(n_SD), vl::v3(n_int_res__RD), vl::v3(n_xcr_0)}; if (s1_.update(udp::mipi_pll_dphy_dff_sr_0, in)) chg = true; }
        n_Q = vl::from3(s1_.st);
        }
        vl::wr(Q, n_Q);
    }
};

class mipi_pll_dphy_SEM_FSDPRBQ_D_2 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> Q{"Q"};
    sc_core::sc_in<sc_dt::sc_logic> CK{"CK"};
    sc_core::sc_in<sc_dt::sc_logic> D{"D"};
    sc_core::sc_in<sc_dt::sc_logic> SI{"SI"};
    sc_core::sc_in<sc_dt::sc_logic> SE{"SE"};
    sc_core::sc_in<sc_dt::sc_logic> RD{"RD"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_FSDPRBQ_D_2);
    explicit mipi_pll_dphy_SEM_FSDPRBQ_D_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << CK << D << SI << SE << RD << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_Q, n_SD, n_delayed_CK, n_delayed_D, n_delayed_SI, n_delayed_SE, n_delayed_RD, n_int_res__D, n_int_res__RD, n_xcr_0, n_CK_org, n_D_org, n_RD_org, n_SE_org, n_SI_org;
    vl::udp_state<3> s0_{3};
    vl::udp_state<6> s1_{3};
    void eval() {
        const vl::logic<1> i_CK = vl::rd(CK);
        const vl::logic<1> i_D = vl::rd(D);
        const vl::logic<1> i_SI = vl::rd(SI);
        const vl::logic<1> i_SE = vl::rd(SE);
        const vl::logic<1> i_RD = vl::rd(RD);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
        bool chg = true;
        for (int it = 0; chg && it < 16; ++it) {
            chg = false;
        n_delayed_SI = i_SI;
        n_SI_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_SI, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_delayed_SE = i_SE;
        n_SE_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_SE, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_delayed_RD = i_RD;
        n_RD_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_RD, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_int_res__RD = vl::not_(n_RD_org);
        n_delayed_D = i_D;
        n_D_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_D, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_int_res__D = vl::ucomb(udp::mipi_pll_dphy_mux2, vl::v3(n_SI_org), vl::v3(n_D_org), vl::v3(n_SE_org));
        n_delayed_CK = i_CK;
        n_CK_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_CK, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_SD = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(vl::logic<1>(0x0ULL), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        { const int in[3] = {vl::v3(n_CK_org), vl::v3(n_int_res__D), vl::v3(n_int_res__RD)}; if (s0_.update(udp::mipi_pll_dphy_dff_r_err, in)) chg = true; }
        n_xcr_0 = vl::from3(s0_.st);
        { const int in[6] = {vl::v3(vl::LX()), vl::v3(n_CK_org), vl::v3(n_int_res__D), vl::v3(n_SD), vl::v3(n_int_res__RD), vl::v3(n_xcr_0)}; if (s1_.update(udp::mipi_pll_dphy_dff_sr_0, in)) chg = true; }
        n_Q = vl::from3(s1_.st);
        }
        vl::wr(Q, n_Q);
    }
};

class mipi_pll_dphy_SEM_FSDPRBQ_D_4 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> Q{"Q"};
    sc_core::sc_in<sc_dt::sc_logic> CK{"CK"};
    sc_core::sc_in<sc_dt::sc_logic> D{"D"};
    sc_core::sc_in<sc_dt::sc_logic> SI{"SI"};
    sc_core::sc_in<sc_dt::sc_logic> SE{"SE"};
    sc_core::sc_in<sc_dt::sc_logic> RD{"RD"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_FSDPRBQ_D_4);
    explicit mipi_pll_dphy_SEM_FSDPRBQ_D_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << CK << D << SI << SE << RD << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_Q, n_SD, n_delayed_CK, n_delayed_D, n_delayed_SI, n_delayed_SE, n_delayed_RD, n_int_res__D, n_int_res__RD, n_xcr_0, n_CK_org, n_D_org, n_RD_org, n_SE_org, n_SI_org;
    vl::udp_state<3> s0_{3};
    vl::udp_state<6> s1_{3};
    void eval() {
        const vl::logic<1> i_CK = vl::rd(CK);
        const vl::logic<1> i_D = vl::rd(D);
        const vl::logic<1> i_SI = vl::rd(SI);
        const vl::logic<1> i_SE = vl::rd(SE);
        const vl::logic<1> i_RD = vl::rd(RD);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
        bool chg = true;
        for (int it = 0; chg && it < 16; ++it) {
            chg = false;
        n_delayed_SI = i_SI;
        n_SI_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_SI, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_delayed_SE = i_SE;
        n_SE_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_SE, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_delayed_RD = i_RD;
        n_RD_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_RD, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_int_res__RD = vl::not_(n_RD_org);
        n_delayed_D = i_D;
        n_D_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_D, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_int_res__D = vl::ucomb(udp::mipi_pll_dphy_mux2, vl::v3(n_SI_org), vl::v3(n_D_org), vl::v3(n_SE_org));
        n_delayed_CK = i_CK;
        n_CK_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_CK, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_SD = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(vl::logic<1>(0x0ULL), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        { const int in[3] = {vl::v3(n_CK_org), vl::v3(n_int_res__D), vl::v3(n_int_res__RD)}; if (s0_.update(udp::mipi_pll_dphy_dff_r_err, in)) chg = true; }
        n_xcr_0 = vl::from3(s0_.st);
        { const int in[6] = {vl::v3(vl::LX()), vl::v3(n_CK_org), vl::v3(n_int_res__D), vl::v3(n_SD), vl::v3(n_int_res__RD), vl::v3(n_xcr_0)}; if (s1_.update(udp::mipi_pll_dphy_dff_sr_0, in)) chg = true; }
        n_Q = vl::from3(s1_.st);
        }
        vl::wr(Q, n_Q);
    }
};

class mipi_pll_dphy_SEM_FSDPRBQ_V2_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> Q{"Q"};
    sc_core::sc_in<sc_dt::sc_logic> CK{"CK"};
    sc_core::sc_in<sc_dt::sc_logic> D{"D"};
    sc_core::sc_in<sc_dt::sc_logic> SI{"SI"};
    sc_core::sc_in<sc_dt::sc_logic> SE{"SE"};
    sc_core::sc_in<sc_dt::sc_logic> RD{"RD"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_FSDPRBQ_V2_1);
    explicit mipi_pll_dphy_SEM_FSDPRBQ_V2_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << CK << D << SI << SE << RD << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_Q, n_SD, n_delayed_CK, n_delayed_D, n_delayed_SI, n_delayed_SE, n_delayed_RD, n_int_res__D, n_int_res__RD, n_xcr_0, n_CK_org, n_D_org, n_RD_org, n_SE_org, n_SI_org;
    vl::udp_state<3> s0_{3};
    vl::udp_state<6> s1_{3};
    void eval() {
        const vl::logic<1> i_CK = vl::rd(CK);
        const vl::logic<1> i_D = vl::rd(D);
        const vl::logic<1> i_SI = vl::rd(SI);
        const vl::logic<1> i_SE = vl::rd(SE);
        const vl::logic<1> i_RD = vl::rd(RD);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
        bool chg = true;
        for (int it = 0; chg && it < 16; ++it) {
            chg = false;
        n_delayed_SI = i_SI;
        n_SI_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_SI, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_delayed_SE = i_SE;
        n_SE_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_SE, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_delayed_RD = i_RD;
        n_RD_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_RD, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_int_res__RD = vl::not_(n_RD_org);
        n_delayed_D = i_D;
        n_D_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_D, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_int_res__D = vl::ucomb(udp::mipi_pll_dphy_mux2, vl::v3(n_SI_org), vl::v3(n_D_org), vl::v3(n_SE_org));
        n_delayed_CK = i_CK;
        n_CK_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_CK, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_SD = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(vl::logic<1>(0x0ULL), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        { const int in[3] = {vl::v3(n_CK_org), vl::v3(n_int_res__D), vl::v3(n_int_res__RD)}; if (s0_.update(udp::mipi_pll_dphy_dff_r_err, in)) chg = true; }
        n_xcr_0 = vl::from3(s0_.st);
        { const int in[6] = {vl::v3(vl::LX()), vl::v3(n_CK_org), vl::v3(n_int_res__D), vl::v3(n_SD), vl::v3(n_int_res__RD), vl::v3(n_xcr_0)}; if (s1_.update(udp::mipi_pll_dphy_dff_sr_0, in)) chg = true; }
        n_Q = vl::from3(s1_.st);
        }
        vl::wr(Q, n_Q);
    }
};

class mipi_pll_dphy_SEM_FSDPRBSBQO_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> Q{"Q"};
    sc_core::sc_out<sc_dt::sc_logic> SO{"SO"};
    sc_core::sc_in<sc_dt::sc_logic> CK{"CK"};
    sc_core::sc_in<sc_dt::sc_logic> D{"D"};
    sc_core::sc_in<sc_dt::sc_logic> SI{"SI"};
    sc_core::sc_in<sc_dt::sc_logic> SE{"SE"};
    sc_core::sc_in<sc_dt::sc_logic> RD{"RD"};
    sc_core::sc_in<sc_dt::sc_logic> SD{"SD"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_FSDPRBSBQO_1);
    explicit mipi_pll_dphy_SEM_FSDPRBSBQO_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << CK << D << SI << SE << RD << SD << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_Q, n_SO, n_delayed_CK, n_delayed_D, n_delayed_SI, n_delayed_SE, n_delayed_RD, n_delayed_SD, n_int_res__D, n_int_res__SD, n_int_res__RD, n_int_res_iq, n_xcr_0, n_CK_org, n_D_org, n_RD_org, n_SD_org, n_SE_org, n_SI_org;
    vl::udp_state<4> s0_{3};
    vl::udp_state<6> s1_{3};
    void eval() {
        const vl::logic<1> i_CK = vl::rd(CK);
        const vl::logic<1> i_D = vl::rd(D);
        const vl::logic<1> i_SI = vl::rd(SI);
        const vl::logic<1> i_SE = vl::rd(SE);
        const vl::logic<1> i_RD = vl::rd(RD);
        const vl::logic<1> i_SD = vl::rd(SD);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
        bool chg = true;
        for (int it = 0; chg && it < 16; ++it) {
            chg = false;
        n_delayed_SI = i_SI;
        n_SI_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_SI, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_delayed_SE = i_SE;
        n_SE_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_SE, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_delayed_RD = i_RD;
        n_RD_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_RD, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_int_res__RD = vl::not_(n_RD_org);
        n_delayed_SD = i_SD;
        n_SD_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_SD, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_int_res__SD = vl::not_(n_SD_org);
        n_delayed_D = i_D;
        n_D_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_D, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_int_res__D = vl::ucomb(udp::mipi_pll_dphy_mux2, vl::v3(n_SI_org), vl::v3(n_D_org), vl::v3(n_SE_org));
        n_delayed_CK = i_CK;
        n_CK_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_CK, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_SO = vl::buf_(n_int_res_iq);
        n_Q = vl::buf_(n_int_res_iq);
        { const int in[4] = {vl::v3(n_CK_org), vl::v3(n_int_res__D), vl::v3(n_int_res__SD), vl::v3(n_int_res__RD)}; if (s0_.update(udp::mipi_pll_dphy_dff_sr_err, in)) chg = true; }
        n_xcr_0 = vl::from3(s0_.st);
        { const int in[6] = {vl::v3(vl::LX()), vl::v3(n_CK_org), vl::v3(n_int_res__D), vl::v3(n_int_res__SD), vl::v3(n_int_res__RD), vl::v3(n_xcr_0)}; if (s1_.update(udp::mipi_pll_dphy_dff_sr_0, in)) chg = true; }
        n_int_res_iq = vl::from3(s1_.st);
        }
        vl::wr(Q, n_Q);
        vl::wr(SO, n_SO);
    }
};

class mipi_pll_dphy_SEM_INV_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_INV_1);
    explicit mipi_pll_dphy_SEM_INV_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A_org;
    void eval() {
        const vl::logic<1> i_A = vl::rd(A);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::not_(n_A_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEM_INV_2 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_INV_2);
    explicit mipi_pll_dphy_SEM_INV_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A_org;
    void eval() {
        const vl::logic<1> i_A = vl::rd(A);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::not_(n_A_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEM_INV_8 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_INV_8);
    explicit mipi_pll_dphy_SEM_INV_8(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A_org;
    void eval() {
        const vl::logic<1> i_A = vl::rd(A);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::not_(n_A_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEM_INV_S_12 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_INV_S_12);
    explicit mipi_pll_dphy_SEM_INV_S_12(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A_org;
    void eval() {
        const vl::logic<1> i_A = vl::rd(A);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::not_(n_A_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEM_INV_S_16 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_INV_S_16);
    explicit mipi_pll_dphy_SEM_INV_S_16(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A_org;
    void eval() {
        const vl::logic<1> i_A = vl::rd(A);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::not_(n_A_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEM_LDNQ_4 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> Q{"Q"};
    sc_core::sc_in<sc_dt::sc_logic> G{"G"};
    sc_core::sc_in<sc_dt::sc_logic> D{"D"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_LDNQ_4);
    explicit mipi_pll_dphy_SEM_LDNQ_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << G << D << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_Q, n_delayed_G, n_delayed_D, n_int_res__G, n_D_org, n_G_org;
    vl::udp_state<3> s0_{3};
    void eval() {
        const vl::logic<1> i_G = vl::rd(G);
        const vl::logic<1> i_D = vl::rd(D);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
        bool chg = true;
        for (int it = 0; chg && it < 16; ++it) {
            chg = false;
        n_delayed_D = i_D;
        n_D_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_D, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_delayed_G = i_G;
        n_G_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_G, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_int_res__G = vl::not_(n_G_org);
        { const int in[3] = {vl::v3(vl::LX()), vl::v3(n_int_res__G), vl::v3(n_D_org)}; if (s0_.update(udp::mipi_pll_dphy_latch, in)) chg = true; }
        n_Q = vl::from3(s0_.st);
        }
        vl::wr(Q, n_Q);
    }
};

class mipi_pll_dphy_SEM_MUX2_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> D0{"D0"};
    sc_core::sc_in<sc_dt::sc_logic> D1{"D1"};
    sc_core::sc_in<sc_dt::sc_logic> S{"S"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_MUX2_1);
    explicit mipi_pll_dphy_SEM_MUX2_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << D0 << D1 << S << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_D0_org, n_D1_org, n_S_org;
    void eval() {
        const vl::logic<1> i_D0 = vl::rd(D0);
        const vl::logic<1> i_D1 = vl::rd(D1);
        const vl::logic<1> i_S = vl::rd(S);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_S_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_S, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_D1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_D1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_D0_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_D0, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::ucomb(udp::mipi_pll_dphy_mux2, vl::v3(n_D1_org), vl::v3(n_D0_org), vl::v3(n_S_org));
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEM_MUX2_2 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> D0{"D0"};
    sc_core::sc_in<sc_dt::sc_logic> D1{"D1"};
    sc_core::sc_in<sc_dt::sc_logic> S{"S"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_MUX2_2);
    explicit mipi_pll_dphy_SEM_MUX2_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << D0 << D1 << S << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_D0_org, n_D1_org, n_S_org;
    void eval() {
        const vl::logic<1> i_D0 = vl::rd(D0);
        const vl::logic<1> i_D1 = vl::rd(D1);
        const vl::logic<1> i_S = vl::rd(S);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_S_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_S, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_D1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_D1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_D0_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_D0, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::ucomb(udp::mipi_pll_dphy_mux2, vl::v3(n_D1_org), vl::v3(n_D0_org), vl::v3(n_S_org));
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEM_MUX2_S_4 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> D0{"D0"};
    sc_core::sc_in<sc_dt::sc_logic> D1{"D1"};
    sc_core::sc_in<sc_dt::sc_logic> S{"S"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_MUX2_S_4);
    explicit mipi_pll_dphy_SEM_MUX2_S_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << D0 << D1 << S << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_D0_org, n_D1_org, n_S_org;
    void eval() {
        const vl::logic<1> i_D0 = vl::rd(D0);
        const vl::logic<1> i_D1 = vl::rd(D1);
        const vl::logic<1> i_S = vl::rd(S);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_S_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_S, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_D1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_D1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_D0_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_D0, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::ucomb(udp::mipi_pll_dphy_mux2, vl::v3(n_D1_org), vl::v3(n_D0_org), vl::v3(n_S_org));
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEM_ND2_S_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_ND2_S_1);
    explicit mipi_pll_dphy_SEM_ND2_S_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A1_org, n_A2_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::nand_(n_A1_org, n_A2_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEM_ND2_S_2 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_ND2_S_2);
    explicit mipi_pll_dphy_SEM_ND2_S_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A1_org, n_A2_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::nand_(n_A1_org, n_A2_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEM_ND3_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> A3{"A3"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_ND3_1);
    explicit mipi_pll_dphy_SEM_ND3_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << A3 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A1_org, n_A2_org, n_A3_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_A3 = vl::rd(A3);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A3_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A3, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::nand_(n_A1_org, n_A2_org, n_A3_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEM_ND4_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> A3{"A3"};
    sc_core::sc_in<sc_dt::sc_logic> A4{"A4"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_ND4_1);
    explicit mipi_pll_dphy_SEM_ND4_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << A3 << A4 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A1_org, n_A2_org, n_A3_org, n_A4_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_A3 = vl::rd(A3);
        const vl::logic<1> i_A4 = vl::rd(A4);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A4_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A4, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A3_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A3, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::nand_(n_A1_org, n_A2_org, n_A3_org, n_A4_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEM_ND4_4 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> A3{"A3"};
    sc_core::sc_in<sc_dt::sc_logic> A4{"A4"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_ND4_4);
    explicit mipi_pll_dphy_SEM_ND4_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << A3 << A4 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A1_org, n_A2_org, n_A3_org, n_A4_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_A3 = vl::rd(A3);
        const vl::logic<1> i_A4 = vl::rd(A4);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A4_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A4, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A3_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A3, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::nand_(n_A1_org, n_A2_org, n_A3_org, n_A4_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEM_NR2_S_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_NR2_S_1);
    explicit mipi_pll_dphy_SEM_NR2_S_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A1_org, n_A2_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::nor_(n_A1_org, n_A2_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEM_NR2_S_2 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_NR2_S_2);
    explicit mipi_pll_dphy_SEM_NR2_S_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A1_org, n_A2_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::nor_(n_A1_org, n_A2_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEM_NR4_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> A3{"A3"};
    sc_core::sc_in<sc_dt::sc_logic> A4{"A4"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_NR4_1);
    explicit mipi_pll_dphy_SEM_NR4_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << A3 << A4 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A1_org, n_A2_org, n_A3_org, n_A4_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_A3 = vl::rd(A3);
        const vl::logic<1> i_A4 = vl::rd(A4);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A4_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A4, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A3_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A3, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::nor_(n_A1_org, n_A2_org, n_A3_org, n_A4_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEM_OA21_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> B{"B"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_OA21_1);
    explicit mipi_pll_dphy_SEM_OA21_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << B << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_int_res_0, n_A1_org, n_A2_org, n_B_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_B = vl::rd(B);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_B_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_0 = vl::or_(n_A1_org, n_A2_org);
    n_X = vl::and_(n_int_res_0, n_B_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEM_OAI21_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> B{"B"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_OAI21_1);
    explicit mipi_pll_dphy_SEM_OAI21_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << B << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_int_res_0, n_A1_org, n_A2_org, n_B_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_B = vl::rd(B);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_B_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_0 = vl::or_(n_A1_org, n_A2_org);
    n_X = vl::nand_(n_int_res_0, n_B_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEM_OR2_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_OR2_1);
    explicit mipi_pll_dphy_SEM_OR2_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A1_org, n_A2_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::or_(n_A1_org, n_A2_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEM_TIE0_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_TIE0_1);
    explicit mipi_pll_dphy_SEM_TIE0_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X;
    void eval() {
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_X = vl::buf_(vl::logic<1>(0x0ULL));
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEM_TIE1_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_TIE1_1);
    explicit mipi_pll_dphy_SEM_TIE1_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X;
    void eval() {
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_X = vl::buf_(vl::logic<1>(0x1ULL));
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEM_TIEDIN_1 : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEM_TIEDIN_1);
    explicit mipi_pll_dphy_SEM_TIEDIN_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << X << VDD << VSS << VBP << VBN;
    }

private:
    void eval() {
        const vl::logic<1> i_X = vl::rd(X);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    }
};

class mipi_pll_dphy_SEP_AN2_S_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_AN2_S_1);
    explicit mipi_pll_dphy_SEP_AN2_S_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A1_org, n_A2_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::and_(n_A1_org, n_A2_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_AN2_S_4 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_AN2_S_4);
    explicit mipi_pll_dphy_SEP_AN2_S_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A1_org, n_A2_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::and_(n_A1_org, n_A2_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_AN2_S_8 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_AN2_S_8);
    explicit mipi_pll_dphy_SEP_AN2_S_8(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A1_org, n_A2_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::and_(n_A1_org, n_A2_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_AN3_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> A3{"A3"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_AN3_1);
    explicit mipi_pll_dphy_SEP_AN3_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << A3 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A1_org, n_A2_org, n_A3_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_A3 = vl::rd(A3);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A3_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A3, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::and_(n_A1_org, n_A2_org, n_A3_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_AN3_2 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> A3{"A3"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_AN3_2);
    explicit mipi_pll_dphy_SEP_AN3_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << A3 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A1_org, n_A2_org, n_A3_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_A3 = vl::rd(A3);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A3_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A3, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::and_(n_A1_org, n_A2_org, n_A3_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_AN3_4 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> A3{"A3"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_AN3_4);
    explicit mipi_pll_dphy_SEP_AN3_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << A3 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A1_org, n_A2_org, n_A3_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_A3 = vl::rd(A3);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A3_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A3, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::and_(n_A1_org, n_A2_org, n_A3_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_AO21B_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> B{"B"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_AO21B_1);
    explicit mipi_pll_dphy_SEP_AO21B_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << B << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_int_res_0, n_int_res_1, n_A1_org, n_A2_org, n_B_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_B = vl::rd(B);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_B_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_1 = vl::not_(n_B_org);
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_0 = vl::and_(n_A1_org, n_A2_org);
    n_X = vl::or_(n_int_res_0, n_int_res_1);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_AO21B_2 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> B{"B"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_AO21B_2);
    explicit mipi_pll_dphy_SEP_AO21B_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << B << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_int_res_0, n_int_res_1, n_A1_org, n_A2_org, n_B_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_B = vl::rd(B);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_B_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_1 = vl::not_(n_B_org);
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_0 = vl::and_(n_A1_org, n_A2_org);
    n_X = vl::or_(n_int_res_0, n_int_res_1);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_AO21B_4 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> B{"B"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_AO21B_4);
    explicit mipi_pll_dphy_SEP_AO21B_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << B << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_int_res_0, n_int_res_1, n_A1_org, n_A2_org, n_B_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_B = vl::rd(B);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_B_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_1 = vl::not_(n_B_org);
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_0 = vl::and_(n_A1_org, n_A2_org);
    n_X = vl::or_(n_int_res_0, n_int_res_1);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_AO22_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> B1{"B1"};
    sc_core::sc_in<sc_dt::sc_logic> B2{"B2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_AO22_1);
    explicit mipi_pll_dphy_SEP_AO22_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << B1 << B2 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_int_res_0, n_int_res_1, n_A1_org, n_A2_org, n_B1_org, n_B2_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_B1 = vl::rd(B1);
        const vl::logic<1> i_B2 = vl::rd(B2);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_B2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_B1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_1 = vl::and_(n_B1_org, n_B2_org);
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_0 = vl::and_(n_A1_org, n_A2_org);
    n_X = vl::or_(n_int_res_0, n_int_res_1);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_AO22_2 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> B1{"B1"};
    sc_core::sc_in<sc_dt::sc_logic> B2{"B2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_AO22_2);
    explicit mipi_pll_dphy_SEP_AO22_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << B1 << B2 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_int_res_0, n_int_res_1, n_A1_org, n_A2_org, n_B1_org, n_B2_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_B1 = vl::rd(B1);
        const vl::logic<1> i_B2 = vl::rd(B2);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_B2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_B1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_1 = vl::and_(n_B1_org, n_B2_org);
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_0 = vl::and_(n_A1_org, n_A2_org);
    n_X = vl::or_(n_int_res_0, n_int_res_1);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_AO22_4 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> B1{"B1"};
    sc_core::sc_in<sc_dt::sc_logic> B2{"B2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_AO22_4);
    explicit mipi_pll_dphy_SEP_AO22_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << B1 << B2 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_int_res_0, n_int_res_1, n_A1_org, n_A2_org, n_B1_org, n_B2_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_B1 = vl::rd(B1);
        const vl::logic<1> i_B2 = vl::rd(B2);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_B2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_B1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_1 = vl::and_(n_B1_org, n_B2_org);
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_0 = vl::and_(n_A1_org, n_A2_org);
    n_X = vl::or_(n_int_res_0, n_int_res_1);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_AO2BB2_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> B1{"B1"};
    sc_core::sc_in<sc_dt::sc_logic> B2{"B2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_AO2BB2_1);
    explicit mipi_pll_dphy_SEP_AO2BB2_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << B1 << B2 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_int_res_0, n_int_res_1, n_int_res_2, n_int_res_3, n_A1_org, n_A2_org, n_B1_org, n_B2_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_B1 = vl::rd(B1);
        const vl::logic<1> i_B2 = vl::rd(B2);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_B2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_B1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_3 = vl::and_(n_B1_org, n_B2_org);
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_1 = vl::not_(n_A2_org);
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_0 = vl::not_(n_A1_org);
    n_int_res_2 = vl::and_(n_int_res_0, n_int_res_1);
    n_X = vl::or_(n_int_res_2, n_int_res_3);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_AO2BB2_2 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> B1{"B1"};
    sc_core::sc_in<sc_dt::sc_logic> B2{"B2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_AO2BB2_2);
    explicit mipi_pll_dphy_SEP_AO2BB2_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << B1 << B2 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_int_res_0, n_int_res_1, n_int_res_2, n_int_res_3, n_A1_org, n_A2_org, n_B1_org, n_B2_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_B1 = vl::rd(B1);
        const vl::logic<1> i_B2 = vl::rd(B2);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_B2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_B1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_3 = vl::and_(n_B1_org, n_B2_org);
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_1 = vl::not_(n_A2_org);
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_0 = vl::not_(n_A1_org);
    n_int_res_2 = vl::and_(n_int_res_0, n_int_res_1);
    n_X = vl::or_(n_int_res_2, n_int_res_3);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_AOAI211_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> B{"B"};
    sc_core::sc_in<sc_dt::sc_logic> C{"C"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_AOAI211_1);
    explicit mipi_pll_dphy_SEP_AOAI211_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << B << C << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_int_res_0, n_int_res_1, n_A1_org, n_A2_org, n_B_org, n_C_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_B = vl::rd(B);
        const vl::logic<1> i_C = vl::rd(C);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_C_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_C, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_B_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_0 = vl::and_(n_A1_org, n_A2_org);
    n_int_res_1 = vl::or_(n_int_res_0, n_B_org);
    n_X = vl::nand_(n_int_res_1, n_C_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_AOI211_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> B1{"B1"};
    sc_core::sc_in<sc_dt::sc_logic> B2{"B2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_AOI211_1);
    explicit mipi_pll_dphy_SEP_AOI211_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << B1 << B2 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_int_res_0, n_A1_org, n_A2_org, n_B1_org, n_B2_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_B1 = vl::rd(B1);
        const vl::logic<1> i_B2 = vl::rd(B2);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_B2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_B1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_0 = vl::and_(n_A1_org, n_A2_org);
    n_X = vl::nor_(n_int_res_0, n_B1_org, n_B2_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_AOI211_2 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> B1{"B1"};
    sc_core::sc_in<sc_dt::sc_logic> B2{"B2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_AOI211_2);
    explicit mipi_pll_dphy_SEP_AOI211_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << B1 << B2 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_int_res_0, n_A1_org, n_A2_org, n_B1_org, n_B2_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_B1 = vl::rd(B1);
        const vl::logic<1> i_B2 = vl::rd(B2);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_B2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_B1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_0 = vl::and_(n_A1_org, n_A2_org);
    n_X = vl::nor_(n_int_res_0, n_B1_org, n_B2_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_AOI21_S_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> B{"B"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_AOI21_S_1);
    explicit mipi_pll_dphy_SEP_AOI21_S_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << B << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_int_res_0, n_A1_org, n_A2_org, n_B_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_B = vl::rd(B);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_B_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_0 = vl::and_(n_A1_org, n_A2_org);
    n_X = vl::nor_(n_int_res_0, n_B_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_AOI21_S_2 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> B{"B"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_AOI21_S_2);
    explicit mipi_pll_dphy_SEP_AOI21_S_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << B << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_int_res_0, n_A1_org, n_A2_org, n_B_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_B = vl::rd(B);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_B_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_0 = vl::and_(n_A1_org, n_A2_org);
    n_X = vl::nor_(n_int_res_0, n_B_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_AOI221_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> B1{"B1"};
    sc_core::sc_in<sc_dt::sc_logic> B2{"B2"};
    sc_core::sc_in<sc_dt::sc_logic> C{"C"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_AOI221_1);
    explicit mipi_pll_dphy_SEP_AOI221_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << B1 << B2 << C << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_int_res_0, n_int_res_1, n_A1_org, n_A2_org, n_B1_org, n_B2_org, n_C_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_B1 = vl::rd(B1);
        const vl::logic<1> i_B2 = vl::rd(B2);
        const vl::logic<1> i_C = vl::rd(C);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_C_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_C, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_B2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_B1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_1 = vl::and_(n_B1_org, n_B2_org);
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_0 = vl::and_(n_A1_org, n_A2_org);
    n_X = vl::nor_(n_int_res_0, n_int_res_1, n_C_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_AOI22_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> B1{"B1"};
    sc_core::sc_in<sc_dt::sc_logic> B2{"B2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_AOI22_1);
    explicit mipi_pll_dphy_SEP_AOI22_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << B1 << B2 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_int_res_0, n_int_res_1, n_A1_org, n_A2_org, n_B1_org, n_B2_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_B1 = vl::rd(B1);
        const vl::logic<1> i_B2 = vl::rd(B2);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_B2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_B1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_1 = vl::and_(n_B1_org, n_B2_org);
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_0 = vl::and_(n_A1_org, n_A2_org);
    n_X = vl::nor_(n_int_res_0, n_int_res_1);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_AOI22_2 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> B1{"B1"};
    sc_core::sc_in<sc_dt::sc_logic> B2{"B2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_AOI22_2);
    explicit mipi_pll_dphy_SEP_AOI22_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << B1 << B2 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_int_res_0, n_int_res_1, n_A1_org, n_A2_org, n_B1_org, n_B2_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_B1 = vl::rd(B1);
        const vl::logic<1> i_B2 = vl::rd(B2);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_B2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_B1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_1 = vl::and_(n_B1_org, n_B2_org);
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_0 = vl::and_(n_A1_org, n_A2_org);
    n_X = vl::nor_(n_int_res_0, n_int_res_1);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_AOI22_4 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> B1{"B1"};
    sc_core::sc_in<sc_dt::sc_logic> B2{"B2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_AOI22_4);
    explicit mipi_pll_dphy_SEP_AOI22_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << B1 << B2 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_int_res_0, n_int_res_1, n_A1_org, n_A2_org, n_B1_org, n_B2_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_B1 = vl::rd(B1);
        const vl::logic<1> i_B2 = vl::rd(B2);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_B2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_B1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_1 = vl::and_(n_B1_org, n_B2_org);
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_0 = vl::and_(n_A1_org, n_A2_org);
    n_X = vl::nor_(n_int_res_0, n_int_res_1);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_AOI32_2 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> A3{"A3"};
    sc_core::sc_in<sc_dt::sc_logic> B1{"B1"};
    sc_core::sc_in<sc_dt::sc_logic> B2{"B2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_AOI32_2);
    explicit mipi_pll_dphy_SEP_AOI32_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << A3 << B1 << B2 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_int_res_0, n_int_res_1, n_A1_org, n_A2_org, n_A3_org, n_B1_org, n_B2_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_A3 = vl::rd(A3);
        const vl::logic<1> i_B1 = vl::rd(B1);
        const vl::logic<1> i_B2 = vl::rd(B2);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_B2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_B1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_1 = vl::and_(n_B1_org, n_B2_org);
    n_A3_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A3, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_0 = vl::and_(n_A1_org, n_A2_org, n_A3_org);
    n_X = vl::nor_(n_int_res_0, n_int_res_1);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_BUF_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_BUF_1);
    explicit mipi_pll_dphy_SEP_BUF_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A_org;
    void eval() {
        const vl::logic<1> i_A = vl::rd(A);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::buf_(n_A_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_BUF_16 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_BUF_16);
    explicit mipi_pll_dphy_SEP_BUF_16(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A_org;
    void eval() {
        const vl::logic<1> i_A = vl::rd(A);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::buf_(n_A_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_BUF_2 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_BUF_2);
    explicit mipi_pll_dphy_SEP_BUF_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A_org;
    void eval() {
        const vl::logic<1> i_A = vl::rd(A);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::buf_(n_A_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_BUF_4 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_BUF_4);
    explicit mipi_pll_dphy_SEP_BUF_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A_org;
    void eval() {
        const vl::logic<1> i_A = vl::rd(A);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::buf_(n_A_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_BUF_8 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_BUF_8);
    explicit mipi_pll_dphy_SEP_BUF_8(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A_org;
    void eval() {
        const vl::logic<1> i_A = vl::rd(A);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::buf_(n_A_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_BUF_S_16 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_BUF_S_16);
    explicit mipi_pll_dphy_SEP_BUF_S_16(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A_org;
    void eval() {
        const vl::logic<1> i_A = vl::rd(A);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::buf_(n_A_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_BUF_S_4 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_BUF_S_4);
    explicit mipi_pll_dphy_SEP_BUF_S_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A_org;
    void eval() {
        const vl::logic<1> i_A = vl::rd(A);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::buf_(n_A_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_BUF_S_8 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_BUF_S_8);
    explicit mipi_pll_dphy_SEP_BUF_S_8(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A_org;
    void eval() {
        const vl::logic<1> i_A = vl::rd(A);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::buf_(n_A_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_EN2_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_EN2_1);
    explicit mipi_pll_dphy_SEP_EN2_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A1_org, n_A2_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::xnor_(n_A1_org, n_A2_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_EN2_2 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_EN2_2);
    explicit mipi_pll_dphy_SEP_EN2_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A1_org, n_A2_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::xnor_(n_A1_org, n_A2_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_EO2_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_EO2_1);
    explicit mipi_pll_dphy_SEP_EO2_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A1_org, n_A2_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::xor_(n_A1_org, n_A2_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_EO2_2 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_EO2_2);
    explicit mipi_pll_dphy_SEP_EO2_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A1_org, n_A2_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::xor_(n_A1_org, n_A2_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_FSDPRBQ_D_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> Q{"Q"};
    sc_core::sc_in<sc_dt::sc_logic> CK{"CK"};
    sc_core::sc_in<sc_dt::sc_logic> D{"D"};
    sc_core::sc_in<sc_dt::sc_logic> SI{"SI"};
    sc_core::sc_in<sc_dt::sc_logic> SE{"SE"};
    sc_core::sc_in<sc_dt::sc_logic> RD{"RD"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_FSDPRBQ_D_1);
    explicit mipi_pll_dphy_SEP_FSDPRBQ_D_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << CK << D << SI << SE << RD << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_Q, n_SD, n_delayed_CK, n_delayed_D, n_delayed_SI, n_delayed_SE, n_delayed_RD, n_int_res__D, n_int_res__RD, n_xcr_0, n_CK_org, n_D_org, n_RD_org, n_SE_org, n_SI_org;
    vl::udp_state<3> s0_{3};
    vl::udp_state<6> s1_{3};
    void eval() {
        const vl::logic<1> i_CK = vl::rd(CK);
        const vl::logic<1> i_D = vl::rd(D);
        const vl::logic<1> i_SI = vl::rd(SI);
        const vl::logic<1> i_SE = vl::rd(SE);
        const vl::logic<1> i_RD = vl::rd(RD);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
        bool chg = true;
        for (int it = 0; chg && it < 16; ++it) {
            chg = false;
        n_delayed_SI = i_SI;
        n_SI_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_SI, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_delayed_SE = i_SE;
        n_SE_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_SE, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_delayed_RD = i_RD;
        n_RD_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_RD, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_int_res__RD = vl::not_(n_RD_org);
        n_delayed_D = i_D;
        n_D_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_D, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_int_res__D = vl::ucomb(udp::mipi_pll_dphy_mux2, vl::v3(n_SI_org), vl::v3(n_D_org), vl::v3(n_SE_org));
        n_delayed_CK = i_CK;
        n_CK_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_CK, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_SD = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(vl::logic<1>(0x0ULL), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        { const int in[3] = {vl::v3(n_CK_org), vl::v3(n_int_res__D), vl::v3(n_int_res__RD)}; if (s0_.update(udp::mipi_pll_dphy_dff_r_err, in)) chg = true; }
        n_xcr_0 = vl::from3(s0_.st);
        { const int in[6] = {vl::v3(vl::LX()), vl::v3(n_CK_org), vl::v3(n_int_res__D), vl::v3(n_SD), vl::v3(n_int_res__RD), vl::v3(n_xcr_0)}; if (s1_.update(udp::mipi_pll_dphy_dff_sr_0, in)) chg = true; }
        n_Q = vl::from3(s1_.st);
        }
        vl::wr(Q, n_Q);
    }
};

class mipi_pll_dphy_SEP_FSDPRBQ_D_2 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> Q{"Q"};
    sc_core::sc_in<sc_dt::sc_logic> CK{"CK"};
    sc_core::sc_in<sc_dt::sc_logic> D{"D"};
    sc_core::sc_in<sc_dt::sc_logic> SI{"SI"};
    sc_core::sc_in<sc_dt::sc_logic> SE{"SE"};
    sc_core::sc_in<sc_dt::sc_logic> RD{"RD"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_FSDPRBQ_D_2);
    explicit mipi_pll_dphy_SEP_FSDPRBQ_D_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << CK << D << SI << SE << RD << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_Q, n_SD, n_delayed_CK, n_delayed_D, n_delayed_SI, n_delayed_SE, n_delayed_RD, n_int_res__D, n_int_res__RD, n_xcr_0, n_CK_org, n_D_org, n_RD_org, n_SE_org, n_SI_org;
    vl::udp_state<3> s0_{3};
    vl::udp_state<6> s1_{3};
    void eval() {
        const vl::logic<1> i_CK = vl::rd(CK);
        const vl::logic<1> i_D = vl::rd(D);
        const vl::logic<1> i_SI = vl::rd(SI);
        const vl::logic<1> i_SE = vl::rd(SE);
        const vl::logic<1> i_RD = vl::rd(RD);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
        bool chg = true;
        for (int it = 0; chg && it < 16; ++it) {
            chg = false;
        n_delayed_SI = i_SI;
        n_SI_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_SI, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_delayed_SE = i_SE;
        n_SE_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_SE, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_delayed_RD = i_RD;
        n_RD_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_RD, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_int_res__RD = vl::not_(n_RD_org);
        n_delayed_D = i_D;
        n_D_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_D, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_int_res__D = vl::ucomb(udp::mipi_pll_dphy_mux2, vl::v3(n_SI_org), vl::v3(n_D_org), vl::v3(n_SE_org));
        n_delayed_CK = i_CK;
        n_CK_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_CK, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_SD = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(vl::logic<1>(0x0ULL), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        { const int in[3] = {vl::v3(n_CK_org), vl::v3(n_int_res__D), vl::v3(n_int_res__RD)}; if (s0_.update(udp::mipi_pll_dphy_dff_r_err, in)) chg = true; }
        n_xcr_0 = vl::from3(s0_.st);
        { const int in[6] = {vl::v3(vl::LX()), vl::v3(n_CK_org), vl::v3(n_int_res__D), vl::v3(n_SD), vl::v3(n_int_res__RD), vl::v3(n_xcr_0)}; if (s1_.update(udp::mipi_pll_dphy_dff_sr_0, in)) chg = true; }
        n_Q = vl::from3(s1_.st);
        }
        vl::wr(Q, n_Q);
    }
};

class mipi_pll_dphy_SEP_FSDPRBQ_V2_2 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> Q{"Q"};
    sc_core::sc_in<sc_dt::sc_logic> CK{"CK"};
    sc_core::sc_in<sc_dt::sc_logic> D{"D"};
    sc_core::sc_in<sc_dt::sc_logic> SI{"SI"};
    sc_core::sc_in<sc_dt::sc_logic> SE{"SE"};
    sc_core::sc_in<sc_dt::sc_logic> RD{"RD"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_FSDPRBQ_V2_2);
    explicit mipi_pll_dphy_SEP_FSDPRBQ_V2_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << CK << D << SI << SE << RD << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_Q, n_SD, n_delayed_CK, n_delayed_D, n_delayed_SI, n_delayed_SE, n_delayed_RD, n_int_res__D, n_int_res__RD, n_xcr_0, n_CK_org, n_D_org, n_RD_org, n_SE_org, n_SI_org;
    vl::udp_state<3> s0_{3};
    vl::udp_state<6> s1_{3};
    void eval() {
        const vl::logic<1> i_CK = vl::rd(CK);
        const vl::logic<1> i_D = vl::rd(D);
        const vl::logic<1> i_SI = vl::rd(SI);
        const vl::logic<1> i_SE = vl::rd(SE);
        const vl::logic<1> i_RD = vl::rd(RD);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
        bool chg = true;
        for (int it = 0; chg && it < 16; ++it) {
            chg = false;
        n_delayed_SI = i_SI;
        n_SI_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_SI, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_delayed_SE = i_SE;
        n_SE_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_SE, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_delayed_RD = i_RD;
        n_RD_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_RD, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_int_res__RD = vl::not_(n_RD_org);
        n_delayed_D = i_D;
        n_D_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_D, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_int_res__D = vl::ucomb(udp::mipi_pll_dphy_mux2, vl::v3(n_SI_org), vl::v3(n_D_org), vl::v3(n_SE_org));
        n_delayed_CK = i_CK;
        n_CK_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_CK, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_SD = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(vl::logic<1>(0x0ULL), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        { const int in[3] = {vl::v3(n_CK_org), vl::v3(n_int_res__D), vl::v3(n_int_res__RD)}; if (s0_.update(udp::mipi_pll_dphy_dff_r_err, in)) chg = true; }
        n_xcr_0 = vl::from3(s0_.st);
        { const int in[6] = {vl::v3(vl::LX()), vl::v3(n_CK_org), vl::v3(n_int_res__D), vl::v3(n_SD), vl::v3(n_int_res__RD), vl::v3(n_xcr_0)}; if (s1_.update(udp::mipi_pll_dphy_dff_sr_0, in)) chg = true; }
        n_Q = vl::from3(s1_.st);
        }
        vl::wr(Q, n_Q);
    }
};

class mipi_pll_dphy_SEP_FSDPRBQ_V2_8 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> Q{"Q"};
    sc_core::sc_in<sc_dt::sc_logic> CK{"CK"};
    sc_core::sc_in<sc_dt::sc_logic> D{"D"};
    sc_core::sc_in<sc_dt::sc_logic> SI{"SI"};
    sc_core::sc_in<sc_dt::sc_logic> SE{"SE"};
    sc_core::sc_in<sc_dt::sc_logic> RD{"RD"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_FSDPRBQ_V2_8);
    explicit mipi_pll_dphy_SEP_FSDPRBQ_V2_8(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << CK << D << SI << SE << RD << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_Q, n_SD, n_delayed_CK, n_delayed_D, n_delayed_SI, n_delayed_SE, n_delayed_RD, n_int_res__D, n_int_res__RD, n_xcr_0, n_CK_org, n_D_org, n_RD_org, n_SE_org, n_SI_org;
    vl::udp_state<3> s0_{3};
    vl::udp_state<6> s1_{3};
    void eval() {
        const vl::logic<1> i_CK = vl::rd(CK);
        const vl::logic<1> i_D = vl::rd(D);
        const vl::logic<1> i_SI = vl::rd(SI);
        const vl::logic<1> i_SE = vl::rd(SE);
        const vl::logic<1> i_RD = vl::rd(RD);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
        bool chg = true;
        for (int it = 0; chg && it < 16; ++it) {
            chg = false;
        n_delayed_SI = i_SI;
        n_SI_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_SI, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_delayed_SE = i_SE;
        n_SE_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_SE, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_delayed_RD = i_RD;
        n_RD_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_RD, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_int_res__RD = vl::not_(n_RD_org);
        n_delayed_D = i_D;
        n_D_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_D, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_int_res__D = vl::ucomb(udp::mipi_pll_dphy_mux2, vl::v3(n_SI_org), vl::v3(n_D_org), vl::v3(n_SE_org));
        n_delayed_CK = i_CK;
        n_CK_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_CK, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_SD = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(vl::logic<1>(0x0ULL), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        { const int in[3] = {vl::v3(n_CK_org), vl::v3(n_int_res__D), vl::v3(n_int_res__RD)}; if (s0_.update(udp::mipi_pll_dphy_dff_r_err, in)) chg = true; }
        n_xcr_0 = vl::from3(s0_.st);
        { const int in[6] = {vl::v3(vl::LX()), vl::v3(n_CK_org), vl::v3(n_int_res__D), vl::v3(n_SD), vl::v3(n_int_res__RD), vl::v3(n_xcr_0)}; if (s1_.update(udp::mipi_pll_dphy_dff_sr_0, in)) chg = true; }
        n_Q = vl::from3(s1_.st);
        }
        vl::wr(Q, n_Q);
    }
};

class mipi_pll_dphy_SEP_INV_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_INV_1);
    explicit mipi_pll_dphy_SEP_INV_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A_org;
    void eval() {
        const vl::logic<1> i_A = vl::rd(A);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::not_(n_A_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_INV_16 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_INV_16);
    explicit mipi_pll_dphy_SEP_INV_16(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A_org;
    void eval() {
        const vl::logic<1> i_A = vl::rd(A);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::not_(n_A_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_INV_2 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_INV_2);
    explicit mipi_pll_dphy_SEP_INV_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A_org;
    void eval() {
        const vl::logic<1> i_A = vl::rd(A);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::not_(n_A_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_INV_4 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_INV_4);
    explicit mipi_pll_dphy_SEP_INV_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A_org;
    void eval() {
        const vl::logic<1> i_A = vl::rd(A);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::not_(n_A_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_INV_S_12 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_INV_S_12);
    explicit mipi_pll_dphy_SEP_INV_S_12(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A_org;
    void eval() {
        const vl::logic<1> i_A = vl::rd(A);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::not_(n_A_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_INV_S_4 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_INV_S_4);
    explicit mipi_pll_dphy_SEP_INV_S_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A_org;
    void eval() {
        const vl::logic<1> i_A = vl::rd(A);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::not_(n_A_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_INV_S_8 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_INV_S_8);
    explicit mipi_pll_dphy_SEP_INV_S_8(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A_org;
    void eval() {
        const vl::logic<1> i_A = vl::rd(A);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::not_(n_A_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_MUX2_S_2 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> D0{"D0"};
    sc_core::sc_in<sc_dt::sc_logic> D1{"D1"};
    sc_core::sc_in<sc_dt::sc_logic> S{"S"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_MUX2_S_2);
    explicit mipi_pll_dphy_SEP_MUX2_S_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << D0 << D1 << S << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_D0_org, n_D1_org, n_S_org;
    void eval() {
        const vl::logic<1> i_D0 = vl::rd(D0);
        const vl::logic<1> i_D1 = vl::rd(D1);
        const vl::logic<1> i_S = vl::rd(S);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_S_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_S, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_D1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_D1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_D0_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_D0, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::ucomb(udp::mipi_pll_dphy_mux2, vl::v3(n_D1_org), vl::v3(n_D0_org), vl::v3(n_S_org));
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_MUX2_S_4 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> D0{"D0"};
    sc_core::sc_in<sc_dt::sc_logic> D1{"D1"};
    sc_core::sc_in<sc_dt::sc_logic> S{"S"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_MUX2_S_4);
    explicit mipi_pll_dphy_SEP_MUX2_S_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << D0 << D1 << S << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_D0_org, n_D1_org, n_S_org;
    void eval() {
        const vl::logic<1> i_D0 = vl::rd(D0);
        const vl::logic<1> i_D1 = vl::rd(D1);
        const vl::logic<1> i_S = vl::rd(S);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_S_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_S, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_D1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_D1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_D0_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_D0, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::ucomb(udp::mipi_pll_dphy_mux2, vl::v3(n_D1_org), vl::v3(n_D0_org), vl::v3(n_S_org));
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_ND2_S_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_ND2_S_1);
    explicit mipi_pll_dphy_SEP_ND2_S_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A1_org, n_A2_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::nand_(n_A1_org, n_A2_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_ND2_S_2 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_ND2_S_2);
    explicit mipi_pll_dphy_SEP_ND2_S_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A1_org, n_A2_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::nand_(n_A1_org, n_A2_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_ND2_S_4 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_ND2_S_4);
    explicit mipi_pll_dphy_SEP_ND2_S_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A1_org, n_A2_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::nand_(n_A1_org, n_A2_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_ND2_S_8 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_ND2_S_8);
    explicit mipi_pll_dphy_SEP_ND2_S_8(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A1_org, n_A2_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::nand_(n_A1_org, n_A2_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_ND3_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> A3{"A3"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_ND3_1);
    explicit mipi_pll_dphy_SEP_ND3_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << A3 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A1_org, n_A2_org, n_A3_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_A3 = vl::rd(A3);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A3_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A3, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::nand_(n_A1_org, n_A2_org, n_A3_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_ND3_2 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> A3{"A3"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_ND3_2);
    explicit mipi_pll_dphy_SEP_ND3_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << A3 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A1_org, n_A2_org, n_A3_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_A3 = vl::rd(A3);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A3_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A3, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::nand_(n_A1_org, n_A2_org, n_A3_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_ND4_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> A3{"A3"};
    sc_core::sc_in<sc_dt::sc_logic> A4{"A4"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_ND4_1);
    explicit mipi_pll_dphy_SEP_ND4_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << A3 << A4 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A1_org, n_A2_org, n_A3_org, n_A4_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_A3 = vl::rd(A3);
        const vl::logic<1> i_A4 = vl::rd(A4);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A4_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A4, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A3_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A3, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::nand_(n_A1_org, n_A2_org, n_A3_org, n_A4_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_ND4_2 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> A3{"A3"};
    sc_core::sc_in<sc_dt::sc_logic> A4{"A4"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_ND4_2);
    explicit mipi_pll_dphy_SEP_ND4_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << A3 << A4 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A1_org, n_A2_org, n_A3_org, n_A4_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_A3 = vl::rd(A3);
        const vl::logic<1> i_A4 = vl::rd(A4);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A4_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A4, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A3_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A3, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::nand_(n_A1_org, n_A2_org, n_A3_org, n_A4_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_ND4_4 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> A3{"A3"};
    sc_core::sc_in<sc_dt::sc_logic> A4{"A4"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_ND4_4);
    explicit mipi_pll_dphy_SEP_ND4_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << A3 << A4 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A1_org, n_A2_org, n_A3_org, n_A4_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_A3 = vl::rd(A3);
        const vl::logic<1> i_A4 = vl::rd(A4);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A4_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A4, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A3_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A3, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::nand_(n_A1_org, n_A2_org, n_A3_org, n_A4_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_NR2_S_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_NR2_S_1);
    explicit mipi_pll_dphy_SEP_NR2_S_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A1_org, n_A2_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::nor_(n_A1_org, n_A2_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_NR2_S_2 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_NR2_S_2);
    explicit mipi_pll_dphy_SEP_NR2_S_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A1_org, n_A2_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::nor_(n_A1_org, n_A2_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_NR2_S_4 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_NR2_S_4);
    explicit mipi_pll_dphy_SEP_NR2_S_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A1_org, n_A2_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::nor_(n_A1_org, n_A2_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_NR4_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> A3{"A3"};
    sc_core::sc_in<sc_dt::sc_logic> A4{"A4"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_NR4_1);
    explicit mipi_pll_dphy_SEP_NR4_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << A3 << A4 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A1_org, n_A2_org, n_A3_org, n_A4_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_A3 = vl::rd(A3);
        const vl::logic<1> i_A4 = vl::rd(A4);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A4_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A4, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A3_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A3, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::nor_(n_A1_org, n_A2_org, n_A3_org, n_A4_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_OA21_2 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> B{"B"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_OA21_2);
    explicit mipi_pll_dphy_SEP_OA21_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << B << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_int_res_0, n_A1_org, n_A2_org, n_B_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_B = vl::rd(B);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_B_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_0 = vl::or_(n_A1_org, n_A2_org);
    n_X = vl::and_(n_int_res_0, n_B_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_OAI21_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> B{"B"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_OAI21_1);
    explicit mipi_pll_dphy_SEP_OAI21_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << B << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_int_res_0, n_A1_org, n_A2_org, n_B_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_B = vl::rd(B);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_B_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_0 = vl::or_(n_A1_org, n_A2_org);
    n_X = vl::nand_(n_int_res_0, n_B_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_OAI21_2 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> B{"B"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_OAI21_2);
    explicit mipi_pll_dphy_SEP_OAI21_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << B << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_int_res_0, n_A1_org, n_A2_org, n_B_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_B = vl::rd(B);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_B_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_0 = vl::or_(n_A1_org, n_A2_org);
    n_X = vl::nand_(n_int_res_0, n_B_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_OR2_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_OR2_1);
    explicit mipi_pll_dphy_SEP_OR2_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A1_org, n_A2_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::or_(n_A1_org, n_A2_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_OR2_12 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_OR2_12);
    explicit mipi_pll_dphy_SEP_OR2_12(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A1_org, n_A2_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::or_(n_A1_org, n_A2_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_OR2_2 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_OR2_2);
    explicit mipi_pll_dphy_SEP_OR2_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A1_org, n_A2_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::or_(n_A1_org, n_A2_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_SEP_OR2_4 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_pll_dphy_SEP_OR2_4);
    explicit mipi_pll_dphy_SEP_OR2_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_A1_org, n_A2_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_X = vl::or_(n_A1_org, n_A2_org);
        vl::wr(X, n_X);
    }
};

class mipi_pll_dphy_mpll_l2h_dcc : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> ck_l2h{"ck_l2h"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> pwr_on{"pwr_on"};
    sc_core::sc_in<sc_dt::sc_logic> pwr_onz{"pwr_onz"};
    sc_core::sc_in<sc_dt::sc_logic> vpgate{"vpgate"};
    sc_core::sc_in<sc_dt::sc_logic> vreg_mpll{"vreg_mpll"};
    sc_core::sc_out<sc_dt::sc_logic> clk_out{"clk_out"};
    SC_HAS_PROCESS(mipi_pll_dphy_mpll_l2h_dcc);
    explicit mipi_pll_dphy_mpll_l2h_dcc(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << ck_l2h << gd << pwr_on << pwr_onz << vpgate << vreg_mpll;
    }

private:
    vl::logic<1> n_clk_out;
    void eval() {
        const vl::logic<1> i_ck_l2h = vl::rd(ck_l2h);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_pwr_on = vl::rd(pwr_on);
        const vl::logic<1> i_pwr_onz = vl::rd(pwr_onz);
        const vl::logic<1> i_vpgate = vl::rd(vpgate);
        const vl::logic<1> i_vreg_mpll = vl::rd(vreg_mpll);
    n_clk_out = vl::bufif1_(~i_ck_l2h, vl::land(vl::land(vl::land(vl::ceq(i_pwr_on, vl::logic<1>(0x1ULL)), vl::ceq(i_pwr_onz, vl::logic<1>(0x0ULL))), vl::ceq(i_gd, vl::logic<1>(0x0ULL))), vl::ceq(i_vreg_mpll, vl::logic<1>(0x1ULL))));
        vl::wr(clk_out, n_clk_out);
    }
};

class mipi_pll_dphy_mpll_l2h_res : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> ip{"ip"};
    sc_core::sc_out<sc_dt::sc_logic> op{"op"};
    SC_HAS_PROCESS(mipi_pll_dphy_mpll_l2h_res);
    explicit mipi_pll_dphy_mpll_l2h_res(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << gd << ip;
    }

private:
    vl::logic<1> n_op;
    void eval() {
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_ip = vl::rd(ip);
    n_op = vl::bufif0_(i_ip, vl::ceq(i_gd, vl::logic<1>(0x0ULL)));
        vl::wr(op, n_op);
    }
};

class mipi_pll_dphy_mpll_rx_ckdrv_dccres : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> inr{"inr"};
    sc_core::sc_out<sc_dt::sc_logic> outr{"outr"};
    SC_HAS_PROCESS(mipi_pll_dphy_mpll_rx_ckdrv_dccres);
    explicit mipi_pll_dphy_mpll_rx_ckdrv_dccres(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << gd << inr;
    }

private:
    vl::logic<1> n_outr;
    void eval() {
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_inr = vl::rd(inr);
    n_outr = i_inr;
        vl::wr(outr, n_outr);
    }
};

class mipi_pll_dphy_mpll_rx_ckdrv_na2x31 : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_in<sc_dt::sc_logic> b{"b"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    SC_HAS_PROCESS(mipi_pll_dphy_mpll_rx_ckdrv_na2x31);
    explicit mipi_pll_dphy_mpll_rx_ckdrv_na2x31(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << a << b << gd << vp;
    }

private:
    vl::logic<1> n_z;
    void eval() {
        const vl::logic<1> i_a = vl::rd(a);
        const vl::logic<1> i_b = vl::rd(b);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vp = vl::rd(vp);
    n_z = vl::bufif1_(~vl::land(i_a, i_b), vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))));
        vl::wr(z, n_z);
    }
};

class mipi_pll_dphy_mpll_rx_ckdrv_na2x31_thresh : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_in<sc_dt::sc_logic> b{"b"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    SC_HAS_PROCESS(mipi_pll_dphy_mpll_rx_ckdrv_na2x31_thresh);
    explicit mipi_pll_dphy_mpll_rx_ckdrv_na2x31_thresh(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << a << b << gd << vp;
    }

private:
    vl::logic<1> n_z;
    void eval() {
        const vl::logic<1> i_a = vl::rd(a);
        const vl::logic<1> i_b = vl::rd(b);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vp = vl::rd(vp);
    n_z = vl::bufif1_(~vl::land(i_a, i_b), vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))));
        vl::wr(z, n_z);
    }
};

class mipi_pll_dphy_mpll_rx_dcc_del_x0_path : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> clk_i{"clk_i"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> hs{"hs"};
    sc_core::sc_in<sc_dt::sc_logic> vdcc_iq{"vdcc_iq"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    sc_core::sc_in<sc_dt::sc_logic> vp_tie{"vp_tie"};
    sc_core::sc_out<sc_dt::sc_logic> clk_o{"clk_o"};
    SC_HAS_PROCESS(mipi_pll_dphy_mpll_rx_dcc_del_x0_path);
    explicit mipi_pll_dphy_mpll_rx_dcc_del_x0_path(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << clk_i << gd << hs << vdcc_iq << vp << vp_tie;
    }

private:
    vl::logic<1> n_clk_o, n_clk1, n_clk2, n_clk3;
    void eval() {
        const vl::logic<1> i_clk_i = vl::rd(clk_i);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_hs = vl::rd(hs);
        const vl::logic<1> i_vdcc_iq = vl::rd(vdcc_iq);
        const vl::logic<1> i_vp = vl::rd(vp);
        const vl::logic<1> i_vp_tie = vl::rd(vp_tie);
    n_clk1 = vl::bufif1_(~i_clk_i, vl::land(vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))), vl::lor(vl::lor(vl::ceq(i_vp_tie, vl::logic<1>(0x1ULL)), vl::ceq(i_vdcc_iq, vl::logic<1>(0x1ULL))), vl::ceq(i_hs, vl::logic<1>(0x1ULL)))));
    n_clk2 = vl::bufif1_(~n_clk1, vl::land(vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))), vl::lor(vl::lor(vl::ceq(i_vp_tie, vl::logic<1>(0x1ULL)), vl::ceq(i_vdcc_iq, vl::logic<1>(0x1ULL))), vl::ceq(i_hs, vl::logic<1>(0x1ULL)))));
    n_clk3 = vl::bufif1_(~n_clk2, vl::land(vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))), vl::lor(vl::lor(vl::ceq(i_vp_tie, vl::logic<1>(0x1ULL)), vl::ceq(i_vdcc_iq, vl::logic<1>(0x1ULL))), vl::ceq(i_hs, vl::logic<1>(0x1ULL)))));
    n_clk_o = vl::bufif1_(~n_clk3, vl::land(vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))), vl::lor(vl::lor(vl::ceq(i_vp_tie, vl::logic<1>(0x1ULL)), vl::ceq(i_vdcc_iq, vl::logic<1>(0x1ULL))), vl::ceq(i_hs, vl::logic<1>(0x1ULL)))));
        vl::wr(clk_o, n_clk_o);
    }
};

class mipi_pll_dphy_sc_ck_tri_nax36_lvt_pwrdlow : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> clk1_m{"clk1_m"};
    sc_core::sc_in<sc_dt::sc_logic> clk1_p{"clk1_p"};
    sc_core::sc_in<sc_dt::sc_logic> pwr_on{"pwr_on"};
    sc_core::sc_in<sc_dt::sc_logic> tie_high{"tie_high"};
    sc_core::sc_out<sc_dt::sc_logic> clk3_m{"clk3_m"};
    sc_core::sc_out<sc_dt::sc_logic> clk3_p{"clk3_p"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_pll_dphy_sc_ck_tri_nax36_lvt_pwrdlow);
    explicit mipi_pll_dphy_sc_ck_tri_nax36_lvt_pwrdlow(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << clk1_m << clk1_p << pwr_on << tie_high << gd << vp;
    }

private:
    vl::logic<1> n_clk3_m, n_clk3_p, n_pwr_on_int;
    void eval() {
        const vl::logic<1> i_clk1_m = vl::rd(clk1_m);
        const vl::logic<1> i_clk1_p = vl::rd(clk1_p);
        const vl::logic<1> i_pwr_on = vl::rd(pwr_on);
        const vl::logic<1> i_tie_high = vl::rd(tie_high);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vp = vl::rd(vp);
    n_pwr_on_int = vl::ceq(i_vp, vl::logic<1>(0x1ULL)) & vl::ceq(i_gd, vl::logic<1>(0x0ULL));
    n_clk3_m = vl::bufif1_((i_clk1_m | (~i_pwr_on)) | (~i_tie_high), vl::ceq(n_pwr_on_int, vl::logic<1>(0x1ULL)));
    n_clk3_p = vl::bufif1_((i_clk1_p | (~i_pwr_on)) | (~i_tie_high), vl::ceq(n_pwr_on_int, vl::logic<1>(0x1ULL)));
        vl::wr(clk3_m, n_clk3_m);
        vl::wr(clk3_p, n_clk3_p);
    }
};

class mipi_pll_dphy_sc_invx12_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_pll_dphy_sc_invx12_lvt);
    explicit mipi_pll_dphy_sc_invx12_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << a << gd << vp;
    }

private:
    vl::logic<1> n_z;
    void eval() {
        const vl::logic<1> i_a = vl::rd(a);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vp = vl::rd(vp);
    n_z = vl::bufif1_(~i_a, vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))));
        vl::wr(z, n_z);
    }
};

class mipi_pll_dphy_sc_invx16_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_pll_dphy_sc_invx16_lvt);
    explicit mipi_pll_dphy_sc_invx16_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << a << gd << vp;
    }

private:
    vl::logic<1> n_z;
    void eval() {
        const vl::logic<1> i_a = vl::rd(a);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vp = vl::rd(vp);
    n_z = vl::bufif1_(~i_a, vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))));
        vl::wr(z, n_z);
    }
};

class mipi_pll_dphy_sc_invx16_lvt_highz : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_in<sc_dt::sc_logic> on{"on"};
    sc_core::sc_in<sc_dt::sc_logic> onz{"onz"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_pll_dphy_sc_invx16_lvt_highz);
    explicit mipi_pll_dphy_sc_invx16_lvt_highz(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << a << on << onz << gd << vp;
    }

private:
    vl::logic<1> n_z;
    void eval() {
        const vl::logic<1> i_a = vl::rd(a);
        const vl::logic<1> i_on = vl::rd(on);
        const vl::logic<1> i_onz = vl::rd(onz);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vp = vl::rd(vp);
    n_z = vl::bufif1_(~i_a, vl::land(vl::land(vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))), vl::ceq(i_on, vl::logic<1>(0x1ULL))), vl::ceq(i_onz, vl::logic<1>(0x0ULL))));
        vl::wr(z, n_z);
    }
};

class mipi_pll_dphy_sc_invx192_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_pll_dphy_sc_invx192_lvt);
    explicit mipi_pll_dphy_sc_invx192_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << a << gd << vp;
    }

private:
    vl::logic<1> n_z;
    void eval() {
        const vl::logic<1> i_a = vl::rd(a);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vp = vl::rd(vp);
    n_z = vl::bufif1_(~i_a, vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))));
        vl::wr(z, n_z);
    }
};

class mipi_pll_dphy_sc_invx24_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_pll_dphy_sc_invx24_lvt);
    explicit mipi_pll_dphy_sc_invx24_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << a << gd << vp;
    }

private:
    vl::logic<1> n_z;
    void eval() {
        const vl::logic<1> i_a = vl::rd(a);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vp = vl::rd(vp);
    n_z = vl::bufif1_(~i_a, vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))));
        vl::wr(z, n_z);
    }
};

class mipi_pll_dphy_sc_invx32_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_pll_dphy_sc_invx32_lvt);
    explicit mipi_pll_dphy_sc_invx32_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << a << gd << vp;
    }

private:
    vl::logic<1> n_z;
    void eval() {
        const vl::logic<1> i_a = vl::rd(a);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vp = vl::rd(vp);
    n_z = vl::bufif1_(~i_a, vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))));
        vl::wr(z, n_z);
    }
};

class mipi_pll_dphy_sc_invx48_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_pll_dphy_sc_invx48_lvt);
    explicit mipi_pll_dphy_sc_invx48_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << a << gd << vp;
    }

private:
    vl::logic<1> n_z;
    void eval() {
        const vl::logic<1> i_a = vl::rd(a);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vp = vl::rd(vp);
    n_z = vl::bufif1_(~i_a, vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))));
        vl::wr(z, n_z);
    }
};

class mipi_pll_dphy_sc_invx4_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_pll_dphy_sc_invx4_lvt);
    explicit mipi_pll_dphy_sc_invx4_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << a << gd << vp;
    }

private:
    vl::logic<1> n_z;
    void eval() {
        const vl::logic<1> i_a = vl::rd(a);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vp = vl::rd(vp);
    n_z = vl::bufif1_(~i_a, vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))));
        vl::wr(z, n_z);
    }
};

class mipi_pll_dphy_sc_invx64_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_pll_dphy_sc_invx64_lvt);
    explicit mipi_pll_dphy_sc_invx64_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << a << gd << vp;
    }

private:
    vl::logic<1> n_z;
    void eval() {
        const vl::logic<1> i_a = vl::rd(a);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vp = vl::rd(vp);
    n_z = vl::bufif1_(~i_a, vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))));
        vl::wr(z, n_z);
    }
};

class mipi_pll_dphy_sc_invx6_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_pll_dphy_sc_invx6_lvt);
    explicit mipi_pll_dphy_sc_invx6_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << a << gd << vp;
    }

private:
    vl::logic<1> n_z;
    void eval() {
        const vl::logic<1> i_a = vl::rd(a);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vp = vl::rd(vp);
    n_z = vl::bufif1_(~i_a, vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))));
        vl::wr(z, n_z);
    }
};

class mipi_pll_dphy_sc_invx8_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_pll_dphy_sc_invx8_lvt);
    explicit mipi_pll_dphy_sc_invx8_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << a << gd << vp;
    }

private:
    vl::logic<1> n_z;
    void eval() {
        const vl::logic<1> i_a = vl::rd(a);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vp = vl::rd(vp);
    n_z = vl::bufif1_(~i_a, vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))));
        vl::wr(z, n_z);
    }
};

class mipi_pll_dphy_sc_loadx16_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_pll_dphy_sc_loadx16_lvt);
    explicit mipi_pll_dphy_sc_loadx16_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << a << gd << vp;
    }

private:
    void eval() {
        const vl::logic<1> i_a = vl::rd(a);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vp = vl::rd(vp);
    }
};

class mipi_pll_dphy_sc_mux2x8_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a0{"a0"};
    sc_core::sc_in<sc_dt::sc_logic> a1{"a1"};
    sc_core::sc_in<sc_dt::sc_logic> s{"s"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_pll_dphy_sc_mux2x8_lvt);
    explicit mipi_pll_dphy_sc_mux2x8_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << a0 << a1 << s << gd << vp;
    }

private:
    vl::logic<1> n_z, n_supply_ok, n_ax_n;
    void eval() {
        const vl::logic<1> i_a0 = vl::rd(a0);
        const vl::logic<1> i_a1 = vl::rd(a1);
        const vl::logic<1> i_s = vl::rd(s);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vp = vl::rd(vp);
    n_ax_n = vl::resolve(vl::bufif1_(~i_a1, vl::ceq(i_s, vl::logic<1>(0x1ULL))), vl::bufif1_(~i_a0, vl::ceq(i_s, vl::logic<1>(0x0ULL))));
    n_supply_ok = vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL)));
    n_z = vl::bufif1_(~n_ax_n, vl::ceq(n_supply_ok, vl::logic<1>(0x1ULL)));
        vl::wr(z, n_z);
    }
};

class mipi_pll_dphy_sc_na2x12_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_in<sc_dt::sc_logic> b{"b"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_pll_dphy_sc_na2x12_lvt);
    explicit mipi_pll_dphy_sc_na2x12_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << a << b << gd << vp;
    }

private:
    vl::logic<1> n_z;
    void eval() {
        const vl::logic<1> i_a = vl::rd(a);
        const vl::logic<1> i_b = vl::rd(b);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vp = vl::rd(vp);
    n_z = vl::bufif1_(~vl::land(i_a, i_b), vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))));
        vl::wr(z, n_z);
    }
};

class mipi_pll_dphy_sc_na2x24_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_in<sc_dt::sc_logic> b{"b"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_pll_dphy_sc_na2x24_lvt);
    explicit mipi_pll_dphy_sc_na2x24_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << a << b << gd << vp;
    }

private:
    vl::logic<1> n_z;
    void eval() {
        const vl::logic<1> i_a = vl::rd(a);
        const vl::logic<1> i_b = vl::rd(b);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vp = vl::rd(vp);
    n_z = vl::bufif1_(~vl::land(i_a, i_b), vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))));
        vl::wr(z, n_z);
    }
};

class mipi_pll_dphy_sc_na2x6_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_in<sc_dt::sc_logic> b{"b"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_pll_dphy_sc_na2x6_lvt);
    explicit mipi_pll_dphy_sc_na2x6_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << a << b << gd << vp;
    }

private:
    vl::logic<1> n_z;
    void eval() {
        const vl::logic<1> i_a = vl::rd(a);
        const vl::logic<1> i_b = vl::rd(b);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vp = vl::rd(vp);
    n_z = vl::bufif1_(~vl::land(i_a, i_b), vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))));
        vl::wr(z, n_z);
    }
};

class mipi_pll_dphy_sc_na3x16_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_in<sc_dt::sc_logic> b{"b"};
    sc_core::sc_in<sc_dt::sc_logic> c{"c"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_pll_dphy_sc_na3x16_lvt);
    explicit mipi_pll_dphy_sc_na3x16_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << a << b << c << gd << vp;
    }

private:
    vl::logic<1> n_z;
    void eval() {
        const vl::logic<1> i_a = vl::rd(a);
        const vl::logic<1> i_b = vl::rd(b);
        const vl::logic<1> i_c = vl::rd(c);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vp = vl::rd(vp);
    n_z = vl::bufif1_(~vl::land(vl::land(i_a, i_b), i_c), vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))));
        vl::wr(z, n_z);
    }
};

class mipi_pll_dphy_sc_na3x6_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_in<sc_dt::sc_logic> b{"b"};
    sc_core::sc_in<sc_dt::sc_logic> c{"c"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_pll_dphy_sc_na3x6_lvt);
    explicit mipi_pll_dphy_sc_na3x6_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << a << b << c << gd << vp;
    }

private:
    vl::logic<1> n_z;
    void eval() {
        const vl::logic<1> i_a = vl::rd(a);
        const vl::logic<1> i_b = vl::rd(b);
        const vl::logic<1> i_c = vl::rd(c);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vp = vl::rd(vp);
    n_z = vl::bufif1_(~vl::land(vl::land(i_a, i_b), i_c), vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))));
        vl::wr(z, n_z);
    }
};

class mipi_pll_dphy_sc_na4x6_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_in<sc_dt::sc_logic> b{"b"};
    sc_core::sc_in<sc_dt::sc_logic> c{"c"};
    sc_core::sc_in<sc_dt::sc_logic> d{"d"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_pll_dphy_sc_na4x6_lvt);
    explicit mipi_pll_dphy_sc_na4x6_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << a << b << c << d << gd << vp;
    }

private:
    vl::logic<1> n_z;
    void eval() {
        const vl::logic<1> i_a = vl::rd(a);
        const vl::logic<1> i_b = vl::rd(b);
        const vl::logic<1> i_c = vl::rd(c);
        const vl::logic<1> i_d = vl::rd(d);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vp = vl::rd(vp);
    n_z = vl::bufif1_(~vl::land(vl::land(vl::land(i_a, i_b), i_c), i_d), vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))));
        vl::wr(z, n_z);
    }
};

class mipi_pll_dphy_sc_no2x12_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_in<sc_dt::sc_logic> b{"b"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_pll_dphy_sc_no2x12_lvt);
    explicit mipi_pll_dphy_sc_no2x12_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << a << b << gd << vp;
    }

private:
    vl::logic<1> n_z;
    void eval() {
        const vl::logic<1> i_a = vl::rd(a);
        const vl::logic<1> i_b = vl::rd(b);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vp = vl::rd(vp);
    n_z = vl::bufif1_(~vl::lor(i_a, i_b), vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))));
        vl::wr(z, n_z);
    }
};

class mipi_pll_dphy_sc_no2x6_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_in<sc_dt::sc_logic> b{"b"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_pll_dphy_sc_no2x6_lvt);
    explicit mipi_pll_dphy_sc_no2x6_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << a << b << gd << vp;
    }

private:
    vl::logic<1> n_z;
    void eval() {
        const vl::logic<1> i_a = vl::rd(a);
        const vl::logic<1> i_b = vl::rd(b);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vp = vl::rd(vp);
    n_z = vl::bufif1_(~vl::lor(i_a, i_b), vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))));
        vl::wr(z, n_z);
    }
};

class mipi_pll_dphy_sc_tgate_x16_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_in<sc_dt::sc_logic> c_m{"c_m"};
    sc_core::sc_in<sc_dt::sc_logic> c_p{"c_p"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_pll_dphy_sc_tgate_x16_lvt);
    explicit mipi_pll_dphy_sc_tgate_x16_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << a << c_m << c_p << gd << vp;
    }

private:
    vl::logic<1> n_z;
    void eval() {
        const vl::logic<1> i_a = vl::rd(a);
        const vl::logic<1> i_c_m = vl::rd(c_m);
        const vl::logic<1> i_c_p = vl::rd(c_p);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vp = vl::rd(vp);
    n_z = vl::bufif1_(i_a, vl::land(vl::land(vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))), vl::ceq(i_c_p, vl::logic<1>(0x1ULL))), vl::ceq(i_c_m, vl::logic<1>(0x0ULL))));
        vl::wr(z, n_z);
    }
};

class mipi_pll_dphy_schc_invx128_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_pll_dphy_schc_invx128_lvt);
    explicit mipi_pll_dphy_schc_invx128_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << a << gd << vp;
    }

private:
    vl::logic<1> n_z;
    void eval() {
        const vl::logic<1> i_a = vl::rd(a);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vp = vl::rd(vp);
    n_z = vl::bufif1_(~i_a, vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))));
        vl::wr(z, n_z);
    }
};

class mipi_pll_dphy_schc_invx32_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_pll_dphy_schc_invx32_lvt);
    explicit mipi_pll_dphy_schc_invx32_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << a << gd << vp;
    }

private:
    vl::logic<1> n_z;
    void eval() {
        const vl::logic<1> i_a = vl::rd(a);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vp = vl::rd(vp);
    n_z = vl::bufif1_(~i_a, vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))));
        vl::wr(z, n_z);
    }
};

class mipi_pll_dphy_schc_invx64_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_pll_dphy_schc_invx64_lvt);
    explicit mipi_pll_dphy_schc_invx64_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << a << gd << vp;
    }

private:
    vl::logic<1> n_z;
    void eval() {
        const vl::logic<1> i_a = vl::rd(a);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vp = vl::rd(vp);
    n_z = vl::bufif1_(~i_a, vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))));
        vl::wr(z, n_z);
    }
};

class mipi_pll_dphy_sctg_inv1x : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};
    SC_HAS_PROCESS(mipi_pll_dphy_sctg_inv1x);
    explicit mipi_pll_dphy_sctg_inv1x(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << a << gd << vph;
    }

private:
    vl::logic<1> n_z;
    void eval() {
        const vl::logic<1> i_a = vl::rd(a);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vph = vl::rd(vph);
    n_z = vl::bufif1_(~i_a, vl::land(vl::ceq(i_vph, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))));
        vl::wr(z, n_z);
    }
};

class mipi_pll_dphy_sctg_inv2x : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};
    SC_HAS_PROCESS(mipi_pll_dphy_sctg_inv2x);
    explicit mipi_pll_dphy_sctg_inv2x(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << a << gd << vph;
    }

private:
    vl::logic<1> n_z;
    void eval() {
        const vl::logic<1> i_a = vl::rd(a);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vph = vl::rd(vph);
    n_z = vl::bufif1_(~i_a, vl::land(vl::ceq(i_vph, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))));
        vl::wr(z, n_z);
    }
};

class mipi_pll_dphy_sctg_inv8x : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};
    SC_HAS_PROCESS(mipi_pll_dphy_sctg_inv8x);
    explicit mipi_pll_dphy_sctg_inv8x(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << a << gd << vph;
    }

private:
    vl::logic<1> n_z;
    void eval() {
        const vl::logic<1> i_a = vl::rd(a);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vph = vl::rd(vph);
    n_z = vl::bufif1_(~i_a, vl::land(vl::ceq(i_vph, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))));
        vl::wr(z, n_z);
    }
};

class mipi_pll_dphy_tieLowHigh_HV : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> high{"high"};
    sc_core::sc_out<sc_dt::sc_logic> low{"low"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};
    SC_HAS_PROCESS(mipi_pll_dphy_tieLowHigh_HV);
    explicit mipi_pll_dphy_tieLowHigh_HV(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << gd << vph;
    }

private:
    vl::logic<1> n_high, n_low;
    void eval() {
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vph = vl::rd(vph);
    n_low = vl::mux(vl::land(vl::ceq(i_vph, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))), vl::logic<1>(0x0ULL), vl::logic<1>::all_z());
    n_high = vl::mux(vl::land(vl::ceq(i_vph, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))), vl::logic<1>(0x1ULL), vl::logic<1>::all_z());
        vl::wr(high, n_high);
        vl::wr(low, n_low);
    }
};

class mipi_pll_dphy_wire_cap : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> w{"w"};
    SC_HAS_PROCESS(mipi_pll_dphy_wire_cap);
    explicit mipi_pll_dphy_wire_cap(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << w;
    }

private:
    void eval() {
        const vl::logic<1> i_w = vl::rd(w);
    }
};

#endif
