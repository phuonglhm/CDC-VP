//--------------------------------------------------------------------------------------------------
// library cells (rx_cells)
//--------------------------------------------------------------------------------------------------
#ifndef VL_RX_CELLS_H
#define VL_RX_CELLS_H

#include "vl/vl_rt.h"
#include "udp_lib.h"

class mipi_4_rx_dphy_ns_DPHY3AFE_nor4hv : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> B{"B"};
    sc_core::sc_in<sc_dt::sc_logic> C{"C"};
    sc_core::sc_in<sc_dt::sc_logic> D{"D"};
    sc_core::sc_out<sc_dt::sc_logic> Z{"Z"};
    sc_core::sc_in<sc_dt::sc_logic> agnd{"agnd"};
    sc_core::sc_in<sc_dt::sc_logic> avdd{"avdd"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHY3AFE_nor4hv);
    explicit mipi_4_rx_dphy_ns_DPHY3AFE_nor4hv(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A << B << C << D << agnd << avdd;
    }

private:
    vl::logic<1> n_Z;
    void eval() {
        const vl::logic<1> i_A = vl::rd(A);
        const vl::logic<1> i_B = vl::rd(B);
        const vl::logic<1> i_C = vl::rd(C);
        const vl::logic<1> i_D = vl::rd(D);
        const vl::logic<1> i_agnd = vl::rd(agnd);
        const vl::logic<1> i_avdd = vl::rd(avdd);
    n_Z = vl::bufif1_(~(((i_A | i_B) | i_C) | i_D), vl::land(vl::ceq(i_avdd, vl::logic<1>(0x1ULL)), vl::ceq(i_agnd, vl::logic<1>(0x0ULL))));
        vl::wr(Z, n_Z);
    }
};

class mipi_4_rx_dphy_ns_DPHYATB_testswitch : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> comm{"comm"};
    sc_core::sc_in<sc_dt::sc_logic> pon{"pon"};
    sc_core::sc_in<sc_dt::sc_logic> ponz{"ponz"};
    sc_core::sc_inout<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYATB_testswitch);
    explicit mipi_4_rx_dphy_ns_DPHYATB_testswitch(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << comm << pon << ponz << A << gd << vph;
    }

private:
    vl::logic<1> n_A;
    void eval() {
        const vl::logic<1> i_comm = vl::rd(comm);
        const vl::logic<1> i_pon = vl::rd(pon);
        const vl::logic<1> i_ponz = vl::rd(ponz);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vph = vl::rd(vph);
    n_A = vl::bufif1_(i_comm, vl::land(vl::land(vl::land(vl::ceq(i_vph, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))), vl::ceq(i_pon, vl::logic<1>(0x1ULL))), vl::ceq(i_ponz, vl::logic<1>(0x0ULL))));
        vl::wr(A, n_A);
    }
};

class mipi_4_rx_dphy_ns_DPHYATB_testswitch_vp : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> comm{"comm"};
    sc_core::sc_in<sc_dt::sc_logic> pon{"pon"};
    sc_core::sc_in<sc_dt::sc_logic> ponz{"ponz"};
    sc_core::sc_inout<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYATB_testswitch_vp);
    explicit mipi_4_rx_dphy_ns_DPHYATB_testswitch_vp(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << comm << pon << ponz << A << gd;
    }

private:
    vl::logic<1> n_A;
    void eval() {
        const vl::logic<1> i_comm = vl::rd(comm);
        const vl::logic<1> i_pon = vl::rd(pon);
        const vl::logic<1> i_ponz = vl::rd(ponz);
        const vl::logic<1> i_gd = vl::rd(gd);
    n_A = vl::bufif1_(i_comm, vl::land(vl::land(vl::ceq(i_gd, vl::logic<1>(0x0ULL)), vl::ceq(i_pon, vl::logic<1>(0x1ULL))), vl::ceq(i_ponz, vl::logic<1>(0x0ULL))));
        vl::wr(A, n_A);
    }
};

class mipi_4_rx_dphy_ns_DPHYHSRX_DDL_MUX_2_1_inv_en : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> i{"i"};
    sc_core::sc_in<sc_dt::sc_logic> on{"on"};
    sc_core::sc_in<sc_dt::sc_logic> onz{"onz"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_out<sc_dt::sc_logic> o{"o"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_DDL_MUX_2_1_inv_en);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_DDL_MUX_2_1_inv_en(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << i << on << onz << gd << vp;
    }

private:
    vl::logic<1> n_o;
    void eval() {
        const vl::logic<1> i_i = vl::rd(i);
        const vl::logic<1> i_on = vl::rd(on);
        const vl::logic<1> i_onz = vl::rd(onz);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vp = vl::rd(vp);
    n_o = vl::bufif1_(~i_i, vl::land(vl::land(vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))), vl::ceq(i_on, vl::logic<1>(0x1ULL))), vl::ceq(i_onz, vl::logic<1>(0x0ULL))));
        vl::wr(o, n_o);
    }
};

class mipi_4_rx_dphy_ns_DPHYHSRX_buf_PLL_bustie : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> lb_pll_bustie_int{"lb_pll_bustie_int"};
    sc_core::sc_in<sc_dt::sc_logic> lb_pll_bustie_intz{"lb_pll_bustie_intz"};
    sc_core::sc_out<sc_dt::sc_logic> clk_pll0_buf{"clk_pll0_buf"};
    sc_core::sc_out<sc_dt::sc_logic> clk_pll180_buf{"clk_pll180_buf"};
    sc_core::sc_out<sc_dt::sc_logic> clk_pll270_buf{"clk_pll270_buf"};
    sc_core::sc_out<sc_dt::sc_logic> clk_pll90_buf{"clk_pll90_buf"};
    sc_core::sc_out<sc_dt::sc_logic> rxclk_lbck_clkout_stop_buf{"rxclk_lbck_clkout_stop_buf"};
    sc_core::sc_out<sc_dt::sc_logic> rxclk_lbdata_clkout_stop_buf{"rxclk_lbdata_clkout_stop_buf"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_buf_PLL_bustie);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_buf_PLL_bustie(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << lb_pll_bustie_int << lb_pll_bustie_intz << gd << vp;
    }

private:
    vl::logic<1> n_clk_pll0_buf, n_clk_pll180_buf, n_clk_pll270_buf, n_clk_pll90_buf, n_rxclk_lbck_clkout_stop_buf, n_rxclk_lbdata_clkout_stop_buf, n_supply_ok;
    void eval() {
        const vl::logic<1> i_lb_pll_bustie_int = vl::rd(lb_pll_bustie_int);
        const vl::logic<1> i_lb_pll_bustie_intz = vl::rd(lb_pll_bustie_intz);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vp = vl::rd(vp);
    n_supply_ok = vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL)));
    n_rxclk_lbck_clkout_stop_buf = vl::bufif1_(vl::logic<1>(0x1ULL), vl::land(vl::ceq(i_lb_pll_bustie_int, vl::logic<1>(0x0ULL)), vl::ceq(n_supply_ok, vl::logic<1>(0x1ULL))));
    n_rxclk_lbdata_clkout_stop_buf = vl::bufif1_(vl::logic<1>(0x1ULL), vl::land(vl::ceq(i_lb_pll_bustie_int, vl::logic<1>(0x0ULL)), vl::ceq(n_supply_ok, vl::logic<1>(0x1ULL))));
    n_clk_pll270_buf = vl::bufif1_(vl::logic<1>(0x0ULL), vl::land(vl::ceq(i_lb_pll_bustie_intz, vl::logic<1>(0x1ULL)), vl::ceq(n_supply_ok, vl::logic<1>(0x1ULL))));
    n_clk_pll180_buf = vl::bufif1_(vl::logic<1>(0x0ULL), vl::land(vl::ceq(i_lb_pll_bustie_intz, vl::logic<1>(0x1ULL)), vl::ceq(n_supply_ok, vl::logic<1>(0x1ULL))));
    n_clk_pll90_buf = vl::bufif1_(vl::logic<1>(0x0ULL), vl::land(vl::ceq(i_lb_pll_bustie_intz, vl::logic<1>(0x1ULL)), vl::ceq(n_supply_ok, vl::logic<1>(0x1ULL))));
    n_clk_pll0_buf = vl::bufif1_(vl::logic<1>(0x0ULL), vl::land(vl::ceq(i_lb_pll_bustie_intz, vl::logic<1>(0x1ULL)), vl::ceq(n_supply_ok, vl::logic<1>(0x1ULL))));
        vl::wr(clk_pll0_buf, n_clk_pll0_buf);
        vl::wr(clk_pll180_buf, n_clk_pll180_buf);
        vl::wr(clk_pll270_buf, n_clk_pll270_buf);
        vl::wr(clk_pll90_buf, n_clk_pll90_buf);
        vl::wr(rxclk_lbck_clkout_stop_buf, n_rxclk_lbck_clkout_stop_buf);
        vl::wr(rxclk_lbdata_clkout_stop_buf, n_rxclk_lbdata_clkout_stop_buf);
    }
};

class mipi_4_rx_dphy_ns_DPHYHSRX_clk_mux_buf : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> i{"i"};
    sc_core::sc_in<sc_dt::sc_logic> on{"on"};
    sc_core::sc_in<sc_dt::sc_logic> onz{"onz"};
    sc_core::sc_out<sc_dt::sc_logic> o{"o"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_clk_mux_buf);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_clk_mux_buf(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << i << on << onz << gd << vp;
    }

private:
    vl::logic<1> n_o;
    void eval() {
        const vl::logic<1> i_i = vl::rd(i);
        const vl::logic<1> i_on = vl::rd(on);
        const vl::logic<1> i_onz = vl::rd(onz);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vp = vl::rd(vp);
    n_o = vl::bufif1_(~i_i, vl::land(vl::land(vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_on, vl::logic<1>(0x1ULL))), vl::ceq(i_gd, vl::logic<1>(0x0ULL))), vl::ceq(i_onz, vl::logic<1>(0x0ULL))));
        vl::wr(o, n_o);
    }
};

class mipi_4_rx_dphy_ns_DPHYHSRX_des_gated_buf : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> data_en{"data_en"};
    sc_core::sc_in<sc_dt::sc_logic> data_enz{"data_enz"};
    sc_core::sc_in<sc_dt::sc_logic> i{"i"};
    sc_core::sc_in<sc_dt::sc_logic> pull_down{"pull_down"};
    sc_core::sc_in<sc_dt::sc_logic> pull_upz{"pull_upz"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_inout<sc_dt::sc_logic> o{"o"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_des_gated_buf);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_des_gated_buf(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << data_en << data_enz << i << pull_down << pull_upz << gd << o << vp;
    }

private:
    vl::logic<1> n_o, n_supply_ok;
    void eval() {
        const vl::logic<1> i_data_en = vl::rd(data_en);
        const vl::logic<1> i_data_enz = vl::rd(data_enz);
        const vl::logic<1> i_i = vl::rd(i);
        const vl::logic<1> i_pull_down = vl::rd(pull_down);
        const vl::logic<1> i_pull_upz = vl::rd(pull_upz);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vp = vl::rd(vp);
    n_supply_ok = vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL)));
    n_o = vl::resolve(vl::resolve(vl::bufif1_(~i_i, vl::land(vl::land(vl::ceq(i_data_en, vl::logic<1>(0x1ULL)), vl::ceq(i_data_enz, vl::logic<1>(0x0ULL))), vl::ceq(n_supply_ok, vl::logic<1>(0x1ULL)))), vl::bufif1_(vl::logic<1>(0x0ULL), vl::land(vl::ceq(i_pull_down, vl::logic<1>(0x1ULL)), vl::ceq(n_supply_ok, vl::logic<1>(0x1ULL))))), vl::bufif1_(vl::logic<1>(0x1ULL), vl::land(vl::ceq(i_pull_upz, vl::logic<1>(0x0ULL)), vl::ceq(n_supply_ok, vl::logic<1>(0x1ULL)))));
        vl::wr(o, n_o);
    }
};

