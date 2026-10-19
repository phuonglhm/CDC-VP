//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYLPTX
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYLPTX_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYLPTX_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_4_rx_dphy_ns_DPHYLPTX_oloop.h"

class mipi_4_rx_dphy_ns_DPHYLPTX : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> atb_line{"atb_line"};
    sc_core::sc_in<sc_dt::sc_logic> atb_lptx1200_on{"atb_lptx1200_on"};
    sc_core::sc_in<sc_dt::sc_lv<2>> din{"din"};
    sc_core::sc_in<sc_dt::sc_logic> ena{"ena"};
    sc_core::sc_in<sc_dt::sc_lv<2>> eniocont{"eniocont"};
    sc_core::sc_in<sc_dt::sc_logic> enlogic{"enlogic"};
    sc_core::sc_in<sc_dt::sc_logic> enpulldwn{"enpulldwn"};
    sc_core::sc_in<sc_dt::sc_logic> enulp{"enulp"};
    sc_core::sc_in<sc_dt::sc_logic> ibias{"ibias"};
    sc_core::sc_in<sc_dt::sc_logic> pon{"pon"};
    sc_core::sc_in<sc_dt::sc_lv<3>> proghv{"proghv"};
    sc_core::sc_in<sc_dt::sc_lv<3>> setrhv{"setrhv"};
    sc_core::sc_in<sc_dt::sc_logic> sr_bypass{"sr_bypass"};
    sc_core::sc_out<sc_dt::sc_logic> dn{"dn"};
    sc_core::sc_out<sc_dt::sc_logic> dp{"dp"};
    sc_core::sc_out<sc_dt::sc_lv<2>> iocont{"iocont"};
    sc_core::sc_inout<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vdd{"vdd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};
    sc_core::sc_inout<sc_dt::sc_logic> vreg{"vreg"};
    sc_core::sc_in<sc_dt::sc_logic> vss{"vss"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYLPTX);
    explicit mipi_4_rx_dphy_ns_DPHYLPTX(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_resolved net111{"net111", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net112{"net112", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_oloop> DRVOLOOP;
};

#endif
