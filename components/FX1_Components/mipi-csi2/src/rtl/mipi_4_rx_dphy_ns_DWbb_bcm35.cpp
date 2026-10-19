//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DWbb_bcm35
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DWbb_bcm35.h"

mipi_4_rx_dphy_ns_DWbb_bcm35::mipi_4_rx_dphy_ns_DWbb_bcm35(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(assign_next_captr_val_cc);
    sensitive << sync_captr;
    SC_METHOD(assign_pulse_captr_s);
    sensitive << sample_sync_spcl;
    SC_METHOD(assign_pulse_captr_cc);
    sensitive << pulse_captr_s;
    SC_METHOD(assign_detect);
    sensitive << sync_captr << hist;
    SC_METHOD(assign_event_d);
    sensitive << detect_q << detect;
    U_SYNC.reset(new mipi_4_rx_dphy_ns_DWbb_bcm21("U_SYNC"));
    U_SYNC->clk_d(clk_d);
    U_SYNC->rst_d_n(rst_d_n);
    U_SYNC->data_s(pulse_captr_cc);
    U_SYNC->data_d(sync_captr);
    SC_METHOD(proc_always_2588);
    sensitive << clk_s.pos() << rst_d_n.neg();
    dont_initialize();
    SC_METHOD(proc_always_2642);
    sensitive << clk_d.pos() << rst_d_n.neg();
    dont_initialize();
}

// actual net name of register uses the 'sample_sync' prefix which
// mipi_4_rx_dphy_ns_will be used to find sync flip-flops in bcm21 modules.  This
// allows the campture FF to be swapped out with a metstability
// hardened flop along with the bcm21 flops.  But for readability
// a macro will be used to 'rename' the net
void mipi_4_rx_dphy_ns_DWbb_bcm35::assign_next_captr_val_cc() {
    vl::wr(next_captr_val_cc, ~vl::rd(sync_captr));
}

void mipi_4_rx_dphy_ns_DWbb_bcm35::assign_pulse_captr_s() {
    vl::wr(pulse_captr_s, vl::rd(sample_sync_spcl));
}

void mipi_4_rx_dphy_ns_DWbb_bcm35::assign_pulse_captr_cc() {
    vl::wr(pulse_captr_cc, vl::rd(pulse_captr_s));
}

void mipi_4_rx_dphy_ns_DWbb_bcm35::assign_detect() {
    vl::wr(detect, vl::rd(sync_captr) ^ vl::rd(hist));
}

void mipi_4_rx_dphy_ns_DWbb_bcm35::assign_event_d() {
    vl::wr(event_d, vl::mux(vl::logic<1>(0x0ULL), vl::rd(detect_q), vl::rd(detect)));
}

void mipi_4_rx_dphy_ns_DWbb_bcm35::proc_always_2588() {
    if (vl::eq(vl::rd(rst_d_n), vl::logic<1>(0x0ULL)).test()) {
        vl::wr(sample_sync_spcl, vl::logic<1>(0x0ULL));
    } else {
        vl::wr(sample_sync_spcl, vl::rd(next_captr_val_cc));
    }
}

// mipi_4_rx_dphy_ns_LMD: 2 clocks are used in module
void mipi_4_rx_dphy_ns_DWbb_bcm35::proc_always_2642() {
    if (vl::eq(vl::rd(rst_d_n), vl::logic<1>(0x0ULL)).test()) {
        vl::wr(hist, vl::logic<1>(0x0ULL));
        vl::wr(detect_q, vl::logic<1>(0x0ULL));
    } else {
        vl::wr(hist, vl::rd(sync_captr));
        vl::wr(detect_q, vl::rd(detect));
    }
}
