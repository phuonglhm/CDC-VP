//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_lb_dirver
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_LB_DIRVER_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_LB_DIRVER_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "rx_cells.h"

class mipi_4_rx_dphy_ns_DPHYHSRX_lb_dirver : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> en_clklb{"en_clklb"};
    sc_core::sc_in<sc_dt::sc_logic> en_lb{"en_lb"};
    sc_core::sc_in<sc_dt::sc_logic> en_lb_h{"en_lb_h"};
    sc_core::sc_in<sc_dt::sc_logic> ibias_lb{"ibias_lb"};
    sc_core::sc_in<sc_dt::sc_logic> lb_clkmarg_m{"lb_clkmarg_m"};
    sc_core::sc_in<sc_dt::sc_logic> lb_clkmarg_p{"lb_clkmarg_p"};
    sc_core::sc_in<sc_dt::sc_logic> lb_marg_m{"lb_marg_m"};
    sc_core::sc_in<sc_dt::sc_logic> lb_marg_p{"lb_marg_p"};
    sc_core::sc_in<sc_dt::sc_logic> lbcal{"lbcal"};
    sc_core::sc_out<sc_dt::sc_logic> lb_outm{"lb_outm"};
    sc_core::sc_out<sc_dt::sc_logic> lb_outp{"lb_outp"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_lb_dirver);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_lb_dirver(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_rv<4> net78{"net78", vl::logic<4>::all_z().lv()};
    sc_core::sc_signal_rv<4> net67{"net67", vl::logic<4>::all_z().lv()};
    sc_core::sc_signal_resolved net19{"net19", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved en_lb_hz{"en_lb_hz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net21{"net21", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net23{"net23", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved pull_dwn{"pull_dwn", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net60{"net60", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net59{"net59", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net114{"net114", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved lb_marg_bufm{"lb_marg_bufm", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net61{"net61", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net25{"net25", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net62{"net62", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net45{"net45", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net27{"net27", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved lb_marg_bufp{"lb_marg_bufp", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net120{"net120", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved en_lbz{"en_lbz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved short_lbz{"short_lbz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved short_lb{"short_lb", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved long_lbz{"long_lbz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved long_lb{"long_lb", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net17{"net17", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net114_int{"net114_int", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net120_int{"net120_int", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved supply_ok{"supply_ok", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_inv1x> sctg_invlb;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_clk_mux_buf> I27;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_clk_mux_buf> I28;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_clk_mux_buf> I29;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_clk_mux_buf> I30;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> I33;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> I34;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> I41;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> I42;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x6_lvt> I35;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x6_lvt> I40;

    void assign_supply_ok();
    void gate_lb_outp_buf();
    void gate_lb_outm_buf();
    void gate_net114_buf();
    void gate_net114_buf2();
    void gate_lb_marg_m_buf0();
    void gate_lb_marg_m_buf1();
    void gate_net120_buf();
    void gate_net120_buf2();
    void gate_lb_marg_p_buf0();
    void gate_lb_marg_p_buf1();
};

#endif
