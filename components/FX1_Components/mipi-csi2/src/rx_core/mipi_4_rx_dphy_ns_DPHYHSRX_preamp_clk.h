//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_preamp_clk
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_PREAMP_CLK_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_PREAMP_CLK_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_lb_dirver.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_logic.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_preamp_clk_current_sources_hv.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_preamp_current_sources_lv.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_preamp_offset_cal.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_preamp_unit1.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_preamp_unit2.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_preamp_unit_noeq.h"
#include "rx_cells.h"

class mipi_4_rx_dphy_ns_DPHYHSRX_preamp_clk : public sc_core::sc_module {
public:
    sc_core::sc_inout<sc_dt::sc_logic> atb_line{"atb_line"};
    sc_core::sc_in<sc_dt::sc_lv<2>> atb_sw_hv{"atb_sw_hv"};
    sc_core::sc_in<sc_dt::sc_logic> bias_ihv{"bias_ihv"};
    sc_core::sc_in<sc_dt::sc_logic> bias_ilv_cc{"bias_ilv_cc"};
    sc_core::sc_in<sc_dt::sc_logic> bias_ilv_cgm{"bias_ilv_cgm"};
    sc_core::sc_in<sc_dt::sc_logic> en_clklb{"en_clklb"};
    sc_core::sc_in<sc_dt::sc_logic> en_lb{"en_lb"};
    sc_core::sc_in<sc_dt::sc_logic> en_lb_h{"en_lb_h"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_lv<2>> gmode{"gmode"};
    sc_core::sc_in<sc_dt::sc_lv<2>> gmode_h{"gmode_h"};
    sc_core::sc_in<sc_dt::sc_logic> lb_clkmarg_m{"lb_clkmarg_m"};
    sc_core::sc_in<sc_dt::sc_logic> lb_clkmarg_p{"lb_clkmarg_p"};
    sc_core::sc_in<sc_dt::sc_lv<4>> lb_current_h{"lb_current_h"};
    sc_core::sc_in<sc_dt::sc_logic> lb_marg_m{"lb_marg_m"};
    sc_core::sc_in<sc_dt::sc_logic> lb_marg_p{"lb_marg_p"};
    sc_core::sc_in<sc_dt::sc_lv<3>> rx_eq_bcal_h{"rx_eq_bcal_h"};
    sc_core::sc_in<sc_dt::sc_lv<6>> rx_hs_bcal{"rx_hs_bcal"};
    sc_core::sc_in<sc_dt::sc_logic> rx_hs_encal{"rx_hs_encal"};
    sc_core::sc_in<sc_dt::sc_logic> rx_hs_encal_h{"rx_hs_encal_h"};
    sc_core::sc_in<sc_dt::sc_logic> rx_pre_burst{"rx_pre_burst"};
    sc_core::sc_in<sc_dt::sc_logic> rx_pre_burst_h{"rx_pre_burst_h"};
    sc_core::sc_in<sc_dt::sc_logic> rx_pre_pon{"rx_pre_pon"};
    sc_core::sc_in<sc_dt::sc_logic> rx_pre_pon_h{"rx_pre_pon_h"};
    sc_core::sc_in<sc_dt::sc_logic> tie_high{"tie_high"};
    sc_core::sc_in<sc_dt::sc_logic> tie_low{"tie_low"};
    sc_core::sc_in<sc_dt::sc_logic> vi_m{"vi_m"};
    sc_core::sc_in<sc_dt::sc_logic> vi_p{"vi_p"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};
    sc_core::sc_out<sc_dt::sc_logic> vpre_m{"vpre_m"};
    sc_core::sc_out<sc_dt::sc_logic> vpre_p{"vpre_p"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_preamp_clk);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_preamp_clk(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_vector<sc_core::sc_signal_resolved> keyhvz{"keyhvz", 4, vl::make_z1};
    sc_core::sc_signal_rv<3> rx_eq_bcal_hz{"rx_eq_bcal_hz", vl::logic<3>::all_z().lv()};
    sc_core::sc_signal_rv<4> key{"key", vl::logic<4>::all_z().lv()};
    sc_core::sc_signal_resolved lbcalsw{"lbcalsw", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved calsw_h{"calsw_h", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved ibias_noeq{"ibias_noeq", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved vcaln{"vcaln", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved vcalp{"vcalp", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved vi_p_prot{"vi_p_prot", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<2> atb_sw_hvz{"atb_sw_hvz", vl::logic<2>::all_z().lv()};
    sc_core::sc_signal_resolved vi_m_prot{"vi_m_prot", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<6> rx_hs_bcalz{"rx_hs_bcalz", vl::logic<6>::all_z().lv()};
    sc_core::sc_signal_rv<4> keyz{"keyz", vl::logic<4>::all_z().lv()};
    sc_core::sc_signal_resolved ibias_cal{"ibias_cal", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved ibias1{"ibias1", sc_dt::SC_LOGIC_Z};
    sc_core::sc_vector<sc_core::sc_signal_resolved> keyhv{"keyhv", 4, vl::make_z1};
    sc_core::sc_signal_resolved ibias2{"ibias2", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved voutn0{"voutn0", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved voutn1{"voutn1", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved calswn_h{"calswn_h", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved vp_bias{"vp_bias", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved ibias_lb{"ibias_lb", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved rx_eq_bcal_bypass_h{"rx_eq_bcal_bypass_h", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved en_lb_hz{"en_lb_hz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<4> lbkeyhv{"lbkeyhv", vl::logic<4>::all_z().lv()};
    sc_core::sc_signal_resolved rx_pre_pon_hz{"rx_pre_pon_hz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved voutp0{"voutp0", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved voutp1{"voutp1", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved en_lbz{"en_lbz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved rx_pre_ponz{"rx_pre_ponz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved supply_ok_offset_path{"supply_ok_offset_path", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved voutn0_int{"voutn0_int", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved voutp0_int{"voutp0_int", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_offset_cal> OFFSET_CAL;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_current_sources_lv> CS_LV;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_lb_dirver> LB_DRV;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_unit2> I127;
    sc_core::sc_signal_rv<4> c_I127_g{"c_I127_g", vl::logic<4>::all_z().lv()};
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_logic> LOGIC;
    sc_core::sc_signal_rv<4> c_LOGIC_keyhv{"c_LOGIC_keyhv", vl::logic<4>::all_z().lv()};
    sc_core::sc_signal_rv<4> c_LOGIC_keyhvz{"c_LOGIC_keyhvz", vl::logic<4>::all_z().lv()};
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_unit1> UNIT1;
    sc_core::sc_signal_rv<4> c_UNIT1_g{"c_UNIT1_g", vl::logic<4>::all_z().lv()};
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_protsw> PROTSW;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_clk_current_sources_hv> CS_HV;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_unit_noeq> EQUALIZER;

    vl::logic<4> rd_keyhv() const;
    vl::logic<4> rd_keyhvz() const;
    void assign_supply_ok_offset_path();
    void assign_vp_bias();
    void gate_voutp0_buf();
    void gate_voutn0_buf();
    void gate_bufvoutp0();
    void gate_bufvoutn0();
    void glue_I127_g();
    void split_LOGIC_keyhv();
    void split_LOGIC_keyhvz();
    void glue_UNIT1_g();
};

#endif
