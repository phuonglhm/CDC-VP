//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DWbb_bcm41_scan__WIDTH1_RST_VAL0 (parameter specialisation)
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DWBB_BCM41_SCAN__WIDTH1_RST_VAL0_H
#define VL_MIPI_4_RX_DPHY_NS_DWBB_BCM41_SCAN__WIDTH1_RST_VAL0_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_4_rx_dphy_ns_DWbb_bcm21_scan__WIDTH1.h"

// Parameters: WIDTH=1, RST_VAL=0, F_SYNC_TYPE=2, VERIF_EN=0
class mipi_4_rx_dphy_ns_DWbb_bcm41_scan__WIDTH1_RST_VAL0 : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> clk_d{"clk_d"};
    // clock input from destination domain
    sc_core::sc_in<sc_dt::sc_logic> rst_d_n{"rst_d_n"};
    // active low asynchronous reset from destination domain
    sc_core::sc_in<sc_dt::sc_logic> data_s{"data_s"};
    // data to be synchronized from source domain
    sc_core::sc_out<sc_dt::sc_logic> data_d{"data_d"};
    // data synchronized to destination domain
    sc_core::sc_in<sc_dt::sc_logic> scanmode{"scanmode"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DWbb_bcm41_scan__WIDTH1_RST_VAL0);
    explicit mipi_4_rx_dphy_ns_DWbb_bcm41_scan__WIDTH1_RST_VAL0(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000.0;  // time precision (fs)
    sc_core::sc_signal_resolved data_s_int{"data_s_int", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved data_d_int{"data_d_int", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_DWbb_bcm21_scan__WIDTH1> U_SYNC;

    void assign_data_s_int();
    void assign_data_d();
};

#endif