class mipi_4_rx_dphy_ns_DPHYHSRX_div_buf : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> i{"i"};
    sc_core::sc_in<sc_dt::sc_logic> on{"on"};
    sc_core::sc_in<sc_dt::sc_logic> onz{"onz"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    sc_core::sc_inout<sc_dt::sc_logic> Z{"Z"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_div_buf);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_div_buf(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << i << on << onz << gd << vp << Z;
    }

private:
    vl::logic<1> n_Z;
    void eval() {
        const vl::logic<1> i_i = vl::rd(i);
        const vl::logic<1> i_on = vl::rd(on);
        const vl::logic<1> i_onz = vl::rd(onz);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vp = vl::rd(vp);
    n_Z = vl::bufif1_(~i_i, vl::land(vl::land(vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_on, vl::logic<1>(0x1ULL))), vl::ceq(i_on, vl::logic<1>(0x1ULL))), vl::ceq(i_onz, vl::logic<1>(0x0ULL))));
        vl::wr(Z, n_Z);
    }
};

class mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> bias{"bias"};
    sc_core::sc_in<sc_dt::sc_logic> onz{"onz"};
    sc_core::sc_inout<sc_dt::sc_logic> o{"o"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << bias << onz << o << vp;
    }

private:
    vl::logic<1> n_o;
    void eval() {
        const vl::logic<1> i_bias = vl::rd(bias);
        const vl::logic<1> i_onz = vl::rd(onz);
        const vl::logic<1> i_vp = vl::rd(vp);
    n_o = vl::bufif1_(vl::logic<1>(0x1ULL), vl::land(vl::land(vl::ceq(i_onz, vl::logic<1>(0x0ULL)), vl::ceq(i_bias, vl::logic<1>(0x1ULL))), vl::ceq(i_vp, vl::logic<1>(0x1ULL))));
        vl::wr(o, n_o);
    }
};

class mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> bias_h{"bias_h"};
    sc_core::sc_in<sc_dt::sc_logic> on_hz{"on_hz"};
    sc_core::sc_inout<sc_dt::sc_logic> o{"o"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << bias_h << on_hz << o << vph;
    }

private:
    vl::logic<1> n_o;
    void eval() {
        const vl::logic<1> i_bias_h = vl::rd(bias_h);
        const vl::logic<1> i_on_hz = vl::rd(on_hz);
        const vl::logic<1> i_vph = vl::rd(vph);
    n_o = vl::bufif1_(vl::logic<1>(0x1ULL), vl::land(vl::land(vl::ceq(i_bias_h, vl::logic<1>(0x1ULL)), vl::eq(i_on_hz, vl::logic<1>(0x0ULL))), vl::ceq(i_vph, vl::logic<1>(0x1ULL))));
        vl::wr(o, n_o);
    }
};

class mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> bias_h{"bias_h"};
    sc_core::sc_in<sc_dt::sc_logic> on_hz{"on_hz"};
    sc_core::sc_in<sc_dt::sc_logic> o{"o"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_current_source_hv_dummy(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << bias_h << on_hz << o << vph;
    }

private:
    void eval() {
        const vl::logic<1> i_bias_h = vl::rd(bias_h);
        const vl::logic<1> i_on_hz = vl::rd(on_hz);
        const vl::logic<1> i_o = vl::rd(o);
        const vl::logic<1> i_vph = vl::rd(vph);
    }
};

class mipi_4_rx_dphy_ns_DPHYHSRX_preamp_protsw : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> calsw_h{"calsw_h"};
    sc_core::sc_in<sc_dt::sc_logic> calswn_h{"calswn_h"};
    sc_core::sc_in<sc_dt::sc_logic> vi_m{"vi_m"};
    sc_core::sc_in<sc_dt::sc_logic> vi_p{"vi_p"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_inout<sc_dt::sc_logic> vi_m_prot{"vi_m_prot"};
    sc_core::sc_inout<sc_dt::sc_logic> vi_n_prot{"vi_n_prot"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_preamp_protsw);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_preamp_protsw(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << calsw_h << calswn_h << vi_m << vi_p << gd << vi_m_prot << vi_n_prot << vph;
    }

private:
    vl::logic<1> n_vi_m_prot, n_vi_n_prot, n_supply_ok;
    void eval() {
        const vl::logic<1> i_calsw_h = vl::rd(calsw_h);
        const vl::logic<1> i_calswn_h = vl::rd(calswn_h);
        const vl::logic<1> i_vi_m = vl::rd(vi_m);
        const vl::logic<1> i_vi_p = vl::rd(vi_p);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vph = vl::rd(vph);
    n_supply_ok = vl::land(vl::ceq(i_vph, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL)));
    n_vi_n_prot = vl::resolve(vl::bufif1_(i_vi_p, vl::land(vl::land(vl::ceq(n_supply_ok, vl::logic<1>(0x1ULL)), vl::ceq(i_calswn_h, vl::logic<1>(0x1ULL))), vl::ceq(i_calsw_h, vl::logic<1>(0x0ULL)))), vl::bufif1_(vl::logic<1>(0x0ULL), vl::land(vl::land(vl::ceq(n_supply_ok, vl::logic<1>(0x1ULL)), vl::ceq(i_calswn_h, vl::logic<1>(0x0ULL))), vl::ceq(i_calsw_h, vl::logic<1>(0x1ULL)))));
    n_vi_m_prot = vl::resolve(vl::bufif1_(i_vi_m, vl::land(vl::land(vl::ceq(n_supply_ok, vl::logic<1>(0x1ULL)), vl::ceq(i_calswn_h, vl::logic<1>(0x1ULL))), vl::ceq(i_calsw_h, vl::logic<1>(0x0ULL)))), vl::bufif1_(vl::logic<1>(0x0ULL), vl::land(vl::land(vl::ceq(n_supply_ok, vl::logic<1>(0x1ULL)), vl::ceq(i_calswn_h, vl::logic<1>(0x0ULL))), vl::ceq(i_calsw_h, vl::logic<1>(0x1ULL)))));
        vl::wr(vi_m_prot, n_vi_m_prot);
        vl::wr(vi_n_prot, n_vi_n_prot);
    }
};

class mipi_4_rx_dphy_ns_DPHYHSTX_drv_mux : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> deemph{"deemph"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> sel_preemph{"sel_preemph"};
    sc_core::sc_in<sc_dt::sc_logic> sel_sym{"sel_sym"};
    sc_core::sc_in<sc_dt::sc_logic> sym{"sym"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSTX_drv_mux);
    explicit mipi_4_rx_dphy_ns_DPHYHSTX_drv_mux(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << deemph << gd << sel_preemph << sel_sym << sym << vp;
    }

private:
    vl::logic<1> n_z, n_supply_ok;
    void eval() {
        const vl::logic<1> i_deemph = vl::rd(deemph);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_sel_preemph = vl::rd(sel_preemph);
        const vl::logic<1> i_sel_sym = vl::rd(sel_sym);
        const vl::logic<1> i_sym = vl::rd(sym);
        const vl::logic<1> i_vp = vl::rd(vp);
    n_supply_ok = vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL)));
    n_z = vl::resolve(vl::bufif1_(~i_sym, vl::land(vl::land(vl::ceq(i_sel_sym, vl::logic<1>(0x1ULL)), vl::ceq(i_sel_preemph, vl::logic<1>(0x0ULL))), vl::ceq(n_supply_ok, vl::logic<1>(0x1ULL)))), vl::bufif1_(~i_deemph, vl::land(vl::land(vl::ceq(i_sel_sym, vl::logic<1>(0x0ULL)), vl::ceq(i_sel_preemph, vl::logic<1>(0x1ULL))), vl::ceq(n_supply_ok, vl::logic<1>(0x1ULL)))));
        vl::wr(z, n_z);
    }
};

class mipi_4_rx_dphy_ns_DPHYHSTX_serial_imux4x_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a0{"a0"};
    sc_core::sc_in<sc_dt::sc_logic> a1{"a1"};
    sc_core::sc_in<sc_dt::sc_logic> a2{"a2"};
    sc_core::sc_in<sc_dt::sc_logic> a3{"a3"};
    sc_core::sc_in<sc_dt::sc_logic> s_m0{"s_m0"};
    sc_core::sc_in<sc_dt::sc_logic> s_m1{"s_m1"};
    sc_core::sc_in<sc_dt::sc_logic> s_m2{"s_m2"};
    sc_core::sc_in<sc_dt::sc_logic> s_m3{"s_m3"};
    sc_core::sc_in<sc_dt::sc_logic> s_p0{"s_p0"};
    sc_core::sc_in<sc_dt::sc_logic> s_p1{"s_p1"};
    sc_core::sc_in<sc_dt::sc_logic> s_p2{"s_p2"};
    sc_core::sc_in<sc_dt::sc_logic> s_p3{"s_p3"};
    sc_core::sc_in<sc_dt::sc_logic> store_on{"store_on"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSTX_serial_imux4x_lvt);
    explicit mipi_4_rx_dphy_ns_DPHYHSTX_serial_imux4x_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << a0 << a1 << a2 << a3 << s_m0 << s_m1 << s_m2 << s_m3 << s_p0 << s_p1 << s_p2 << s_p3 << store_on << gd << vp;
    }

private:
    vl::logic<1> n_z, n_supply_ok, n_ax;
    void eval() {
        const vl::logic<1> i_a0 = vl::rd(a0);
        const vl::logic<1> i_a1 = vl::rd(a1);
        const vl::logic<1> i_a2 = vl::rd(a2);
        const vl::logic<1> i_a3 = vl::rd(a3);
        const vl::logic<1> i_s_m0 = vl::rd(s_m0);
        const vl::logic<1> i_s_m1 = vl::rd(s_m1);
        const vl::logic<1> i_s_m2 = vl::rd(s_m2);
        const vl::logic<1> i_s_m3 = vl::rd(s_m3);
        const vl::logic<1> i_s_p0 = vl::rd(s_p0);
        const vl::logic<1> i_s_p1 = vl::rd(s_p1);
        const vl::logic<1> i_s_p2 = vl::rd(s_p2);
        const vl::logic<1> i_s_p3 = vl::rd(s_p3);
        const vl::logic<1> i_store_on = vl::rd(store_on);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vp = vl::rd(vp);
    n_supply_ok = vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL)));
    n_ax = vl::resolve(vl::resolve(vl::resolve(vl::resolve(vl::bufif1_(i_a3, vl::land(vl::land(vl::ceq(i_s_p3, vl::logic<1>(0x1ULL)), vl::ceq(i_s_m3, vl::logic<1>(0x0ULL))), vl::ceq(n_supply_ok, vl::logic<1>(0x1ULL)))), vl::bufif1_(i_a2, vl::land(vl::land(vl::ceq(i_s_p2, vl::logic<1>(0x1ULL)), vl::ceq(i_s_m2, vl::logic<1>(0x0ULL))), vl::ceq(n_supply_ok, vl::logic<1>(0x1ULL))))), vl::bufif1_(i_a1, vl::land(vl::land(vl::ceq(i_s_p1, vl::logic<1>(0x1ULL)), vl::ceq(i_s_m1, vl::logic<1>(0x0ULL))), vl::ceq(n_supply_ok, vl::logic<1>(0x1ULL))))), vl::bufif1_(i_a0, vl::land(vl::land(vl::ceq(i_s_p0, vl::logic<1>(0x1ULL)), vl::ceq(i_s_m0, vl::logic<1>(0x0ULL))), vl::ceq(n_supply_ok, vl::logic<1>(0x1ULL))))), vl::bufif1_(vl::logic<1>(0x1ULL), vl::land(vl::ceq(n_supply_ok, vl::logic<1>(0x1ULL)), vl::ceq(i_store_on, vl::logic<1>(0x0ULL)))));
    n_z = vl::bufif1_(~n_ax, vl::ceq(n_supply_ok, vl::logic<1>(0x1ULL)));
        vl::wr(z, n_z);
    }
};

