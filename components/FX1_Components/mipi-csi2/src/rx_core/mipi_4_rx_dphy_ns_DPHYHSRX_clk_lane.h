//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_clk_lane
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_CLK_LANE_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_CLK_LANE_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_4_rx_dphy_ns_DPHYATB_dec4to9_hv.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_LB_serial_buffer_sym.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_clk_mux.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_clock_cdmprot.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_clock_levelconvs.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_cml2cmos_clk.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_div124.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_preamp_clk.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_slicer_a_ptype.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_term.h"
#include "mipi_4_rx_dphy_ns_DPHYHSTX_serial.h"
#include "mipi_4_rx_dphy_ns_DPHYRESTUNE.h"
#include "mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_demux.h"
#include "mipi_4_rx_dphy_ns_sc_tie_hilow_lvt.h"
#include "rx_cells.h"

class mipi_4_rx_dphy_ns_DPHYHSRX_clk_lane : public sc_core::sc_module {
public:
    sc_core::sc_inout<sc_dt::sc_logic> atb_line{"atb_line"};
    sc_core::sc_in<sc_dt::sc_lv<4>> atb_sw{"atb_sw"};
    sc_core::sc_in<sc_dt::sc_logic> cfg_clk{"cfg_clk"};
    sc_core::sc_in<sc_dt::sc_logic> clk_bias_ihv{"clk_bias_ihv"};
    sc_core::sc_in<sc_dt::sc_logic> clk_bias_ilv_cc{"clk_bias_ilv_cc"};
    sc_core::sc_in<sc_dt::sc_logic> clk_bias_ilv_cgm{"clk_bias_ilv_cgm"};
    sc_core::sc_in<sc_dt::sc_logic> clk_en{"clk_en"};
    sc_core::sc_in<sc_dt::sc_logic> clk_in_m{"clk_in_m"};
    sc_core::sc_in<sc_dt::sc_logic> clk_in_p{"clk_in_p"};
    sc_core::sc_in<sc_dt::sc_lv<6>> clk_offset_cal{"clk_offset_cal"};
    sc_core::sc_in<sc_dt::sc_logic> clk_offset_cal_en{"clk_offset_cal_en"};
    sc_core::sc_inout<sc_dt::sc_logic> clk_pll0{"clk_pll0"};
    sc_core::sc_inout<sc_dt::sc_logic> clk_pll180{"clk_pll180"};
    sc_core::sc_inout<sc_dt::sc_logic> clk_pll270{"clk_pll270"};
    sc_core::sc_inout<sc_dt::sc_logic> clk_pll90{"clk_pll90"};
    sc_core::sc_in<sc_dt::sc_logic> clk_to_long_channel{"clk_to_long_channel"};
    sc_core::sc_in<sc_dt::sc_logic> cml2cmos_bias{"cml2cmos_bias"};
    sc_core::sc_in<sc_dt::sc_lv<2>> ddr_clk_div{"ddr_clk_div"};
    sc_core::sc_in<sc_dt::sc_logic> ddr_clk_en{"ddr_clk_en"};
    sc_core::sc_in<sc_dt::sc_logic> ddr_clk_sel{"ddr_clk_sel"};
    sc_core::sc_in<sc_dt::sc_logic> enterm{"enterm"};
    sc_core::sc_in<sc_dt::sc_logic> feed_int_clk{"feed_int_clk"};
    sc_core::sc_in<sc_dt::sc_lv<2>> gmode{"gmode"};
    sc_core::sc_in<sc_dt::sc_lv<2>> gmode_hv{"gmode_hv"};
    sc_core::sc_in<sc_dt::sc_logic> int_clk_m{"int_clk_m"};
    sc_core::sc_in<sc_dt::sc_logic> int_clk_p{"int_clk_p"};
    sc_core::sc_in<sc_dt::sc_logic> int_clk_sel{"int_clk_sel"};
    sc_core::sc_in<sc_dt::sc_logic> lb_pll_bustie{"lb_pll_bustie"};
    sc_core::sc_in<sc_dt::sc_lv<4>> lb_prog{"lb_prog"};
    sc_core::sc_in<sc_dt::sc_logic> lb_res_cm_en{"lb_res_cm_en"};
    sc_core::sc_in<sc_dt::sc_logic> lb_res_dn_en{"lb_res_dn_en"};
    sc_core::sc_in<sc_dt::sc_logic> lb_res_dp_en{"lb_res_dp_en"};
    sc_core::sc_in<sc_dt::sc_logic> lb_sel{"lb_sel"};
    sc_core::sc_in<sc_dt::sc_logic> lbck_clk_en{"lbck_clk_en"};
    sc_core::sc_in<sc_dt::sc_logic> lbck_clk_kill{"lbck_clk_kill"};
    sc_core::sc_inout<sc_dt::sc_logic> lbck_clkout_stop{"lbck_clkout_stop"};
    sc_core::sc_in<sc_dt::sc_lv<8>> lbck_data{"lbck_data"};
    sc_core::sc_in<sc_dt::sc_logic> lbck_div_en{"lbck_div_en"};
    sc_core::sc_in<sc_dt::sc_logic> lbck_serial_clk_en{"lbck_serial_clk_en"};
    sc_core::sc_in<sc_dt::sc_logic> lbck_serial_en{"lbck_serial_en"};
    sc_core::sc_in<sc_dt::sc_logic> lbdata_clk_en{"lbdata_clk_en"};
    sc_core::sc_in<sc_dt::sc_logic> lbdata_clk_kill{"lbdata_clk_kill"};
    sc_core::sc_inout<sc_dt::sc_logic> lbdata_clkout_stop{"lbdata_clkout_stop"};
    sc_core::sc_in<sc_dt::sc_lv<8>> lbdata_data{"lbdata_data"};
    sc_core::sc_in<sc_dt::sc_logic> lbdata_div_en{"lbdata_div_en"};
    sc_core::sc_in<sc_dt::sc_logic> lbdata_serial_clk_en{"lbdata_serial_clk_en"};
    sc_core::sc_in<sc_dt::sc_logic> lbdata_serial_en{"lbdata_serial_en"};
    sc_core::sc_in<sc_dt::sc_logic> osc_pon{"osc_pon"};
    sc_core::sc_in<sc_dt::sc_logic> osc_res_en{"osc_res_en"};
    sc_core::sc_in<sc_dt::sc_logic> pull_long_channel{"pull_long_channel"};
    sc_core::sc_in<sc_dt::sc_logic> rx_en{"rx_en"};
    sc_core::sc_in<sc_dt::sc_logic> rx_enlogic{"rx_enlogic"};
    sc_core::sc_in<sc_dt::sc_logic> rx_pon{"rx_pon"};
    sc_core::sc_in<sc_dt::sc_logic> rxclk_cal_obs_on{"rxclk_cal_obs_on"};
    sc_core::sc_in<sc_dt::sc_logic> rxclk_osc_mux_on{"rxclk_osc_mux_on"};
    sc_core::sc_in<sc_dt::sc_logic> rxclk_rxhs_rx_en200mode{"rxclk_rxhs_rx_en200mode"};
    sc_core::sc_in<sc_dt::sc_lv<3>> setrx_hv{"setrx_hv"};
    sc_core::sc_out<sc_dt::sc_logic> clk_m_cal{"clk_m_cal"};
    sc_core::sc_out<sc_dt::sc_logic> clk_out_m_short_ch{"clk_out_m_short_ch"};
    sc_core::sc_out<sc_dt::sc_logic> clk_out_p_short_ch{"clk_out_p_short_ch"};
    sc_core::sc_out<sc_dt::sc_logic> clk_p_cal{"clk_p_cal"};
    sc_core::sc_out<sc_dt::sc_logic> clkosc2bg{"clkosc2bg"};
    sc_core::sc_out<sc_dt::sc_logic> ddr_clk{"ddr_clk"};
    sc_core::sc_out<sc_dt::sc_logic> int_clk_m_out{"int_clk_m_out"};
    sc_core::sc_out<sc_dt::sc_logic> int_clk_p_out{"int_clk_p_out"};
    sc_core::sc_out<sc_dt::sc_logic> int_data_m_out{"int_data_m_out"};
    sc_core::sc_out<sc_dt::sc_logic> int_data_p_out{"int_data_p_out"};
    sc_core::sc_out<sc_dt::sc_logic> lbck_word_clk{"lbck_word_clk"};
    sc_core::sc_out<sc_dt::sc_logic> lbdata_word_clk{"lbdata_word_clk"};
    sc_core::sc_out<sc_dt::sc_logic> osc_out{"osc_out"};
    sc_core::sc_inout<sc_dt::sc_logic> clk_out_m_long_ch{"clk_out_m_long_ch"};
    sc_core::sc_inout<sc_dt::sc_logic> clk_out_p_long_ch{"clk_out_p_long_ch"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vdd{"vdd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};
    sc_core::sc_in<sc_dt::sc_logic> vss{"vss"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_clk_lane);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_clk_lane(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_resolved rx_en_hvz{"rx_en_hvz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved lbck_sym_a{"lbck_sym_a", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved lbck_sym_b{"lbck_sym_b", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<8> lbck_datalv{"lbck_datalv", vl::logic<8>::all_z().lv()};
    sc_core::sc_signal_resolved clk_m_cal_int{"clk_m_cal_int", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved tie_low_hv{"tie_low_hv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_vector<sc_core::sc_signal_resolved> atb_sw_dec{"atb_sw_dec", 6, vl::make_z1};
    sc_core::sc_signal_resolved feed_int_clk_hv{"feed_int_clk_hv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved rx_pon_lvz{"rx_pon_lvz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clk_p_cmos{"clk_p_cmos", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved tie_low{"tie_low", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved lbck_clkout_stoplv{"lbck_clkout_stoplv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clk_m_cmos{"clk_m_cmos", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved rxclk_osc_mux_on_lv{"rxclk_osc_mux_on_lv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved lbdata_div_enlv{"lbdata_div_enlv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved lbdata_sym_a{"lbdata_sym_a", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved lbdata_sym_b{"lbdata_sym_b", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved feed_int_clk_hvz{"feed_int_clk_hvz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net840{"net840", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net841{"net841", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved buf_on_lb{"buf_on_lb", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved lbdata_clk_killlv{"lbdata_clk_killlv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net844{"net844", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved feed_int_clk_lv{"feed_int_clk_lv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net846{"net846", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<2> gmode_lv{"gmode_lv", vl::logic<2>::all_z().lv()};
    sc_core::sc_signal_resolved net849{"net849", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net850{"net850", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved lbdata_clk_enlv{"lbdata_clk_enlv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net852{"net852", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved osc_pon_hv{"osc_pon_hv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net853{"net853", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved lbdata_clkout_stoplv{"lbdata_clkout_stoplv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<6> offset_cal_lv{"offset_cal_lv", vl::logic<6>::all_z().lv()};
    sc_core::sc_signal_rv<4> atb_sw_hvz{"atb_sw_hvz", vl::logic<4>::all_z().lv()};
    sc_core::sc_signal_resolved rx_pon_hv{"rx_pon_hv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved rxhs_rx_en200mode_hv{"rxhs_rx_en200mode_hv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved lbdata_serial_clk_enlv{"lbdata_serial_clk_enlv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clk_gated{"clk_gated", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved rx_en_lvz{"rx_en_lvz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved config_clk_lvz{"config_clk_lvz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved lb_pll_bustie_intz{"lb_pll_bustie_intz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net869{"net869", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved config_clk_lv{"config_clk_lv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved en_cal_obs{"en_cal_obs", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved int_clk_sel_lv{"int_clk_sel_lv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved lbres_dp_enz_hv{"lbres_dp_enz_hv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved osc_m{"osc_m", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved osc_pon_lv{"osc_pon_lv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved lb_sellv{"lb_sellv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved osc_p{"osc_p", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clk_ser_m{"clk_ser_m", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved rx_pon_lv{"rx_pon_lv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clk_ser_p{"clk_ser_p", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved lbres_dn_enz_hv{"lbres_dn_enz_hv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clk_pre_m{"clk_pre_m", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clk_pre_p{"clk_pre_p", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved rx_offset_cal_en_hvz{"rx_offset_cal_en_hvz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved lbck_clk_killlv{"lbck_clk_killlv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clk_to_long_channel_lv{"clk_to_long_channel_lv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved tie_high{"tie_high", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved rx_en_hv{"rx_en_hv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved rxcm{"rxcm", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved enterm_hvz{"enterm_hvz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved dn_atb_en_hv{"dn_atb_en_hv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<4> atb_sw_hv{"atb_sw_hv", vl::logic<4>::all_z().lv()};
    sc_core::sc_vector<sc_core::sc_signal_resolved> net634{"net634", 2, vl::make_z1};
    sc_core::sc_signal_resolved pull_long_channel_lv{"pull_long_channel_lv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved lbck_div_enlv{"lbck_div_enlv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved rx_en_lv{"rx_en_lv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved dp_atb_en_hv{"dp_atb_en_hv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved lbdata_serial_enlv{"lbdata_serial_enlv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved lbdata_word_clklv{"lbdata_word_clklv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved osc_res_en_lv{"osc_res_en_lv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clk_en_lv{"clk_en_lv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved lbck_clk_enlv{"lbck_clk_enlv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved cap_10pF{"cap_10pF", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved lbres_cm_en_hv{"lbres_cm_en_hv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved offset_cal_en_lv{"offset_cal_en_lv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved lb_pll_bustie_int{"lb_pll_bustie_int", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved tie_low_cdm{"tie_low_cdm", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved rx_offset_cal_en_hv{"rx_offset_cal_en_hv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved data_ser_m{"data_ser_m", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved lbck_serial_enlv{"lbck_serial_enlv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net710{"net710", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved enterm_hv{"enterm_hv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net711{"net711", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved data_ser_p{"data_ser_p", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved ddr_clk_sel_lv{"ddr_clk_sel_lv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clk_p_cal_int{"clk_p_cal_int", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved rx_pon_hvz{"rx_pon_hvz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved rxhs_rx_en200mode_hvz{"rxhs_rx_en200mode_hvz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved lbck_serial_clk_enlv{"lbck_serial_clk_enlv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clk2dig{"clk2dig", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved lbck_word_clklv{"lbck_word_clklv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved tie_high_cdm{"tie_high_cdm", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<8> lbdata_datalv{"lbdata_datalv", vl::logic<8>::all_z().lv()};
    sc_core::sc_signal_rv<4> lb_current_h{"lb_current_h", vl::logic<4>::all_z().lv()};
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_term> RXTERM;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_clock_cdmprot> CDM;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_clk_mux> OSC_MUX;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_demux> CLK_BUF;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_buf_PLL_bustie> I167;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYRESTUNE> I120;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_tie_hilow_lvt> sc_lvt_I1;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_clk> PREAMP_CLK;
    sc_core::sc_signal_rv<2> c_PREAMP_CLK_atb_sw_hv{"c_PREAMP_CLK_atb_sw_hv", vl::logic<2>::all_z().lv()};
    sc_core::sc_signal_rv<3> c_PREAMP_CLK_rx_eq_bcal_h{"c_PREAMP_CLK_rx_eq_bcal_h", vl::logic<3>::all_z().lv()};
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYATB_dec4to9_hv> I95;
    sc_core::sc_signal_rv<4> c_I95_A_in{"c_I95_A_in", vl::logic<4>::all_z().lv()};
    sc_core::sc_signal_rv<4> c_I95_A_inz{"c_I95_A_inz", vl::logic<4>::all_z().lv()};
    sc_core::sc_signal_rv<9> c_I95_Z_out{"c_I95_Z_out", vl::logic<9>::all_z().lv()};
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_clock_levelconvs> LVL;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_cml2cmos_clk> DIFF2CMOS;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_div124> DDR_DIV;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_slicer_a_ptype> SLICER_DUMMY_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_slicer_a_ptype> SLICER_DUMMY_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSTX_serial> lb_serial_clk;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSTX_serial> lb_serial_data;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_LB_serial_buffer_sym> I165;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_LB_serial_buffer_sym> I166;

    vl::logic<6> rd_atb_sw_dec() const;
    vl::logic<2> rd_net634() const;
    void glue_PREAMP_CLK_atb_sw_hv();
    void glue_PREAMP_CLK_rx_eq_bcal_h();
    void glue_I95_A_in();
    void glue_I95_A_inz();
    void split_I95_Z_out();
};

#endif
