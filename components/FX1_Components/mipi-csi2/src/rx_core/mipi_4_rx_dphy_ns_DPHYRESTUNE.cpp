//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYRESTUNE
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYRESTUNE.h"

mipi_4_rx_dphy_ns_DPHYRESTUNE::mipi_4_rx_dphy_ns_DPHYRESTUNE(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(drive_clk2dig);
    sensitive << clk2dig_r;
    SC_METHOD(assign_supply_ok);
    sensitive << vp << vph << gd;
    SC_METHOD(proc_initial_160572);
    SC_METHOD(proc_initial_160573);
    SC_METHOD(proc_always_160575);
    sensitive << corner << en_res0_en_cap1_n;
    SC_METHOD(proc_always_160584);
    sensitive << supply_ok << pon_restune_hv << clk2dig_r << FREQ;
}

void mipi_4_rx_dphy_ns_DPHYRESTUNE::drive_clk2dig() { vl::wr(clk2dig, vl::rd(clk2dig_r)); }

void mipi_4_rx_dphy_ns_DPHYRESTUNE::assign_supply_ok() {
    vl::wr(supply_ok, vl::land(vl::land(vl::ceq(vl::rd(vp), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(vph), vl::logic<1>(0x1ULL))), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))));
}

void mipi_4_rx_dphy_ns_DPHYRESTUNE::proc_initial_160572() {
    vl::logic<1> clk2dig_sh = vl::rd(clk2dig_r);
    clk2dig_sh = vl::logic<1>(0x0ULL);
    vl::wr(clk2dig_r, clk2dig_sh);
}

void mipi_4_rx_dphy_ns_DPHYRESTUNE::proc_initial_160573() {
    vl::logic<2> corner_sh = vl::rd(corner);
    corner_sh = vl::random().ext<2>();
    vl::wr(corner, corner_sh);
}

void mipi_4_rx_dphy_ns_DPHYRESTUNE::proc_always_160575() {
    double FREQ_sh = FREQ.read();
    {
        const vl::logic<2> sel = vl::rd(corner);
        if (vl::case_eq(sel, vl::logic<2>(0x0ULL))) {
            FREQ_sh = vl::mux(vl::rd(en_res0_en_cap1_n), 51.02, 45.43);
            FREQ.write(FREQ_sh);
        } else if (vl::case_eq(sel, vl::logic<2>(0x1ULL))) {
            FREQ_sh = vl::mux(vl::rd(en_res0_en_cap1_n), 91.55, 82.06);
            FREQ.write(FREQ_sh);
        } else if (vl::case_eq(sel, vl::logic<2>(0x3ULL))) {
            FREQ_sh = vl::mux(vl::rd(en_res0_en_cap1_n), 94.16, 103.29);
            FREQ.write(FREQ_sh);
        } else {
            FREQ_sh = vl::mux(vl::rd(en_res0_en_cap1_n), 51.02, 82.06);
            FREQ.write(FREQ_sh);
        }
    }
}

void mipi_4_rx_dphy_ns_DPHYRESTUNE::proc_always_160584() {
    if (vl::ceq(vl::rd(supply_ok), vl::logic<1>(0x1ULL)).test()) {
        if (vl::ceq(vl::rd(pon_restune_hv), vl::logic<1>(0x1ULL)).test()) {
            { const vl::logic<1> v_ = ~vl::rd(clk2dig_r); aq_.after(vl::dly(((vl::to_real(vl::logic<32>(0x3e8ULL), true) / FREQ.read()) / vl::to_real(vl::logic<32>(0x2ULL), true)), TU_, TP_), [this, v_]() { vl::wr(clk2dig_r, v_); }); }
        } else {
            vl::wr(clk2dig_r, vl::logic<1>(0x0ULL));
        }
    } else {
        vl::wr(clk2dig_r, vl::logic<1>::all_z());
    }
}
