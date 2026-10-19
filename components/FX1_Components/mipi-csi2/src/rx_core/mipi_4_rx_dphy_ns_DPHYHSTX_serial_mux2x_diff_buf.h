//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSTX_serial_mux2x_diff_buf
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYHSTX_SERIAL_MUX2X_DIFF_BUF_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYHSTX_SERIAL_MUX2X_DIFF_BUF_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"

class mipi_4_rx_dphy_ns_DPHYHSTX_serial_mux2x_diff_buf : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_in<sc_dt::sc_logic> b{"b"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> sa{"sa"};
    sc_core::sc_in<sc_dt::sc_logic> sb{"sb"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    sc_core::sc_out<sc_dt::sc_logic> z{"z"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSTX_serial_mux2x_diff_buf);
    explicit mipi_4_rx_dphy_ns_DPHYHSTX_serial_mux2x_diff_buf(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal<sc_dt::sc_logic> a_bar{"a_bar", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_logic> b_bar{"b_bar", vl::logic<1>().sl()};
    sc_core::sc_signal_resolved supply_ok{"supply_ok", sc_dt::SC_LOGIC_Z};

    void assign_supply_ok();
    void proc_always_159007();
    void proc_always_159021();
    void proc_always_159035();
};

#endif