class mipi_4_rx_dphy_ns_DPHYLPTX_core_log_inv0 : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYLPTX_core_log_inv0);
    explicit mipi_4_rx_dphy_ns_DPHYLPTX_core_log_inv0(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << gd << a << vph;
    }

private:
    vl::logic<1> n_z;
    void eval() {
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_a = vl::rd(a);
        const vl::logic<1> i_vph = vl::rd(vph);
    n_z = vl::bufif1_(~i_a, vl::land(vl::ceq(i_vph, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))));
        vl::wr(z, n_z);
    }
};

class mipi_4_rx_dphy_ns_DPHYLPTX_core_log_nand2 : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> B{"B"};
    sc_core::sc_out<sc_dt::sc_logic> Y{"Y"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYLPTX_core_log_nand2);
    explicit mipi_4_rx_dphy_ns_DPHYLPTX_core_log_nand2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A << B << gd << vph;
    }

private:
    vl::logic<1> n_Y;
    void eval() {
        const vl::logic<1> i_A = vl::rd(A);
        const vl::logic<1> i_B = vl::rd(B);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vph = vl::rd(vph);
    n_Y = vl::bufif1_(~(i_A & i_B), vl::land(vl::ceq(i_vph, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))));
        vl::wr(Y, n_Y);
    }
};

class mipi_4_rx_dphy_ns_DPHYLPTX_core_log_nor2 : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> B{"B"};
    sc_core::sc_out<sc_dt::sc_logic> Y{"Y"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYLPTX_core_log_nor2);
    explicit mipi_4_rx_dphy_ns_DPHYLPTX_core_log_nor2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A << B << gd << vph;
    }

private:
    vl::logic<1> n_Y;
    void eval() {
        const vl::logic<1> i_A = vl::rd(A);
        const vl::logic<1> i_B = vl::rd(B);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vph = vl::rd(vph);
    n_Y = vl::bufif1_(~(i_A | i_B), vl::land(vl::ceq(i_vph, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))));
        vl::wr(Y, n_Y);
    }
};

class mipi_4_rx_dphy_ns_DPHYLPTX_switch : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_inout<sc_dt::sc_logic> y{"y"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};
    sc_core::sc_in<sc_dt::sc_logic> pon{"pon"};
    sc_core::sc_in<sc_dt::sc_logic> ponz{"ponz"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYLPTX_switch);
    explicit mipi_4_rx_dphy_ns_DPHYLPTX_switch(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << a << y << gd << vph << pon << ponz;
    }

private:
    vl::logic<1> n_y;
    void eval() {
        const vl::logic<1> i_a = vl::rd(a);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vph = vl::rd(vph);
        const vl::logic<1> i_pon = vl::rd(pon);
        const vl::logic<1> i_ponz = vl::rd(ponz);
    n_y = vl::bufif1_(i_a, vl::land(vl::land(vl::land(vl::ceq(i_vph, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))), vl::ceq(i_pon, vl::logic<1>(0x1ULL))), vl::ceq(i_ponz, vl::logic<1>(0x0ULL))));
        vl::wr(y, n_y);
    }
};

class mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_demux_buf_long : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> on{"on"};
    sc_core::sc_in<sc_dt::sc_logic> onz{"onz"};
    sc_core::sc_in<sc_dt::sc_logic> vin{"vin"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    sc_core::sc_inout<sc_dt::sc_logic> vout{"vout"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_demux_buf_long);
    explicit mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_demux_buf_long(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << on << onz << vin << gd << vp << vout;
    }

private:
    vl::logic<1> n_vout;
    void eval() {
        const vl::logic<1> i_on = vl::rd(on);
        const vl::logic<1> i_onz = vl::rd(onz);
        const vl::logic<1> i_vin = vl::rd(vin);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vp = vl::rd(vp);
    n_vout = vl::bufif1_(~i_vin, vl::land(vl::land(vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_on, vl::logic<1>(0x1ULL))), vl::ceq(i_onz, vl::logic<1>(0x0ULL))), vl::ceq(i_gd, vl::logic<1>(0x0ULL))));
        vl::wr(vout, n_vout);
    }
};

class mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_demux_buf_short : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> i{"i"};
    sc_core::sc_in<sc_dt::sc_logic> on{"on"};
    sc_core::sc_in<sc_dt::sc_logic> onz{"onz"};
    sc_core::sc_out<sc_dt::sc_logic> o{"o"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_demux_buf_short);
    explicit mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_demux_buf_short(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << i << on << onz << gd << vp;
    }

private:
    vl::logic<1> n_o;
    void eval() {
        const vl::logic<1> i_i = vl::rd(i);
        const vl::logic<1> i_on = vl::rd(on);
        const vl::logic<1> i_onz = vl::rd(onz);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vp = vl::rd(vp);
    n_o = vl::bufif1_(~i_i, vl::land(vl::land(vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_on, vl::logic<1>(0x1ULL))), vl::ceq(i_onz, vl::logic<1>(0x0ULL))), vl::ceq(i_gd, vl::logic<1>(0x0ULL))));
        vl::wr(o, n_o);
    }
};

class mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_inv : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> i{"i"};
    sc_core::sc_out<sc_dt::sc_logic> o{"o"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_inv);
    explicit mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_inv(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << i << gd << vp;
    }

private:
    vl::logic<1> n_o;
    void eval() {
        const vl::logic<1> i_i = vl::rd(i);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vp = vl::rd(vp);
    n_o = vl::bufif1_(~i_i, vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))));
        vl::wr(o, n_o);
    }
};

class mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_mux_buf : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> i{"i"};
    sc_core::sc_in<sc_dt::sc_logic> on{"on"};
    sc_core::sc_in<sc_dt::sc_logic> onz{"onz"};
    sc_core::sc_out<sc_dt::sc_logic> o{"o"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_mux_buf);
    explicit mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_mux_buf(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << i << on << onz << gd << vp;
    }

private:
    vl::logic<1> n_o;
    void eval() {
        const vl::logic<1> i_i = vl::rd(i);
        const vl::logic<1> i_on = vl::rd(on);
        const vl::logic<1> i_onz = vl::rd(onz);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vp = vl::rd(vp);
    n_o = vl::bufif1_(~i_i, vl::land(vl::land(vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))), vl::ceq(i_on, vl::logic<1>(0x1ULL))), vl::ceq(i_onz, vl::logic<1>(0x0ULL))));
        vl::wr(o, n_o);
    }
};

class mipi_4_rx_dphy_ns_LEVELCONV_bias : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> onz_hv{"onz_hv"};
    sc_core::sc_in<sc_dt::sc_logic> tie_low_hv{"tie_low_hv"};
    sc_core::sc_out<sc_dt::sc_logic> vblvhv{"vblvhv"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_LEVELCONV_bias);
    explicit mipi_4_rx_dphy_ns_LEVELCONV_bias(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_in<sc_dt::sc_logic> tie_high_cdm{"tie_high_cdm"};
    sc_core::sc_in<sc_dt::sc_logic> tie_low_cdm{"tie_low_cdm"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt);
    explicit mipi_4_rx_dphy_ns_LEVELCONV_cdm_na3x6_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_LEVELCONV_cdm_nand_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_in<sc_dt::sc_logic> tie_high_cdm{"tie_high_cdm"};
    sc_core::sc_in<sc_dt::sc_logic> tie_low_cdm{"tie_low_cdm"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_LEVELCONV_cdm_nand_lvt);
    explicit mipi_4_rx_dphy_ns_LEVELCONV_cdm_nand_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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
    n_z = vl::bufif1_(~i_a, vl::land(vl::land(vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))), vl::ceq(i_tie_high_cdm, vl::logic<1>(0x1ULL))), vl::ceq(i_tie_low_cdm, vl::logic<1>(0x0ULL))));
        vl::wr(z, n_z);
    }
};

class mipi_4_rx_dphy_ns_LEVELCONV_l2l_cdm_tie_LowHigh : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    sc_core::sc_out<sc_dt::sc_logic> tie_high{"tie_high"};
    sc_core::sc_out<sc_dt::sc_logic> tie_low{"tie_low"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_LEVELCONV_l2l_cdm_tie_LowHigh);
    explicit mipi_4_rx_dphy_ns_LEVELCONV_l2l_cdm_tie_LowHigh(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << gd << vp;
    }

private:
    vl::logic<1> n_tie_high, n_tie_low;
    void eval() {
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vp = vl::rd(vp);
    n_tie_low = vl::bufif1_(vl::logic<1>(0x0ULL), vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))));
    n_tie_high = vl::bufif1_(vl::logic<1>(0x1ULL), vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))));
        vl::wr(tie_high, n_tie_high);
        vl::wr(tie_low, n_tie_low);
    }
};

class mipi_4_rx_dphy_ns_LEVELCONV_vp_det_soc : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> A_in{"A_in"};
    sc_core::sc_out<sc_dt::sc_logic> Z{"Z"};
    sc_core::sc_out<sc_dt::sc_logic> Z_neg{"Z_neg"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};
    sc_core::sc_in<sc_dt::sc_logic> vss{"vss"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_LEVELCONV_vp_det_soc);
    explicit mipi_4_rx_dphy_ns_LEVELCONV_vp_det_soc(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_LEVELCONV_vph_det_soc : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> A_in{"A_in"};
    sc_core::sc_out<sc_dt::sc_logic> Z{"Z"};
    sc_core::sc_out<sc_dt::sc_logic> Z_neg{"Z_neg"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vdd{"vdd"};
    sc_core::sc_in<sc_dt::sc_logic> vss{"vss"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_LEVELCONV_vph_det_soc);
    explicit mipi_4_rx_dphy_ns_LEVELCONV_vph_det_soc(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A_in << gd << vdd << vss;
    }

private:
    vl::logic<1> n_Z, n_Z_neg;
    void eval() {
        const vl::logic<1> i_A_in = vl::rd(A_in);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vdd = vl::rd(vdd);
        const vl::logic<1> i_vss = vl::rd(vss);
    n_Z_neg = vl::bufif1_(~i_A_in, vl::land(vl::land(vl::ceq(i_vdd, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))), vl::ceq(i_vss, vl::logic<1>(0x0ULL))));
    n_Z = vl::bufif1_(i_A_in, vl::land(vl::land(vl::ceq(i_vdd, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))), vl::ceq(i_vss, vl::logic<1>(0x0ULL))));
        vl::wr(Z, n_Z);
        vl::wr(Z_neg, n_Z_neg);
    }
};

class mipi_4_rx_dphy_ns_SEM_ADDF_V1_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> S{"S"};
    sc_core::sc_out<sc_dt::sc_logic> CO{"CO"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> B{"B"};
    sc_core::sc_in<sc_dt::sc_logic> CI{"CI"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_ADDF_V1_1);
    explicit mipi_4_rx_dphy_ns_SEM_ADDF_V1_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A << B << CI << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_S, n_CO, n_int_res_0, n_int_res_1, n_int_res_2, n_A_org, n_B_org, n_CI_org;
    void eval() {
        const vl::logic<1> i_A = vl::rd(A);
        const vl::logic<1> i_B = vl::rd(B);
        const vl::logic<1> i_CI = vl::rd(CI);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_CI_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_CI, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_B_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_2 = vl::and_(n_B_org, n_CI_org);
    n_A_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_S = vl::xor_(n_A_org, n_B_org, n_CI_org);
    n_int_res_1 = vl::and_(n_A_org, n_CI_org);
    n_int_res_0 = vl::and_(n_A_org, n_B_org);
    n_CO = vl::or_(n_int_res_0, n_int_res_1, n_int_res_2);
        vl::wr(S, n_S);
        vl::wr(CO, n_CO);
    }
};

