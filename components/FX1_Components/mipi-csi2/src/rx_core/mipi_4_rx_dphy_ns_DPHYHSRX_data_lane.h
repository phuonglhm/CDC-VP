//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_data_lane
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_DATA_LANE_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_DATA_LANE_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_4_rx_dphy_ns_DPHYATB_dec4to9_hv.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_CMOS_DELAY.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_DUAL_DDL.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_cml2cmos_data.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_data_cdmprot.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_data_cdmprot_ddl.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_data_levelconvs.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_deserial.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_div1248.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_preamp_data.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_slicer_a_ptype.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_term.h"
#include "mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_mux_ddl_sync.h"
#include "mipi_4_rx_dphy_ns_sc_tie_hilow_lvt.h"
#include "rx_cells.h"

class mipi_4_rx_dphy_ns_DPHYHSRX_data_lane : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_lv<4>> atb_sw{"atb_sw"};
    sc_core::sc_in<sc_dt::sc_logic> bias_ddl{"bias_ddl"};
    sc_core::sc_in<sc_dt::sc_logic> clk_from_long_channel{"clk_from_long_channel"};
    sc_core::sc_in<sc_dt::sc_logic> clk_in_m_long_ch{"clk_in_m_long_ch"};
    sc_core::sc_in<sc_dt::sc_logic> clk_in_m_short_ch{"clk_in_m_short_ch"};
    sc_core::sc_in<sc_dt::sc_logic> clk_in_p_long_ch{"clk_in_p_long_ch"};
    sc_core::sc_in<sc_dt::sc_logic> clk_in_p_short_ch{"clk_in_p_short_ch"};
    sc_core::sc_in<sc_dt::sc_logic> cml2cmos_bias_data{"cml2cmos_bias_data"};
    sc_core::sc_in<sc_dt::sc_logic> compatibility_mode{"compatibility_mode"};
    sc_core::sc_in<sc_dt::sc_logic> data_bias_ihv{"data_bias_ihv"};
    sc_core::sc_in<sc_dt::sc_logic> data_bias_ilv_cc{"data_bias_ilv_cc"};
    sc_core::sc_in<sc_dt::sc_logic> data_bias_ilv_cgm{"data_bias_ilv_cgm"};
    sc_core::sc_in<sc_dt::sc_lv<3>> data_eq{"data_eq"};
    sc_core::sc_in<sc_dt::sc_logic> data_in_m{"data_in_m"};
    sc_core::sc_in<sc_dt::sc_logic> data_in_p{"data_in_p"};
    sc_core::sc_in<sc_dt::sc_lv<6>> data_offset_cal{"data_offset_cal"};
    sc_core::sc_in<sc_dt::sc_logic> data_offset_cal_en{"data_offset_cal_en"};
    sc_core::sc_in<sc_dt::sc_lv<2>> ddl_clk_out_div{"ddl_clk_out_div"};
    sc_core::sc_in<sc_dt::sc_logic> ddl_phase_change_p{"ddl_phase_change_p"};
    sc_core::sc_in<sc_dt::sc_lv<5>> ddl_phase_clk{"ddl_phase_clk"};
    sc_core::sc_in<sc_dt::sc_lv<4>> ddl_phase_data{"ddl_phase_data"};
    sc_core::sc_in<sc_dt::sc_lv<5>> ddl_tune{"ddl_tune"};
    sc_core::sc_in<sc_dt::sc_logic> ddl_tune_en{"ddl_tune_en"};
    sc_core::sc_in<sc_dt::sc_logic> des_en{"des_en"};
    sc_core::sc_in<sc_dt::sc_logic> deserial_data_en{"deserial_data_en"};
    sc_core::sc_in<sc_dt::sc_logic> deserial_gated_polarity{"deserial_gated_polarity"};
    sc_core::sc_in<sc_dt::sc_logic> en_lb{"en_lb"};
    sc_core::sc_in<sc_dt::sc_logic> enterm{"enterm"};
    sc_core::sc_in<sc_dt::sc_lv<2>> gmode{"gmode"};
    sc_core::sc_in<sc_dt::sc_lv<2>> gmode_hv{"gmode_hv"};
    sc_core::sc_in<sc_dt::sc_logic> lb_in_m{"lb_in_m"};
    sc_core::sc_in<sc_dt::sc_logic> lb_in_p{"lb_in_p"};
    sc_core::sc_in<sc_dt::sc_lv<4>> lb_prog{"lb_prog"};
    sc_core::sc_in<sc_dt::sc_logic> lb_res_cm_en{"lb_res_cm_en"};
    sc_core::sc_in<sc_dt::sc_logic> lb_res_dn_en{"lb_res_dn_en"};
    sc_core::sc_in<sc_dt::sc_logic> lb_res_dp_en{"lb_res_dp_en"};
    sc_core::sc_in<sc_dt::sc_logic> rx_en{"rx_en"};
    sc_core::sc_in<sc_dt::sc_logic> rx_enlogic{"rx_enlogic"};
    sc_core::sc_in<sc_dt::sc_logic> rx_pon{"rx_pon"};
    sc_core::sc_in<sc_dt::sc_lv<3>> setrx_hv{"setrx_hv"};
    sc_core::sc_out<sc_dt::sc_lv<8>> data{"data"};
    sc_core::sc_out<sc_dt::sc_logic> ddl_tune_clk{"ddl_tune_clk"};
    sc_core::sc_out<sc_dt::sc_logic> word_clk{"word_clk"};
    sc_core::sc_inout<sc_dt::sc_logic> atb_line{"atb_line"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vdd{"vdd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};
    sc_core::sc_in<sc_dt::sc_logic> vss{"vss"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_data_lane);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_data_lane(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_rv<6> offset_cal_lv{"offset_cal_lv", vl::logic<6>::all_z().lv()};
    sc_core::sc_signal_resolved tie_high{"tie_high", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved rx_en_hv{"rx_en_hv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<4> atb_sw_hv{"atb_sw_hv", vl::logic<4>::all_z().lv()};
    sc_core::sc_signal_rv<4> atb_sw_hvz{"atb_sw_hvz", vl::logic<4>::all_z().lv()};
    sc_core::sc_signal_resolved rxcm{"rxcm", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved enterm_hvz{"enterm_hvz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<3> data_eq_hv{"data_eq_hv", vl::logic<3>::all_z().lv()};
    sc_core::sc_signal_resolved rx_pon_hv{"rx_pon_hv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved rx_en_hvz{"rx_en_hvz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved ddl_loop_clkn{"ddl_loop_clkn", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<4> ddl_phase_data_lv{"ddl_phase_data_lv", vl::logic<4>::all_z().lv()};
    sc_core::sc_signal_resolved tie_low_cdm{"tie_low_cdm", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved tie_high_hv{"tie_high_hv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved ddl_loop_clkp{"ddl_loop_clkp", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved rx_en_lvz{"rx_en_lvz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved dn_atb_en_hv{"dn_atb_en_hv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_vector<sc_core::sc_signal_resolved> atb_sw_dec_hv{"atb_sw_dec_hv", 6, vl::make_z1};
    sc_core::sc_signal_resolved enterm_hv{"enterm_hv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved ddl_phase_change_p_lv{"ddl_phase_change_p_lv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved compatibility_on_lvz{"compatibility_on_lvz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved enlogic_lv{"enlogic_lv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved tie_low_cdm_vdd{"tie_low_cdm_vdd", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net724{"net724", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved feed_int_clk_hv{"feed_int_clk_hv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved enlogic_lvz{"enlogic_lvz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved rx_pon_hvz{"rx_pon_hvz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net725{"net725", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clk_out_m{"clk_out_m", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net726{"net726", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved compatibility_on_lv{"compatibility_on_lv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clk_out_p{"clk_out_p", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved rx_pon_lvz{"rx_pon_lvz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved data_offset_cal_en_lv{"data_offset_cal_en_lv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved tie_low{"tie_low", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved lbres_dp_enz_hv{"lbres_dp_enz_hv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net968{"net968", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<5> ddl_tune_lv{"ddl_tune_lv", vl::logic<5>::all_z().lv()};
    sc_core::sc_signal_resolved data_offset_cal_en_hvz{"data_offset_cal_en_hvz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved data_preamp_m{"data_preamp_m", sc_dt::SC_LOGIC_Z};
    sc_core::sc_vector<sc_core::sc_signal_resolved> net657{"net657", 2, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> data_out{"data_out", 2, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> data_out_cmos{"data_out_cmos", 2, vl::make_z1};
    sc_core::sc_signal_resolved rx_en_lv{"rx_en_lv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved data_preamp_p{"data_preamp_p", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clk_from_long_channel_lv{"clk_from_long_channel_lv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved enlogic_hv{"enlogic_hv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<5> ddl_phase_clk_lv{"ddl_phase_clk_lv", vl::logic<5>::all_z().lv()};
    sc_core::sc_signal_resolved rx_pon_lv{"rx_pon_lv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved dp_atb_en_hv{"dp_atb_en_hv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved lbres_dn_enz_hv{"lbres_dn_enz_hv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved ddl_tune_en_lv{"ddl_tune_en_lv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved tie_high_cdm{"tie_high_cdm", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<3> data_eq_hvz{"data_eq_hvz", vl::logic<3>::all_z().lv()};
    sc_core::sc_signal_resolved clk_in_m{"clk_in_m", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved feed_int_clk_hvz{"feed_int_clk_hvz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved data_out_cmos_m{"data_out_cmos_m", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clk_in_p{"clk_in_p", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved data_cmos_m{"data_cmos_m", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved tie_high_cdm_vdd{"tie_high_cdm_vdd", sc_dt::SC_LOGIC_Z};
    sc_core::sc_vector<sc_core::sc_signal_resolved> data_out_pre{"data_out_pre", 2, vl::make_z1};
    sc_core::sc_signal_resolved data_out_cmos_p{"data_out_cmos_p", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved data_cmos_p{"data_cmos_p", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved data_out_cmos_delay_m{"data_out_cmos_delay_m", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<4> lb_current_h{"lb_current_h", vl::logic<4>::all_z().lv()};
    sc_core::sc_signal_resolved data_offset_cal_en_hv{"data_offset_cal_en_hv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net611{"net611", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved data_out_cmos_delay_p{"data_out_cmos_delay_p", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net612{"net612", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved feed_int_clk_lv{"feed_int_clk_lv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<2> gmode_lv{"gmode_lv", vl::logic<2>::all_z().lv()};
    sc_core::sc_signal_resolved lbres_cm_en_hv{"lbres_cm_en_hv", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_div1248> DDL_DIV;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_deserial> DES;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_data_levelconvs> LVL;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_data_cdmprot_ddl> CDM_PROT_DDL;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYRXCLK_CLL_mux_ddl_sync> DATA_BUF;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_slicer_a_ptype> SLICER_CML_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_slicer_a_ptype> SLICER_CML_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_slicer_a_ptype> SLICER_CMOS_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_slicer_a_ptype> SLICER_CMOS_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_slicer_a_ptype> SLICER_DUMMY_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_slicer_a_ptype> SLICER_DUMMY_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_cml2cmos_data> CML2CMOS_DATA;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_tie_hilow_lvt> sc_lvt_I16;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_preamp_data> PREAMP_DATA;
    sc_core::sc_signal_rv<2> c_PREAMP_DATA_atb_sw_hv{"c_PREAMP_DATA_atb_sw_hv", vl::logic<2>::all_z().lv()};
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_term> RXTERM;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_CMOS_DELAY> CMOS_DELAY_DATA;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_data_cdmprot> CDM;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_mux2x8_lvt> MUX_DATA_OUT_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_mux2x8_lvt> MUX_DATA_OUT_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_DUAL_DDL> I97;
    sc_core::sc_signal_rv<4> c_I97_atb_sw_hv{"c_I97_atb_sw_hv", vl::logic<4>::all_z().lv()};
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYATB_dec4to9_hv> I95;
    sc_core::sc_signal_rv<4> c_I95_A_in{"c_I95_A_in", vl::logic<4>::all_z().lv()};
    sc_core::sc_signal_rv<4> c_I95_A_inz{"c_I95_A_inz", vl::logic<4>::all_z().lv()};
    sc_core::sc_signal_rv<9> c_I95_Z_out{"c_I95_Z_out", vl::logic<9>::all_z().lv()};

    vl::logic<6> rd_atb_sw_dec_hv() const;
    vl::logic<2> rd_data_out() const;
    vl::logic<2> rd_data_out_cmos() const;
    vl::logic<2> rd_data_out_pre() const;
    vl::logic<2> rd_net657() const;
    void glue_PREAMP_DATA_atb_sw_hv();
    void glue_I97_atb_sw_hv();
    void glue_I95_A_in();
    void glue_I95_A_inz();
    void split_I95_Z_out();
};

#endif
