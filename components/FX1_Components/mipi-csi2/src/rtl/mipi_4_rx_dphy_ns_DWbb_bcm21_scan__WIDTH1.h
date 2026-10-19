//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DWbb_bcm21_scan__WIDTH1 (parameter specialisation)
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DWBB_BCM21_SCAN__WIDTH1_H
#define VL_MIPI_4_RX_DPHY_NS_DWBB_BCM21_SCAN__WIDTH1_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"

// Parameters: WIDTH=1, F_SYNC_TYPE=10, VERIF_EN=0
class mipi_4_rx_dphy_ns_DWbb_bcm21_scan__WIDTH1 : public sc_core::sc_module {
public:
    // RANGE 0 to 5
    sc_core::sc_in<sc_dt::sc_logic> clk_d{"clk_d"};
    // clock input from destination domain
    sc_core::sc_in<sc_dt::sc_logic> rst_d_n{"rst_d_n"};
    // active low asynchronous reset from destination domain
    sc_core::sc_in<sc_dt::sc_logic> data_s{"data_s"};
    // data to be synchronized from source domain
    sc_core::sc_out<sc_dt::sc_logic> data_d{"data_d"};
    // data synchronized to destination domain
    sc_core::sc_in<sc_dt::sc_logic> scanmode{"scanmode"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DWbb_bcm21_scan__WIDTH1);
    explicit mipi_4_rx_dphy_ns_DWbb_bcm21_scan__WIDTH1(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000.0;  // time precision (fs)
    sc_core::sc_signal_resolved clk_d_scanable{"clk_d_scanable", sc_dt::SC_LOGIC_Z};
    vl::logic<1> sample_meta_n;
    sc_core::sc_signal<sc_dt::sc_logic> sample_meta{"sample_meta", vl::logic<1>().sl()};
    vl::logic<1> sample_syncm1;
    vl::logic<1> sample_syncm2;
    sc_core::sc_signal<sc_dt::sc_logic> sample_syncl{"sample_syncl", vl::logic<1>().sl()};
    sc_core::sc_signal_resolved next_sample_meta{"next_sample_meta", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved next_sample_syncm1{"next_sample_syncm1", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved next_sample_syncm2{"next_sample_syncm2", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved next_sample_syncl{"next_sample_syncl", sc_dt::SC_LOGIC_Z};
    vl::logic<32> parameter_check_param_err_flg;

    void assign_clk_d_scanable();
    void assign_next_sample_syncm1();
    void assign_next_sample_meta();
    void assign_next_sample_syncl();
    void assign_data_d();
    void proc_initial_3774();
    void proc_initial_3816();
    void parameter_check();
    void proc_always_4295();
};

#endif