class mipi_4_rx_dphy_ns_SEM_AN2_S_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_AN2_S_1);
    explicit mipi_4_rx_dphy_ns_SEM_AN2_S_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_AN2_S_2 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_AN2_S_2);
    explicit mipi_4_rx_dphy_ns_SEM_AN2_S_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_AN2_S_4 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_AN2_S_4);
    explicit mipi_4_rx_dphy_ns_SEM_AN2_S_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_AN2_S_8 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_AN2_S_8);
    explicit mipi_4_rx_dphy_ns_SEM_AN2_S_8(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_AN3_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> A3{"A3"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_AN3_1);
    explicit mipi_4_rx_dphy_ns_SEM_AN3_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_AN3_2 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> A3{"A3"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_AN3_2);
    explicit mipi_4_rx_dphy_ns_SEM_AN3_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_AN3_4 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> A3{"A3"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_AN3_4);
    explicit mipi_4_rx_dphy_ns_SEM_AN3_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_AN3_8 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> A3{"A3"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_AN3_8);
    explicit mipi_4_rx_dphy_ns_SEM_AN3_8(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_AO21B_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> B{"B"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_AO21B_1);
    explicit mipi_4_rx_dphy_ns_SEM_AO21B_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_AO21B_2 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> B{"B"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_AO21B_2);
    explicit mipi_4_rx_dphy_ns_SEM_AO21B_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_AO21B_4 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> B{"B"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_AO21B_4);
    explicit mipi_4_rx_dphy_ns_SEM_AO21B_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_AO22_1 : public sc_core::sc_module {
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
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_AO22_1);
    explicit mipi_4_rx_dphy_ns_SEM_AO22_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_AO22_2 : public sc_core::sc_module {
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
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_AO22_2);
    explicit mipi_4_rx_dphy_ns_SEM_AO22_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_AO22_4 : public sc_core::sc_module {
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
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_AO22_4);
    explicit mipi_4_rx_dphy_ns_SEM_AO22_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_AO2BB2_1 : public sc_core::sc_module {
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
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_AO2BB2_1);
    explicit mipi_4_rx_dphy_ns_SEM_AO2BB2_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_AO2BB2_2 : public sc_core::sc_module {
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
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_AO2BB2_2);
    explicit mipi_4_rx_dphy_ns_SEM_AO2BB2_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_AO2BB2_4 : public sc_core::sc_module {
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
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_AO2BB2_4);
    explicit mipi_4_rx_dphy_ns_SEM_AO2BB2_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_AO2BB2_8 : public sc_core::sc_module {
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
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_AO2BB2_8);
    explicit mipi_4_rx_dphy_ns_SEM_AO2BB2_8(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_AOAI211_1 : public sc_core::sc_module {
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
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_AOAI211_1);
    explicit mipi_4_rx_dphy_ns_SEM_AOAI211_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_AOAI211_2 : public sc_core::sc_module {
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
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_AOAI211_2);
    explicit mipi_4_rx_dphy_ns_SEM_AOAI211_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_AOAI211_4 : public sc_core::sc_module {
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
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_AOAI211_4);
    explicit mipi_4_rx_dphy_ns_SEM_AOAI211_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_AOI211_1 : public sc_core::sc_module {
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
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_AOI211_1);
    explicit mipi_4_rx_dphy_ns_SEM_AOI211_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_AOI211_2 : public sc_core::sc_module {
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
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_AOI211_2);
    explicit mipi_4_rx_dphy_ns_SEM_AOI211_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_AOI211_4 : public sc_core::sc_module {
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
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_AOI211_4);
    explicit mipi_4_rx_dphy_ns_SEM_AOI211_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_AOI21_S_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> B{"B"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_AOI21_S_1);
    explicit mipi_4_rx_dphy_ns_SEM_AOI21_S_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_AOI21_S_2 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> B{"B"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_AOI21_S_2);
    explicit mipi_4_rx_dphy_ns_SEM_AOI21_S_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_AOI21_S_4 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> B{"B"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_AOI21_S_4);
    explicit mipi_4_rx_dphy_ns_SEM_AOI21_S_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_AOI221_1 : public sc_core::sc_module {
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
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_AOI221_1);
    explicit mipi_4_rx_dphy_ns_SEM_AOI221_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_AOI221_2 : public sc_core::sc_module {
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
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_AOI221_2);
    explicit mipi_4_rx_dphy_ns_SEM_AOI221_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_AOI221_4 : public sc_core::sc_module {
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
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_AOI221_4);
    explicit mipi_4_rx_dphy_ns_SEM_AOI221_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_AOI222_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> B1{"B1"};
    sc_core::sc_in<sc_dt::sc_logic> B2{"B2"};
    sc_core::sc_in<sc_dt::sc_logic> C1{"C1"};
    sc_core::sc_in<sc_dt::sc_logic> C2{"C2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_AOI222_1);
    explicit mipi_4_rx_dphy_ns_SEM_AOI222_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << B1 << B2 << C1 << C2 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_int_res_0, n_int_res_1, n_int_res_2, n_A1_org, n_A2_org, n_B1_org, n_B2_org, n_C1_org, n_C2_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_B1 = vl::rd(B1);
        const vl::logic<1> i_B2 = vl::rd(B2);
        const vl::logic<1> i_C1 = vl::rd(C1);
        const vl::logic<1> i_C2 = vl::rd(C2);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_C2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_C2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_C1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_C1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_2 = vl::and_(n_C1_org, n_C2_org);
    n_B2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_B1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_1 = vl::and_(n_B1_org, n_B2_org);
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_0 = vl::and_(n_A1_org, n_A2_org);
    n_X = vl::nor_(n_int_res_0, n_int_res_1, n_int_res_2);
        vl::wr(X, n_X);
    }
};

class mipi_4_rx_dphy_ns_SEM_AOI222_2 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> B1{"B1"};
    sc_core::sc_in<sc_dt::sc_logic> B2{"B2"};
    sc_core::sc_in<sc_dt::sc_logic> C1{"C1"};
    sc_core::sc_in<sc_dt::sc_logic> C2{"C2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_AOI222_2);
    explicit mipi_4_rx_dphy_ns_SEM_AOI222_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << B1 << B2 << C1 << C2 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_int_res_0, n_int_res_1, n_int_res_2, n_A1_org, n_A2_org, n_B1_org, n_B2_org, n_C1_org, n_C2_org;
    void eval() {
        const vl::logic<1> i_A1 = vl::rd(A1);
        const vl::logic<1> i_A2 = vl::rd(A2);
        const vl::logic<1> i_B1 = vl::rd(B1);
        const vl::logic<1> i_B2 = vl::rd(B2);
        const vl::logic<1> i_C1 = vl::rd(C1);
        const vl::logic<1> i_C2 = vl::rd(C2);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
    n_C2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_C2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_C1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_C1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_2 = vl::and_(n_C1_org, n_C2_org);
    n_B2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_B1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_B1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_1 = vl::and_(n_B1_org, n_B2_org);
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_0 = vl::and_(n_A1_org, n_A2_org);
    n_X = vl::nor_(n_int_res_0, n_int_res_1, n_int_res_2);
        vl::wr(X, n_X);
    }
};

class mipi_4_rx_dphy_ns_SEM_AOI22_1 : public sc_core::sc_module {
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
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_AOI22_1);
    explicit mipi_4_rx_dphy_ns_SEM_AOI22_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_AOI22_2 : public sc_core::sc_module {
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
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_AOI22_2);
    explicit mipi_4_rx_dphy_ns_SEM_AOI22_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_AOI22_4 : public sc_core::sc_module {
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
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_AOI22_4);
    explicit mipi_4_rx_dphy_ns_SEM_AOI22_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_AOI31_1 : public sc_core::sc_module {
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
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_AOI31_1);
    explicit mipi_4_rx_dphy_ns_SEM_AOI31_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_AOI31_2 : public sc_core::sc_module {
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
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_AOI31_2);
    explicit mipi_4_rx_dphy_ns_SEM_AOI31_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_AOI31_4 : public sc_core::sc_module {
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
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_AOI31_4);
    explicit mipi_4_rx_dphy_ns_SEM_AOI31_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_AOI32_1 : public sc_core::sc_module {
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
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_AOI32_1);
    explicit mipi_4_rx_dphy_ns_SEM_AOI32_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_AOI32_2 : public sc_core::sc_module {
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
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_AOI32_2);
    explicit mipi_4_rx_dphy_ns_SEM_AOI32_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_AOI32_4 : public sc_core::sc_module {
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
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_AOI32_4);
    explicit mipi_4_rx_dphy_ns_SEM_AOI32_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_BUF_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_BUF_1);
    explicit mipi_4_rx_dphy_ns_SEM_BUF_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_BUF_12 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_BUF_12);
    explicit mipi_4_rx_dphy_ns_SEM_BUF_12(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_BUF_16 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_BUF_16);
    explicit mipi_4_rx_dphy_ns_SEM_BUF_16(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_BUF_2 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_BUF_2);
    explicit mipi_4_rx_dphy_ns_SEM_BUF_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_BUF_4 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_BUF_4);
    explicit mipi_4_rx_dphy_ns_SEM_BUF_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_BUF_8 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_BUF_8);
    explicit mipi_4_rx_dphy_ns_SEM_BUF_8(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_BUF_S_12 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_BUF_S_12);
    explicit mipi_4_rx_dphy_ns_SEM_BUF_S_12(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_BUF_S_16 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_BUF_S_16);
    explicit mipi_4_rx_dphy_ns_SEM_BUF_S_16(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_BUF_S_4 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_BUF_S_4);
    explicit mipi_4_rx_dphy_ns_SEM_BUF_S_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_BUF_S_8 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_BUF_S_8);
    explicit mipi_4_rx_dphy_ns_SEM_BUF_S_8(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_CKGTPLT_V7_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> Q{"Q"};
    sc_core::sc_in<sc_dt::sc_logic> CK{"CK"};
    sc_core::sc_in<sc_dt::sc_logic> EN{"EN"};
    sc_core::sc_in<sc_dt::sc_logic> SE{"SE"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_CKGTPLT_V7_1);
    explicit mipi_4_rx_dphy_ns_SEM_CKGTPLT_V7_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << CK << EN << SE << VDD << VBP << VBN << VSS;
    }

private:
    vl::logic<1> n_Q, n_delayed_CK, n_delayed_EN, n_delayed_SE, n_int_res__CK, n_int_res__prectrl, n_int_res_iq, n_CK_org, n_EN_org, n_SE_org;
    vl::udp_state<3> s0_{3};
    void eval() {
        const vl::logic<1> i_CK = vl::rd(CK);
        const vl::logic<1> i_EN = vl::rd(EN);
        const vl::logic<1> i_SE = vl::rd(SE);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        bool chg = true;
        for (int it = 0; chg && it < 16; ++it) {
            chg = false;
        n_delayed_SE = i_SE;
        n_SE_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL))), vl::cne(i_VSS, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_SE, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL))), vl::cne(i_VSS, vl::logic<1>(0x0ULL)))));
        n_delayed_EN = i_EN;
        n_EN_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL))), vl::cne(i_VSS, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_EN, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL))), vl::cne(i_VSS, vl::logic<1>(0x0ULL)))));
        n_int_res__prectrl = vl::or_(n_EN_org, n_SE_org);
        n_delayed_CK = i_CK;
        n_CK_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL))), vl::cne(i_VSS, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_CK, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL))), vl::cne(i_VSS, vl::logic<1>(0x0ULL)))));
        n_Q = vl::and_(n_int_res_iq, n_CK_org);
        n_int_res__CK = vl::not_(n_CK_org);
        { const int in[3] = {vl::v3(vl::LX()), vl::v3(n_int_res__CK), vl::v3(n_int_res__prectrl)}; if (s0_.update(udp::mipi_4_rx_dphy_ns_latch, in)) chg = true; }
        n_int_res_iq = vl::from3(s0_.st);
        }
        vl::wr(Q, n_Q);
    }
};

