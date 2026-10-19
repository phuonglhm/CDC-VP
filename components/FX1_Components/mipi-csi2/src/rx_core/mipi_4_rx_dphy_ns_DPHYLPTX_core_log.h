//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYLPTX_core_log
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYLPTX_CORE_LOG_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYLPTX_CORE_LOG_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "rx_cells.h"

class mipi_4_rx_dphy_ns_DPHYLPTX_core_log : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> din{"din"};
    sc_core::sc_in<sc_dt::sc_logic> dinz{"dinz"};
    sc_core::sc_in<sc_dt::sc_logic> enulp{"enulp"};
    sc_core::sc_in<sc_dt::sc_logic> onb{"onb"};
    sc_core::sc_out<sc_dt::sc_logic> enn{"enn"};
    sc_core::sc_out<sc_dt::sc_logic> enp{"enp"};
    sc_core::sc_out<sc_dt::sc_logic> enpullupz{"enpullupz"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYLPTX_core_log);
    explicit mipi_4_rx_dphy_ns_DPHYLPTX_core_log(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_resolved net1503{"net1503", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net1496{"net1496", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved ennz{"ennz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved enpz{"enpz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net1481{"net1481", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net1485{"net1485", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net1486{"net1486", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net1502{"net1502", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_nand2x> sctg_I270;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_nand2x> sctg_I271;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_nand2x> sctg_I272;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_inv1x> sctg_I267;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_inv1x> sctg_I273;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_inv3x> sctg_I268;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_inv3x> sctg_I275;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_inv9x> sctg_I269;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_inv9x> sctg_I274;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_inv9x> sctg_I276;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_inv9x> sctg_I277;
};

#endif
