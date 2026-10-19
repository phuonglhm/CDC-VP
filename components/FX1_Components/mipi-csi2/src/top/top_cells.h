//--------------------------------------------------------------------------------------------------
// library cells (top_cells)
//--------------------------------------------------------------------------------------------------
#ifndef VL_TOP_CELLS_H
#define VL_TOP_CELLS_H

#include "vl/vl_rt.h"
#include "udp_lib.h"

class mipi_4_rx_io_DPHY3RAFE4L_bump : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> ATB{"ATB"};
    sc_core::sc_in<sc_dt::sc_logic> REXT{"REXT"};
    sc_core::sc_in<sc_dt::sc_logic> dn0{"dn0"};
    sc_core::sc_in<sc_dt::sc_logic> dn1{"dn1"};
    sc_core::sc_in<sc_dt::sc_logic> dn2{"dn2"};
    sc_core::sc_in<sc_dt::sc_logic> dn3{"dn3"};
    sc_core::sc_in<sc_dt::sc_logic> dnclk{"dnclk"};
    sc_core::sc_in<sc_dt::sc_logic> dp0{"dp0"};
    sc_core::sc_in<sc_dt::sc_logic> dp1{"dp1"};
    sc_core::sc_in<sc_dt::sc_logic> dp2{"dp2"};
    sc_core::sc_in<sc_dt::sc_logic> dp3{"dp3"};
    sc_core::sc_in<sc_dt::sc_logic> dpclk{"dpclk"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};
    SC_HAS_PROCESS(mipi_4_rx_io_DPHY3RAFE4L_bump);
    explicit mipi_4_rx_io_DPHY3RAFE4L_bump(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << ATB << REXT << dn0 << dn1 << dn2 << dn3 << dnclk << dp0 << dp1 << dp2 << dp3 << dpclk << gd << vp << vph;
    }

private:
    void eval() {
        const vl::logic<1> i_ATB = vl::rd(ATB);
        const vl::logic<1> i_REXT = vl::rd(REXT);
        const vl::logic<1> i_dn0 = vl::rd(dn0);
        const vl::logic<1> i_dn1 = vl::rd(dn1);
        const vl::logic<1> i_dn2 = vl::rd(dn2);
        const vl::logic<1> i_dn3 = vl::rd(dn3);
        const vl::logic<1> i_dnclk = vl::rd(dnclk);
        const vl::logic<1> i_dp0 = vl::rd(dp0);
        const vl::logic<1> i_dp1 = vl::rd(dp1);
        const vl::logic<1> i_dp2 = vl::rd(dp2);
        const vl::logic<1> i_dp3 = vl::rd(dp3);
        const vl::logic<1> i_dpclk = vl::rd(dpclk);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vp = vl::rd(vp);
        const vl::logic<1> i_vph = vl::rd(vph);
    }
};

class mipi_4_rx_pll_io_DPHY3RAFE4L_PLL_bump : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> ATB{"ATB"};
    sc_core::sc_in<sc_dt::sc_logic> REXT{"REXT"};
    sc_core::sc_in<sc_dt::sc_logic> dn0{"dn0"};
    sc_core::sc_in<sc_dt::sc_logic> dn1{"dn1"};
    sc_core::sc_in<sc_dt::sc_logic> dn2{"dn2"};
    sc_core::sc_in<sc_dt::sc_logic> dn3{"dn3"};
    sc_core::sc_in<sc_dt::sc_logic> dnclk{"dnclk"};
    sc_core::sc_in<sc_dt::sc_logic> dp0{"dp0"};
    sc_core::sc_in<sc_dt::sc_logic> dp1{"dp1"};
    sc_core::sc_in<sc_dt::sc_logic> dp2{"dp2"};
    sc_core::sc_in<sc_dt::sc_logic> dp3{"dp3"};
    sc_core::sc_in<sc_dt::sc_logic> dpclk{"dpclk"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};
    SC_HAS_PROCESS(mipi_4_rx_pll_io_DPHY3RAFE4L_PLL_bump);
    explicit mipi_4_rx_pll_io_DPHY3RAFE4L_PLL_bump(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << ATB << REXT << dn0 << dn1 << dn2 << dn3 << dnclk << dp0 << dp1 << dp2 << dp3 << dpclk << gd << vp << vph;
    }

private:
    void eval() {
        const vl::logic<1> i_ATB = vl::rd(ATB);
        const vl::logic<1> i_REXT = vl::rd(REXT);
        const vl::logic<1> i_dn0 = vl::rd(dn0);
        const vl::logic<1> i_dn1 = vl::rd(dn1);
        const vl::logic<1> i_dn2 = vl::rd(dn2);
        const vl::logic<1> i_dn3 = vl::rd(dn3);
        const vl::logic<1> i_dnclk = vl::rd(dnclk);
        const vl::logic<1> i_dp0 = vl::rd(dp0);
        const vl::logic<1> i_dp1 = vl::rd(dp1);
        const vl::logic<1> i_dp2 = vl::rd(dp2);
        const vl::logic<1> i_dp3 = vl::rd(dp3);
        const vl::logic<1> i_dpclk = vl::rd(dpclk);
        const vl::logic<1> i_gd = vl::rd(gd);
        const vl::logic<1> i_vp = vl::rd(vp);
        const vl::logic<1> i_vph = vl::rd(vph);
    }
};