class mipi_4_rx_dphy_ns_SEM_CKGTPLT_V7_4 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> Q{"Q"};
    sc_core::sc_in<sc_dt::sc_logic> CK{"CK"};
    sc_core::sc_in<sc_dt::sc_logic> EN{"EN"};
    sc_core::sc_in<sc_dt::sc_logic> SE{"SE"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_CKGTPLT_V7_4);
    explicit mipi_4_rx_dphy_ns_SEM_CKGTPLT_V7_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << CK << EN << SE << VDD << VBP << VBN << VSS;
    }

private:
    vl::logic<1> n_Q, n_delayed_CK, n_delayed_EN, n_delayed_SE, n_int_res__CK, n_int_res__prectrl, n_int_res_iq, n_CK_org, n_EN_org, n_SE_org;
    vl::udp_state<3> s0_{3};
    void eval() {
        const vl::logic<1> i_CK = vl::rd(CK);
        const vl::logic<1> i_EN = vl::rd(EN);
        const vl::logic<1> i_SE = vl::rd(SE);
        const vl::logic<1> i_VDD = vl::rd(VDD);
        const vl::logic<1> i_VBP = vl::rd(VBP);
        const vl::logic<1> i_VBN = vl::rd(VBN);
        const vl::logic<1> i_VSS = vl::rd(VSS);
        bool chg = true;
        for (int it = 0; chg && it < 16; ++it) {
            chg = false;
        n_delayed_SE = i_SE;
        n_SE_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL))), vl::cne(i_VSS, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_SE, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL))), vl::cne(i_VSS, vl::logic<1>(0x0ULL)))));
        n_delayed_EN = i_EN;
        n_EN_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL))), vl::cne(i_VSS, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_EN, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL))), vl::cne(i_VSS, vl::logic<1>(0x0ULL)))));
        n_int_res__prectrl = vl::or_(n_EN_org, n_SE_org);
        n_delayed_CK = i_CK;
        n_CK_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL))), vl::cne(i_VSS, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_CK, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL))), vl::cne(i_VSS, vl::logic<1>(0x0ULL)))));
        n_Q = vl::and_(n_int_res_iq, n_CK_org);
        n_int_res__CK = vl::not_(n_CK_org);
        { const int in[3] = {vl::v3(vl::LX()), vl::v3(n_int_res__CK), vl::v3(n_int_res__prectrl)}; if (s0_.update(udp::mipi_4_rx_dphy_ns_latch, in)) chg = true; }
        n_int_res_iq = vl::from3(s0_.st);
        }
        vl::wr(Q, n_Q);
    }
};

class mipi_4_rx_dphy_ns_SEM_DCAP_12 : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_DCAP_12);
    explicit mipi_4_rx_dphy_ns_SEM_DCAP_12(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_DCAP_16 : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_DCAP_16);
    explicit mipi_4_rx_dphy_ns_SEM_DCAP_16(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_DCAP_4 : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_DCAP_4);
    explicit mipi_4_rx_dphy_ns_SEM_DCAP_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_DCAP_6 : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_DCAP_6);
    explicit mipi_4_rx_dphy_ns_SEM_DCAP_6(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_DCAP_8 : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_DCAP_8);
    explicit mipi_4_rx_dphy_ns_SEM_DCAP_8(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_DEL_L4_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_DEL_L4_1);
    explicit mipi_4_rx_dphy_ns_SEM_DEL_L4_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_DEL_L4_2 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_DEL_L4_2);
    explicit mipi_4_rx_dphy_ns_SEM_DEL_L4_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_DEL_L4_4 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_DEL_L4_4);
    explicit mipi_4_rx_dphy_ns_SEM_DEL_L4_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_DEL_L6_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_DEL_L6_1);
    explicit mipi_4_rx_dphy_ns_SEM_DEL_L6_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_DEL_L6_2 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_DEL_L6_2);
    explicit mipi_4_rx_dphy_ns_SEM_DEL_L6_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_DEL_L6_4 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_DEL_L6_4);
    explicit mipi_4_rx_dphy_ns_SEM_DEL_L6_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_DEL_L6_8 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_DEL_L6_8);
    explicit mipi_4_rx_dphy_ns_SEM_DEL_L6_8(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_EN2_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_EN2_1);
    explicit mipi_4_rx_dphy_ns_SEM_EN2_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_EO2_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_EO2_1);
    explicit mipi_4_rx_dphy_ns_SEM_EO2_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_EO2_2 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_EO2_2);
    explicit mipi_4_rx_dphy_ns_SEM_EO2_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_FSDPRBQO_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> Q{"Q"};
    sc_core::sc_out<sc_dt::sc_logic> SO{"SO"};
    sc_core::sc_in<sc_dt::sc_logic> CK{"CK"};
    sc_core::sc_in<sc_dt::sc_logic> D{"D"};
    sc_core::sc_in<sc_dt::sc_logic> SI{"SI"};
    sc_core::sc_in<sc_dt::sc_logic> SE{"SE"};
    sc_core::sc_in<sc_dt::sc_logic> RD{"RD"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_FSDPRBQO_1);
    explicit mipi_4_rx_dphy_ns_SEM_FSDPRBQO_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << CK << D << SI << SE << RD << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_Q, n_SO, n_SD, n_delayed_CK, n_delayed_D, n_delayed_SI, n_delayed_SE, n_delayed_RD, n_int_res__D, n_int_res__RD, n_int_res_iq, n_xcr_0, n_CK_org, n_D_org, n_RD_org, n_SE_org, n_SI_org;
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
        n_int_res__D = vl::ucomb(udp::mipi_4_rx_dphy_ns_mux2, vl::v3(n_SI_org), vl::v3(n_D_org), vl::v3(n_SE_org));
        n_delayed_CK = i_CK;
        n_CK_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_CK, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_SO = vl::buf_(n_int_res_iq);
        n_Q = vl::buf_(n_int_res_iq);
        n_SD = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(vl::logic<1>(0x0ULL), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        { const int in[3] = {vl::v3(n_CK_org), vl::v3(n_int_res__D), vl::v3(n_int_res__RD)}; if (s0_.update(udp::mipi_4_rx_dphy_ns_dff_r_err, in)) chg = true; }
        n_xcr_0 = vl::from3(s0_.st);
        { const int in[6] = {vl::v3(vl::LX()), vl::v3(n_CK_org), vl::v3(n_int_res__D), vl::v3(n_SD), vl::v3(n_int_res__RD), vl::v3(n_xcr_0)}; if (s1_.update(udp::mipi_4_rx_dphy_ns_dff_sr_0, in)) chg = true; }
        n_int_res_iq = vl::from3(s1_.st);
        }
        vl::wr(Q, n_Q);
        vl::wr(SO, n_SO);
    }
};

class mipi_4_rx_dphy_ns_SEM_FSDPRBQ_D_1 : public sc_core::sc_module {
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
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_FSDPRBQ_D_1);
    explicit mipi_4_rx_dphy_ns_SEM_FSDPRBQ_D_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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
        n_int_res__D = vl::ucomb(udp::mipi_4_rx_dphy_ns_mux2, vl::v3(n_SI_org), vl::v3(n_D_org), vl::v3(n_SE_org));
        n_delayed_CK = i_CK;
        n_CK_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_CK, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_SD = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(vl::logic<1>(0x0ULL), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        { const int in[3] = {vl::v3(n_CK_org), vl::v3(n_int_res__D), vl::v3(n_int_res__RD)}; if (s0_.update(udp::mipi_4_rx_dphy_ns_dff_r_err, in)) chg = true; }
        n_xcr_0 = vl::from3(s0_.st);
        { const int in[6] = {vl::v3(vl::LX()), vl::v3(n_CK_org), vl::v3(n_int_res__D), vl::v3(n_SD), vl::v3(n_int_res__RD), vl::v3(n_xcr_0)}; if (s1_.update(udp::mipi_4_rx_dphy_ns_dff_sr_0, in)) chg = true; }
        n_Q = vl::from3(s1_.st);
        }
        vl::wr(Q, n_Q);
    }
};

class mipi_4_rx_dphy_ns_SEM_FSDPRBQ_D_2 : public sc_core::sc_module {
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
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_FSDPRBQ_D_2);
    explicit mipi_4_rx_dphy_ns_SEM_FSDPRBQ_D_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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
        n_int_res__D = vl::ucomb(udp::mipi_4_rx_dphy_ns_mux2, vl::v3(n_SI_org), vl::v3(n_D_org), vl::v3(n_SE_org));
        n_delayed_CK = i_CK;
        n_CK_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_CK, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_SD = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(vl::logic<1>(0x0ULL), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        { const int in[3] = {vl::v3(n_CK_org), vl::v3(n_int_res__D), vl::v3(n_int_res__RD)}; if (s0_.update(udp::mipi_4_rx_dphy_ns_dff_r_err, in)) chg = true; }
        n_xcr_0 = vl::from3(s0_.st);
        { const int in[6] = {vl::v3(vl::LX()), vl::v3(n_CK_org), vl::v3(n_int_res__D), vl::v3(n_SD), vl::v3(n_int_res__RD), vl::v3(n_xcr_0)}; if (s1_.update(udp::mipi_4_rx_dphy_ns_dff_sr_0, in)) chg = true; }
        n_Q = vl::from3(s1_.st);
        }
        vl::wr(Q, n_Q);
    }
};

class mipi_4_rx_dphy_ns_SEM_FSDPRBQ_D_4 : public sc_core::sc_module {
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
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_FSDPRBQ_D_4);
    explicit mipi_4_rx_dphy_ns_SEM_FSDPRBQ_D_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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
        n_int_res__D = vl::ucomb(udp::mipi_4_rx_dphy_ns_mux2, vl::v3(n_SI_org), vl::v3(n_D_org), vl::v3(n_SE_org));
        n_delayed_CK = i_CK;
        n_CK_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_CK, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_SD = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(vl::logic<1>(0x0ULL), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        { const int in[3] = {vl::v3(n_CK_org), vl::v3(n_int_res__D), vl::v3(n_int_res__RD)}; if (s0_.update(udp::mipi_4_rx_dphy_ns_dff_r_err, in)) chg = true; }
        n_xcr_0 = vl::from3(s0_.st);
        { const int in[6] = {vl::v3(vl::LX()), vl::v3(n_CK_org), vl::v3(n_int_res__D), vl::v3(n_SD), vl::v3(n_int_res__RD), vl::v3(n_xcr_0)}; if (s1_.update(udp::mipi_4_rx_dphy_ns_dff_sr_0, in)) chg = true; }
        n_Q = vl::from3(s1_.st);
        }
        vl::wr(Q, n_Q);
    }
};

class mipi_4_rx_dphy_ns_SEM_FSDPRBQ_V2_1 : public sc_core::sc_module {
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
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_FSDPRBQ_V2_1);
    explicit mipi_4_rx_dphy_ns_SEM_FSDPRBQ_V2_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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
        n_int_res__D = vl::ucomb(udp::mipi_4_rx_dphy_ns_mux2, vl::v3(n_SI_org), vl::v3(n_D_org), vl::v3(n_SE_org));
        n_delayed_CK = i_CK;
        n_CK_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_CK, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_SD = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(vl::logic<1>(0x0ULL), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        { const int in[3] = {vl::v3(n_CK_org), vl::v3(n_int_res__D), vl::v3(n_int_res__RD)}; if (s0_.update(udp::mipi_4_rx_dphy_ns_dff_r_err, in)) chg = true; }
        n_xcr_0 = vl::from3(s0_.st);
        { const int in[6] = {vl::v3(vl::LX()), vl::v3(n_CK_org), vl::v3(n_int_res__D), vl::v3(n_SD), vl::v3(n_int_res__RD), vl::v3(n_xcr_0)}; if (s1_.update(udp::mipi_4_rx_dphy_ns_dff_sr_0, in)) chg = true; }
        n_Q = vl::from3(s1_.st);
        }
        vl::wr(Q, n_Q);
    }
};

