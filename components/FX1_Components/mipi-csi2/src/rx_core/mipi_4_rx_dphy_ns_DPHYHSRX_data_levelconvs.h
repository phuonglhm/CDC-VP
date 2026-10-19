//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_data_levelconvs
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_DATA_LEVELCONVS_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_DATA_LEVELCONVS_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc.h"
#include "rx_cells.h"

class mipi_4_rx_dphy_ns_DPHYHSRX_data_levelconvs : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_lv<4>> atb_sw{"atb_sw"};
    sc_core::sc_in<sc_dt::sc_lv<3>> data_eq{"data_eq"};
    sc_core::sc_in<sc_dt::sc_logic> ena{"ena"};
    sc_core::sc_in<sc_dt::sc_logic> enlogic{"enlogic"};
    sc_core::sc_in<sc_dt::sc_logic> enterm{"enterm"};
    sc_core::sc_in<sc_dt::sc_logic> feed_int_clk{"feed_int_clk"};
    sc_core::sc_in<sc_dt::sc_lv<4>> lb_prog{"lb_prog"};
    sc_core::sc_in<sc_dt::sc_logic> lb_res_cm_en{"lb_res_cm_en"};
    sc_core::sc_in<sc_dt::sc_logic> lb_res_dn_en{"lb_res_dn_en"};
    sc_core::sc_in<sc_dt::sc_logic> lb_res_dp_en{"lb_res_dp_en"};
    sc_core::sc_in<sc_dt::sc_logic> offset_cal_en{"offset_cal_en"};
    sc_core::sc_in<sc_dt::sc_logic> pon{"pon"};
    sc_core::sc_out<sc_dt::sc_lv<4>> atb_swhv{"atb_swhv"};
    sc_core::sc_out<sc_dt::sc_lv<4>> atb_swhvz{"atb_swhvz"};
    sc_core::sc_out<sc_dt::sc_lv<3>> data_eq_hv{"data_eq_hv"};
    sc_core::sc_out<sc_dt::sc_lv<3>> data_eq_hvz{"data_eq_hvz"};
    sc_core::sc_out<sc_dt::sc_logic> enahv{"enahv"};
    sc_core::sc_out<sc_dt::sc_logic> enahvz{"enahvz"};
    sc_core::sc_out<sc_dt::sc_logic> enlogic_hv{"enlogic_hv"};
    sc_core::sc_out<sc_dt::sc_logic> entermhv{"entermhv"};
    sc_core::sc_out<sc_dt::sc_logic> entermhvz{"entermhvz"};
    sc_core::sc_out<sc_dt::sc_logic> feed_int_clk_hv{"feed_int_clk_hv"};
    sc_core::sc_out<sc_dt::sc_logic> feed_int_clk_hvz{"feed_int_clk_hvz"};
    sc_core::sc_out<sc_dt::sc_lv<4>> lb_prog_hv{"lb_prog_hv"};
    sc_core::sc_out<sc_dt::sc_logic> lb_res_cm_en_hv{"lb_res_cm_en_hv"};
    sc_core::sc_out<sc_dt::sc_logic> lb_res_dn_enz_hv{"lb_res_dn_enz_hv"};
    sc_core::sc_out<sc_dt::sc_logic> lb_res_dp_enz_hv{"lb_res_dp_enz_hv"};
    sc_core::sc_out<sc_dt::sc_logic> offset_cal_en_hv{"offset_cal_en_hv"};
    sc_core::sc_out<sc_dt::sc_logic> offset_cal_en_hvz{"offset_cal_en_hvz"};
    sc_core::sc_out<sc_dt::sc_logic> ponhv{"ponhv"};
    sc_core::sc_out<sc_dt::sc_logic> ponhvz{"ponhvz"};
    sc_core::sc_out<sc_dt::sc_logic> tie_high_hv{"tie_high_hv"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vdd{"vdd"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};
    sc_core::sc_in<sc_dt::sc_logic> vss{"vss"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_data_levelconvs);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_data_levelconvs(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_vector<sc_core::sc_signal_resolved> atb_sw_b{"atb_sw_b", 4, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> data_eq_b{"data_eq_b", 3, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> lb_prog_b{"lb_prog_b", 4, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> atb_swhv_b{"atb_swhv_b", 4, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> atb_swhvz_b{"atb_swhvz_b", 4, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> data_eq_hv_b{"data_eq_hv_b", 3, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> data_eq_hvz_b{"data_eq_hvz_b", 3, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> lb_prog_hv_b{"lb_prog_hv_b", 4, vl::make_z1};
    sc_core::sc_signal_resolved vblvhv{"vblvhv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_vector<sc_core::sc_signal_resolved> net160{"net160", 4, vl::make_z1};
    sc_core::sc_signal_resolved tie_low_hv{"tie_low_hv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved enlogic_hvz{"enlogic_hvz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved entermhv_int{"entermhv_int", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved lb_res_dn_en_hv{"lb_res_dn_en_hv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved lb_res_cm_enz_hv{"lb_res_cm_enz_hv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net190{"net190", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved lb_res_dp_en_hv{"lb_res_dp_en_hv", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_vp_det_soc> lv2hvctrl;
    std::unique_ptr<mipi_4_rx_dphy_ns_tieLowHigh_HV> I46;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_bias> ref;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc> I48_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc> I48_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc> I48_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc> I48_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc> I58_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc> I58_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc> I58_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc> I62_4;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc> I62_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc> I62_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc> I62_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc> I62_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc> I82_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc> I82_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc> I82_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc> I83_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc> I83_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc> I83_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc> I83_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_inv16x> I73;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_inv8x> I72;

    void split_atb_sw();
    void split_data_eq();
    void split_lb_prog();
    void merge_atb_swhv();
    void merge_atb_swhvz();
    void merge_data_eq_hv();
    void merge_data_eq_hvz();
    void merge_lb_prog_hv();
    vl::logic<4> rd_atb_sw() const;
    vl::logic<4> rd_atb_swhv() const;
    vl::logic<4> rd_atb_swhvz() const;
    vl::logic<3> rd_data_eq() const;
    vl::logic<3> rd_data_eq_hv() const;
    vl::logic<3> rd_data_eq_hvz() const;
    vl::logic<4> rd_lb_prog() const;
    vl::logic<4> rd_lb_prog_hv() const;
    vl::logic<4> rd_net160() const;
};

#endif
