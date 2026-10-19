//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSTX_serial_mux2x_diff_buf
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYHSTX_serial_mux2x_diff_buf.h"

mipi_4_rx_dphy_ns_DPHYHSTX_serial_mux2x_diff_buf::mipi_4_rx_dphy_ns_DPHYHSTX_serial_mux2x_diff_buf(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(assign_supply_ok);
    sensitive << vp << gd;
    SC_METHOD(proc_always_159007);
    sensitive << supply_ok << a << sa;
    SC_METHOD(proc_always_159021);
    sensitive << supply_ok << b << sb;
    SC_METHOD(proc_always_159035);
    sensitive << supply_ok << a_bar << b_bar;
}

void mipi_4_rx_dphy_ns_DPHYHSTX_serial_mux2x_diff_buf::assign_supply_ok() {
    vl::wr(supply_ok, vl::land(vl::ceq(vl::rd(vp), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))));
}

void mipi_4_rx_dphy_ns_DPHYHSTX_serial_mux2x_diff_buf::proc_always_159007() {
    if (vl::rd(supply_ok).test()) {
        if (vl::rd(a).truth_val() == 0) {
            vl::wr(a_bar, vl::logic<1>(0x1ULL));
        } else {
            if (vl::rd(sa).test()) {
                vl::wr(a_bar, vl::logic<1>(0x0ULL));
            } else {
                vl::wr(a_bar, vl::logic<1>(0x1ULL));
            }
        }
    } else {
        vl::wr(a_bar, vl::logic<1>());
    }
}

void mipi_4_rx_dphy_ns_DPHYHSTX_serial_mux2x_diff_buf::proc_always_159021() {
    if (vl::rd(supply_ok).test()) {
        if (vl::rd(b).truth_val() == 0) {
            vl::wr(b_bar, vl::logic<1>(0x1ULL));
        } else {
            if (vl::rd(sb).test()) {
                vl::wr(b_bar, vl::logic<1>(0x0ULL));
            } else {
                vl::wr(b_bar, vl::logic<1>(0x1ULL));
            }
        }
    } else {
        vl::wr(b_bar, vl::logic<1>());
    }
}

void mipi_4_rx_dphy_ns_DPHYHSTX_serial_mux2x_diff_buf::proc_always_159035() {
    if (vl::rd(supply_ok).test()) {
        if (vl::rd(a_bar).test()) {
            vl::wr(z, ~vl::rd(b_bar));
        } else if (vl::rd(b_bar).test()) {
            vl::wr(z, ~vl::rd(a_bar));
        } else {
            vl::wr(z, vl::logic<1>());
        }
    } else {
        vl::wr(z, vl::logic<1>());
    }
}