class mipi_4_rx_dphy_ns_SEM_FSDPRBQ_V2_2 : public sc_core::sc_module {
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
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_FSDPRBQ_V2_2);
    explicit mipi_4_rx_dphy_ns_SEM_FSDPRBQ_V2_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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
        n_int_res__D = vl::ucomb(udp::mipi_4_rx_dphy_ns_mux2, vl::v3(n_SI_org), vl::v3(n_D_org), vl::v3(n_SE_org));
        n_delayed_CK = i_CK;
        n_CK_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_CK, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_SD = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(vl::logic<1>(0x0ULL), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        { const int in[3] = {vl::v3(n_CK_org), vl::v3(n_int_res__D), vl::v3(n_int_res__RD)}; if (s0_.update(udp::mipi_4_rx_dphy_ns_dff_r_err, in)) chg = true; }
        n_xcr_0 = vl::from3(s0_.st);
        { const int in[6] = {vl::v3(vl::LX()), vl::v3(n_CK_org), vl::v3(n_int_res__D), vl::v3(n_SD), vl::v3(n_int_res__RD), vl::v3(n_xcr_0)}; if (s1_.update(udp::mipi_4_rx_dphy_ns_dff_sr_0, in)) chg = true; }
        n_Q = vl::from3(s1_.st);
        }
        vl::wr(Q, n_Q);
    }
};

class mipi_4_rx_dphy_ns_SEM_FSDPRBQ_V2_4 : public sc_core::sc_module {
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
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_FSDPRBQ_V2_4);
    explicit mipi_4_rx_dphy_ns_SEM_FSDPRBQ_V2_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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
        n_int_res__D = vl::ucomb(udp::mipi_4_rx_dphy_ns_mux2, vl::v3(n_SI_org), vl::v3(n_D_org), vl::v3(n_SE_org));
        n_delayed_CK = i_CK;
        n_CK_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_CK, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_SD = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(vl::logic<1>(0x0ULL), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        { const int in[3] = {vl::v3(n_CK_org), vl::v3(n_int_res__D), vl::v3(n_int_res__RD)}; if (s0_.update(udp::mipi_4_rx_dphy_ns_dff_r_err, in)) chg = true; }
        n_xcr_0 = vl::from3(s0_.st);
        { const int in[6] = {vl::v3(vl::LX()), vl::v3(n_CK_org), vl::v3(n_int_res__D), vl::v3(n_SD), vl::v3(n_int_res__RD), vl::v3(n_xcr_0)}; if (s1_.update(udp::mipi_4_rx_dphy_ns_dff_sr_0, in)) chg = true; }
        n_Q = vl::from3(s1_.st);
        }
        vl::wr(Q, n_Q);
    }
};

class mipi_4_rx_dphy_ns_SEM_FSDPRBQ_V2_8 : public sc_core::sc_module {
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
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_FSDPRBQ_V2_8);
    explicit mipi_4_rx_dphy_ns_SEM_FSDPRBQ_V2_8(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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
        n_int_res__D = vl::ucomb(udp::mipi_4_rx_dphy_ns_mux2, vl::v3(n_SI_org), vl::v3(n_D_org), vl::v3(n_SE_org));
        n_delayed_CK = i_CK;
        n_CK_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_CK, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_SD = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(vl::logic<1>(0x0ULL), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        { const int in[3] = {vl::v3(n_CK_org), vl::v3(n_int_res__D), vl::v3(n_int_res__RD)}; if (s0_.update(udp::mipi_4_rx_dphy_ns_dff_r_err, in)) chg = true; }
        n_xcr_0 = vl::from3(s0_.st);
        { const int in[6] = {vl::v3(vl::LX()), vl::v3(n_CK_org), vl::v3(n_int_res__D), vl::v3(n_SD), vl::v3(n_int_res__RD), vl::v3(n_xcr_0)}; if (s1_.update(udp::mipi_4_rx_dphy_ns_dff_sr_0, in)) chg = true; }
        n_Q = vl::from3(s1_.st);
        }
        vl::wr(Q, n_Q);
    }
};

class mipi_4_rx_dphy_ns_SEM_FSDPSBQ_V2_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> Q{"Q"};
    sc_core::sc_in<sc_dt::sc_logic> CK{"CK"};
    sc_core::sc_in<sc_dt::sc_logic> D{"D"};
    sc_core::sc_in<sc_dt::sc_logic> SI{"SI"};
    sc_core::sc_in<sc_dt::sc_logic> SE{"SE"};
    sc_core::sc_in<sc_dt::sc_logic> SD{"SD"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_FSDPSBQ_V2_1);
    explicit mipi_4_rx_dphy_ns_SEM_FSDPSBQ_V2_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << CK << D << SI << SE << SD << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_Q, n_RD, n_delayed_CK, n_delayed_D, n_delayed_SI, n_delayed_SE, n_delayed_SD, n_int_res__D, n_int_res__SD, n_xcr_0, n_CK_org, n_D_org, n_SD_org, n_SE_org, n_SI_org;
    vl::udp_state<3> s0_{3};
    vl::udp_state<6> s1_{3};
    void eval() {
        const vl::logic<1> i_CK = vl::rd(CK);
        const vl::logic<1> i_D = vl::rd(D);
        const vl::logic<1> i_SI = vl::rd(SI);
        const vl::logic<1> i_SE = vl::rd(SE);
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
        n_delayed_SD = i_SD;
        n_SD_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_SD, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_int_res__SD = vl::not_(n_SD_org);
        n_delayed_D = i_D;
        n_D_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_D, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_int_res__D = vl::ucomb(udp::mipi_4_rx_dphy_ns_mux2, vl::v3(n_SI_org), vl::v3(n_D_org), vl::v3(n_SE_org));
        n_delayed_CK = i_CK;
        n_CK_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_CK, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_RD = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(vl::logic<1>(0x0ULL), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        { const int in[3] = {vl::v3(n_CK_org), vl::v3(n_int_res__D), vl::v3(n_int_res__SD)}; if (s0_.update(udp::mipi_4_rx_dphy_ns_dff_s_err, in)) chg = true; }
        n_xcr_0 = vl::from3(s0_.st);
        { const int in[6] = {vl::v3(vl::LX()), vl::v3(n_CK_org), vl::v3(n_int_res__D), vl::v3(n_int_res__SD), vl::v3(n_RD), vl::v3(n_xcr_0)}; if (s1_.update(udp::mipi_4_rx_dphy_ns_dff_sr_0, in)) chg = true; }
        n_Q = vl::from3(s1_.st);
        }
        vl::wr(Q, n_Q);
    }
};

class mipi_4_rx_dphy_ns_SEM_FSDPSBQ_V2_2 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> Q{"Q"};
    sc_core::sc_in<sc_dt::sc_logic> CK{"CK"};
    sc_core::sc_in<sc_dt::sc_logic> D{"D"};
    sc_core::sc_in<sc_dt::sc_logic> SI{"SI"};
    sc_core::sc_in<sc_dt::sc_logic> SE{"SE"};
    sc_core::sc_in<sc_dt::sc_logic> SD{"SD"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_FSDPSBQ_V2_2);
    explicit mipi_4_rx_dphy_ns_SEM_FSDPSBQ_V2_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << CK << D << SI << SE << SD << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_Q, n_RD, n_delayed_CK, n_delayed_D, n_delayed_SI, n_delayed_SE, n_delayed_SD, n_int_res__D, n_int_res__SD, n_xcr_0, n_CK_org, n_D_org, n_SD_org, n_SE_org, n_SI_org;
    vl::udp_state<3> s0_{3};
    vl::udp_state<6> s1_{3};
    void eval() {
        const vl::logic<1> i_CK = vl::rd(CK);
        const vl::logic<1> i_D = vl::rd(D);
        const vl::logic<1> i_SI = vl::rd(SI);
        const vl::logic<1> i_SE = vl::rd(SE);
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
        n_delayed_SD = i_SD;
        n_SD_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_SD, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_int_res__SD = vl::not_(n_SD_org);
        n_delayed_D = i_D;
        n_D_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_D, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_int_res__D = vl::ucomb(udp::mipi_4_rx_dphy_ns_mux2, vl::v3(n_SI_org), vl::v3(n_D_org), vl::v3(n_SE_org));
        n_delayed_CK = i_CK;
        n_CK_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_CK, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_RD = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(vl::logic<1>(0x0ULL), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        { const int in[3] = {vl::v3(n_CK_org), vl::v3(n_int_res__D), vl::v3(n_int_res__SD)}; if (s0_.update(udp::mipi_4_rx_dphy_ns_dff_s_err, in)) chg = true; }
        n_xcr_0 = vl::from3(s0_.st);
        { const int in[6] = {vl::v3(vl::LX()), vl::v3(n_CK_org), vl::v3(n_int_res__D), vl::v3(n_int_res__SD), vl::v3(n_RD), vl::v3(n_xcr_0)}; if (s1_.update(udp::mipi_4_rx_dphy_ns_dff_sr_0, in)) chg = true; }
        n_Q = vl::from3(s1_.st);
        }
        vl::wr(Q, n_Q);
    }
};

class mipi_4_rx_dphy_ns_SEM_FSDPSBQ_V2_4 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> Q{"Q"};
    sc_core::sc_in<sc_dt::sc_logic> CK{"CK"};
    sc_core::sc_in<sc_dt::sc_logic> D{"D"};
    sc_core::sc_in<sc_dt::sc_logic> SI{"SI"};
    sc_core::sc_in<sc_dt::sc_logic> SE{"SE"};
    sc_core::sc_in<sc_dt::sc_logic> SD{"SD"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_FSDPSBQ_V2_4);
    explicit mipi_4_rx_dphy_ns_SEM_FSDPSBQ_V2_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << CK << D << SI << SE << SD << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_Q, n_RD, n_delayed_CK, n_delayed_D, n_delayed_SI, n_delayed_SE, n_delayed_SD, n_int_res__D, n_int_res__SD, n_xcr_0, n_CK_org, n_D_org, n_SD_org, n_SE_org, n_SI_org;
    vl::udp_state<3> s0_{3};
    vl::udp_state<6> s1_{3};
    void eval() {
        const vl::logic<1> i_CK = vl::rd(CK);
        const vl::logic<1> i_D = vl::rd(D);
        const vl::logic<1> i_SI = vl::rd(SI);
        const vl::logic<1> i_SE = vl::rd(SE);
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
        n_delayed_SD = i_SD;
        n_SD_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_SD, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_int_res__SD = vl::not_(n_SD_org);
        n_delayed_D = i_D;
        n_D_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_D, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_int_res__D = vl::ucomb(udp::mipi_4_rx_dphy_ns_mux2, vl::v3(n_SI_org), vl::v3(n_D_org), vl::v3(n_SE_org));
        n_delayed_CK = i_CK;
        n_CK_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(n_delayed_CK, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        n_RD = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(vl::logic<1>(0x0ULL), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
        { const int in[3] = {vl::v3(n_CK_org), vl::v3(n_int_res__D), vl::v3(n_int_res__SD)}; if (s0_.update(udp::mipi_4_rx_dphy_ns_dff_s_err, in)) chg = true; }
        n_xcr_0 = vl::from3(s0_.st);
        { const int in[6] = {vl::v3(vl::LX()), vl::v3(n_CK_org), vl::v3(n_int_res__D), vl::v3(n_int_res__SD), vl::v3(n_RD), vl::v3(n_xcr_0)}; if (s1_.update(udp::mipi_4_rx_dphy_ns_dff_sr_0, in)) chg = true; }
        n_Q = vl::from3(s1_.st);
        }
        vl::wr(Q, n_Q);
    }
};

