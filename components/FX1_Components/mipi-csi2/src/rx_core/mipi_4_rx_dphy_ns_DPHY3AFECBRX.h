//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHY3AFECBRX
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHY3AFECBRX_H
#define VL_MIPI_4_RX_DPHY_NS_DPHY3AFECBRX_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_4_rx_dphy_ns_DPHYATB.h"
#include "mipi_4_rx_dphy_ns_DPHYCBRX_bias.h"
#include "mipi_4_rx_dphy_ns_DPHYCBRX_lc.h"
#include "mipi_4_rx_dphy_ns_DPHYCB_preamp_bias.h"
#include "mipi_4_rx_dphy_ns_DPHY_iocont_buffer.h"

class mipi_4_rx_dphy_ns_DPHY3AFECBRX : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> atb_comp_clk{"atb_comp_clk"};
    sc_core::sc_in<sc_dt::sc_logic> atb_ext_con{"atb_ext_con"};
    sc_core::sc_in<sc_dt::sc_logic> atb_line{"atb_line"};
    sc_core::sc_in<sc_dt::sc_logic> atb_on{"atb_on"};
    sc_core::sc_in<sc_dt::sc_logic> atb_vbe_sel{"atb_vbe_sel"};
    sc_core::sc_in<sc_dt::sc_lv<2>> cb_atb_sel{"cb_atb_sel"};
    sc_core::sc_in<sc_dt::sc_logic> cb_bg_en{"cb_bg_en"};
    sc_core::sc_inout<sc_dt::sc_logic> cb_burn_oscilator_p{"cb_burn_oscilator_p"};
    sc_core::sc_in<sc_dt::sc_logic> cb_cal_en{"cb_cal_en"};
    sc_core::sc_in<sc_dt::sc_lv<4>> cb_cal_repl{"cb_cal_repl"};
    sc_core::sc_in<sc_dt::sc_logic> cb_chop_clk{"cb_chop_clk"};
    sc_core::sc_in<sc_dt::sc_logic> cb_chop_clk_en{"cb_chop_clk_en"};
    sc_core::sc_in<sc_dt::sc_logic> cb_en_45_ohm{"cb_en_45_ohm"};
    sc_core::sc_in<sc_dt::sc_logic> cb_ibias_en{"cb_ibias_en"};
    sc_core::sc_in<sc_dt::sc_logic> cb_ical_x4{"cb_ical_x4"};
    sc_core::sc_in<sc_dt::sc_logic> cb_lb_en_rext{"cb_lb_en_rext"};
    sc_core::sc_in<sc_dt::sc_lv<3>> cb_lb_setrx{"cb_lb_setrx"};
    sc_core::sc_in<sc_dt::sc_logic> cb_lpibias_en{"cb_lpibias_en"};
    sc_core::sc_in<sc_dt::sc_logic> cb_on{"cb_on"};
    sc_core::sc_in<sc_dt::sc_logic> cb_pre_iref_on{"cb_pre_iref_on"};
    sc_core::sc_in<sc_dt::sc_logic> cb_rext_iocont_en{"cb_rext_iocont_en"};
    sc_core::sc_in<sc_dt::sc_logic> cb_sel_chop_clk{"cb_sel_chop_clk"};
    sc_core::sc_in<sc_dt::sc_lv<3>> cb_sel_v400_prog{"cb_sel_v400_prog"};
    sc_core::sc_in<sc_dt::sc_lv<2>> cb_sel_vref_lprx{"cb_sel_vref_lprx"};
    sc_core::sc_in<sc_dt::sc_lv<2>> cb_sel_vrefcd_lprx{"cb_sel_vrefcd_lprx"};
    sc_core::sc_in<sc_dt::sc_logic> cb_sel_vreflptx_prog{"cb_sel_vreflptx_prog"};
    sc_core::sc_in<sc_dt::sc_lv<3>> cb_vref_mpll_reg_sel{"cb_vref_mpll_reg_sel"};
    sc_core::sc_in<sc_dt::sc_lv<10>> dac_prog{"dac_prog"};
    sc_core::sc_in<sc_dt::sc_lv<2>> gmode{"gmode"};
    sc_core::sc_in<sc_dt::sc_logic> rext{"rext"};
    sc_core::sc_in<sc_dt::sc_logic> rxlp_bias_prog{"rxlp_bias_prog"};
    sc_core::sc_in<sc_dt::sc_lv<3>> setrx{"setrx"};
    sc_core::sc_in<sc_dt::sc_lv<3>> txlp_prog{"txlp_prog"};
    sc_core::sc_in<sc_dt::sc_logic> vsense{"vsense"};
    sc_core::sc_out<sc_dt::sc_logic> atb{"atb"};
    sc_core::sc_out<sc_dt::sc_lv<4>> cb_10u_ibias_ddl{"cb_10u_ibias_ddl"};
    sc_core::sc_out<sc_dt::sc_logic> cb_10u_ilptx{"cb_10u_ilptx"};
    sc_core::sc_out<sc_dt::sc_lv<10>> cb_5u_ilprx{"cb_5u_ilprx"};
    sc_core::sc_out<sc_dt::sc_logic> cb_comp_out{"cb_comp_out"};
    sc_core::sc_out<sc_dt::sc_logic> cb_det_supply{"cb_det_supply"};
    sc_core::sc_out<sc_dt::sc_lv<5>> cb_ibias_d2c_bias{"cb_ibias_d2c_bias"};
    sc_core::sc_out<sc_dt::sc_lv<5>> cb_ihv_hsrx{"cb_ihv_hsrx"};
    sc_core::sc_out<sc_dt::sc_lv<5>> cb_ilv_ccm_hsrx{"cb_ilv_ccm_hsrx"};
    sc_core::sc_out<sc_dt::sc_lv<5>> cb_ilv_hsrx{"cb_ilv_hsrx"};
    sc_core::sc_out<sc_dt::sc_logic> cb_rext_iocont{"cb_rext_iocont"};
    sc_core::sc_out<sc_dt::sc_logic> cb_vref_lprx{"cb_vref_lprx"};
    sc_core::sc_out<sc_dt::sc_logic> cb_vref_mpll_reg{"cb_vref_mpll_reg"};
    sc_core::sc_out<sc_dt::sc_logic> cb_vrefcd_lprx{"cb_vrefcd_lprx"};
    sc_core::sc_out<sc_dt::sc_logic> cb_vreflptx{"cb_vreflptx"};
    sc_core::sc_out<sc_dt::sc_lv<2>> gmode_hv{"gmode_hv"};
    sc_core::sc_out<sc_dt::sc_lv<2>> gmode_hvz{"gmode_hvz"};
    sc_core::sc_out<sc_dt::sc_logic> rxlp_bias_prog_hv{"rxlp_bias_prog_hv"};
    sc_core::sc_out<sc_dt::sc_logic> rxlp_bias_prog_hvz{"rxlp_bias_prog_hvz"};
    sc_core::sc_out<sc_dt::sc_lv<3>> setrx_hv{"setrx_hv"};
    sc_core::sc_out<sc_dt::sc_lv<3>> txlp_prog_hv{"txlp_prog_hv"};
    sc_core::sc_out<sc_dt::sc_lv<3>> txlp_prog_hvz{"txlp_prog_hvz"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vdd{"vdd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};
    sc_core::sc_in<sc_dt::sc_logic> vss{"vss"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHY3AFECBRX);
    explicit mipi_4_rx_dphy_ns_DPHY3AFECBRX(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_vector<sc_core::sc_signal_resolved> cb_10u_ibias_ddl_b{"cb_10u_ibias_ddl_b", 4, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> cb_5u_ilprx_b{"cb_5u_ilprx_b", 10, vl::make_z1};
    sc_core::sc_signal<sc_dt::sc_logic, sc_core::SC_UNCHECKED_WRITERS> cb_tx_ok{"cb_tx_ok", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_logic, sc_core::SC_UNCHECKED_WRITERS> cb_tx_lp_ok{"cb_tx_lp_ok", vl::logic<1>().sl()};
    sc_core::sc_signal_resolved supply_ok{"supply_ok", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<4> vctrl_replica_int{"vctrl_replica_int", vl::logic<4>::all_z().lv()};
    sc_core::sc_signal_rv<10> vctrl_replica_int_calibration{"vctrl_replica_int_calibration", vl::logic<10>::all_z().lv()};
    sc_core::sc_signal_resolved atb_ok{"atb_ok", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved enable{"enable", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved cb_vref_mpll_reg_int{"cb_vref_mpll_reg_int", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<3> lb_setzrx_hv{"lb_setzrx_hv", vl::logic<3>::all_z().lv()};
    sc_core::sc_signal_resolved cb_ibias_en_int_z{"cb_ibias_en_int_z", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved cb_burn_oscilator_int{"cb_burn_oscilator_int", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved tie_high_hv{"tie_high_hv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<2> cb_sel_vrefcd_lprx_hv{"cb_sel_vrefcd_lprx_hv", vl::logic<2>::all_z().lv()};
    sc_core::sc_signal_resolved atb_en_hvz{"atb_en_hvz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<3> cb_sel_vbg_prog_hvz{"cb_sel_vbg_prog_hvz", vl::logic<3>::all_z().lv()};
    sc_core::sc_signal_resolved repl_oloop{"repl_oloop", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved enz_rx_hv{"enz_rx_hv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved vref_1000_int{"vref_1000_int", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved atb_ext_con_hvz{"atb_ext_con_hvz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved rext_esd{"rext_esd", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<10> dac_value_hv{"dac_value_hv", vl::logic<10>::all_z().lv()};
    sc_core::sc_signal_resolved cb_ical_x4_hvz{"cb_ical_x4_hvz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved tie_low_hv{"tie_low_hv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved cb_en_int{"cb_en_int", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved cb_cal_en_hv{"cb_cal_en_hv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<4> cb_cal_repl_hvz{"cb_cal_repl_hvz", vl::logic<4>::all_z().lv()};
    sc_core::sc_signal_resolved nbias{"nbias", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved vbg_prog{"vbg_prog", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved cb_sel_lptx_hvz{"cb_sel_lptx_hvz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved cb_repl{"cb_repl", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved cb_en_45_ohm_hv{"cb_en_45_ohm_hv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved cb_en_up_1st_hvz{"cb_en_up_1st_hvz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved vref_cal{"vref_cal", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved vref_1000{"vref_1000", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<2> cb_sel_vref_lprx_hvz{"cb_sel_vref_lprx_hvz", vl::logic<2>::all_z().lv()};
    sc_core::sc_signal_resolved cb_ibias_en_hv{"cb_ibias_en_hv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved vref_31{"vref_31", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved atb_en_hv{"atb_en_hv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<10> dac_value_hvz{"dac_value_hvz", vl::logic<10>::all_z().lv()};
    sc_core::sc_signal_resolved cb_rext_iocont_en_hv{"cb_rext_iocont_en_hv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved cb_rext_iocont_en_hvz{"cb_rext_iocont_en_hvz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved cb_cal_en_hvz{"cb_cal_en_hvz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved cb_en_up_1st_hv{"cb_en_up_1st_hv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved cb_on_hvz{"cb_on_hvz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved vref_bias{"vref_bias", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved cb_lpibias_en_hvz{"cb_lpibias_en_hvz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<2> cb_sel_vrefcd_lprx_hvz{"cb_sel_vrefcd_lprx_hvz", vl::logic<2>::all_z().lv()};
    sc_core::sc_signal_rv<2> atb_sel_hv{"atb_sel_hv", vl::logic<2>::all_z().lv()};
    sc_core::sc_signal_resolved cb_ical_x4_hv{"cb_ical_x4_hv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved cb_ibias_en_hvz{"cb_ibias_en_hvz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<2> atb_sel_hvz{"atb_sel_hvz", vl::logic<2>::all_z().lv()};
    sc_core::sc_signal_rv<3> cb_sel_vbg_prog_hv{"cb_sel_vbg_prog_hv", vl::logic<3>::all_z().lv()};
    sc_core::sc_signal_resolved cb_ibias_en_int{"cb_ibias_en_int", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved rx_pre_pon_hv{"rx_pre_pon_hv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<2> cb_sel_vref_lprx_hv{"cb_sel_vref_lprx_hv", vl::logic<2>::all_z().lv()};
    sc_core::sc_signal_resolved atb_clk_ref_hv{"atb_clk_ref_hv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<4> cb_cal_repl_hv{"cb_cal_repl_hv", vl::logic<4>::all_z().lv()};
    sc_core::sc_signal_resolved cb_en_int_z{"cb_en_int_z", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved rx_pre_pon_hvz{"rx_pre_pon_hvz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved atb_clk_ref_hvz{"atb_clk_ref_hvz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved cb_lpibias_en_hv{"cb_lpibias_en_hv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<10> vctrl_replica{"vctrl_replica", vl::logic<10>::all_z().lv()};
    sc_core::sc_signal_resolved atb_ext_con_hv{"atb_ext_con_hv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved cb_sel_lptx_hv{"cb_sel_lptx_hv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<3> cb_vref_mpll_reg_sel_hv{"cb_vref_mpll_reg_sel_hv", vl::logic<3>::all_z().lv()};
    sc_core::sc_signal_resolved cb_on_hv{"cb_on_hv", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYCB_preamp_bias> bias_preamp;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHY_iocont_buffer> IOcont;
    // rppwl  R560( .b(gd), .in(vref_1000_int), .out(gd) );
    // rppwl  R561( .b(gd), .in(vctrl_replica), .out(gd) );
    // rppwl  R562( .b(gd), .in(cb_en_up_1st_hv), .out(gd) );
    // rppwl  R563( .b(vph), .in(cb_en_up_1st_hvz), .out(vph) );
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYCBRX_bias> bias;
    sc_core::sc_signal_rv<4> c_bias_cb_10u_ibias_ddl{"c_bias_cb_10u_ibias_ddl", vl::logic<4>::all_z().lv()};
    sc_core::sc_signal_rv<10> c_bias_cb_5u_ilprx{"c_bias_cb_5u_ilprx", vl::logic<10>::all_z().lv()};
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYATB> dphyatb;
    // DPHYCBRX_rext_drv  I593( .enz_rx_hv(enz_rx_hv),
    // .lb_setzrx_hv(lb_setzrx_hv), .rex(rext), .vph(vph) );
    // DPHYCBRX_cal  cal( .cb_cal_en_hv(cb_cal_en_hv),
    // .cb_cal_repl_hv(cb_cal_repl_hv), .cb_ical_x4_hv(cb_ical_x4_hv),
    // .en_45_ohm_hv(cb_en_45_ohm_hv), .rext(rext), .rext_2(vsense),
    // .vref_cal(vref_cal), .cb_repl(cb_repl), .rext_esd(rext_esd),
    // .gd(gd), .vph(vph) );
    // DPHYCB_vgen  vgen( .cb_bg_en(cb_bg_en),
    // .cb_burn_oscilator_int(cb_burn_oscilator_int),
    // .cb_cal_en_up_hv(tie_low_hv), .cb_cal_en_up_hvz(tie_high_hv),
    // .cb_chop_clk(cb_chop_clk), .cb_chop_clk_en(cb_chop_clk_en),
    // .cb_lpibias_en_hv(cb_lpibias_en_hv), .cb_on(cb_on),
    // .cb_sel_chop_clk(cb_sel_chop_clk), .cb_sel_lptx_hv(cb_sel_lptx_hv),
    // .cb_sel_lptx_hvz(cb_sel_lptx_hvz),
    // .cb_sel_vbg_prog_hv(cb_sel_vbg_prog_hv),
    // .cb_sel_vbg_prog_hvz(cb_sel_vbg_prog_hvz),
    // .cb_sel_vref_lprx_hv(cb_sel_vref_lprx_hv),
    // .cb_sel_vrefcd_lprx_hv(cb_sel_vrefcd_lprx_hv),
    // .cb_vbe_on(atb_vbe_sel),
    // .cb_vref_mpll_reg_sel_hv(cb_vref_mpll_reg_sel_hv),
    // .atb_vbe_out(atb_line), .cb_lptx(cb_vreflptx),
    // .cb_vref_lprx(cb_vref_lprx), .cb_vref_mpll_reg(cb_vref_mpll_reg),
    // .cb_vrefcd_lprx(cb_vrefcd_lprx), .vbg_prog(vbg_prog),
    // .vref_1000(vref_1000), .vref_15(vref_bias), .vref_31(vref_31),
    // .vref_cal(vref_cal), .gd(gd), .vdd(vdd), .vph(vph), .vss(vss) );
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYCBRX_lc> levelshifters;

    void merge_cb_10u_ibias_ddl();
    void merge_cb_5u_ilprx();
    vl::logic<4> rd_cb_10u_ibias_ddl() const;
    vl::logic<10> rd_cb_5u_ilprx() const;
    void assign_supply_ok();
    void assign_enable();
    void assign_cb_vref_lprx();
    void assign_cb_vrefcd_lprx();
    void assign_cb_vreflptx();
    void assign_cb_5u_ilprx();
    void assign_cb_10u_ilptx();
    void assign_cb_10u_ibias_ddl();
    void assign_cb_vref_mpll_reg_int();
    void assign_cb_vref_mpll_reg();
    void assign_vctrl_replica_int();
    void assign_vctrl_replica_int_calibration();
    void assign_vref_bias();
    void split_bias_cb_10u_ibias_ddl();
    void split_bias_cb_5u_ilprx();
    void proc_initial_146331();
    void proc_always_146336();
    void proc_always_146350();
};

#endif