class mipirxphy_misc : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> clkout0_left{"clkout0_left"};
    sc_core::sc_in<sc_dt::sc_logic> clkout0_right{"clkout0_right"};
    sc_core::sc_in<sc_dt::sc_logic> clkout180_left{"clkout180_left"};
    sc_core::sc_in<sc_dt::sc_logic> clkout180_right{"clkout180_right"};
    sc_core::sc_in<sc_dt::sc_logic> clkout270_left{"clkout270_left"};
    sc_core::sc_in<sc_dt::sc_logic> clkout270_right{"clkout270_right"};
    sc_core::sc_in<sc_dt::sc_logic> clkout90_left{"clkout90_left"};
    sc_core::sc_in<sc_dt::sc_logic> clkout90_right{"clkout90_right"};
    sc_core::sc_in<sc_dt::sc_logic> clkout_stop_left{"clkout_stop_left"};
    sc_core::sc_in<sc_dt::sc_logic> clkout_stop_right{"clkout_stop_right"};
    sc_core::sc_in<sc_dt::sc_logic> pll_clkouten_left{"pll_clkouten_left"};
    sc_core::sc_in<sc_dt::sc_logic> pll_clkouten_right{"pll_clkouten_right"};
    sc_core::sc_out<sc_dt::sc_logic> pll_clkout0{"pll_clkout0"};
    sc_core::sc_out<sc_dt::sc_logic> pll_clkout180{"pll_clkout180"};
    sc_core::sc_out<sc_dt::sc_logic> pll_clkout270{"pll_clkout270"};
    sc_core::sc_out<sc_dt::sc_logic> pll_clkout90{"pll_clkout90"};
    sc_core::sc_out<sc_dt::sc_logic> pll_clkout_stop{"pll_clkout_stop"};
    sc_core::sc_out<sc_dt::sc_logic> MIPIRXPHY_CONT_DAT0_oen{"MIPIRXPHY_CONT_DAT0_oen"};
    sc_core::sc_out<sc_dt::sc_logic> MIPIRXPHY_CONT_DAT1_oen{"MIPIRXPHY_CONT_DAT1_oen"};
    sc_core::sc_out<sc_dt::sc_logic> MIPIRXPHY_CONT_DAT2_oen{"MIPIRXPHY_CONT_DAT2_oen"};
    sc_core::sc_out<sc_dt::sc_logic> MIPIRXPHY_CONT_DAT3_oen{"MIPIRXPHY_CONT_DAT3_oen"};
    sc_core::sc_out<sc_dt::sc_logic> MIPIRXPHY_CONT_DAT4_oen{"MIPIRXPHY_CONT_DAT4_oen"};
    sc_core::sc_out<sc_dt::sc_logic> MIPIRXPHY_CONT_DAT5_oen{"MIPIRXPHY_CONT_DAT5_oen"};
    sc_core::sc_out<sc_dt::sc_logic> MIPIRXPHY_CONT_DAT6_oen{"MIPIRXPHY_CONT_DAT6_oen"};
    sc_core::sc_out<sc_dt::sc_logic> MIPIRXPHY_CONT_DAT7_oen{"MIPIRXPHY_CONT_DAT7_oen"};
    sc_core::sc_out<sc_dt::sc_logic> MIPIRXPHY_CONT_DAT8_oen{"MIPIRXPHY_CONT_DAT8_oen"};
    sc_core::sc_out<sc_dt::sc_logic> MIPIRXPHY_CONT_DAT9_oen{"MIPIRXPHY_CONT_DAT9_oen"};
    sc_core::sc_out<sc_dt::sc_logic> MIPIRXPHY_CONT_DAT10_oen{"MIPIRXPHY_CONT_DAT10_oen"};
    SC_HAS_PROCESS(mipirxphy_misc);
    explicit mipirxphy_misc(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
        SC_METHOD(eval);
        sensitive << clkout0_left << clkout0_right << clkout180_left << clkout180_right << clkout270_left << clkout270_right << clkout90_left << clkout90_right << clkout_stop_left << clkout_stop_right << pll_clkouten_left << pll_clkouten_right;
    }