class mipi_4_rx_dphy_ns_SEM_INV_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_INV_1);
    explicit mipi_4_rx_dphy_ns_SEM_INV_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_INV_12 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_INV_12);
    explicit mipi_4_rx_dphy_ns_SEM_INV_12(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_INV_16 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_INV_16);
    explicit mipi_4_rx_dphy_ns_SEM_INV_16(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_INV_2 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_INV_2);
    explicit mipi_4_rx_dphy_ns_SEM_INV_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_INV_4 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_INV_4);
    explicit mipi_4_rx_dphy_ns_SEM_INV_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_INV_8 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_INV_8);
    explicit mipi_4_rx_dphy_ns_SEM_INV_8(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_INV_S_12 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_INV_S_12);
    explicit mipi_4_rx_dphy_ns_SEM_INV_S_12(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_INV_S_16 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_INV_S_16);
    explicit mipi_4_rx_dphy_ns_SEM_INV_S_16(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_INV_S_4 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_INV_S_4);
    explicit mipi_4_rx_dphy_ns_SEM_INV_S_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_INV_S_8 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A{"A"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_INV_S_8);
    explicit mipi_4_rx_dphy_ns_SEM_INV_S_8(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_LDNQ_4 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> Q{"Q"};
    sc_core::sc_in<sc_dt::sc_logic> G{"G"};
    sc_core::sc_in<sc_dt::sc_logic> D{"D"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_LDNQ_4);
    explicit mipi_4_rx_dphy_ns_SEM_LDNQ_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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
        { const int in[3] = {vl::v3(vl::LX()), vl::v3(n_int_res__G), vl::v3(n_D_org)}; if (s0_.update(udp::mipi_4_rx_dphy_ns_latch, in)) chg = true; }
        n_Q = vl::from3(s0_.st);
        }
        vl::wr(Q, n_Q);
    }
};

class mipi_4_rx_dphy_ns_SEM_MAJ3_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> A3{"A3"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_MAJ3_1);
    explicit mipi_4_rx_dphy_ns_SEM_MAJ3_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << A1 << A2 << A3 << VDD << VSS << VBP << VBN;
    }

private:
    vl::logic<1> n_X, n_int_res_0, n_int_res_1, n_int_res_2, n_A1_org, n_A2_org, n_A3_org;
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
    n_int_res_2 = vl::and_(n_A2_org, n_A3_org);
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_1 = vl::and_(n_A1_org, n_A3_org);
    n_int_res_0 = vl::and_(n_A1_org, n_A2_org);
    n_X = vl::or_(n_int_res_0, n_int_res_1, n_int_res_2);
        vl::wr(X, n_X);
    }
};

class mipi_4_rx_dphy_ns_SEM_MUX2_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> D0{"D0"};
    sc_core::sc_in<sc_dt::sc_logic> D1{"D1"};
    sc_core::sc_in<sc_dt::sc_logic> S{"S"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_MUX2_1);
    explicit mipi_4_rx_dphy_ns_SEM_MUX2_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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
    n_X = vl::ucomb(udp::mipi_4_rx_dphy_ns_mux2, vl::v3(n_D1_org), vl::v3(n_D0_org), vl::v3(n_S_org));
        vl::wr(X, n_X);
    }
};

class mipi_4_rx_dphy_ns_SEM_MUX2_2 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> D0{"D0"};
    sc_core::sc_in<sc_dt::sc_logic> D1{"D1"};
    sc_core::sc_in<sc_dt::sc_logic> S{"S"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_MUX2_2);
    explicit mipi_4_rx_dphy_ns_SEM_MUX2_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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
    n_X = vl::ucomb(udp::mipi_4_rx_dphy_ns_mux2, vl::v3(n_D1_org), vl::v3(n_D0_org), vl::v3(n_S_org));
        vl::wr(X, n_X);
    }
};

class mipi_4_rx_dphy_ns_SEM_MUX2_4 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> D0{"D0"};
    sc_core::sc_in<sc_dt::sc_logic> D1{"D1"};
    sc_core::sc_in<sc_dt::sc_logic> S{"S"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_MUX2_4);
    explicit mipi_4_rx_dphy_ns_SEM_MUX2_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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
    n_X = vl::ucomb(udp::mipi_4_rx_dphy_ns_mux2, vl::v3(n_D1_org), vl::v3(n_D0_org), vl::v3(n_S_org));
        vl::wr(X, n_X);
    }
};

class mipi_4_rx_dphy_ns_SEM_MUX2_S_2 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> D0{"D0"};
    sc_core::sc_in<sc_dt::sc_logic> D1{"D1"};
    sc_core::sc_in<sc_dt::sc_logic> S{"S"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_MUX2_S_2);
    explicit mipi_4_rx_dphy_ns_SEM_MUX2_S_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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
    n_X = vl::ucomb(udp::mipi_4_rx_dphy_ns_mux2, vl::v3(n_D1_org), vl::v3(n_D0_org), vl::v3(n_S_org));
        vl::wr(X, n_X);
    }
};

class mipi_4_rx_dphy_ns_SEM_MUX2_S_4 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> D0{"D0"};
    sc_core::sc_in<sc_dt::sc_logic> D1{"D1"};
    sc_core::sc_in<sc_dt::sc_logic> S{"S"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_MUX2_S_4);
    explicit mipi_4_rx_dphy_ns_SEM_MUX2_S_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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
    n_X = vl::ucomb(udp::mipi_4_rx_dphy_ns_mux2, vl::v3(n_D1_org), vl::v3(n_D0_org), vl::v3(n_S_org));
        vl::wr(X, n_X);
    }
};

class mipi_4_rx_dphy_ns_SEM_ND2_S_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_ND2_S_1);
    explicit mipi_4_rx_dphy_ns_SEM_ND2_S_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_ND2_S_12 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_ND2_S_12);
    explicit mipi_4_rx_dphy_ns_SEM_ND2_S_12(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_ND2_S_16 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_ND2_S_16);
    explicit mipi_4_rx_dphy_ns_SEM_ND2_S_16(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_ND2_S_2 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_ND2_S_2);
    explicit mipi_4_rx_dphy_ns_SEM_ND2_S_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_ND2_S_4 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_ND2_S_4);
    explicit mipi_4_rx_dphy_ns_SEM_ND2_S_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_ND2_S_8 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_ND2_S_8);
    explicit mipi_4_rx_dphy_ns_SEM_ND2_S_8(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_ND3_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> A3{"A3"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_ND3_1);
    explicit mipi_4_rx_dphy_ns_SEM_ND3_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_ND3_2 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> A3{"A3"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_ND3_2);
    explicit mipi_4_rx_dphy_ns_SEM_ND3_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_ND3_4 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> A3{"A3"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_ND3_4);
    explicit mipi_4_rx_dphy_ns_SEM_ND3_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_ND4_1 : public sc_core::sc_module {
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
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_ND4_1);
    explicit mipi_4_rx_dphy_ns_SEM_ND4_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_ND4_2 : public sc_core::sc_module {
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
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_ND4_2);
    explicit mipi_4_rx_dphy_ns_SEM_ND4_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_ND4_4 : public sc_core::sc_module {
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
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_ND4_4);
    explicit mipi_4_rx_dphy_ns_SEM_ND4_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_NR2_S_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_NR2_S_1);
    explicit mipi_4_rx_dphy_ns_SEM_NR2_S_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_NR2_S_2 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_NR2_S_2);
    explicit mipi_4_rx_dphy_ns_SEM_NR2_S_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_NR2_S_4 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_NR2_S_4);
    explicit mipi_4_rx_dphy_ns_SEM_NR2_S_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_NR2_S_6 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_NR2_S_6);
    explicit mipi_4_rx_dphy_ns_SEM_NR2_S_6(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_NR2_S_8 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_NR2_S_8);
    explicit mipi_4_rx_dphy_ns_SEM_NR2_S_8(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_NR4_1 : public sc_core::sc_module {
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
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_NR4_1);
    explicit mipi_4_rx_dphy_ns_SEM_NR4_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_NR4_2 : public sc_core::sc_module {
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
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_NR4_2);
    explicit mipi_4_rx_dphy_ns_SEM_NR4_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_OA21_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> B{"B"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_OA21_1);
    explicit mipi_4_rx_dphy_ns_SEM_OA21_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_OA21_2 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> B{"B"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_OA21_2);
    explicit mipi_4_rx_dphy_ns_SEM_OA21_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_OAI211_1 : public sc_core::sc_module {
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
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_OAI211_1);
    explicit mipi_4_rx_dphy_ns_SEM_OAI211_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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
    n_int_res_0 = vl::or_(n_A1_org, n_A2_org);
    n_X = vl::nand_(n_int_res_0, n_B1_org, n_B2_org);
        vl::wr(X, n_X);
    }
};

class mipi_4_rx_dphy_ns_SEM_OAI21_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> B{"B"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_OAI21_1);
    explicit mipi_4_rx_dphy_ns_SEM_OAI21_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_OAI21_2 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> B{"B"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_OAI21_2);
    explicit mipi_4_rx_dphy_ns_SEM_OAI21_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_OAI21_4 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> B{"B"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_OAI21_4);
    explicit mipi_4_rx_dphy_ns_SEM_OAI21_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_OAI22_1 : public sc_core::sc_module {
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
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_OAI22_1);
    explicit mipi_4_rx_dphy_ns_SEM_OAI22_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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
    n_int_res_1 = vl::or_(n_B1_org, n_B2_org);
    n_A2_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A2, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_A1_org = vl::resolve(vl::bufif1_(vl::logic<1>(), vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))), vl::bufif0_(i_A1, vl::lor(vl::lor(vl::lor(vl::cne(i_VDD, vl::logic<1>(0x1ULL)), vl::cne(i_VSS, vl::logic<1>(0x0ULL))), vl::cne(i_VBP, vl::logic<1>(0x1ULL))), vl::cne(i_VBN, vl::logic<1>(0x0ULL)))));
    n_int_res_0 = vl::or_(n_A1_org, n_A2_org);
    n_X = vl::nand_(n_int_res_0, n_int_res_1);
        vl::wr(X, n_X);
    }
};

class mipi_4_rx_dphy_ns_SEM_OAOI211_1 : public sc_core::sc_module {
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
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_OAOI211_1);
    explicit mipi_4_rx_dphy_ns_SEM_OAOI211_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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
    n_int_res_0 = vl::or_(n_A1_org, n_A2_org);
    n_int_res_1 = vl::and_(n_int_res_0, n_B_org);
    n_X = vl::nor_(n_int_res_1, n_C_org);
        vl::wr(X, n_X);
    }
};

