//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYLPTX_levelconvs
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYLPTX_LEVELCONVS_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYLPTX_LEVELCONVS_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc.h"
#include "rx_cells.h"

class mipi_4_rx_dphy_ns_DPHYLPTX_levelconvs : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> atb_lptx1200_on{"atb_lptx1200_on"};
    sc_core::sc_in<sc_dt::sc_lv<2>> din{"din"};
    sc_core::sc_in<sc_dt::sc_logic> ena{"ena"};
    sc_core::sc_in<sc_dt::sc_lv<2>> eniocont{"eniocont"};
    sc_core::sc_in<sc_dt::sc_logic> enlogic{"enlogic"};
    sc_core::sc_in<sc_dt::sc_logic> enpulldwn{"enpulldwn"};
    sc_core::sc_in<sc_dt::sc_logic> enulp{"enulp"};
    sc_core::sc_in<sc_dt::sc_logic> pon{"pon"};
    sc_core::sc_in<sc_dt::sc_logic> sr_bypass{"sr_bypass"};
    sc_core::sc_out<sc_dt::sc_logic> atb_lptx1200_on_hv{"atb_lptx1200_on_hv"};
    sc_core::sc_out<sc_dt::sc_logic> atb_lptx1200_on_hvz{"atb_lptx1200_on_hvz"};
    sc_core::sc_out<sc_dt::sc_lv<2>> dinhv{"dinhv"};
    sc_core::sc_out<sc_dt::sc_lv<2>> dinhvz{"dinhvz"};
    sc_core::sc_out<sc_dt::sc_logic> enahv{"enahv"};
    sc_core::sc_out<sc_dt::sc_logic> enahvz{"enahvz"};
    sc_core::sc_out<sc_dt::sc_lv<2>> enioconthv{"enioconthv"};
    sc_core::sc_out<sc_dt::sc_lv<2>> enioconthvz{"enioconthvz"};
    sc_core::sc_out<sc_dt::sc_logic> enpulldwnhv{"enpulldwnhv"};
    sc_core::sc_out<sc_dt::sc_logic> enpulldwnhvz{"enpulldwnhvz"};
    sc_core::sc_out<sc_dt::sc_logic> enulphv{"enulphv"};
    sc_core::sc_out<sc_dt::sc_logic> enulphvz{"enulphvz"};
    sc_core::sc_out<sc_dt::sc_logic> ponhv{"ponhv"};
    sc_core::sc_out<sc_dt::sc_logic> ponhvz{"ponhvz"};
    sc_core::sc_out<sc_dt::sc_logic> sr_bypasshv{"sr_bypasshv"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vdd{"vdd"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};
    sc_core::sc_in<sc_dt::sc_logic> vss{"vss"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYLPTX_levelconvs);
    explicit mipi_4_rx_dphy_ns_DPHYLPTX_levelconvs(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_vector<sc_core::sc_signal_resolved> din_b{"din_b", 2, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> eniocont_b{"eniocont_b", 2, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> dinhv_b{"dinhv_b", 2, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> dinhvz_b{"dinhvz_b", 2, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> enioconthv_b{"enioconthv_b", 2, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> enioconthvz_b{"enioconthvz_b", 2, vl::make_z1};
    sc_core::sc_signal_resolved tie_high_hv{"tie_high_hv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved vblvhv{"vblvhv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved enlogic_hvz{"enlogic_hvz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved tie_low_hv{"tie_low_hv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved enlogic_hv{"enlogic_hv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net126{"net126", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_bias> ref;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_vp_det_soc> lv2hvctrl_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_vp_det_soc> lv2hvctrl_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc> lv2hv_6;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc> lv2hv_5;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc> lv2hv_4;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc> lv2hv_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc> lv2hv_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc> lv2hv_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc> lv2hv_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc> lv2hv_7;
    std::unique_ptr<mipi_4_rx_dphy_ns_LEVELCONV_l2h_bias_soc> sr_bypas;
    std::unique_ptr<mipi_4_rx_dphy_ns_tieLowHigh_HV> I46;

    void split_din();
    void split_eniocont();
    void merge_dinhv();
    void merge_dinhvz();
    void merge_enioconthv();
    void merge_enioconthvz();
    vl::logic<2> rd_din() const;
    vl::logic<2> rd_dinhv() const;
    vl::logic<2> rd_dinhvz() const;
    vl::logic<2> rd_eniocont() const;
    vl::logic<2> rd_enioconthv() const;
    vl::logic<2> rd_enioconthvz() const;
};

#endif