private:
    vl::logic<1> n_pll_clkout0, n_pll_clkout180, n_pll_clkout270, n_pll_clkout90, n_pll_clkout_stop, n_MIPIRXPHY_CONT_DAT0_oen, n_MIPIRXPHY_CONT_DAT1_oen, n_MIPIRXPHY_CONT_DAT2_oen, n_MIPIRXPHY_CONT_DAT3_oen, n_MIPIRXPHY_CONT_DAT4_oen, n_MIPIRXPHY_CONT_DAT5_oen, n_MIPIRXPHY_CONT_DAT6_oen, n_MIPIRXPHY_CONT_DAT7_oen, n_MIPIRXPHY_CONT_DAT8_oen, n_MIPIRXPHY_CONT_DAT9_oen, n_MIPIRXPHY_CONT_DAT10_oen;
    void eval() {
        const vl::logic<1> i_clkout0_left = vl::rd(clkout0_left);
        const vl::logic<1> i_clkout0_right = vl::rd(clkout0_right);
        const vl::logic<1> i_clkout180_left = vl::rd(clkout180_left);
        const vl::logic<1> i_clkout180_right = vl::rd(clkout180_right);
        const vl::logic<1> i_clkout270_left = vl::rd(clkout270_left);
        const vl::logic<1> i_clkout270_right = vl::rd(clkout270_right);
        const vl::logic<1> i_clkout90_left = vl::rd(clkout90_left);
        const vl::logic<1> i_clkout90_right = vl::rd(clkout90_right);
        const vl::logic<1> i_clkout_stop_left = vl::rd(clkout_stop_left);
        const vl::logic<1> i_clkout_stop_right = vl::rd(clkout_stop_right);
        const vl::logic<1> i_pll_clkouten_left = vl::rd(pll_clkouten_left);
        const vl::logic<1> i_pll_clkouten_right = vl::rd(pll_clkouten_right);
    n_MIPIRXPHY_CONT_DAT10_oen = vl::logic<1>(0x0ULL);
    n_MIPIRXPHY_CONT_DAT9_oen = vl::logic<1>(0x0ULL);
    n_MIPIRXPHY_CONT_DAT8_oen = vl::logic<1>(0x0ULL);
    n_MIPIRXPHY_CONT_DAT7_oen = vl::logic<1>(0x0ULL);
    n_MIPIRXPHY_CONT_DAT6_oen = vl::logic<1>(0x0ULL);
    n_MIPIRXPHY_CONT_DAT5_oen = vl::logic<1>(0x0ULL);
    n_MIPIRXPHY_CONT_DAT4_oen = vl::logic<1>(0x0ULL);
    n_MIPIRXPHY_CONT_DAT3_oen = vl::logic<1>(0x0ULL);
    n_MIPIRXPHY_CONT_DAT2_oen = vl::logic<1>(0x0ULL);
    n_MIPIRXPHY_CONT_DAT1_oen = vl::logic<1>(0x0ULL);
    n_MIPIRXPHY_CONT_DAT0_oen = vl::logic<1>(0x0ULL);
    n_pll_clkout_stop = vl::mux(i_pll_clkouten_left, i_clkout_stop_left, vl::mux(i_pll_clkouten_right, i_clkout_stop_right, vl::logic<1>(0x0ULL)));
    n_pll_clkout90 = vl::mux(i_pll_clkouten_left, i_clkout90_left, vl::mux(i_pll_clkouten_right, i_clkout90_right, vl::logic<1>(0x0ULL)));
    n_pll_clkout270 = vl::mux(i_pll_clkouten_left, i_clkout270_left, vl::mux(i_pll_clkouten_right, i_clkout270_right, vl::logic<1>(0x0ULL)));
    n_pll_clkout180 = vl::mux(i_pll_clkouten_left, i_clkout180_left, vl::mux(i_pll_clkouten_right, i_clkout180_right, vl::logic<1>(0x0ULL)));
    n_pll_clkout0 = vl::mux(i_pll_clkouten_left, i_clkout0_left, vl::mux(i_pll_clkouten_right, i_clkout0_right, vl::logic<1>(0x0ULL)));
        vl::wr(pll_clkout0, n_pll_clkout0);
        vl::wr(pll_clkout180, n_pll_clkout180);
        vl::wr(pll_clkout270, n_pll_clkout270);
        vl::wr(pll_clkout90, n_pll_clkout90);
        vl::wr(pll_clkout_stop, n_pll_clkout_stop);
        vl::wr(MIPIRXPHY_CONT_DAT0_oen, n_MIPIRXPHY_CONT_DAT0_oen);
        vl::wr(MIPIRXPHY_CONT_DAT1_oen, n_MIPIRXPHY_CONT_DAT1_oen);
        vl::wr(MIPIRXPHY_CONT_DAT2_oen, n_MIPIRXPHY_CONT_DAT2_oen);
        vl::wr(MIPIRXPHY_CONT_DAT3_oen, n_MIPIRXPHY_CONT_DAT3_oen);
        vl::wr(MIPIRXPHY_CONT_DAT4_oen, n_MIPIRXPHY_CONT_DAT4_oen);
        vl::wr(MIPIRXPHY_CONT_DAT5_oen, n_MIPIRXPHY_CONT_DAT5_oen);
        vl::wr(MIPIRXPHY_CONT_DAT6_oen, n_MIPIRXPHY_CONT_DAT6_oen);
        vl::wr(MIPIRXPHY_CONT_DAT7_oen, n_MIPIRXPHY_CONT_DAT7_oen);
        vl::wr(MIPIRXPHY_CONT_DAT8_oen, n_MIPIRXPHY_CONT_DAT8_oen);
        vl::wr(MIPIRXPHY_CONT_DAT9_oen, n_MIPIRXPHY_CONT_DAT9_oen);
        vl::wr(MIPIRXPHY_CONT_DAT10_oen, n_MIPIRXPHY_CONT_DAT10_oen);
    }
};

#endif