class mipi_4_rx_dphy_ns_SEM_OR2_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_OR2_1);
    explicit mipi_4_rx_dphy_ns_SEM_OR2_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_OR2_12 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_OR2_12);
    explicit mipi_4_rx_dphy_ns_SEM_OR2_12(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_OR2_2 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_OR2_2);
    explicit mipi_4_rx_dphy_ns_SEM_OR2_2(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_OR2_4 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_OR2_4);
    explicit mipi_4_rx_dphy_ns_SEM_OR2_4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_OR2_8 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> A1{"A1"};
    sc_core::sc_in<sc_dt::sc_logic> A2{"A2"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_OR2_8);
    explicit mipi_4_rx_dphy_ns_SEM_OR2_8(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_TIE0_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_TIE0_1);
    explicit mipi_4_rx_dphy_ns_SEM_TIE0_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_TIE1_1 : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_TIE1_1);
    explicit mipi_4_rx_dphy_ns_SEM_TIE1_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_SEM_TIEDIN_1 : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> X{"X"};
    sc_core::sc_in<sc_dt::sc_logic> VDD{"VDD"};
    sc_core::sc_in<sc_dt::sc_logic> VSS{"VSS"};
    sc_core::sc_in<sc_dt::sc_logic> VBP{"VBP"};
    sc_core::sc_in<sc_dt::sc_logic> VBN{"VBN"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_SEM_TIEDIN_1);
    explicit mipi_4_rx_dphy_ns_SEM_TIEDIN_1(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_sc_invx128_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_sc_invx128_lvt);
    explicit mipi_4_rx_dphy_ns_sc_invx128_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_sc_invx12_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_sc_invx12_lvt);
    explicit mipi_4_rx_dphy_ns_sc_invx12_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_sc_invx16_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_sc_invx16_lvt);
    explicit mipi_4_rx_dphy_ns_sc_invx16_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_sc_invx24_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_sc_invx24_lvt);
    explicit mipi_4_rx_dphy_ns_sc_invx24_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_sc_invx32_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_sc_invx32_lvt);
    explicit mipi_4_rx_dphy_ns_sc_invx32_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_sc_invx48_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_sc_invx48_lvt);
    explicit mipi_4_rx_dphy_ns_sc_invx48_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_sc_invx4_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_sc_invx4_lvt);
    explicit mipi_4_rx_dphy_ns_sc_invx4_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_sc_invx64_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_sc_invx64_lvt);
    explicit mipi_4_rx_dphy_ns_sc_invx64_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_sc_invx6_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_sc_invx6_lvt);
    explicit mipi_4_rx_dphy_ns_sc_invx6_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_sc_invx8_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_sc_invx8_lvt);
    explicit mipi_4_rx_dphy_ns_sc_invx8_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_sc_invx96_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_sc_invx96_lvt);
    explicit mipi_4_rx_dphy_ns_sc_invx96_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_sc_mux2x8_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a0{"a0"};
    sc_core::sc_in<sc_dt::sc_logic> a1{"a1"};
    sc_core::sc_in<sc_dt::sc_logic> s{"s"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_sc_mux2x8_lvt);
    explicit mipi_4_rx_dphy_ns_sc_mux2x8_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_sc_na2x12_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_in<sc_dt::sc_logic> b{"b"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_sc_na2x12_lvt);
    explicit mipi_4_rx_dphy_ns_sc_na2x12_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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
    n_z = vl::bufif1_(~(i_a & i_b), vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))));
        vl::wr(z, n_z);
    }
};

class mipi_4_rx_dphy_ns_sc_na2x24_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_in<sc_dt::sc_logic> b{"b"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_sc_na2x24_lvt);
    explicit mipi_4_rx_dphy_ns_sc_na2x24_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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
    n_z = vl::bufif1_(~(i_a & i_b), vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))));
        vl::wr(z, n_z);
    }
};

class mipi_4_rx_dphy_ns_sc_na2x6_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_in<sc_dt::sc_logic> b{"b"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_sc_na2x6_lvt);
    explicit mipi_4_rx_dphy_ns_sc_na2x6_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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
    n_z = vl::bufif1_(~(i_a & i_b), vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))));
        vl::wr(z, n_z);
    }
};

class mipi_4_rx_dphy_ns_sc_na3x16_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_in<sc_dt::sc_logic> b{"b"};
    sc_core::sc_in<sc_dt::sc_logic> c{"c"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_sc_na3x16_lvt);
    explicit mipi_4_rx_dphy_ns_sc_na3x16_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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
    n_z = vl::bufif1_(~((i_a & i_b) & i_c), vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))));
        vl::wr(z, n_z);
    }
};

class mipi_4_rx_dphy_ns_sc_na3x6_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_in<sc_dt::sc_logic> b{"b"};
    sc_core::sc_in<sc_dt::sc_logic> c{"c"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_sc_na3x6_lvt);
    explicit mipi_4_rx_dphy_ns_sc_na3x6_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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
    n_z = vl::bufif1_(~((i_a & i_b) & i_c), vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))));
        vl::wr(z, n_z);
    }
};

class mipi_4_rx_dphy_ns_sc_na4x6_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_in<sc_dt::sc_logic> b{"b"};
    sc_core::sc_in<sc_dt::sc_logic> c{"c"};
    sc_core::sc_in<sc_dt::sc_logic> d{"d"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_sc_na4x6_lvt);
    explicit mipi_4_rx_dphy_ns_sc_na4x6_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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
    n_z = vl::bufif1_(~(((i_a & i_b) & i_c) & i_d), vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))));
        vl::wr(z, n_z);
    }
};

class mipi_4_rx_dphy_ns_sc_no2x12_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_in<sc_dt::sc_logic> b{"b"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_sc_no2x12_lvt);
    explicit mipi_4_rx_dphy_ns_sc_no2x12_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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
    n_z = vl::bufif1_(~(i_a | i_b), vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))));
        vl::wr(z, n_z);
    }
};

class mipi_4_rx_dphy_ns_sc_no2x6_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_in<sc_dt::sc_logic> b{"b"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_sc_no2x6_lvt);
    explicit mipi_4_rx_dphy_ns_sc_no2x6_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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
    n_z = vl::bufif1_(~(i_a | i_b), vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))));
        vl::wr(z, n_z);
    }
};

class mipi_4_rx_dphy_ns_sc_no3x6_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_in<sc_dt::sc_logic> b{"b"};
    sc_core::sc_in<sc_dt::sc_logic> c{"c"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_sc_no3x6_lvt);
    explicit mipi_4_rx_dphy_ns_sc_no3x6_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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
    n_z = vl::bufif1_(~((i_a | i_b) | i_c), vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))));
        vl::wr(z, n_z);
    }
};

class mipi_4_rx_dphy_ns_sc_tgate_x12_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_in<sc_dt::sc_logic> c_m{"c_m"};
    sc_core::sc_in<sc_dt::sc_logic> c_p{"c_p"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_sc_tgate_x12_lvt);
    explicit mipi_4_rx_dphy_ns_sc_tgate_x12_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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
    n_z = vl::bufif1_(i_a, vl::land(vl::land(vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))), vl::ceq(i_c_m, vl::logic<1>(0x0ULL))), vl::ceq(i_c_p, vl::logic<1>(0x1ULL))));
        vl::wr(z, n_z);
    }
};

class mipi_4_rx_dphy_ns_sc_tgate_x8_lvt : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_in<sc_dt::sc_logic> c_m{"c_m"};
    sc_core::sc_in<sc_dt::sc_logic> c_p{"c_p"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_sc_tgate_x8_lvt);
    explicit mipi_4_rx_dphy_ns_sc_tgate_x8_lvt(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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
    n_z = vl::bufif1_(i_a, vl::land(vl::land(vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))), vl::ceq(i_c_m, vl::logic<1>(0x0ULL))), vl::ceq(i_c_p, vl::logic<1>(0x1ULL))));
        vl::wr(z, n_z);
    }
};

class mipi_4_rx_dphy_ns_sctg_inv16x : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_sctg_inv16x);
    explicit mipi_4_rx_dphy_ns_sctg_inv16x(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_sctg_inv1x : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_sctg_inv1x);
    explicit mipi_4_rx_dphy_ns_sctg_inv1x(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_sctg_inv3x : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_sctg_inv3x);
    explicit mipi_4_rx_dphy_ns_sctg_inv3x(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_sctg_inv4x : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_sctg_inv4x);
    explicit mipi_4_rx_dphy_ns_sctg_inv4x(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_sctg_inv8x : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_sctg_inv8x);
    explicit mipi_4_rx_dphy_ns_sctg_inv8x(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_sctg_inv9x : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_sctg_inv9x);
    explicit mipi_4_rx_dphy_ns_sctg_inv9x(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_sctg_nand2x : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a0{"a0"};
    sc_core::sc_in<sc_dt::sc_logic> a1{"a1"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_sctg_nand2x);
    explicit mipi_4_rx_dphy_ns_sctg_nand2x(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << a0 << a1 << gd << vph;
    }

private:
    vl::logic<1> n_z;
    void eval() {
        const vl::logic<1> i_a0 = vl::rd(a0);
        const vl::logic<1> i_a1 = vl::rd(a1);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vph = vl::rd(vph);
    n_z = vl::bufif1_(~(i_a0 & i_a1), vl::land(vl::ceq(i_vph, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))));
        vl::wr(z, n_z);
    }
};

class mipi_4_rx_dphy_ns_sctg_nand4x : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a0{"a0"};
    sc_core::sc_in<sc_dt::sc_logic> a1{"a1"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_sctg_nand4x);
    explicit mipi_4_rx_dphy_ns_sctg_nand4x(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << a0 << a1 << gd << vph;
    }

private:
    vl::logic<1> n_z;
    void eval() {
        const vl::logic<1> i_a0 = vl::rd(a0);
        const vl::logic<1> i_a1 = vl::rd(a1);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vph = vl::rd(vph);
    n_z = vl::bufif1_(~(i_a0 & i_a1), vl::land(vl::ceq(i_vph, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))));
        vl::wr(z, n_z);
    }
};

class mipi_4_rx_dphy_ns_sctg_nor1x : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a0{"a0"};
    sc_core::sc_in<sc_dt::sc_logic> a1{"a1"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_sctg_nor1x);
    explicit mipi_4_rx_dphy_ns_sctg_nor1x(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << a0 << a1 << gd << vph;
    }

private:
    vl::logic<1> n_z;
    void eval() {
        const vl::logic<1> i_a0 = vl::rd(a0);
        const vl::logic<1> i_a1 = vl::rd(a1);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vph = vl::rd(vph);
    n_z = vl::bufif1_(~(i_a0 | i_a1), vl::land(vl::ceq(i_vph, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))));
        vl::wr(z, n_z);
    }
};

class mipi_4_rx_dphy_ns_sctg_nor4x : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a0{"a0"};
    sc_core::sc_in<sc_dt::sc_logic> a1{"a1"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_sctg_nor4x);
    explicit mipi_4_rx_dphy_ns_sctg_nor4x(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << a0 << a1 << gd << vph;
    }

private:
    vl::logic<1> n_z;
    void eval() {
        const vl::logic<1> i_a0 = vl::rd(a0);
        const vl::logic<1> i_a1 = vl::rd(a1);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vph = vl::rd(vph);
    n_z = vl::bufif1_(~(i_a0 | i_a1), vl::land(vl::ceq(i_vph, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))));
        vl::wr(z, n_z);
    }
};

class mipi_4_rx_dphy_ns_tieLowHigh_HV : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> high{"high"};
    sc_core::sc_out<sc_dt::sc_logic> low{"low"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_tieLowHigh_HV);
    explicit mipi_4_rx_dphy_ns_tieLowHigh_HV(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
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

class mipi_4_rx_dphy_ns_tieLowHigh_LV : public sc_core::sc_module {
public:
    sc_core::sc_out<sc_dt::sc_logic> high{"high"};
    sc_core::sc_out<sc_dt::sc_logic> low{"low"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_tieLowHigh_LV);
    explicit mipi_4_rx_dphy_ns_tieLowHigh_LV(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << gd << vp;
    }

private:
    vl::logic<1> n_high, n_low;
    void eval() {
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vp = vl::rd(vp);
    n_low = vl::mux(vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))), vl::logic<1>(0x0ULL), vl::logic<1>::all_z());
    n_high = vl::mux(vl::land(vl::ceq(i_vp, vl::logic<1>(0x1ULL)), vl::ceq(i_gd, vl::logic<1>(0x0ULL))), vl::logic<1>(0x1ULL), vl::logic<1>::all_z());
        vl::wr(high, n_high);
        vl::wr(low, n_low);
    }
};

#endif
