//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYLPTX_oloop
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYLPTX_OLOOP_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYLPTX_OLOOP_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_4_rx_dphy_ns_DPHYLPTX_bias.h"
#include "mipi_4_rx_dphy_ns_DPHYLPTX_core.h"
#include "mipi_4_rx_dphy_ns_DPHYLPTX_levelconvs.h"
#include "mipi_4_rx_dphy_ns_DPHYLPTX_log.h"
#include "mipi_4_rx_dphy_ns_DPHY_iocont_buffer.h"
#include "rx_cells.h"

class mipi_4_rx_dphy_ns_DPHYLPTX_oloop : public sc_core::sc_module {
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
    sc_core::sc_in<sc_dt::sc_logic> loopinn{"loopinn"};
    sc_core::sc_in<sc_dt::sc_logic> loopinp{"loopinp"};
    sc_core::sc_in<sc_dt::sc_logic> pon{"pon"};
    sc_core::sc_in<sc_dt::sc_lv<3>> proghv{"proghv"};
    sc_core::sc_in<sc_dt::sc_lv<3>> setrhv{"setrhv"};
    sc_core::sc_in<sc_dt::sc_logic> sr_bypass{"sr_bypass"};
    sc_core::sc_out<sc_dt::sc_logic> dn{"dn"};
    sc_core::sc_out<sc_dt::sc_logic> dp{"dp"};
    sc_core::sc_out<sc_dt::sc_lv<2>> iocont{"iocont"};
    sc_core::sc_out<sc_dt::sc_logic> loopoutn{"loopoutn"};
    sc_core::sc_out<sc_dt::sc_logic> loopoutp{"loopoutp"};
    sc_core::sc_inout<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vdd{"vdd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};
    sc_core::sc_inout<sc_dt::sc_logic> vreg{"vreg"};
    sc_core::sc_in<sc_dt::sc_logic> vss{"vss"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYLPTX_oloop);
    explicit mipi_4_rx_dphy_ns_DPHYLPTX_oloop(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_vector<sc_core::sc_signal_resolved> iocont_b{"iocont_b", 2, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> enioconthv{"enioconthv", 2, vl::make_z1};
    sc_core::sc_signal_resolved tie_high{"tie_high", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved ponhvz{"ponhvz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_vector<sc_core::sc_signal_resolved> dinhvz{"dinhvz", 2, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> dinhv{"dinhv", 2, vl::make_z1};
    sc_core::sc_signal_resolved cascn{"cascn", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<3> setrz{"setrz", vl::logic<3>::all_z().lv()};
    sc_core::sc_signal_resolved cascp{"cascp", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved biasn_fb{"biasn_fb", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved biasn{"biasn", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved onbhvz{"onbhvz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved biasp{"biasp", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved enahv{"enahv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<3> proghvz{"proghvz", vl::logic<3>::all_z().lv()};
    sc_core::sc_signal_resolved enablehv{"enablehv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved tie_low{"tie_low", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net652{"net652", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved enahvz{"enahvz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<2> enioconthvz{"enioconthvz", vl::logic<2>::all_z().lv()};
    sc_core::sc_signal_resolved atb_lptx1200_on_hv{"atb_lptx1200_on_hv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved enulphv{"enulphv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved tie_lowHV{"tie_lowHV", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved enpulldwnhvz{"enpulldwnhvz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved sr_bypasshv{"sr_bypasshv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved atb_lptx1200_on_hvz{"atb_lptx1200_on_hvz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved enulphvz{"enulphvz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved onbhv{"onbhv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved ponhv{"ponhv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved enablehvz{"enablehvz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved enpulldwnhv{"enpulldwnhv", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_levelconvs> lconvs;
    sc_core::sc_signal_rv<2> c_lconvs_dinhv{"c_lconvs_dinhv", vl::logic<2>::all_z().lv()};
    sc_core::sc_signal_rv<2> c_lconvs_dinhvz{"c_lconvs_dinhvz", vl::logic<2>::all_z().lv()};
    sc_core::sc_signal_rv<2> c_lconvs_enioconthv{"c_lconvs_enioconthv", vl::logic<2>::all_z().lv()};
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYATB_testswitch> ATBsw;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_log> LOG;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_core> drvn;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_core> drvp;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_bias> BIAS;
    sc_core::sc_signal_rv<3> c_BIAS_cur_setz{"c_BIAS_cur_setz", vl::logic<3>::all_z().lv()};
    std::unique_ptr<mipi_4_rx_dphy_ns_tieLowHigh_HV> LowHigh;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHY_iocont_buffer> IOcont_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHY_iocont_buffer> IOcont_0;

    void merge_iocont();
    vl::logic<2> rd_dinhv() const;
    vl::logic<2> rd_dinhvz() const;
    vl::logic<2> rd_enioconthv() const;
    vl::logic<2> rd_iocont() const;
    void split_lconvs_dinhv();
    void split_lconvs_dinhvz();
    void split_lconvs_enioconthv();
    void glue_BIAS_cur_setz();
};

#endif
