//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_gen_sync_bus__reset_edge0_bus_width4 (parameter specialisation)
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_GEN_SYNC_BUS__RESET_EDGE0_BUS_WIDTH4_H
#define VL_MIPI_4_RX_DPHY_NS_GEN_SYNC_BUS__RESET_EDGE0_BUS_WIDTH4_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_4_rx_dphy_ns_DWbb_bcm41_scan__WIDTH4_RST_VAL0.h"

// Parameters: reset_value=0, reset_edge=0, bus_width=4, bus_reset=0, timing_viol_en=2
class mipi_4_rx_dphy_ns_gen_sync_bus__reset_edge0_bus_width4 : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> clk{"clk"};
    sc_core::sc_in<sc_dt::sc_logic> clk_inv{"clk_inv"};
    sc_core::sc_in<sc_dt::sc_logic> rst{"rst"};
    sc_core::sc_in<sc_dt::sc_lv<4>> data_in{"data_in"};
    sc_core::sc_out<sc_dt::sc_lv<4>> data_out{"data_out"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_gen_sync_bus__reset_edge0_bus_width4);
    explicit mipi_4_rx_dphy_ns_gen_sync_bus__reset_edge0_bus_width4(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000.0;  // time precision (fs)
    sc_core::sc_signal_resolved int_rst{"int_rst", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved data_stable{"data_stable", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<4> data_sync{"data_sync", vl::logic<4>::all_z().lv()};
    sc_core::sc_signal<sc_dt::sc_lv<4>> data_sync2{"data_sync2", vl::logic<4>().lv()};
    std::unique_ptr<mipi_4_rx_dphy_ns_DWbb_bcm41_scan__WIDTH4_RST_VAL0> U1;
    sc_core::sc_signal_resolved tie0_{"tie0_", sc_dt::SC_LOGIC_0};
    vl::tie tie0__drv{"tie0__drv", sc_dt::SC_LOGIC_0};

    void assign_int_rst();
    void assign_data_stable();
    void proc_always_813();
    void proc_always_823();
};

#endif
