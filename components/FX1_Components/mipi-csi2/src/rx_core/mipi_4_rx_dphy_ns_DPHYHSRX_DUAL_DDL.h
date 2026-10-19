//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_DUAL_DDL
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_DUAL_DDL_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_DUAL_DDL_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_DDL.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_DDL_4bit.h"

class mipi_4_rx_dphy_ns_DPHYHSRX_DUAL_DDL : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_lv<4>> atb_sw_hv{"atb_sw_hv"};
    sc_core::sc_in<sc_dt::sc_logic> clk_inn{"clk_inn"};
    sc_core::sc_in<sc_dt::sc_logic> clk_inp{"clk_inp"};
    sc_core::sc_in<sc_dt::sc_logic> data_inn{"data_inn"};
    sc_core::sc_in<sc_dt::sc_logic> data_inp{"data_inp"};
    sc_core::sc_in<sc_dt::sc_logic> ddl_clk_on{"ddl_clk_on"};
    sc_core::sc_in<sc_dt::sc_logic> ddl_clk_on_hv{"ddl_clk_on_hv"};
    sc_core::sc_in<sc_dt::sc_logic> ddl_clk_onz{"ddl_clk_onz"};
    sc_core::sc_in<sc_dt::sc_logic> ddl_data_on{"ddl_data_on"};
    sc_core::sc_in<sc_dt::sc_logic> ddl_data_onz{"ddl_data_onz"};
    sc_core::sc_in<sc_dt::sc_logic> gate{"gate"};
    sc_core::sc_in<sc_dt::sc_logic> gatez{"gatez"};
    sc_core::sc_in<sc_dt::sc_logic> iref{"iref"};
    sc_core::sc_in<sc_dt::sc_lv<5>> phase_clk{"phase_clk"};
    sc_core::sc_in<sc_dt::sc_lv<4>> phase_data{"phase_data"};
    sc_core::sc_in<sc_dt::sc_logic> phase_sync{"phase_sync"};
    sc_core::sc_in<sc_dt::sc_lv<5>> tune{"tune"};
    sc_core::sc_in<sc_dt::sc_logic> tune_en{"tune_en"};
    sc_core::sc_out<sc_dt::sc_logic> clk_outn{"clk_outn"};
    sc_core::sc_out<sc_dt::sc_logic> clk_outp{"clk_outp"};
    sc_core::sc_out<sc_dt::sc_logic> data_outn{"data_outn"};
    sc_core::sc_out<sc_dt::sc_logic> data_outp{"data_outp"};
    sc_core::sc_out<sc_dt::sc_logic> ddl_clk_outn{"ddl_clk_outn"};
    sc_core::sc_out<sc_dt::sc_logic> ddl_clk_outp{"ddl_clk_outp"};
    sc_core::sc_inout<sc_dt::sc_logic> atb_line{"atb_line"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_DUAL_DDL);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_DUAL_DDL(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_resolved bn{"bn", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved bp{"bp", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net120{"net120", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net119{"net119", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_DDL> DDL_CLK;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_DDL_4bit> DDL_DATA;

    void proc_always_156553();
};

#endif
