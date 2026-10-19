//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DWbb_bcm21_scan__WIDTH4
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DWbb_bcm21_scan__WIDTH4.h"

mipi_4_rx_dphy_ns_DWbb_bcm21_scan__WIDTH4::mipi_4_rx_dphy_ns_DWbb_bcm21_scan__WIDTH4(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(assign_clk_d_scanable);
    sensitive << scanmode << clk_d;
    SC_METHOD(assign_next_sample_syncm1);
    sensitive << sample_meta;
    SC_METHOD(assign_next_sample_meta);
    sensitive << data_s;
    SC_METHOD(assign_next_sample_syncl);
    sensitive << next_sample_syncm1;
    SC_METHOD(assign_data_d);
    sensitive << sample_syncl;
    SC_METHOD(proc_initial_3774);
    SC_METHOD(proc_initial_3816);
    SC_METHOD(parameter_check);
    SC_METHOD(proc_always_4295);
    sensitive << clk_d.pos() << rst_d_n.neg();
    dont_initialize();
}

void mipi_4_rx_dphy_ns_DWbb_bcm21_scan__WIDTH4::assign_clk_d_scanable() {
    vl::wr(clk_d_scanable, vl::mux(vl::rd(scanmode), ~vl::rd(clk_d), vl::rd(clk_d)));
}

void mipi_4_rx_dphy_ns_DWbb_bcm21_scan__WIDTH4::assign_next_sample_syncm1() {
    vl::wr(next_sample_syncm1, vl::rd(sample_meta));
}

// parameter_check
void mipi_4_rx_dphy_ns_DWbb_bcm21_scan__WIDTH4::assign_next_sample_meta() {
    vl::wr(next_sample_meta, vl::rd(data_s));
}

void mipi_4_rx_dphy_ns_DWbb_bcm21_scan__WIDTH4::assign_next_sample_syncl() {
    vl::wr(next_sample_syncl, vl::rd(next_sample_syncm1));
}

void mipi_4_rx_dphy_ns_DWbb_bcm21_scan__WIDTH4::assign_data_d() {
    vl::wr(data_d, vl::rd(sample_syncl));
}

void mipi_4_rx_dphy_ns_DWbb_bcm21_scan__WIDTH4::proc_initial_3774() {
    if (vl::logic<1>(0x0ULL).test()) {
        vl::display(this, TU_, true, "Information: mipi_4_rx_dphy_ns_*** Instance %m module is using the <Double Register Synchronizer (1)> Clock Domain Crossing Method ***", {});
    }
}

void mipi_4_rx_dphy_ns_DWbb_bcm21_scan__WIDTH4::proc_initial_3816() {
    if (vl::logic<1>(0x0ULL).test()) {
        vl::display(this, TU_, true, " ", {});
        vl::display(this, TU_, true, " * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *", {});
        vl::display(this, TU_, true, "ATTENTION - %m - Module intentionally using NEGATIVE EDGE REGISTERS! <<----", {});
        vl::display(this, TU_, true, "This happened because this block is configured with F_SYNC_TYPE set to 1.", {});
        vl::display(this, TU_, true, " * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *", {});
        vl::display(this, TU_, true, " ", {});
    }
}

void mipi_4_rx_dphy_ns_DWbb_bcm21_scan__WIDTH4::parameter_check() {
    parameter_check_param_err_flg = vl::logic<32>(0x0ULL);
    if (vl::logic<1>(0x0ULL).test()) {
        parameter_check_param_err_flg = vl::logic<32>(0x1ULL);
        vl::display(this, TU_, true, "ERROR: %m :\n  Invalid value (%d) for parameter WIDTH (lower bound: 1 )", {vl::A(vl::logic<32>(0x4ULL), true)});
    }
    if (vl::logic<1>(0x0ULL).test()) {
        parameter_check_param_err_flg = vl::logic<32>(0x1ULL);
        vl::display(this, TU_, true, "ERROR: %m :\n  Invalid value (%d) for parameter (F_SYNC_TYPE & 7) (legal range: 0 to 4)", {vl::A(vl::logic<32>(0x2ULL), true)});
    }
    if (vl::logic<1>(0x0ULL).test()) {
        parameter_check_param_err_flg = vl::logic<32>(0x1ULL);
        vl::display(this, TU_, true, "ERROR: %m :\n  Invalid value (%d) for parameter VERIF_EN (legal range: 0 to 5)", {vl::A(vl::logic<32>(0x0ULL), true)});
    }
    if (vl::logic<1>(0x0ULL).test()) {
        parameter_check_param_err_flg = vl::logic<32>(0x1ULL);
        vl::display(this, TU_, true, "ERROR: %m : \n  When VERIF_EN is '5', then F_SYNC_TYPE must be '2'.", {});
    }
    // VCS coverage off
    if (vl::eq(parameter_check_param_err_flg, vl::logic<32>(0x1ULL)).test()) {
        vl::display(this, TU_, true, "%m :\n  Simulation aborted due to invalid parameter value(s)", {});
        vl::finish();
    }
}

void mipi_4_rx_dphy_ns_DWbb_bcm21_scan__WIDTH4::proc_always_4295() {
    if (vl::eq(vl::rd(rst_d_n), vl::logic<1>(0x0ULL)).test()) {
        vl::wr(sample_meta, vl::logic<4>(0x0ULL));
        vl::wr(sample_syncl, vl::logic<4>(0x0ULL));
    } else {
        vl::wr(sample_meta, vl::rd(next_sample_meta));
        vl::wr(sample_syncl, vl::rd(next_sample_syncl));
    }
}
