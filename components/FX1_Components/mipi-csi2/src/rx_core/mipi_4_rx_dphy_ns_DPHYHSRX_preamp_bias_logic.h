//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_logic
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_PREAMP_BIAS_LOGIC_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_PREAMP_BIAS_LOGIC_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "rx_cells.h"

class mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_logic : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_lv<2>> atb_sw_hv{"atb_sw_hv"};
    sc_core::sc_in<sc_dt::sc_logic> en_lb{"en_lb"};
    sc_core::sc_in<sc_dt::sc_logic> en_lb_h{"en_lb_h"};
    sc_core::sc_in<sc_dt::sc_lv<2>> gear{"gear"};
    sc_core::sc_in<sc_dt::sc_lv<2>> gearhv{"gearhv"};
    sc_core::sc_in<sc_dt::sc_lv<4>> lb_current_h{"lb_current_h"};
    sc_core::sc_in<sc_dt::sc_logic> on{"on"};
    sc_core::sc_in<sc_dt::sc_logic> onhv{"onhv"};
    sc_core::sc_in<sc_dt::sc_lv<3>> rx_eq_bcal_h{"rx_eq_bcal_h"};
    sc_core::sc_in<sc_dt::sc_lv<6>> rx_hs_bcal{"rx_hs_bcal"};
    sc_core::sc_in<sc_dt::sc_logic> rx_hs_encal{"rx_hs_encal"};
    sc_core::sc_in<sc_dt::sc_logic> rx_hs_encal_h{"rx_hs_encal_h"};
    sc_core::sc_in<sc_dt::sc_logic> rx_pre_burst{"rx_pre_burst"};
    sc_core::sc_in<sc_dt::sc_logic> rx_pre_bursthv{"rx_pre_bursthv"};
    sc_core::sc_out<sc_dt::sc_lv<2>> atb_sw_hvz{"atb_sw_hvz"};
    sc_core::sc_out<sc_dt::sc_logic> calsw_h{"calsw_h"};
    sc_core::sc_out<sc_dt::sc_logic> calswn_h{"calswn_h"};
    sc_core::sc_out<sc_dt::sc_logic> en_lb_hz{"en_lb_hz"};
    sc_core::sc_out<sc_dt::sc_logic> en_lbz{"en_lbz"};
    sc_core::sc_out<sc_dt::sc_lv<4>> key{"key"};
    sc_core::sc_out<sc_dt::sc_lv<4>> keyhv{"keyhv"};
    sc_core::sc_out<sc_dt::sc_lv<4>> keyhvz{"keyhvz"};
    sc_core::sc_out<sc_dt::sc_lv<4>> keyz{"keyz"};
    sc_core::sc_out<sc_dt::sc_logic> lbcalsw{"lbcalsw"};
    sc_core::sc_out<sc_dt::sc_lv<4>> lbkeyhv{"lbkeyhv"};
    sc_core::sc_out<sc_dt::sc_logic> onhvz{"onhvz"};
    sc_core::sc_out<sc_dt::sc_logic> onz{"onz"};
    sc_core::sc_out<sc_dt::sc_logic> rx_eq_bcal_bypass_h{"rx_eq_bcal_bypass_h"};
    sc_core::sc_out<sc_dt::sc_lv<3>> rx_eq_bcal_hz{"rx_eq_bcal_hz"};
    sc_core::sc_out<sc_dt::sc_lv<6>> rx_hs_bcalz{"rx_hs_bcalz"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_logic);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_preamp_bias_logic(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_vector<sc_core::sc_signal_resolved> atb_sw_hv_b{"atb_sw_hv_b", 2, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> gear_b{"gear_b", 2, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> gearhv_b{"gearhv_b", 2, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> lb_current_h_b{"lb_current_h_b", 4, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> rx_eq_bcal_h_b{"rx_eq_bcal_h_b", 3, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> rx_hs_bcal_b{"rx_hs_bcal_b", 6, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> atb_sw_hvz_b{"atb_sw_hvz_b", 2, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> key_b{"key_b", 4, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> keyhv_b{"keyhv_b", 4, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> keyhvz_b{"keyhvz_b", 4, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> keyz_b{"keyz_b", 4, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> lbkeyhv_b{"lbkeyhv_b", 4, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> rx_eq_bcal_hz_b{"rx_eq_bcal_hz_b", 3, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> rx_hs_bcalz_b{"rx_hs_bcalz_b", 6, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> keyxhvz{"keyxhvz", 3, vl::make_z1};
    sc_core::sc_signal_resolved net155{"net155", sc_dt::SC_LOGIC_Z};
    sc_core::sc_vector<sc_core::sc_signal_resolved> keyxz{"keyxz", 3, vl::make_z1};
    sc_core::sc_signal_resolved net64{"net64", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net67{"net67", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net179{"net179", sc_dt::SC_LOGIC_Z};
    sc_core::sc_vector<sc_core::sc_signal_resolved> keyxhv{"keyxhv", 3, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> keyx{"keyx", 3, vl::make_z1};
    sc_core::sc_signal_resolved encal_z{"encal_z", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na2x6_lvt> I69;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na2x6_lvt> sc_lvt_I30;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na2x6_lvt> sc_lvt_I33_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na2x6_lvt> sc_lvt_I33_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na2x6_lvt> sc_lvt_I33_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na2x6_lvt> sc_lvt_NAND0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_inv1x> sctg_I28;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_inv1x> sctg_I42;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_inv1x> sctg_I43_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_inv1x> sctg_I43_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_inv1x> sctg_I43_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_inv1x> sctg_I56_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_inv1x> sctg_I56_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_inv1x> sctg_I56_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_inv1x> sctg_I58;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_inv1x> sctg_INVH0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_inv1x> sctg_INVH1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_inv1x> sctg_INVH5;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_inv1x> sctg_INVH6_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_inv1x> sctg_INVH6_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_inv1x> sctg_INVH6_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_inv1x> sctg_INVH7_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_inv1x> sctg_INVH7_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> I70;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> sc_lvt_I31;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> sc_lvt_I34_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> sc_lvt_I34_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> sc_lvt_I34_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> sc_lvt_I57_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> sc_lvt_I57_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> sc_lvt_I57_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> sc_lvt_INV0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> sc_lvt_INV1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> sc_lvt_INV4_5;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> sc_lvt_INV4_4;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> sc_lvt_INV4_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> sc_lvt_INV4_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> sc_lvt_INV4_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> sc_lvt_INV4_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> sc_lvt_INV5;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x6_lvt> sc_lvt_I32;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_nor1x> sctg_I29;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_nor1x> sctg_NORH1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_nand4x> sctg_I40;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_nand4x> sctg_I44_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_nand4x> sctg_I44_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_nand4x> sctg_I44_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_nand4x> sctg_I59;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_nand4x> sctg_I65_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_nand4x> sctg_I65_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_nand4x> sctg_I65_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_nand4x> sctg_I65_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_nand4x> sctg_NANDH0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_nor4x> sctg_I41;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_nor4x> sctg_NORH0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_nor4x> scyg_I64;

    void split_atb_sw_hv();
    void split_gear();
    void split_gearhv();
    void split_lb_current_h();
    void split_rx_eq_bcal_h();
    void split_rx_hs_bcal();
    void merge_atb_sw_hvz();
    void merge_key();
    void merge_keyhv();
    void merge_keyhvz();
    void merge_keyz();
    void merge_lbkeyhv();
    void merge_rx_eq_bcal_hz();
    void merge_rx_hs_bcalz();
    vl::logic<2> rd_atb_sw_hv() const;
    vl::logic<2> rd_atb_sw_hvz() const;
    vl::logic<2> rd_gear() const;
    vl::logic<2> rd_gearhv() const;
    vl::logic<4> rd_key() const;
    vl::logic<4> rd_keyhv() const;
    vl::logic<4> rd_keyhvz() const;
    vl::logic<3> rd_keyx() const;
    vl::logic<3> rd_keyxhv() const;
    vl::logic<3> rd_keyxhvz() const;
    vl::logic<3> rd_keyxz() const;
    vl::logic<4> rd_keyz() const;
    vl::logic<4> rd_lb_current_h() const;
    vl::logic<4> rd_lbkeyhv() const;
    vl::logic<3> rd_rx_eq_bcal_h() const;
    vl::logic<3> rd_rx_eq_bcal_hz() const;
    vl::logic<6> rd_rx_hs_bcal() const;
    vl::logic<6> rd_rx_hs_bcalz() const;
};

#endif
