//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_if
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_if.h"

mipi_4_rx_dphy_ns_if::mipi_4_rx_dphy_ns_if(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    tie0__drv.o(tie0_);
    SC_METHOD(assign_rstzrx_clklane);
    sensitive << resetz << enableclk;
    SC_METHOD(assign_enable_clklane);
    sensitive << enableclk << enable_lanes;
    SC_METHOD(assign_polarity_clklane);
    sensitive << polarity_clklane_int;
    SC_METHOD(assign_atb_sel_clklane);
    sensitive << atb_sel_clklane_tester;
    SC_METHOD(assign_rxbyteclkhs);
    sensitive << rxbyteclkhs_if;
    SC_METHOD(assign_rxclkactivehs);
    sensitive << rxclkactivehs_clklane;
    SC_METHOD(assign_stopstateclk);
    sensitive << stopstaterx_clklane;
    SC_METHOD(assign_rxulpsclknot);
    sensitive << rxulpsclknot_clklane;
    SC_METHOD(assign_txrequestesc_lane0);
    sensitive << txrequestesc_0;
    SC_METHOD(assign_txlpdtesc_lane0);
    sensitive << txlpdtesc_0;
    SC_METHOD(assign_txulpsexit_lane0);
    sensitive << txulpsexit_0;
    SC_METHOD(assign_txulpsesc_lane0);
    sensitive << txulpsesc_0;
    SC_METHOD(assign_txtriggeresc_lane0);
    sensitive << txtriggeresc_0;
    SC_METHOD(assign_txdataesc_lane0);
    sensitive << txdataesc_0;
    SC_METHOD(assign_txvalidesc_lane0);
    sensitive << txvalidesc_0;
    SC_METHOD(assign_turnrequest_lane0);
    sensitive << turnrequest_0;
    SC_METHOD(assign_turndisable_lane0);
    sensitive << turndisable_0;
    SC_METHOD(assign_forcerxmode_lane0);
    sensitive << forcerxmode_0;
    SC_METHOD(assign_forcetxstopmode_lane0);
    sensitive << forcetxstopmode_0;
    SC_METHOD(assign_enable_lane0);
    sensitive << enable_0 << enable_lanes;
    SC_METHOD(assign_basedir_lane0);
    sensitive << basedir_0;
    SC_METHOD(assign_polarity_lane0);
    sensitive << polarity_lane0_int;
    SC_METHOD(assign_tafailsafez_lane0);
    sensitive << tafailsafez_int;
    SC_METHOD(assign_rx0_txlp_atb_lptx1200_on_if);
    sensitive << atb_lptx1200_on_lane0_tester;
    SC_METHOD(assign_atb_sel_lane0);
    sensitive << atb_sel_lane0_tester;
    SC_METHOD(assign_rx0_rxhs_feed_int_clk_if);
    sensitive << rx0_rxhs_feed_int_clk_ovr_en << rx0_rxhs_feed_int_clk_ovr << rxlane_feed_int_clk_ifpowerctrl << enable_0;
    SC_METHOD(assign_txreadyesc_0);
    sensitive << txreadyesc_lane0;
    SC_METHOD(assign_rxdatahs_0);
    sensitive << rxdatahs_lane0;
    SC_METHOD(assign_rxvalidhs_0);
    sensitive << rxvalidhs_lane0 << rxskewcalhs_if;
    SC_METHOD(assign_rxactivehs_0);
    sensitive << rxactivehs_lane0;
    SC_METHOD(assign_rxsynchs_0);
    sensitive << rxsynchs_lane0;
    SC_METHOD(assign_errsoths_0);
    sensitive << errsoths_lane0;
    SC_METHOD(assign_errsotsynchs_0);
    sensitive << errsotsynchs_lane0;
    SC_METHOD(assign_rxclkesc_0);
    sensitive << rxclkesc_lane0;
    SC_METHOD(assign_rxlpdtesc_0);
    sensitive << rxlpdtesc_lane0;
    SC_METHOD(assign_rxulpsesc_0);
    sensitive << rxulpsesc_lane0;
    SC_METHOD(assign_rxtriggeresc_0);
    sensitive << rxtriggeresc_lane0;
    SC_METHOD(assign_rxdataesc_0);
    sensitive << rxdataesc_lane0;
    SC_METHOD(assign_rxvalidesc_0);
    sensitive << rxvalidesc_lane0;
    SC_METHOD(assign_direction_0);
    sensitive << direction_lane0;
    SC_METHOD(assign_stopstatedata_0);
    sensitive << stopstatedata_0_int;
    SC_METHOD(assign_ulpsactivenot_0);
    sensitive << ulpsactivenot_0_int;
    SC_METHOD(assign_erresc_0);
    sensitive << erresc_lane0;
    SC_METHOD(assign_errsyncesc_0);
    sensitive << errsyncesc_lane0;
    SC_METHOD(assign_errcontrol_0);
    sensitive << errcontrol_lane0;
    SC_METHOD(assign_errcontentionlp0_0);
    sensitive << errcontentionlp0_lane0;
    SC_METHOD(assign_errcontentionlp1_0);
    sensitive << errcontentionlp1_lane0;
    SC_METHOD(assign_rstzrx_lane1);
    sensitive << resetz << enable_1;
    SC_METHOD(assign_enable_lane1);
    sensitive << enable_1 << enable_lanes;
    SC_METHOD(assign_polarity_lane1);
    sensitive << polarity_lane1_int;
    SC_METHOD(assign_forcerxmode_lane1);
    sensitive << forcerxmode_1;
    SC_METHOD(assign_atb_sel_lane1);
    sensitive << atb_sel_lane1_tester;
    SC_METHOD(assign_rx1_rxhs_feed_int_clk_if);
    sensitive << rx1_rxhs_feed_int_clk_ovr_en << rx1_rxhs_feed_int_clk_ovr << rxlane_feed_int_clk_ifpowerctrl << enable_1;
    SC_METHOD(assign_rxdatahs_1);
    sensitive << rxdatahs_lane1;
    SC_METHOD(assign_rxvalidhs_1);
    sensitive << rxvalidhs_lane1 << rxskewcalhs_if;
    SC_METHOD(assign_rxactivehs_1);
    sensitive << rxactivehs_lane1;
    SC_METHOD(assign_rxsynchs_1);
    sensitive << rxsynchs_lane1;
    SC_METHOD(assign_rxclkesc_1);
    sensitive << rxclkesc_lane1;
    SC_METHOD(assign_rxlpdtesc_1);
    sensitive << rxlpdtesc_lane1;
    SC_METHOD(assign_rxulpsesc_1);
    sensitive << rxulpsesc_lane1;
    SC_METHOD(assign_rxtriggeresc_1);
    sensitive << rxtriggeresc_lane1;
    SC_METHOD(assign_rxdataesc_1);
    sensitive << rxdataesc_lane1;
    SC_METHOD(assign_rxvalidesc_1);
    sensitive << rxvalidesc_lane1;
    SC_METHOD(assign_errsoths_1);
    sensitive << errsoths_lane1;
    SC_METHOD(assign_errsotsynchs_1);
    sensitive << errsotsynchs_lane1;
    SC_METHOD(assign_erresc_1);
    sensitive << erresc_lane1;
    SC_METHOD(assign_errsyncesc_1);
    sensitive << errsyncesc_lane1;
    SC_METHOD(assign_errcontrol_1);
    sensitive << errcontrol_lane1;
    SC_METHOD(assign_stopstatedata_1);
    sensitive << stopstaterx_lane1;
    SC_METHOD(assign_ulpsactivenot_1);
    sensitive << ulpsactivenotrx_lane1;
    SC_METHOD(assign_rstzrx_lane2);
    sensitive << resetz << enable_2;
    SC_METHOD(assign_enable_lane2);
    sensitive << enable_2 << enable_lanes;
    SC_METHOD(assign_polarity_lane2);
    sensitive << polarity_lane2_int;
    SC_METHOD(assign_forcerxmode_lane2);
    sensitive << forcerxmode_2;
    SC_METHOD(assign_atb_sel_lane2);
    sensitive << atb_sel_lane2_tester;
    SC_METHOD(assign_rx2_rxhs_feed_int_clk_if);
    sensitive << rx2_rxhs_feed_int_clk_ovr_en << rx2_rxhs_feed_int_clk_ovr << rxlane_feed_int_clk_ifpowerctrl << enable_2;
    SC_METHOD(assign_rxdatahs_2);
    sensitive << rxdatahs_lane2;
    SC_METHOD(assign_rxvalidhs_2);
    sensitive << rxvalidhs_lane2 << rxskewcalhs_if;
    SC_METHOD(assign_rxactivehs_2);
    sensitive << rxactivehs_lane2;
    SC_METHOD(assign_rxsynchs_2);
    sensitive << rxsynchs_lane2;
    SC_METHOD(assign_rxclkesc_2);
    sensitive << rxclkesc_lane2;
    SC_METHOD(assign_rxlpdtesc_2);
    sensitive << rxlpdtesc_lane2;
    SC_METHOD(assign_rxulpsesc_2);
    sensitive << rxulpsesc_lane2;
    SC_METHOD(assign_rxtriggeresc_2);
    sensitive << rxtriggeresc_lane2;
    SC_METHOD(assign_rxdataesc_2);
    sensitive << rxdataesc_lane2;
    SC_METHOD(assign_rxvalidesc_2);
    sensitive << rxvalidesc_lane2;
    SC_METHOD(assign_errsoths_2);
    sensitive << errsoths_lane2;
    SC_METHOD(assign_errsotsynchs_2);
    sensitive << errsotsynchs_lane2;
    SC_METHOD(assign_erresc_2);
    sensitive << erresc_lane2;
    SC_METHOD(assign_errsyncesc_2);
    sensitive << errsyncesc_lane2;
    SC_METHOD(assign_errcontrol_2);
    sensitive << errcontrol_lane2;
    SC_METHOD(assign_stopstatedata_2);
    sensitive << stopstaterx_lane2;
    SC_METHOD(assign_ulpsactivenot_2);
    sensitive << ulpsactivenotrx_lane2;
    SC_METHOD(assign_rstzrx_lane3);
    sensitive << resetz << enable_3;
    SC_METHOD(assign_enable_lane3);
    sensitive << enable_3 << enable_lanes;
    SC_METHOD(assign_polarity_lane3);
    sensitive << polarity_lane3_int;
    SC_METHOD(assign_forcerxmode_lane3);
    sensitive << forcerxmode_3;
    SC_METHOD(assign_atb_sel_lane3);
    sensitive << atb_sel_lane3_tester;
    SC_METHOD(assign_rx3_rxhs_feed_int_clk_if);
    sensitive << rx3_rxhs_feed_int_clk_ovr_en << rx3_rxhs_feed_int_clk_ovr << rxlane_feed_int_clk_ifpowerctrl << enable_3;
    SC_METHOD(assign_rxdatahs_3);
    sensitive << rxdatahs_lane3;
    SC_METHOD(assign_rxvalidhs_3);
    sensitive << rxvalidhs_lane3 << rxskewcalhs_if;
    SC_METHOD(assign_rxactivehs_3);
    sensitive << rxactivehs_lane3;
    SC_METHOD(assign_rxsynchs_3);
    sensitive << rxsynchs_lane3;
    SC_METHOD(assign_rxclkesc_3);
    sensitive << rxclkesc_lane3;
    SC_METHOD(assign_rxlpdtesc_3);
    sensitive << rxlpdtesc_lane3;
    SC_METHOD(assign_rxulpsesc_3);
    sensitive << rxulpsesc_lane3;
    SC_METHOD(assign_rxtriggeresc_3);
    sensitive << rxtriggeresc_lane3;
    SC_METHOD(assign_rxdataesc_3);
    sensitive << rxdataesc_lane3;
    SC_METHOD(assign_rxvalidesc_3);
    sensitive << rxvalidesc_lane3;
    SC_METHOD(assign_errsoths_3);
    sensitive << errsoths_lane3;
    SC_METHOD(assign_errsotsynchs_3);
    sensitive << errsotsynchs_lane3;
    SC_METHOD(assign_erresc_3);
    sensitive << erresc_lane3;
    SC_METHOD(assign_errsyncesc_3);
    sensitive << errsyncesc_lane3;
    SC_METHOD(assign_errcontrol_3);
    sensitive << errcontrol_lane3;
    SC_METHOD(assign_stopstatedata_3);
    sensitive << stopstaterx_lane3;
    SC_METHOD(assign_ulpsactivenot_3);
    sensitive << ulpsactivenotrx_lane3;
    SC_METHOD(assign_cont_en_if);
    sensitive << cont_en;
    SC_METHOD(assign_cont_data);
    sensitive << cont_data_if;
    SC_METHOD(assign_cfgclk_if);
    sensitive << cfg_clk;
    SC_METHOD(assign_rstz_if);
    sensitive << resetz;
    SC_METHOD(assign_txclkesc_if);
    sensitive << txclkesc;
    SC_METHOD(assign_rxskewcalhs);
    sensitive << rxskewcalhs_if;
    SC_METHOD(assign_biston_lanes);
    sensitive << biston_int;
    SC_METHOD(assign_ttasure_datalanes);
    sensitive << ttasure_datalanes_int;
    SC_METHOD(assign_ttago_datalanes);
    sensitive << ttago_datalanes_int;
    SC_METHOD(assign_ttawait_datalanes);
    sensitive << ttawait_datalanes_int;
    SC_METHOD(assign_rx_timebase_if);
    sensitive << timebase_ovr_en << timebase_ovr << cfgclkfreqrange;
    SC_METHOD(assign_enable_lanes);
    sensitive << enable_lanes_ovr_en << enable_lanes_ovr << enable_lanes_int;
    SC_METHOD(assign_timebase_cal);
    sensitive << rx_timebase_if;
    SC_METHOD(assign_rx_cb_on_if);
    sensitive << cb_on_ovr_en << cb_on_ovr << bintpon_ifpowerctrl;
    SC_METHOD(assign_rx_cb_sel_vrefcd_lprx_if);
    sensitive << cb_sel_vrefcd_lprx_int;
    SC_METHOD(assign_rx_cb_sel_vref_lprx_if);
    sensitive << cb_sel_vref_lprx_int;
    SC_METHOD(assign_rx_cb_bg_en_if);
    sensitive << cb_bg_en_ovr_en << cb_bg_en_ovr << bintpon_ifpowerctrl;
    SC_METHOD(assign_rx_cb_chop_clk_en_if);
    sensitive << cb_chop_clk_en_int;
    SC_METHOD(assign_rx_cb_sel_chop_clk_if);
    sensitive << cb_sel_chop_clk_int;
    SC_METHOD(assign_rx_cb_chop_clk_if);
    sensitive << cfg_clk;
    SC_METHOD(assign_rx_cb_sel_v400_prog_if);
    sensitive << cb_sel_v400_prog_int;
    SC_METHOD(assign_rx_cb_lpibias_en_if);
    sensitive << cb_lpibias_en_ovr_en << cb_lpibias_en_ovr << bintpon_ifpowerctrl;
    SC_METHOD(assign_rx_cb_sel_vreflptx_prog_if);
    sensitive << cb_sel_vreflptx_prog_int;
    SC_METHOD(assign_rx_atb_on_if);
    sensitive << cb_atb_on_ovr_en << cb_atb_on_ovr << cb_ical_x4_int << cb_atb_on_rcal_int;
    SC_METHOD(assign_rx_atb_comp_clk_if_tester);
    sensitive << cb_atb_comp_clk_ovr_en << cb_atb_comp_clk_ovr << rescal_en << cal_clock_comp << cb_atb_comp_clk_int;
    SC_METHOD(assign_rx_atb_comp_clk_if);
    sensitive << rx_atb_comp_clk_if_tester << shutdownz;
    SC_METHOD(assign_rx_atb_ext_con_if);
    sensitive << cb_atb_ext_con_int;
    SC_METHOD(assign_rx_cb_atb_sel_if);
    sensitive << cb_atb_sel_int;
    SC_METHOD(assign_rx_atb_vbe_sel_if);
    sensitive << cb_atb_vbe_sel_int;
    SC_METHOD(assign_rx_cb_cal_en_if);
    sensitive << cb_cal_en_ovr_en << cb_cal_en_ovr << cb_cal_en_ifpowerctrl;
    SC_METHOD(assign_rx_cb_ical_x4_if);
    sensitive << cb_ical_x4_ovr_en << cb_ical_x4_ovr << cb_ical_x4_int;
    SC_METHOD(assign_rx_cb_dac_prog_if);
    sensitive << cb_dac_prog_ovr_en << cb_dac_prog_ovr << cb_dac_prog_int;
    SC_METHOD(assign_rx_cb_en_45_ohm_if);
    sensitive << cb_en_45_ohm_int;
    SC_METHOD(assign_resetz);
    sensitive << rstz << shutdownz;
    SC_METHOD(assign_cfg_clk_scanable);
    sensitive << cfg_clk;
    SC_METHOD(assign_testclk_scanable);
    sensitive << testclk;
    SC_METHOD(assign_testclr_scanable);
    sensitive << scan_mode << scan_set_rst_n << testclr;
    SC_METHOD(assign_aresetz_scanable);
    sensitive << scan_mode << scan_set_rst_n << resetz;
    SC_METHOD(assign_rx_ddl_offset_error_if);
    sensitive << rx_ddl_offset_error_tester;
    SC_METHOD(assign_rx0_ddl_start_calibration_if);
    sensitive << rx0_ddl_start_calibration_ovr_en << rx0_ddl_start_calibration_ovr << ddltunning_en_ifpowerctrl << enable_0;
    SC_METHOD(assign_rx0_rxhs_ddl_tune_en_if);
    sensitive << rx0_rxhs_ddl_tune_en_tester;
    SC_METHOD(assign_rx0_ddl_finished_int);
    sensitive << rx0_ddl_finished_ovr_en << rx0_ddl_finished_ovr << rx0_ddl_finished_if;
    SC_METHOD(assign_rx1_ddl_start_calibration_if);
    sensitive << rx1_ddl_start_calibration_ovr_en << rx1_ddl_start_calibration_ovr << ddltunning_en_ifpowerctrl << enable_1;
    SC_METHOD(assign_rx1_rxhs_ddl_tune_en_if);
    sensitive << rx1_rxhs_ddl_tune_en_tester;
    SC_METHOD(assign_rx1_ddl_finished_int);
    sensitive << rx1_ddl_finished_ovr_en << rx1_ddl_finished_ovr << rx1_ddl_finished_if;
    SC_METHOD(assign_rx2_ddl_start_calibration_if);
    sensitive << rx2_ddl_start_calibration_ovr_en << rx2_ddl_start_calibration_ovr << ddltunning_en_ifpowerctrl << enable_2;
    SC_METHOD(assign_rx2_rxhs_ddl_tune_en_if);
    sensitive << rx2_rxhs_ddl_tune_en_tester;
    SC_METHOD(assign_rx2_ddl_finished_int);
    sensitive << rx2_ddl_finished_ovr_en << rx2_ddl_finished_ovr << rx2_ddl_finished_if;
    SC_METHOD(assign_rx3_ddl_start_calibration_if);
    sensitive << rx3_ddl_start_calibration_ovr_en << rx3_ddl_start_calibration_ovr << ddltunning_en_ifpowerctrl << enable_3;
    SC_METHOD(assign_rx3_rxhs_ddl_tune_en_if);
    sensitive << rx3_rxhs_ddl_tune_en_tester;
    SC_METHOD(assign_rx3_ddl_finished_int);
    sensitive << rx3_ddl_finished_ovr_en << rx3_ddl_finished_ovr << rx3_ddl_finished_if;
    SC_METHOD(assign_global_ddltunning_done);
    sensitive << rx0_ddl_finished_int << enable_0 << rx1_ddl_finished_int << enable_1 << rx2_ddl_finished_int << enable_2 << rx3_ddl_finished_int << enable_3;
    SC_METHOD(assign_rx_rxlp_bias_prog_if);
    sensitive << rx_rxlp_bias_prog_int;
    SC_METHOD(assign_adc_done_scan);
    sensitive << adc_done_int << scan_mode;
    SC_METHOD(assign_rx_rxhs_compatibility_mode_if);
    sensitive << rx_rxhs_compatibility_mode << shutdownz;
    SC_METHOD(assign_deskew_legacyz);
    sensitive << deskew_legacyz_ovr_en << deskew_legacyz_ovr << rx_rxhs_compatibility_mode;
    SC_METHOD(assign_burnin_cal_en_if);
    sensitive << burnin_cal_en_ovr_en_sync << burnin_cal_en_ovr_sync << burnin_cal_en_int;
    SC_METHOD(assign_stability_time);
    sensitive << stability_time_tester << rx_timebase_if;
    SC_METHOD(assign_rxclk_rxhs_osc_pon_if);
    sensitive << rxclk_rxhs_osc_pon_ovr_en << rxclk_rxhs_osc_pon_ovr << rxclk_rxhs_osc_pon_int;
    SC_METHOD(assign_rxclk_rxhs_osc_res_en_if);
    sensitive << rxclk_rxhs_osc_res_en_ovr_en << rxclk_rxhs_osc_res_en_ovr << rxclk_rxhs_osc_res_en_int;
    SC_METHOD(assign_burnin_cal_done_int);
    sensitive << norexttune_done_ovr_en << norexttune_done_ovr << burnin_cal_done_norext_machine;
    SC_METHOD(assign_rx_cb_cal_repl_if_sum);
    sensitive << rx_cb_cal_repl_if;
    SC_METHOD(assign_rxclk_start_calibration);
    sensitive << rxclk_start_calibration_ovr_en_sync << rxclk_start_calibration_ovr_sync << hsrxencal_offset_ifpowerctrl << enableclk;
    SC_METHOD(assign_rxclk_rxhs_clk_offset_cal_en_if);
    sensitive << rxclk_rxhs_start_calibration_ovr_en_sync << rxclk_rxhs_start_calibration_ovr_sync << clklane_startcal_int;
    SC_METHOD(assign_hsrxpon_if_clklane);
    sensitive << hsrxpon_bypass_clklane << hsrxpon_clklane_tester << hsrxpon_clklane_ifpowerctrl << enableclk << do_offs_and_skew_enableclk_not;
    SC_METHOD(assign_hsrx_en_bypass_clklane);
    sensitive << hsrx_en_if_bypass_clklane_tester;
    SC_METHOD(assign_hsrx_en_if_clklane);
    sensitive << hsrx_en_bypass_clklane << hsrx_en_if_clklane_tester << des_en_for_datalane_offset_cal_ifpowerctrl << clklane_startcal_int << enableclk << do_offs_and_skew_enableclk_not;
    SC_METHOD(assign_clk_en_lanes_bypass);
    sensitive << clk_en_lanes_bypass_tester;
    SC_METHOD(assign_clk_en_lanes);
    sensitive << clk_en_lanes_bypass << clk_en_lanes_tester << des_en_for_datalane_offset_cal_ifpowerctrl << enableclk << do_offs_and_skew_enableclk_not;
    SC_METHOD(assign_rxclk_rxhs_clk_offset_cal_if);
    sensitive << rxclk_rxhs_clk_offset_cal_int << shutdownz;
    SC_METHOD(assign_rxclk_caldone);
    sensitive << rxclk_caldone_ovr_en << rxclk_caldone_ovr << caldone_clklane_int << enableclk;
    SC_METHOD(assign_rx0_rxhs_data_int);
    sensitive << rx0_rxhs_data_ovr_en << rx0_rxhs_data_ovr << rx0_rxhs_data_if;
    SC_METHOD(assign_rx0_rxhs_data_offset_cal_en_if);
    sensitive << rx0_rxhs_data_offset_cal_en_ovr_en << rx0_rxhs_data_offset_cal_en_ovr << rx0_rxhs_data_offset_cal_en_int << enable_0;
    SC_METHOD(assign_rx0_rxhs_data_offset_cal_if);
    sensitive << rx0_rxhs_data_offset_cal_ovr_en << rx0_rxhs_data_offset_cal_ovr << rx0_rxhs_data_offset_cal_int << enable_0 << shutdownz;
    SC_METHOD(assign_hsrx_des_en_bypass_lane0);
    sensitive << rx_des_en_bypass_lane0_tester;
    SC_METHOD(assign_hsrx_des_en_if_lane0);
    sensitive << hsrx_des_en_bypass_lane0 << rx_des_en_lane0_tester << des_en_for_datalane_offset_cal_ifpowerctrl << enable_0;
    SC_METHOD(assign_hsrxpon_if_lane0);
    sensitive << hsrxpon_bypass_lane0 << hsrxpon_lane0_tester << hsrxpon_datalane_ifpowerctrl << enable_0;
    SC_METHOD(assign_rx0_start_calibration_int);
    sensitive << rx0_start_calibration_ovr_en << rx0_start_calibration_ovr << hsrxencal_lane_offset_ifpowerctrl << enable_0 << direction_lane0_sync;
    SC_METHOD(assign_rx0_cal_done);
    sensitive << rx0_cal_done_ovr_en << rx0_cal_done_ovr << rx0_cal_done_int;
    SC_METHOD(assign_hsrx_en_bypass_lane0);
    sensitive << hsrx_en_if_bypass_lane0_tester;
    SC_METHOD(assign_hsrx_en_if_lane0);
    sensitive << hsrx_en_bypass_lane0 << hsrx_en_if_lane0_tester << des_en_for_datalane_offset_cal_ifpowerctrl << enable_0;
    SC_METHOD(assign_rx1_rxhs_data_int);
    sensitive << rx1_rxhs_data_ovr_en << rx1_rxhs_data_ovr << rx1_rxhs_data_if;
    SC_METHOD(assign_rx1_rxhs_data_offset_cal_en_if);
    sensitive << rx1_rxhs_data_offset_cal_en_ovr_en << rx1_rxhs_data_offset_cal_en_ovr << rx1_rxhs_data_offset_cal_en_int << enable_1;
    SC_METHOD(assign_rx1_rxhs_data_offset_cal_if);
    sensitive << rx1_rxhs_data_offset_cal_ovr_en << rx1_rxhs_data_offset_cal_ovr << rx1_rxhs_data_offset_cal_int << enable_1 << shutdownz;
    SC_METHOD(assign_hsrx_des_en_bypass_lane1);
    sensitive << rx_des_en_bypass_lane1_tester;
    SC_METHOD(assign_hsrx_des_en_if_lane1);
    sensitive << hsrx_des_en_bypass_lane1 << rx_des_en_lane1_tester << des_en_for_datalane_offset_cal_ifpowerctrl << enable_1;
    SC_METHOD(assign_hsrxpon_if_lane1);
    sensitive << hsrxpon_bypass_lane1 << hsrxpon_lane1_tester << hsrxpon_datalane_ifpowerctrl << enable_1;
    SC_METHOD(assign_rx1_start_calibration_int);
    sensitive << rx1_start_calibration_ovr_en << rx1_start_calibration_ovr << hsrxencal_lane_offset_ifpowerctrl << enable_1;
    SC_METHOD(assign_rx1_cal_done);
    sensitive << rx1_cal_done_ovr_en << rx1_cal_done_ovr << rx1_cal_done_int;
    SC_METHOD(assign_hsrx_en_bypass_lane1);
    sensitive << hsrx_en_if_bypass_lane1_tester;
    SC_METHOD(assign_hsrx_en_if_lane1);
    sensitive << hsrx_en_bypass_lane1 << hsrx_en_if_lane1_tester << des_en_for_datalane_offset_cal_ifpowerctrl << enable_1;
    SC_METHOD(assign_rx2_rxhs_data_int);
    sensitive << rx2_rxhs_data_ovr_en << rx2_rxhs_data_ovr << rx2_rxhs_data_if;
    SC_METHOD(assign_rx2_rxhs_data_offset_cal_en_if);
    sensitive << rx2_rxhs_data_offset_cal_en_ovr_en << rx2_rxhs_data_offset_cal_en_ovr << rx2_rxhs_data_offset_cal_en_int << enable_2;
    SC_METHOD(assign_rx2_rxhs_data_offset_cal_if);
    sensitive << rx2_rxhs_data_offset_cal_ovr_en << rx2_rxhs_data_offset_cal_ovr << rx2_rxhs_data_offset_cal_int << enable_2 << shutdownz;
    SC_METHOD(assign_hsrx_des_en_bypass_lane2);
    sensitive << rx_des_en_bypass_lane2_tester;
    SC_METHOD(assign_hsrx_des_en_if_lane2);
    sensitive << hsrx_des_en_bypass_lane2 << rx_des_en_lane2_tester << des_en_for_datalane_offset_cal_ifpowerctrl << enable_2;
    SC_METHOD(assign_hsrxpon_if_lane2);
    sensitive << hsrxpon_bypass_lane2 << hsrxpon_lane2_tester << hsrxpon_datalane_ifpowerctrl << enable_2;
    SC_METHOD(assign_rx2_start_calibration_int);
    sensitive << rx2_start_calibration_ovr_en << rx2_start_calibration_ovr << hsrxencal_lane_offset_ifpowerctrl << enable_2;
    SC_METHOD(assign_rx2_cal_done);
    sensitive << rx2_cal_done_ovr_en << rx2_cal_done_ovr << rx2_cal_done_int;
    SC_METHOD(assign_hsrx_en_bypass_lane2);
    sensitive << hsrx_en_if_bypass_lane2_tester;
    SC_METHOD(assign_hsrx_en_if_lane2);
    sensitive << hsrx_en_bypass_lane2 << hsrx_en_if_lane2_tester << des_en_for_datalane_offset_cal_ifpowerctrl << enable_2;
    SC_METHOD(assign_rx3_rxhs_data_int);
    sensitive << rx3_rxhs_data_ovr_en << rx3_rxhs_data_ovr << rx3_rxhs_data_if;
    SC_METHOD(assign_rx3_rxhs_data_offset_cal_en_if);
    sensitive << rx3_rxhs_data_offset_cal_en_ovr_en << rx3_rxhs_data_offset_cal_en_ovr << rx3_rxhs_data_offset_cal_en_int << enable_3;
    SC_METHOD(assign_rx3_rxhs_data_offset_cal_if);
    sensitive << rx3_rxhs_data_offset_cal_ovr_en << rx3_rxhs_data_offset_cal_ovr << rx3_rxhs_data_offset_cal_int << enable_3 << shutdownz;
    SC_METHOD(assign_hsrx_des_en_bypass_lane3);
    sensitive << rx_des_en_bypass_lane3_tester;
    SC_METHOD(assign_hsrx_des_en_if_lane3);
    sensitive << hsrx_des_en_bypass_lane3 << rx_des_en_lane3_tester << des_en_for_datalane_offset_cal_ifpowerctrl << enable_3;
    SC_METHOD(assign_hsrxpon_if_lane3);
    sensitive << hsrxpon_bypass_lane3 << hsrxpon_lane3_tester << hsrxpon_datalane_ifpowerctrl << enable_3;
    SC_METHOD(assign_rx3_start_calibration_int);
    sensitive << rx3_start_calibration_ovr_en << rx3_start_calibration_ovr << hsrxencal_lane_offset_ifpowerctrl << enable_3;
    SC_METHOD(assign_rx3_cal_done);
    sensitive << rx3_cal_done_ovr_en << rx3_cal_done_ovr << rx3_cal_done_int;
    SC_METHOD(assign_hsrx_en_bypass_lane3);
    sensitive << hsrx_en_if_bypass_lane3_tester;
    SC_METHOD(assign_hsrx_en_if_lane3);
    sensitive << hsrx_en_bypass_lane3 << hsrx_en_if_lane3_tester << des_en_for_datalane_offset_cal_ifpowerctrl << enable_3;
    SC_METHOD(assign_rstztxlp_lane0);
    sensitive << resetz << enable_0 << direction_lane0;
    SC_METHOD(assign_rstzrx_lane0);
    sensitive << resetz << enable_0 << direction_lane0;
    SC_METHOD(assign_rstzta_lane0);
    sensitive << resetz << enable_0;
    SC_METHOD(assign_bintpon_bypass_clklane);
    sensitive << bintpon_bypass_clklane_tester;
    SC_METHOD(assign_hsrxpon_bypass_clklane);
    sensitive << hsrxpon_bypass_clklane_tester;
    SC_METHOD(assign_lptxpon_bypass_lane0);
    sensitive << lptxpon_bypass_lane0_tester << hsdirect;
    SC_METHOD(assign_lptxena_bypass_lane0);
    sensitive << lptxena_bypass_lane0_tester;
    SC_METHOD(assign_lptxdin_bypass_lane0);
    sensitive << lptxdin_bypass_lane0_tester;
    SC_METHOD(assign_lptxenpulldwn_bypass_lane0);
    sensitive << lptxenpulldwn_bypass_lane0_tester;
    SC_METHOD(assign_lptxenulp_bypass_lane0);
    sensitive << lptxenulp_bypass_lane0_tester << hsdirect;
    SC_METHOD(assign_bintpon_bypass_lane0);
    sensitive << bintpon_bypass_lane0_tester;
    SC_METHOD(assign_lprxponlp_bypass_lane0);
    sensitive << lprxponlp_bypass_lane0_tester;
    SC_METHOD(assign_lprxponulp_bypass_lane0);
    sensitive << lprxponulp_bypass_lane0_tester;
    SC_METHOD(assign_lprxponcd_bypass_lane0);
    sensitive << lprxponcd_bypass_lane0_tester;
    SC_METHOD(assign_hsrxpon_bypass_lane0);
    sensitive << hsrxpon_bypass_lane0_tester;
    SC_METHOD(assign_bintpon_bypass_lane1);
    sensitive << bintpon_bypass_lane1_tester;
    SC_METHOD(assign_hsrxpon_bypass_lane1);
    sensitive << hsrxpon_bypass_lane1_tester;
    SC_METHOD(assign_bintpon_bypass_lane2);
    sensitive << bintpon_bypass_lane2_tester;
    SC_METHOD(assign_hsrxpon_bypass_lane2);
    sensitive << hsrxpon_bypass_lane2_tester;
    SC_METHOD(assign_bintpon_bypass_lane3);
    sensitive << bintpon_bypass_lane3_tester;
    SC_METHOD(assign_hsrxpon_bypass_lane3);
    sensitive << hsrxpon_bypass_lane3_tester;
    SC_METHOD(assign_skew_back_comp_en);
    sensitive << skew_back_comp_en_ovr_en << skew_back_comp_en_ovr << skew_back_comp_en_ifpowerctrl;
    SC_METHOD(assign_bintpon_if_clklane);
    sensitive << bintpon_bypass_clklane_tester << bintpon_clklane_tester << bintpon_ifpowerctrl << enableclk << do_offs_and_skew_enableclk_not;
    SC_METHOD(assign_lprxponlp_if_clklane);
    sensitive << lprxponlp_clklane_tester;
    SC_METHOD(assign_lprxponulp_if_clklane);
    sensitive << lprxponulp_clklane_tester;
    SC_METHOD(assign_lprxponcd_if_clklane);
    sensitive << lprxponcd_clklane_tester;
    SC_METHOD(assign_lprxponlp_bypass_clklane);
    sensitive << lprxponlp_bypass_clklane_tester;
    SC_METHOD(assign_lprxponulp_bypass_clklane);
    sensitive << lprxponulp_bypass_clklane_tester;
    SC_METHOD(assign_lprxponcd_bypass_clklane);
    sensitive << lprxponcd_bypass_clklane_tester;
    SC_METHOD(assign_bintpon_if_lane0);
    sensitive << bintpon_bypass_lane0_tester << bintpon_lane0_tester << bintpon_ifpowerctrl << enable_0;
    SC_METHOD(assign_lprxponlp_if_lane0);
    sensitive << lprxponlp_lane0_tester;
    SC_METHOD(assign_lprxponulp_if_lane0);
    sensitive << lprxponulp_lane0_tester;
    SC_METHOD(assign_lprxponcd_if_lane0);
    sensitive << lprxponcd_lane0_tester;
    SC_METHOD(assign_lptxpon_if_lane0);
    sensitive << lptxpon_bypass_lane0_tester << lptxpon_lane0_tester << hsdirect << lptxpon_ifpowerctrl << enable_0;
    SC_METHOD(assign_lptxena_if_lane0);
    sensitive << lptxena_bypass_lane0_tester << lptxena_lane0_tester << lptxena_ifpowerctrl << enable_0 << direction_lane0;
    SC_METHOD(assign_lptxdin_if_lane0);
    sensitive << lptxdin_bypass_lane0_tester << lptxdin_lane0_tester << lptxdin_ifpowerctrl << enable_0 << direction_lane0;
    SC_METHOD(assign_lptxenpulldn_if_lane0);
    sensitive << lptxenpulldwn_bypass_lane0_tester << lptxenpulldwn_lane0_tester << lptxenpulldwn_ifpowerctrl << enable_0 << direction_lane0;
    SC_METHOD(assign_lptxenulp_if_lane0);
    sensitive << lptxenulp_bypass_lane0_tester << lptxenulp_lane0_tester;
    SC_METHOD(assign_bintpon_if_lane1);
    sensitive << bintpon_bypass_lane1_tester << bintpon_lane1_tester << bintpon_ifpowerctrl << enable_1;
    SC_METHOD(assign_lprxponlp_if_lane1);
    sensitive << lprxponlp_lane1_tester;
    SC_METHOD(assign_lprxponulp_if_lane1);
    sensitive << lprxponulp_lane1_tester;
    SC_METHOD(assign_lprxponcd_if_lane1);
    sensitive << lprxponcd_lane1_tester;
    SC_METHOD(assign_lprxponlp_bypass_lane1);
    sensitive << lprxponlp_bypass_lane1_tester;
    SC_METHOD(assign_lprxponulp_bypass_lane1);
    sensitive << lprxponulp_bypass_lane1_tester;
    SC_METHOD(assign_lprxponcd_bypass_lane1);
    sensitive << lprxponcd_bypass_lane1_tester;
    SC_METHOD(assign_bintpon_if_lane2);
    sensitive << bintpon_bypass_lane2_tester << bintpon_lane2_tester << bintpon_ifpowerctrl << enable_2;
    SC_METHOD(assign_lprxponlp_if_lane2);
    sensitive << lprxponlp_lane2_tester;
    SC_METHOD(assign_lprxponulp_if_lane2);
    sensitive << lprxponulp_lane2_tester;
    SC_METHOD(assign_lprxponcd_if_lane2);
    sensitive << lprxponcd_lane2_tester;
    SC_METHOD(assign_lprxponlp_bypass_lane2);
    sensitive << lprxponlp_bypass_lane2_tester;
    SC_METHOD(assign_lprxponulp_bypass_lane2);
    sensitive << lprxponulp_bypass_lane2_tester;
    SC_METHOD(assign_lprxponcd_bypass_lane2);
    sensitive << lprxponcd_bypass_lane2_tester;
    SC_METHOD(assign_bintpon_if_lane3);
    sensitive << bintpon_bypass_lane3_tester << bintpon_lane3_tester << bintpon_ifpowerctrl << enable_3;
    SC_METHOD(assign_lprxponlp_if_lane3);
    sensitive << lprxponlp_lane3_tester;
    SC_METHOD(assign_lprxponulp_if_lane3);
    sensitive << lprxponulp_lane3_tester;
    SC_METHOD(assign_lprxponcd_if_lane3);
    sensitive << lprxponcd_lane3_tester;
    SC_METHOD(assign_lprxponlp_bypass_lane3);
    sensitive << lprxponlp_bypass_lane3_tester;
    SC_METHOD(assign_lprxponulp_bypass_lane3);
    sensitive << lprxponulp_bypass_lane3_tester;
    SC_METHOD(assign_lprxponcd_bypass_lane3);
    sensitive << lprxponcd_bypass_lane3_tester;
    SC_METHOD(assign_global_offset_cal_done);
    sensitive << rx0_cal_done << enable_0 << direction_lane0 << rx1_cal_done << enable_1 << rx2_cal_done << enable_2 << rx3_cal_done << enable_3;
    SC_METHOD(assign_txulpsexit_global);
    sensitive << txulpsexit_lane0 << direction_lane0 << enable_0;
    SC_METHOD(assign_ulpsactivenot_global);
    sensitive << ulpsactivenotclk << enableclk << ulpsactivenot_0 << enable_0 << ulpsactivenot_1 << enable_1 << ulpsactivenot_2 << enable_2 << ulpsactivenot_3 << enable_3;
    SC_METHOD(assign_global_skew_cal_done);
    sensitive << rx0_deskewcaldone_if << enable_0 << rx1_deskewcaldone_if << enable_1 << rx2_deskewcaldone_if << enable_2 << rx3_deskewcaldone_if << enable_3;
    SC_METHOD(assign_rxclk_rxhs_int_clk_sel_if);
    sensitive << rxclk_rxhs_int_clk_sel_int;
    SC_METHOD(assign_perform_skew);
    sensitive << skew_mux_sel << skew_mux_run << deskew_legacyz;
    SC_METHOD(assign_hsfreqrange_int);
    sensitive << hsfreqrange_ovr_en << hsfreqrange_ovr << hsfreqrange;
    SC_METHOD(assign_rx0_osc_freq_target_if);
    sensitive << rx0_osc_freq_target_if_ovr_en << rx0_osc_freq_target_if_ovr << ddl_osc_freq_target;
    SC_METHOD(assign_rx1_osc_freq_target_if);
    sensitive << rx1_osc_freq_target_if_ovr_en << rx1_osc_freq_target_if_ovr << ddl_osc_freq_target;
    SC_METHOD(assign_rx2_osc_freq_target_if);
    sensitive << rx2_osc_freq_target_if_ovr_en << rx2_osc_freq_target_if_ovr << ddl_osc_freq_target;
    SC_METHOD(assign_rx3_osc_freq_target_if);
    sensitive << rx3_osc_freq_target_if_ovr_en << rx3_osc_freq_target_if_ovr << ddl_osc_freq_target;
    SC_METHOD(assign_tclk_miss_opt);
    sensitive << tclk_miss_div2_ovr_en << tclk_miss_div2_ovr << tclk_miss_div2 << tclk_miss_opt_int;
    SC_METHOD(assign_pll_n_int);
    sensitive << pll_n_ovr_en << pll_n_ovr;
    SC_METHOD(assign_rx_rxhs_gmode_if);
    sensitive << rx_rxhs_gmode_int << shutdownz;
    SC_METHOD(assign_rxclk_rxhs_ddr_clk_div_if);
    sensitive << rxclk_rxhs_ddr_clk_div_int << shutdownz;
    SC_METHOD(assign_arstz_scanable);
    sensitive << scan_mode << scan_set_rst_n;
    SC_METHOD(assign_pll_refclk);
    sensitive << scan_mode << refclk;
    SC_METHOD(assign_pll_pwron);
    sensitive << scan_mode << pll_pwron_ovr_en << pll_pwron_ovr << pll_pwron_int;
    SC_METHOD(assign_pll_reset);
    sensitive << scan_mode << pll_reset_ovr_en << pll_reset_ovr << pll_reset_int;
    SC_METHOD(assign_pll_gear_shift);
    sensitive << scan_mode << pll_gear_shift_ovr_en << pll_gear_shift_ovr << pll_gear_shift_int;
    SC_METHOD(assign_pll_opmode);
    sensitive << scan_mode << pll_opmode_int;
    SC_METHOD(assign_pll_lock_sel);
    sensitive << scan_mode << pll_lock_sel_int;
    SC_METHOD(assign_pll_th1);
    sensitive << scan_mode << pll_th1_int;
    SC_METHOD(assign_pll_th2);
    sensitive << scan_mode << pll_th2_int;
    SC_METHOD(assign_pll_th3);
    sensitive << scan_mode << pll_th3_int;
    SC_METHOD(assign_pll_th_delay);
    sensitive << scan_mode << pll_th_delay_int;
    SC_METHOD(assign_pll_lock_det_on);
    sensitive << scan_mode << pll_lock_det_on_ovr_en << pll_lock_det_on_ovr << pll_lock_det_on_int;
    SC_METHOD(assign_pll_lockdet_mode);
    sensitive << scan_mode << pll_lockdet_mode_int;
    SC_METHOD(assign_pll_tstplldig);
    sensitive << scan_mode << pll_tstplldig_int;
    SC_METHOD(assign_pll_meas_iv);
    sensitive << scan_mode << pll_meas_iv_int;
    SC_METHOD(assign_pll_mpll_prog);
    sensitive << scan_mode << pll_mpll_prog_int;
    SC_METHOD(assign_pll_atb_sense_sel);
    sensitive << scan_mode << pll_atb_sense_sel_int;
    SC_METHOD(assign_pll_vco_cntrl);
    sensitive << scan_mode << pll_vco_cntrl_int << shutdownz;
    SC_METHOD(assign_pll_prop_cntrl);
    sensitive << scan_mode << pll_prop_cntrl_int;
    SC_METHOD(assign_pll_int_cntrl);
    sensitive << scan_mode << pll_int_cntrl_int;
    SC_METHOD(assign_pll_gmp_cntrl);
    sensitive << scan_mode << pll_gmp_cntrl_int;
    SC_METHOD(assign_pll_cpbias_cntrl);
    sensitive << scan_mode << pll_cpbias_cntrl_int;
    SC_METHOD(assign_pll_n);
    sensitive << scan_mode << pll_n_int << shutdownz;
    SC_METHOD(assign_pll_m);
    sensitive << scan_mode << pll_m_int << shutdownz;
    SC_METHOD(assign_pll_force_lock);
    sensitive << scan_mode << pll_force_lock_int;
    SC_METHOD(assign_pll_testlock_int);
    sensitive << scan_mode << scantie << pll_testlock;
    SC_METHOD(assign_pll_lock_int);
    sensitive << scan_mode << scantie << pll_lock_ovr_en << pll_lock_ovr << pll_lock;
    SC_METHOD(assign_pll_clkouten_left);
    sensitive << scan_mode << pll_clkouten_left_int;
    SC_METHOD(assign_pll_clkouten_right);
    sensitive << scan_mode << pll_clkouten_right_int;
    SC_METHOD(assign_pll_vpl_det_int);
    sensitive << scan_mode << scantie << pll_vpl_det;
    SC_METHOD(assign_lane_bist_to_run);
    sensitive << bist_run_lane3 << bist_run_lane2 << bist_run_lane1 << bist_run_lane0;
    SC_METHOD(assign_biston_int);
    sensitive << biston << bist_manual << enable_lanes;
    SC_METHOD(assign_fsm_trigger_int);
    sensitive << pll_lock_int_sync << pll_startup_done;
    SC_METHOD(assign_fsm_trigger);
    sensitive << fsm_trigger_ovr_en << fsm_trigger_ovr << fsm_trigger_int;
    SC_METHOD(assign_del_datalane_int);
    sensitive << rx_timebase_if;
    SC_METHOD(assign_del_datalane);
    sensitive << del_datalane_ovr_en << del_datalane_ovr << del_datalane_int;
    SC_METHOD(assign_rx0_pm_sample_error_counter_if);
    sensitive << rx0_pm_man_error_latch << rx0_pm_sample_error_counter_int << bistdone;
    SC_METHOD(assign_rx1_pm_sample_error_counter_if);
    sensitive << rx1_pm_man_error_latch << rx1_pm_sample_error_counter_int << bistdone;
    SC_METHOD(assign_rx2_pm_sample_error_counter_if);
    sensitive << rx2_pm_man_error_latch << rx2_pm_sample_error_counter_int << bistdone;
    SC_METHOD(assign_rx3_pm_sample_error_counter_if);
    sensitive << rx3_pm_man_error_latch << rx3_pm_sample_error_counter_int << bistdone;
    SC_METHOD(assign_rxclk_lbck_clk_en_if);
    sensitive << rxclk_lbck_clk_en_ovr_en << rxclk_lbck_clk_en_ovr << rxclk_lbck_clk_en_int;
    SC_METHOD(assign_rxclk_lbck_div_en_if);
    sensitive << rxclk_lbck_div_en_ovr_en << rxclk_lbck_div_en_ovr << rxclk_lbck_div_en_int;
    SC_METHOD(assign_rxclk_lbck_serial_clk_en_if);
    sensitive << rxclk_lbck_serial_clk_en_ovr_en << rxclk_lbck_serial_clk_en_ovr << rxclk_lbck_serial_clk_en_int;
    SC_METHOD(assign_rxclk_lbck_serial_en_if);
    sensitive << rxclk_lbck_serial_en_ovr_en << rxclk_lbck_serial_en_ovr << rxclk_lbck_serial_en_int;
    SC_METHOD(assign_rxclk_lbdata_clk_en_if);
    sensitive << rxclk_lbdata_clk_en_ovr_en << rxclk_lbdata_clk_en_ovr << rxclk_lbdata_clk_en_int;
    SC_METHOD(assign_rxclk_lbdata_div_en_if);
    sensitive << rxclk_lbdata_div_en_ovr_en << rxclk_lbdata_div_en_ovr << rxclk_lbdata_div_en_int;
    SC_METHOD(assign_rxclk_lbdata_serial_clk_en_if);
    sensitive << rxclk_lbdata_serial_clk_en_ovr_en << rxclk_lbdata_serial_clk_en_ovr << rxclk_lbdata_serial_clk_en_int;
    SC_METHOD(assign_rxclk_lbdata_serial_en_if);
    sensitive << rxclk_lbdata_serial_en_ovr_en << rxclk_lbdata_serial_en_ovr << rxclk_lbdata_serial_en_int;
    SC_METHOD(assign_rxclk_lb_sel_if);
    sensitive << rxclk_lb_sel_ovr_en << rxclk_lb_sel_ovr << rxclk_lb_sel_int;
    SC_METHOD(assign_rx0_pm_start_control_if);
    sensitive << rx0_pm_start_control_ovr_en << rx0_pm_start_control_ovr << bist_sync_en_int << bist_run_lane0;
    SC_METHOD(assign_rx1_pm_start_control_if);
    sensitive << rx1_pm_start_control_ovr_en << rx1_pm_start_control_ovr << bist_sync_en_int << bist_run_lane1;
    SC_METHOD(assign_rx2_pm_start_control_if);
    sensitive << rx2_pm_start_control_ovr_en << rx2_pm_start_control_ovr << bist_sync_en_int << bist_run_lane2;
    SC_METHOD(assign_rx3_pm_start_control_if);
    sensitive << rx3_pm_start_control_ovr_en << rx3_pm_start_control_ovr << bist_sync_en_int << bist_run_lane3;
    SC_METHOD(assign_hsdirect);
    sensitive << trigger_hsdirect_int << hsdirect_int;
    SC_METHOD(assign_hsdirect_if);
    sensitive << hsdirect << enable_lanes;
    SC_METHOD(assign_onpll);
    sensitive << onpll_ovr_en << onpll_ovr << pll_boot_from_start << pll_boot_fsm_int << onpll_int;
    SC_METHOD(assign_pll_clksel_en);
    sensitive << scan_mode << pll_clksel_en_ovr_en << pll_clksel_en_ovr << pll_clksel_en_int;
    SC_METHOD(assign_rx_rxhs_prog_int);
    sensitive << rx_rxhs_prog_mux_pwron_int;
    SC_METHOD(assign_mastermacro);
    sensitive << mastermacro_int;
    SC_METHOD(assign_phy_launch_des_en_out);
    sensitive << launch_des_en_out;
    SC_METHOD(assign_launch_des_en_in);
    sensitive << phy_launch_des_en_in;
    SC_METHOD(assign_phy_rxclk_rxhs_clk_en_out);
    sensitive << rxclk_rxhs_clk_en_out;
    SC_METHOD(assign_rxclk_rxhs_clk_en_in);
    sensitive << phy_rxclk_rxhs_clk_en_in;
    SC_METHOD(assign_phy_rxclk_rxhs_rx_en_out);
    sensitive << rxclk_rxhs_rx_en_out;
    SC_METHOD(assign_rxclk_rxhs_rx_en_in);
    sensitive << phy_rxclk_rxhs_rx_en_in;
    SC_METHOD(assign_phy_rxclk_rxhs_rx_pon_out);
    sensitive << rxclk_rxhs_rx_pon_out;
    SC_METHOD(assign_rxclk_rxhs_rx_pon_in);
    sensitive << phy_rxclk_rxhs_rx_pon_in;
    SC_METHOD(assign_phy_rxclk_rxhs_rx_enlogic_out);
    sensitive << rxclk_rxhs_rx_enlogic_out;
    SC_METHOD(assign_rxclk_rxhs_rx_enlogic_in);
    sensitive << phy_rxclk_rxhs_rx_enlogic_in;
    SC_METHOD(assign_rx_rxhs_prog_if);
    sensitive << rx_rxhs_prog_ovr_en << rx_rxhs_prog_ovr << mastermacro << rx_rxhs_prog_int << phy_rx_rxhs_prog_in;
    SC_METHOD(assign_phy_rx_rxhs_prog_out);
    sensitive << rx_rxhs_prog_int;
    SC_METHOD(assign_rxclk_rxhs_feed_int_clk_if);
    sensitive << rxclk_rxhs_feed_int_clk_ovr_en << rxclk_rxhs_feed_int_clk_ovr << mastermacro << rxclk_feed_int_clk_ifpowerctrl << phy_rxclk_rxhs_feed_int_clk_in << enableclk << do_offs_and_skew_enableclk_not;
    SC_METHOD(assign_phy_rxclk_rxhs_feed_int_clk_out);
    sensitive << rxclk_feed_int_clk_ifpowerctrl << enableclk << do_offs_and_skew_enableclk_not;
    SC_METHOD(assign_ulpsactivenotclk);
    sensitive << mastermacro << ulpsactivenotrx_clklane << phy_ulpsactivenotclk_in;
    SC_METHOD(glue_testclkinv_scanmux_d0);
    sensitive << testclk;
    // assign testclkinv_scanable          = scanmode ? scanclk  :   ~testclk                          ;
    // gen_mux testclkinv_scanmux (.z(testclkinv_scanable),  .sel(scanmode), .d0(~testclk), .d1(scanclk) );
    testclkinv_scanmux.reset(new mipi_4_rx_dphy_ns_gen_mux("testclkinv_scanmux"));
    testclkinv_scanmux->z(testclkinv_scanable);
    testclkinv_scanmux->sel(scan_mode);
    testclkinv_scanmux->d0(c_testclkinv_scanmux_d0);
    testclkinv_scanmux->d1(testclk);
    sresetz_cfg_clk_sync.reset(new mipi_4_rx_dphy_ns_gen_sync_rst("sresetz_cfg_clk_sync"));
    sresetz_cfg_clk_sync->clk(cfg_clk_scanable);
    sresetz_cfg_clk_sync->rst(aresetz_scanable);
    sresetz_cfg_clk_sync->srst(sresetz_cfg_clk);
    sresetz_cfg_clk_scanmux.reset(new mipi_4_rx_dphy_ns_gen_mux("sresetz_cfg_clk_scanmux"));
    sresetz_cfg_clk_scanmux->z(sresetz_cfg_clk_scanable);
    sresetz_cfg_clk_scanmux->sel(scan_mode);
    sresetz_cfg_clk_scanmux->d0(sresetz_cfg_clk);
    sresetz_cfg_clk_scanmux->d1(scan_set_rst_n);
    // Synchronizations
    gen_direction_lane0_sync.reset(new mipi_4_rx_dphy_ns_gen_sync__reset_edge0("gen_direction_lane0_sync"));
    gen_direction_lane0_sync->clk(cfg_clk_scanable);
    gen_direction_lane0_sync->rst(sresetz_cfg_clk_scanable);
    gen_direction_lane0_sync->data_in(direction_lane0);
    gen_direction_lane0_sync->scanmode(tie0_);
    gen_direction_lane0_sync->data_out(direction_lane0_sync);
    gen_direction_lane0_sync->data_out_dly(nc_gen_direction_lane0_sync_data_out_dly);
    SC_METHOD(glue_gen_stopstatetx_lane0_sync_pulse_in);
    sensitive << scan_mode << stopstatetx_lane0;
    gen_stopstatetx_lane0_sync.reset(new mipi_4_rx_dphy_ns_static_pulse_catcher__reset_edge0("gen_stopstatetx_lane0_sync"));
    gen_stopstatetx_lane0_sync->pulse_in(c_gen_stopstatetx_lane0_sync_pulse_in);
    gen_stopstatetx_lane0_sync->clk(cfg_clk_scanable);
    gen_stopstatetx_lane0_sync->rst(sresetz_cfg_clk_scanable);
    gen_stopstatetx_lane0_sync->clk_cap(cfg_clk_scanable);
    gen_stopstatetx_lane0_sync->rst_cap(sresetz_cfg_clk_scanable);
    gen_stopstatetx_lane0_sync->pulse_out(stopstatetx_lane0_sync);
    gen_stopstatetx_lane0_sync->pulse_in_ff(nc_gen_stopstatetx_lane0_sync_pulse_in_ff);
    SC_METHOD(glue_gen_stopstaterx_lane0_sync_pulse_in);
    sensitive << scan_mode << stopstaterx_lane0;
    gen_stopstaterx_lane0_sync.reset(new mipi_4_rx_dphy_ns_static_pulse_catcher__reset_edge0("gen_stopstaterx_lane0_sync"));
    gen_stopstaterx_lane0_sync->pulse_in(c_gen_stopstaterx_lane0_sync_pulse_in);
    gen_stopstaterx_lane0_sync->clk(cfg_clk_scanable);
    gen_stopstaterx_lane0_sync->rst(sresetz_cfg_clk_scanable);
    gen_stopstaterx_lane0_sync->clk_cap(cfg_clk_scanable);
    gen_stopstaterx_lane0_sync->rst_cap(sresetz_cfg_clk_scanable);
    gen_stopstaterx_lane0_sync->pulse_out(stopstaterx_lane0_sync);
    gen_stopstaterx_lane0_sync->pulse_in_ff(nc_gen_stopstaterx_lane0_sync_pulse_in_ff);
    SC_METHOD(glue_gen_ulpsactivenottx_lane0_sync_pulse_in);
    sensitive << scan_mode << ulpsactivenottx_lane0;
    gen_ulpsactivenottx_lane0_sync.reset(new mipi_4_rx_dphy_ns_static_pulse_catcher__reset_edge0("gen_ulpsactivenottx_lane0_sync"));
    gen_ulpsactivenottx_lane0_sync->pulse_in(c_gen_ulpsactivenottx_lane0_sync_pulse_in);
    gen_ulpsactivenottx_lane0_sync->clk(cfg_clk_scanable);
    gen_ulpsactivenottx_lane0_sync->rst(sresetz_cfg_clk_scanable);
    gen_ulpsactivenottx_lane0_sync->clk_cap(cfg_clk_scanable);
    gen_ulpsactivenottx_lane0_sync->rst_cap(sresetz_cfg_clk_scanable);
    gen_ulpsactivenottx_lane0_sync->pulse_out(ulpsactivenottx_lane0_sync);
    gen_ulpsactivenottx_lane0_sync->pulse_in_ff(nc_gen_ulpsactivenottx_lane0_sync_pulse_in_ff);
    SC_METHOD(glue_gen_ulpsactivenotrx_lane0_sync_pulse_in);
    sensitive << scan_mode << ulpsactivenotrx_lane0;
    gen_ulpsactivenotrx_lane0_sync.reset(new mipi_4_rx_dphy_ns_static_pulse_catcher__reset_edge0("gen_ulpsactivenotrx_lane0_sync"));
    gen_ulpsactivenotrx_lane0_sync->pulse_in(c_gen_ulpsactivenotrx_lane0_sync_pulse_in);
    gen_ulpsactivenotrx_lane0_sync->clk(cfg_clk_scanable);
    gen_ulpsactivenotrx_lane0_sync->rst(sresetz_cfg_clk_scanable);
    gen_ulpsactivenotrx_lane0_sync->clk_cap(cfg_clk_scanable);
    gen_ulpsactivenotrx_lane0_sync->rst_cap(sresetz_cfg_clk_scanable);
    gen_ulpsactivenotrx_lane0_sync->pulse_out(ulpsactivenotrx_lane0_sync);
    gen_ulpsactivenotrx_lane0_sync->pulse_in_ff(nc_gen_ulpsactivenotrx_lane0_sync_pulse_in_ff);
    clkterm_mux.reset(new mipi_4_rx_dphy_ns_gen_mux("clkterm_mux"));
    clkterm_mux->z(cal_clock_comp);
    clkterm_mux->sel(scan_mode);
    clkterm_mux->d0(cal_clock);
    clkterm_mux->d1(cfg_clk_scanable);
    SC_METHOD(glue_I01dphytermcal_timertermcal);
    // Analog Comparator control muxing : termcal(up/down) or ADC machines performed
    I01dphytermcal.reset(new mipi_4_rx_dphy_ns_dphytermcal("I01dphytermcal"));
    I01dphytermcal->cfg_clk(cfg_clk_scanable);
    I01dphytermcal->clock1m(cal_clock_comp);
    I01dphytermcal->resetz(sresetz_cfg_clk_scanable);
    I01dphytermcal->cal_en(rescal_en);
    I01dphytermcal->cal_round_max(tie0_);
    I01dphytermcal->timertermcal(c_I01dphytermcal_timertermcal);
    I01dphytermcal->cmpout(rx_cb_comp_out_if);
    I01dphytermcal->setrd(setr_termcal_rext);
    I01dphytermcal->cal_error(rescal_error);
    I01dphytermcal->cal_done(rescal_done);
    I01dphytermcal->rtunpon(cb_ical_x4_int);
    cfg_clk_div2_mux.reset(new mipi_4_rx_dphy_ns_gen_mux("cfg_clk_div2_mux"));
    cfg_clk_div2_mux->z(cfg_clk_div2_scanable);
    cfg_clk_div2_mux->sel(scan_mode);
    cfg_clk_div2_mux->d0(cfg_clk_div2);
    cfg_clk_div2_mux->d1(cfg_clk_scanable);
    SC_METHOD(glue_I01_rcal_async_reset);
    sensitive << sresetz_cfg_clk_scanable;
    SC_METHOD(glue_I01_rcal_d_tx_term);
    // RCAL ADC
    I01_rcal.reset(new mipi_4_rx_dphy_ns_rcal_adc("I01_rcal"));
    I01_rcal->async_reset(c_I01_rcal_async_reset);
    I01_rcal->ck_ref_s(cfg_clk_div2_scanable);
    I01_rcal->rcal_enb(tie0_);
    I01_rcal->adc_enb(adc_en);
    I01_rcal->d_tx_term(c_I01_rcal_d_tx_term);
    I01_rcal->sup_comp_rt_result(rx_cb_comp_out_if);
    I01_rcal->rcal_adc_done(adc_done_int);
    I01_rcal->sup_comp_mode_i(nc_I01_rcal_sup_comp_mode_i);
    I01_rcal->sup_comp_rt_pwron_i(cb_atb_on_rcal_int);
    I01_rcal->sup_comp_rt_r_i(cb_atb_comp_clk_int);
    I01_rcal->sup_dac(nc_I01_rcal_sup_dac);
    I01_rcal->rescal_rep_i(nc_I01_rcal_rescal_rep_i);
    I01_rcal->tx_rescal_i(nc_I01_rcal_tx_rescal_i);
    I01_rcal->adc_val(cb_dac_prog_int);
    gen_adc_done_clear_sync.reset(new mipi_4_rx_dphy_ns_gen_sync__reset_edge0("gen_adc_done_clear_sync"));
    gen_adc_done_clear_sync->clk(cfg_clk_scanable);
    gen_adc_done_clear_sync->rst(sresetz_cfg_clk_scanable);
    gen_adc_done_clear_sync->data_in(adc_done_clear);
    gen_adc_done_clear_sync->scanmode(tie0_);
    gen_adc_done_clear_sync->data_out(adc_done_clear_sync);
    gen_adc_done_clear_sync->data_out_dly(nc_gen_adc_done_clear_sync_data_out_dly);
    osc_in_mux.reset(new mipi_4_rx_dphy_ns_gen_mux("osc_in_mux"));
    osc_in_mux->z(rxclk_rxhs_osc_out_scanable);
    osc_in_mux->sel(scan_mode);
    osc_in_mux->d0(rxclk_rxhs_osc_out_if);
    osc_in_mux->d1(scan_rxclk_rxhs_osc_out_if_clk);
    resetz_rxclk_rxhs_osc_out_if_sync.reset(new mipi_4_rx_dphy_ns_gen_sync_rst("resetz_rxclk_rxhs_osc_out_if_sync"));
    resetz_rxclk_rxhs_osc_out_if_sync->clk(rxclk_rxhs_osc_out_scanable);
    resetz_rxclk_rxhs_osc_out_if_sync->rst(aresetz_scanable);
    resetz_rxclk_rxhs_osc_out_if_sync->srst(resetz_rxclk_rxhs_osc_out_if);
    rstz_osc_in_mux.reset(new mipi_4_rx_dphy_ns_gen_mux("rstz_osc_in_mux"));
    rstz_osc_in_mux->z(resetz_rxclk_rxhs_osc_out_if_scanable);
    rstz_osc_in_mux->sel(scan_mode);
    rstz_osc_in_mux->d0(resetz_rxclk_rxhs_osc_out_if);
    rstz_osc_in_mux->d1(scan_set_rst_n);
    SC_METHOD(glue_I01dphytune_timebase);
    sensitive << rx_timebase_if;
    // Termination tune algorithm without external resistor
    I01dphytune.reset(new mipi_4_rx_dphy_ns_dphynorexttune("I01dphytune"));
    I01dphytune->cfg_clk(cfg_clk_scanable);
    I01dphytune->rstz(sresetz_cfg_clk_scanable);
    I01dphytune->osc_in(rxclk_rxhs_osc_out_scanable);
    I01dphytune->resetz_osc_in(resetz_rxclk_rxhs_osc_out_if_scanable);
    I01dphytune->timebase(c_I01dphytune_timebase);
    I01dphytune->setr_max(setr_max);
    I01dphytune->setr_typ(setr_typ);
    I01dphytune->setr_min(setr_min);
    I01dphytune->count_ref(count_ref);
    I01dphytune->stability_time(stability_time);
    I01dphytune->start_tune(burnin_cal_en_if);
    I01dphytune->osc_pon(rxclk_rxhs_osc_pon_int);
    I01dphytune->osc_en(rxclk_rxhs_osc_res_en_int);
    I01dphytune->setr(setr_burnin_int);
    I01dphytune->tune_done(burnin_cal_done_norext_machine);
    I01dphytune->count_osc0(count_osc0);
    I01dphytune->count_osc1(count_osc1);
    I01dphytune->noext_burn_error(noext_burn_error);
    gen_rxclk_start_calibration_ovr_sync.reset(new mipi_4_rx_dphy_ns_gen_sync__reset_edge0("gen_rxclk_start_calibration_ovr_sync"));
    gen_rxclk_start_calibration_ovr_sync->clk(cfg_clk_scanable);
    gen_rxclk_start_calibration_ovr_sync->rst(sresetz_cfg_clk_scanable);
    gen_rxclk_start_calibration_ovr_sync->data_in(rxclk_start_calibration_ovr);
    gen_rxclk_start_calibration_ovr_sync->scanmode(tie0_);
    gen_rxclk_start_calibration_ovr_sync->data_out(rxclk_start_calibration_ovr_sync);
    gen_rxclk_start_calibration_ovr_sync->data_out_dly(nc_gen_rxclk_start_calibration_ovr_sync_data_out_dly);
    gen_rxclk_start_calibration_ovr_en_sync.reset(new mipi_4_rx_dphy_ns_gen_sync__reset_edge0("gen_rxclk_start_calibration_ovr_en_sync"));
    gen_rxclk_start_calibration_ovr_en_sync->clk(cfg_clk_scanable);
    gen_rxclk_start_calibration_ovr_en_sync->rst(sresetz_cfg_clk_scanable);
    gen_rxclk_start_calibration_ovr_en_sync->data_in(rxclk_start_calibration_ovr_en);
    gen_rxclk_start_calibration_ovr_en_sync->scanmode(tie0_);
    gen_rxclk_start_calibration_ovr_en_sync->data_out(rxclk_start_calibration_ovr_en_sync);
    gen_rxclk_start_calibration_ovr_en_sync->data_out_dly(nc_gen_rxclk_start_calibration_ovr_en_sync_data_out_dly);
    gen_rxclk_rxhs_start_calibration_ovr_sync.reset(new mipi_4_rx_dphy_ns_gen_sync__reset_edge0("gen_rxclk_rxhs_start_calibration_ovr_sync"));
    gen_rxclk_rxhs_start_calibration_ovr_sync->clk(cfg_clk_scanable);
    gen_rxclk_rxhs_start_calibration_ovr_sync->rst(sresetz_cfg_clk_scanable);
    gen_rxclk_rxhs_start_calibration_ovr_sync->data_in(rxclk_rxhs_start_calibration_ovr);
    gen_rxclk_rxhs_start_calibration_ovr_sync->scanmode(tie0_);
    gen_rxclk_rxhs_start_calibration_ovr_sync->data_out(rxclk_rxhs_start_calibration_ovr_sync);
    gen_rxclk_rxhs_start_calibration_ovr_sync->data_out_dly(nc_gen_rxclk_rxhs_start_calibration_ovr_sync_data_out_dly);
    gen_rxclk_rxhs_start_calibration_ovr_en_sync.reset(new mipi_4_rx_dphy_ns_gen_sync__reset_edge0("gen_rxclk_rxhs_start_calibration_ovr_en_sync"));
    gen_rxclk_rxhs_start_calibration_ovr_en_sync->clk(cfg_clk_scanable);
    gen_rxclk_rxhs_start_calibration_ovr_en_sync->rst(sresetz_cfg_clk_scanable);
    gen_rxclk_rxhs_start_calibration_ovr_en_sync->data_in(rxclk_rxhs_start_calibration_ovr_en);
    gen_rxclk_rxhs_start_calibration_ovr_en_sync->scanmode(tie0_);
    gen_rxclk_rxhs_start_calibration_ovr_en_sync->data_out(rxclk_rxhs_start_calibration_ovr_en_sync);
    gen_rxclk_rxhs_start_calibration_ovr_en_sync->data_out_dly(nc_gen_rxclk_rxhs_start_calibration_ovr_en_sync_data_out_dly);
    gen_rxclk_rxhs_clk_offset_cal_ovr_en_sync.reset(new mipi_4_rx_dphy_ns_gen_sync__reset_edge0("gen_rxclk_rxhs_clk_offset_cal_ovr_en_sync"));
    gen_rxclk_rxhs_clk_offset_cal_ovr_en_sync->clk(cfg_clk_scanable);
    gen_rxclk_rxhs_clk_offset_cal_ovr_en_sync->rst(sresetz_cfg_clk_scanable);
    gen_rxclk_rxhs_clk_offset_cal_ovr_en_sync->data_in(rxclk_rxhs_clk_offset_cal_ovr_en);
    gen_rxclk_rxhs_clk_offset_cal_ovr_en_sync->scanmode(tie0_);
    gen_rxclk_rxhs_clk_offset_cal_ovr_en_sync->data_out(rxclk_rxhs_clk_offset_cal_ovr_en_sync);
    gen_rxclk_rxhs_clk_offset_cal_ovr_en_sync->data_out_dly(nc_gen_rxclk_rxhs_clk_offset_cal_ovr_en_sync_data_out_dly);
    SC_METHOD(glue_I01dphyhsrxoffscal_hsrxdouta);
    sensitive << rxclk_rxhs_clk_p_cal_if << rxclk_rxhs_clk_m_cal_if;
    // Offset calibration machine for CLOCK LANE
    I01dphyhsrxoffscal.reset(new mipi_4_rx_dphy_ns_dphyhsrxoffscal("I01dphyhsrxoffscal"));
    I01dphyhsrxoffscal->resetz(sresetz_cfg_clk_scanable);
    I01dphyhsrxoffscal->cfg_clk(cfg_clk_scanable);
    I01dphyhsrxoffscal->encal(rxclk_start_calibration);
    I01dphyhsrxoffscal->hsrxdouta(c_I01dphyhsrxoffscal_hsrxdouta);
    I01dphyhsrxoffscal->setoffs_ovr(rxclk_rxhs_clk_offset_cal_ovr);
    I01dphyhsrxoffscal->setoffs_ovr_en(rxclk_rxhs_clk_offset_cal_ovr_en_sync);
    I01dphyhsrxoffscal->startcal(clklane_startcal_int);
    I01dphyhsrxoffscal->setoffs(rxclk_rxhs_clk_offset_cal_int);
    I01dphyhsrxoffscal->caldone(caldone_clklane_int);
    I01dphyhsrxoffscal->errcal(errcal_clklane);
    I01dphyhsrxoffscal->scanmode(scan_mode);
    I01dphyhsrxoffscal->scanclk(cfg_clk_scanable);
    // Offset calibration machine for the datalane
    // gen_mux rx0_rxhs_word_clk_if_scanmux(  .z(rx0_rxhs_word_clk_if_scanable),  .sel(scan_mode), .d0(rx0_rxhs_word_clk_if), .d1(scan_set_rst_n) );
    rx0_rxhs_word_clk_if_scanmux.reset(new mipi_4_rx_dphy_ns_gen_mux("rx0_rxhs_word_clk_if_scanmux"));
    rx0_rxhs_word_clk_if_scanmux->z(rx0_rxhs_word_clk_if_scanable);
    rx0_rxhs_word_clk_if_scanmux->sel(scan_mode);
    rx0_rxhs_word_clk_if_scanmux->d0(rx0_rxhs_word_clk_if);
    rx0_rxhs_word_clk_if_scanmux->d1(scan_rx0_rxhs_word_if_clk);
    sresetz_rx0_rxhs_word_clk_sync.reset(new mipi_4_rx_dphy_ns_gen_sync_rst("sresetz_rx0_rxhs_word_clk_sync"));
    sresetz_rx0_rxhs_word_clk_sync->clk(rx0_rxhs_word_clk_if_scanable);
    sresetz_rx0_rxhs_word_clk_sync->rst(aresetz_scanable);
    sresetz_rx0_rxhs_word_clk_sync->srst(sresetz_rx0_rxhs_word_clk);
    sresetz_rx0_rxhs_word_clk_scanmux.reset(new mipi_4_rx_dphy_ns_gen_mux("sresetz_rx0_rxhs_word_clk_scanmux"));
    sresetz_rx0_rxhs_word_clk_scanmux->z(sresetz_rx0_rxhs_word_clk_scanable);
    sresetz_rx0_rxhs_word_clk_scanmux->sel(scan_mode);
    sresetz_rx0_rxhs_word_clk_scanmux->d0(sresetz_rx0_rxhs_word_clk);
    sresetz_rx0_rxhs_word_clk_scanmux->d1(scan_set_rst_n);
    gen_rx0_rxhs_repeatcal_sync.reset(new mipi_4_rx_dphy_ns_gen_sync__reset_edge0("gen_rx0_rxhs_repeatcal_sync"));
    gen_rx0_rxhs_repeatcal_sync->clk(cfg_clk_scanable);
    gen_rx0_rxhs_repeatcal_sync->rst(sresetz_cfg_clk_scanable);
    gen_rx0_rxhs_repeatcal_sync->data_in(rx0_rxhs_repeatcal);
    gen_rx0_rxhs_repeatcal_sync->scanmode(tie0_);
    gen_rx0_rxhs_repeatcal_sync->data_out(rx0_rxhs_repeatcal_sync);
    gen_rx0_rxhs_repeatcal_sync->data_out_dly(nc_gen_rx0_rxhs_repeatcal_sync_data_out_dly);
    SC_METHOD(glue_I00offset_process_core_slicer_deserializer_0);
    sensitive << rx0_rxhs_data_int;
    SC_METHOD(glue_I00offset_process_core_slicer_deserializer_1);
    sensitive << rx0_rxhs_data_int;
    SC_METHOD(glue_I00offset_process_core_cfgclkfreq);
    sensitive << rx_timebase_if;
    I00offset_process_core.reset(new mipi_4_rx_dphy_ns_offset_process_core("I00offset_process_core"));
    I00offset_process_core->slicer_deserializer_0(c_I00offset_process_core_slicer_deserializer_0);
    I00offset_process_core->slicer_deserializer_1(c_I00offset_process_core_slicer_deserializer_1);
    I00offset_process_core->wordclk(rx0_rxhs_word_clk_if_scanable);
    I00offset_process_core->resetz_wordclk(sresetz_rx0_rxhs_word_clk_scanable);
    I00offset_process_core->resetz_hsclock(sresetz_cfg_clk_scanable);
    I00offset_process_core->start_calibration(rx0_start_calibration_int);
    I00offset_process_core->hsclock(cfg_clk_scanable);
    I00offset_process_core->cal_done(rx0_cal_done_int);
    I00offset_process_core->no_convergence(rx0_cal_error_int);
    I00offset_process_core->control_current_word(rx0_rxhs_data_offset_cal_int);
    I00offset_process_core->cfgclkfreq(c_I00offset_process_core_cfgclkfreq);
    I00offset_process_core->cmp_polarity(cmp_polarity);
    I00offset_process_core->total_ones(rx0_total_ones);
    I00offset_process_core->offset_comp_threshold(offset_comp_threshold);
    I00offset_process_core->tolerance(tolerance);
    I00offset_process_core->number_of_ready_count(number_of_ready_count);
    I00offset_process_core->repeatcal(rx0_rxhs_repeatcal_sync);
    I00offset_process_core->afe_offset_cal_en(rx0_rxhs_data_offset_cal_en_int);
    // gen_mux rx1_rxhs_word_clk_if_scanmux(  .z(rx1_rxhs_word_clk_if_scanable),  .sel(scan_mode), .d0(rx1_rxhs_word_clk_if), .d1(scan_set_rst_n) );
    rx1_rxhs_word_clk_if_scanmux.reset(new mipi_4_rx_dphy_ns_gen_mux("rx1_rxhs_word_clk_if_scanmux"));
    rx1_rxhs_word_clk_if_scanmux->z(rx1_rxhs_word_clk_if_scanable);
    rx1_rxhs_word_clk_if_scanmux->sel(scan_mode);
    rx1_rxhs_word_clk_if_scanmux->d0(rx1_rxhs_word_clk_if);
    rx1_rxhs_word_clk_if_scanmux->d1(scan_rx1_rxhs_word_if_clk);
    sresetz_rx1_rxhs_word_clk_sync.reset(new mipi_4_rx_dphy_ns_gen_sync_rst("sresetz_rx1_rxhs_word_clk_sync"));
    sresetz_rx1_rxhs_word_clk_sync->clk(rx1_rxhs_word_clk_if_scanable);
    sresetz_rx1_rxhs_word_clk_sync->rst(aresetz_scanable);
    sresetz_rx1_rxhs_word_clk_sync->srst(sresetz_rx1_rxhs_word_clk);
    sresetz_rx1_rxhs_word_clk_scanmux.reset(new mipi_4_rx_dphy_ns_gen_mux("sresetz_rx1_rxhs_word_clk_scanmux"));
    sresetz_rx1_rxhs_word_clk_scanmux->z(sresetz_rx1_rxhs_word_clk_scanable);
    sresetz_rx1_rxhs_word_clk_scanmux->sel(scan_mode);
    sresetz_rx1_rxhs_word_clk_scanmux->d0(sresetz_rx1_rxhs_word_clk);
    sresetz_rx1_rxhs_word_clk_scanmux->d1(scan_set_rst_n);
    gen_rx1_rxhs_repeatcal_sync.reset(new mipi_4_rx_dphy_ns_gen_sync__reset_edge0("gen_rx1_rxhs_repeatcal_sync"));
    gen_rx1_rxhs_repeatcal_sync->clk(cfg_clk_scanable);
    gen_rx1_rxhs_repeatcal_sync->rst(sresetz_cfg_clk_scanable);
    gen_rx1_rxhs_repeatcal_sync->data_in(rx1_rxhs_repeatcal);
    gen_rx1_rxhs_repeatcal_sync->scanmode(tie0_);
    gen_rx1_rxhs_repeatcal_sync->data_out(rx1_rxhs_repeatcal_sync);
    gen_rx1_rxhs_repeatcal_sync->data_out_dly(nc_gen_rx1_rxhs_repeatcal_sync_data_out_dly);
    SC_METHOD(glue_I01offset_process_core_slicer_deserializer_0);
    sensitive << rx1_rxhs_data_int;
    SC_METHOD(glue_I01offset_process_core_slicer_deserializer_1);
    sensitive << rx1_rxhs_data_int;
    SC_METHOD(glue_I01offset_process_core_cfgclkfreq);
    sensitive << rx_timebase_if;
    I01offset_process_core.reset(new mipi_4_rx_dphy_ns_offset_process_core("I01offset_process_core"));
    I01offset_process_core->slicer_deserializer_0(c_I01offset_process_core_slicer_deserializer_0);
    I01offset_process_core->slicer_deserializer_1(c_I01offset_process_core_slicer_deserializer_1);
    I01offset_process_core->wordclk(rx1_rxhs_word_clk_if_scanable);
    I01offset_process_core->resetz_wordclk(sresetz_rx1_rxhs_word_clk_scanable);
    I01offset_process_core->resetz_hsclock(sresetz_cfg_clk_scanable);
    I01offset_process_core->start_calibration(rx1_start_calibration_int);
    I01offset_process_core->hsclock(cfg_clk_scanable);
    I01offset_process_core->cal_done(rx1_cal_done_int);
    I01offset_process_core->no_convergence(rx1_cal_error_int);
    I01offset_process_core->control_current_word(rx1_rxhs_data_offset_cal_int);
    I01offset_process_core->cfgclkfreq(c_I01offset_process_core_cfgclkfreq);
    I01offset_process_core->cmp_polarity(cmp_polarity);
    I01offset_process_core->total_ones(rx1_total_ones);
    I01offset_process_core->offset_comp_threshold(offset_comp_threshold);
    I01offset_process_core->tolerance(tolerance);
    I01offset_process_core->number_of_ready_count(number_of_ready_count);
    I01offset_process_core->repeatcal(rx1_rxhs_repeatcal_sync);
    I01offset_process_core->afe_offset_cal_en(rx1_rxhs_data_offset_cal_en_int);
    // gen_mux rx2_rxhs_word_clk_if_scanmux(  .z(rx2_rxhs_word_clk_if_scanable),  .sel(scan_mode), .d0(rx2_rxhs_word_clk_if), .d1(scan_set_rst_n) );
    rx2_rxhs_word_clk_if_scanmux.reset(new mipi_4_rx_dphy_ns_gen_mux("rx2_rxhs_word_clk_if_scanmux"));
    rx2_rxhs_word_clk_if_scanmux->z(rx2_rxhs_word_clk_if_scanable);
    rx2_rxhs_word_clk_if_scanmux->sel(scan_mode);
    rx2_rxhs_word_clk_if_scanmux->d0(rx2_rxhs_word_clk_if);
    rx2_rxhs_word_clk_if_scanmux->d1(scan_rx2_rxhs_word_if_clk);
    sresetz_rx2_rxhs_word_clk_sync.reset(new mipi_4_rx_dphy_ns_gen_sync_rst("sresetz_rx2_rxhs_word_clk_sync"));
    sresetz_rx2_rxhs_word_clk_sync->clk(rx2_rxhs_word_clk_if_scanable);
    sresetz_rx2_rxhs_word_clk_sync->rst(aresetz_scanable);
    sresetz_rx2_rxhs_word_clk_sync->srst(sresetz_rx2_rxhs_word_clk);
    sresetz_rx2_rxhs_word_clk_scanmux.reset(new mipi_4_rx_dphy_ns_gen_mux("sresetz_rx2_rxhs_word_clk_scanmux"));
    sresetz_rx2_rxhs_word_clk_scanmux->z(sresetz_rx2_rxhs_word_clk_scanable);
    sresetz_rx2_rxhs_word_clk_scanmux->sel(scan_mode);
    sresetz_rx2_rxhs_word_clk_scanmux->d0(sresetz_rx2_rxhs_word_clk);
    sresetz_rx2_rxhs_word_clk_scanmux->d1(scan_set_rst_n);
    gen_rx2_rxhs_repeatcal_sync.reset(new mipi_4_rx_dphy_ns_gen_sync__reset_edge0("gen_rx2_rxhs_repeatcal_sync"));
    gen_rx2_rxhs_repeatcal_sync->clk(cfg_clk_scanable);
    gen_rx2_rxhs_repeatcal_sync->rst(sresetz_cfg_clk_scanable);
    gen_rx2_rxhs_repeatcal_sync->data_in(rx2_rxhs_repeatcal);
    gen_rx2_rxhs_repeatcal_sync->scanmode(tie0_);
    gen_rx2_rxhs_repeatcal_sync->data_out(rx2_rxhs_repeatcal_sync);
    gen_rx2_rxhs_repeatcal_sync->data_out_dly(nc_gen_rx2_rxhs_repeatcal_sync_data_out_dly);
    SC_METHOD(glue_I02offset_process_core_slicer_deserializer_0);
    sensitive << rx2_rxhs_data_int;
    SC_METHOD(glue_I02offset_process_core_slicer_deserializer_1);
    sensitive << rx2_rxhs_data_int;
    SC_METHOD(glue_I02offset_process_core_cfgclkfreq);
    sensitive << rx_timebase_if;
    I02offset_process_core.reset(new mipi_4_rx_dphy_ns_offset_process_core("I02offset_process_core"));
    I02offset_process_core->slicer_deserializer_0(c_I02offset_process_core_slicer_deserializer_0);
    I02offset_process_core->slicer_deserializer_1(c_I02offset_process_core_slicer_deserializer_1);
    I02offset_process_core->wordclk(rx2_rxhs_word_clk_if_scanable);
    I02offset_process_core->resetz_wordclk(sresetz_rx2_rxhs_word_clk_scanable);
    I02offset_process_core->resetz_hsclock(sresetz_cfg_clk_scanable);
    I02offset_process_core->start_calibration(rx2_start_calibration_int);
    I02offset_process_core->hsclock(cfg_clk_scanable);
    I02offset_process_core->cal_done(rx2_cal_done_int);
    I02offset_process_core->no_convergence(rx2_cal_error_int);
    I02offset_process_core->control_current_word(rx2_rxhs_data_offset_cal_int);
    I02offset_process_core->cfgclkfreq(c_I02offset_process_core_cfgclkfreq);
    I02offset_process_core->cmp_polarity(cmp_polarity);
    I02offset_process_core->total_ones(rx2_total_ones);
    I02offset_process_core->offset_comp_threshold(offset_comp_threshold);
    I02offset_process_core->tolerance(tolerance);
    I02offset_process_core->number_of_ready_count(number_of_ready_count);
    I02offset_process_core->repeatcal(rx2_rxhs_repeatcal_sync);
    I02offset_process_core->afe_offset_cal_en(rx2_rxhs_data_offset_cal_en_int);
    // gen_mux rx3_rxhs_word_clk_if_scanmux(  .z(rx3_rxhs_word_clk_if_scanable),  .sel(scan_mode), .d0(rx3_rxhs_word_clk_if), .d1(scan_set_rst_n) );
    rx3_rxhs_word_clk_if_scanmux.reset(new mipi_4_rx_dphy_ns_gen_mux("rx3_rxhs_word_clk_if_scanmux"));
    rx3_rxhs_word_clk_if_scanmux->z(rx3_rxhs_word_clk_if_scanable);
    rx3_rxhs_word_clk_if_scanmux->sel(scan_mode);
    rx3_rxhs_word_clk_if_scanmux->d0(rx3_rxhs_word_clk_if);
    rx3_rxhs_word_clk_if_scanmux->d1(scan_rx3_rxhs_word_if_clk);
    sresetz_rx3_rxhs_word_clk_sync.reset(new mipi_4_rx_dphy_ns_gen_sync_rst("sresetz_rx3_rxhs_word_clk_sync"));
    sresetz_rx3_rxhs_word_clk_sync->clk(rx3_rxhs_word_clk_if_scanable);
    sresetz_rx3_rxhs_word_clk_sync->rst(aresetz_scanable);
    sresetz_rx3_rxhs_word_clk_sync->srst(sresetz_rx3_rxhs_word_clk);
    sresetz_rx3_rxhs_word_clk_scanmux.reset(new mipi_4_rx_dphy_ns_gen_mux("sresetz_rx3_rxhs_word_clk_scanmux"));
    sresetz_rx3_rxhs_word_clk_scanmux->z(sresetz_rx3_rxhs_word_clk_scanable);
    sresetz_rx3_rxhs_word_clk_scanmux->sel(scan_mode);
    sresetz_rx3_rxhs_word_clk_scanmux->d0(sresetz_rx3_rxhs_word_clk);
    sresetz_rx3_rxhs_word_clk_scanmux->d1(scan_set_rst_n);
    gen_rx3_rxhs_repeatcal_sync.reset(new mipi_4_rx_dphy_ns_gen_sync__reset_edge0("gen_rx3_rxhs_repeatcal_sync"));
    gen_rx3_rxhs_repeatcal_sync->clk(cfg_clk_scanable);
    gen_rx3_rxhs_repeatcal_sync->rst(sresetz_cfg_clk_scanable);
    gen_rx3_rxhs_repeatcal_sync->data_in(rx3_rxhs_repeatcal);
    gen_rx3_rxhs_repeatcal_sync->scanmode(tie0_);
    gen_rx3_rxhs_repeatcal_sync->data_out(rx3_rxhs_repeatcal_sync);
    gen_rx3_rxhs_repeatcal_sync->data_out_dly(nc_gen_rx3_rxhs_repeatcal_sync_data_out_dly);
    SC_METHOD(glue_I03offset_process_core_slicer_deserializer_0);
    sensitive << rx3_rxhs_data_int;
    SC_METHOD(glue_I03offset_process_core_slicer_deserializer_1);
    sensitive << rx3_rxhs_data_int;
    SC_METHOD(glue_I03offset_process_core_cfgclkfreq);
    sensitive << rx_timebase_if;
    I03offset_process_core.reset(new mipi_4_rx_dphy_ns_offset_process_core("I03offset_process_core"));
    I03offset_process_core->slicer_deserializer_0(c_I03offset_process_core_slicer_deserializer_0);
    I03offset_process_core->slicer_deserializer_1(c_I03offset_process_core_slicer_deserializer_1);
    I03offset_process_core->wordclk(rx3_rxhs_word_clk_if_scanable);
    I03offset_process_core->resetz_wordclk(sresetz_rx3_rxhs_word_clk_scanable);
    I03offset_process_core->resetz_hsclock(sresetz_cfg_clk_scanable);
    I03offset_process_core->start_calibration(rx3_start_calibration_int);
    I03offset_process_core->hsclock(cfg_clk_scanable);
    I03offset_process_core->cal_done(rx3_cal_done_int);
    I03offset_process_core->no_convergence(rx3_cal_error_int);
    I03offset_process_core->control_current_word(rx3_rxhs_data_offset_cal_int);
    I03offset_process_core->cfgclkfreq(c_I03offset_process_core_cfgclkfreq);
    I03offset_process_core->cmp_polarity(cmp_polarity);
    I03offset_process_core->total_ones(rx3_total_ones);
    I03offset_process_core->offset_comp_threshold(offset_comp_threshold);
    I03offset_process_core->tolerance(tolerance);
    I03offset_process_core->number_of_ready_count(number_of_ready_count);
    I03offset_process_core->repeatcal(rx3_rxhs_repeatcal_sync);
    I03offset_process_core->afe_offset_cal_en(rx3_rxhs_data_offset_cal_en_int);
    // synchronizing txulpsexit_global and ulpsactivenot_global
    gen_txulpsexit_global_sync.reset(new mipi_4_rx_dphy_ns_gen_sync__reset_edge0("gen_txulpsexit_global_sync"));
    gen_txulpsexit_global_sync->clk(cfg_clk_scanable);
    gen_txulpsexit_global_sync->rst(sresetz_cfg_clk_scanable);
    gen_txulpsexit_global_sync->data_in(txulpsexit_global);
    gen_txulpsexit_global_sync->scanmode(tie0_);
    gen_txulpsexit_global_sync->data_out(txulpsexit_global_sync);
    gen_txulpsexit_global_sync->data_out_dly(nc_gen_txulpsexit_global_sync_data_out_dly);
    gen_ulpsactivenot_global_sync.reset(new mipi_4_rx_dphy_ns_gen_sync__reset_edge0("gen_ulpsactivenot_global_sync"));
    gen_ulpsactivenot_global_sync->clk(cfg_clk_scanable);
    gen_ulpsactivenot_global_sync->rst(sresetz_cfg_clk_scanable);
    gen_ulpsactivenot_global_sync->data_in(ulpsactivenot_global);
    gen_ulpsactivenot_global_sync->scanmode(tie0_);
    gen_ulpsactivenot_global_sync->data_out(ulpsactivenot_global_sync);
    gen_ulpsactivenot_global_sync->data_out_dly(nc_gen_ulpsactivenot_global_sync_data_out_dly);
    gen_cb_cal_en_ovr_en_sync.reset(new mipi_4_rx_dphy_ns_gen_sync__reset_edge0("gen_cb_cal_en_ovr_en_sync"));
    gen_cb_cal_en_ovr_en_sync->clk(cfg_clk_scanable);
    gen_cb_cal_en_ovr_en_sync->rst(sresetz_cfg_clk_scanable);
    gen_cb_cal_en_ovr_en_sync->data_in(cb_cal_en_ovr_en);
    gen_cb_cal_en_ovr_en_sync->scanmode(tie0_);
    gen_cb_cal_en_ovr_en_sync->data_out(cb_cal_en_ovr_en_sync);
    gen_cb_cal_en_ovr_en_sync->data_out_dly(nc_gen_cb_cal_en_ovr_en_sync_data_out_dly);
    gen_burnin_cal_en_ovr_en_sync.reset(new mipi_4_rx_dphy_ns_gen_sync__reset_edge0("gen_burnin_cal_en_ovr_en_sync"));
    gen_burnin_cal_en_ovr_en_sync->clk(cfg_clk_scanable);
    gen_burnin_cal_en_ovr_en_sync->rst(sresetz_cfg_clk_scanable);
    gen_burnin_cal_en_ovr_en_sync->data_in(burnin_cal_en_ovr_en);
    gen_burnin_cal_en_ovr_en_sync->scanmode(tie0_);
    gen_burnin_cal_en_ovr_en_sync->data_out(burnin_cal_en_ovr_en_sync);
    gen_burnin_cal_en_ovr_en_sync->data_out_dly(nc_gen_burnin_cal_en_ovr_en_sync_data_out_dly);
    gen_burnin_cal_en_ovr_sync.reset(new mipi_4_rx_dphy_ns_gen_sync__reset_edge0("gen_burnin_cal_en_ovr_sync"));
    gen_burnin_cal_en_ovr_sync->clk(cfg_clk_scanable);
    gen_burnin_cal_en_ovr_sync->rst(sresetz_cfg_clk_scanable);
    gen_burnin_cal_en_ovr_sync->data_in(burnin_cal_en_ovr);
    gen_burnin_cal_en_ovr_sync->scanmode(tie0_);
    gen_burnin_cal_en_ovr_sync->data_out(burnin_cal_en_ovr_sync);
    gen_burnin_cal_en_ovr_sync->data_out_dly(nc_gen_burnin_cal_en_ovr_sync_data_out_dly);
    gen_perform_skew_sync.reset(new mipi_4_rx_dphy_ns_gen_sync__reset_edge0("gen_perform_skew_sync"));
    gen_perform_skew_sync->clk(cfg_clk_scanable);
    gen_perform_skew_sync->rst(sresetz_cfg_clk_scanable);
    gen_perform_skew_sync->data_in(perform_skew);
    gen_perform_skew_sync->scanmode(tie0_);
    gen_perform_skew_sync->data_out(perform_skew_sync);
    gen_perform_skew_sync->data_out_dly(nc_gen_perform_skew_sync_data_out_dly);
    gen_bypass_offset_machine_sync.reset(new mipi_4_rx_dphy_ns_gen_sync__reset_edge0("gen_bypass_offset_machine_sync"));
    gen_bypass_offset_machine_sync->clk(cfg_clk_scanable);
    gen_bypass_offset_machine_sync->rst(sresetz_cfg_clk_scanable);
    gen_bypass_offset_machine_sync->data_in(bypass_offset_machine);
    gen_bypass_offset_machine_sync->scanmode(tie0_);
    gen_bypass_offset_machine_sync->data_out(bypass_offset_machine_sync);
    gen_bypass_offset_machine_sync->data_out_dly(nc_gen_bypass_offset_machine_sync_data_out_dly);
    gen_bypass_skew_machine_sync.reset(new mipi_4_rx_dphy_ns_gen_sync__reset_edge0("gen_bypass_skew_machine_sync"));
    gen_bypass_skew_machine_sync->clk(cfg_clk_scanable);
    gen_bypass_skew_machine_sync->rst(sresetz_cfg_clk_scanable);
    gen_bypass_skew_machine_sync->data_in(bypass_skew_machine);
    gen_bypass_skew_machine_sync->scanmode(tie0_);
    gen_bypass_skew_machine_sync->data_out(bypass_skew_machine_sync);
    gen_bypass_skew_machine_sync->data_out_dly(nc_gen_bypass_skew_machine_sync_data_out_dly);
    gen_bypass_ddltunning_machine_sync.reset(new mipi_4_rx_dphy_ns_gen_sync__reset_edge0("gen_bypass_ddltunning_machine_sync"));
    gen_bypass_ddltunning_machine_sync->clk(cfg_clk_scanable);
    gen_bypass_ddltunning_machine_sync->rst(sresetz_cfg_clk_scanable);
    gen_bypass_ddltunning_machine_sync->data_in(bypass_ddltunning_machine);
    gen_bypass_ddltunning_machine_sync->scanmode(tie0_);
    gen_bypass_ddltunning_machine_sync->data_out(bypass_ddltunning_machine_sync);
    gen_bypass_ddltunning_machine_sync->data_out_dly(nc_gen_bypass_ddltunning_machine_sync_data_out_dly);
    gen_global_offset_cal_done_sync.reset(new mipi_4_rx_dphy_ns_gen_sync__reset_edge0("gen_global_offset_cal_done_sync"));
    gen_global_offset_cal_done_sync->clk(cfg_clk_scanable);
    gen_global_offset_cal_done_sync->rst(sresetz_cfg_clk_scanable);
    gen_global_offset_cal_done_sync->data_in(global_offset_cal_done);
    gen_global_offset_cal_done_sync->scanmode(tie0_);
    gen_global_offset_cal_done_sync->data_out(global_offset_cal_done_sync);
    gen_global_offset_cal_done_sync->data_out_dly(nc_gen_global_offset_cal_done_sync_data_out_dly);
    gen_global_ddltunning_done_sync.reset(new mipi_4_rx_dphy_ns_gen_sync__reset_edge0("gen_global_ddltunning_done_sync"));
    gen_global_ddltunning_done_sync->clk(cfg_clk_scanable);
    gen_global_ddltunning_done_sync->rst(sresetz_cfg_clk_scanable);
    gen_global_ddltunning_done_sync->data_in(global_ddltunning_done);
    gen_global_ddltunning_done_sync->scanmode(tie0_);
    gen_global_ddltunning_done_sync->data_out(global_ddltunning_done_sync);
    gen_global_ddltunning_done_sync->data_out_dly(nc_gen_global_ddltunning_done_sync_data_out_dly);
    gen_global_skew_cal_done_sync.reset(new mipi_4_rx_dphy_ns_gen_sync__reset_edge0("gen_global_skew_cal_done_sync"));
    gen_global_skew_cal_done_sync->clk(cfg_clk_scanable);
    gen_global_skew_cal_done_sync->rst(sresetz_cfg_clk_scanable);
    gen_global_skew_cal_done_sync->data_in(global_skew_cal_done);
    gen_global_skew_cal_done_sync->scanmode(tie0_);
    gen_global_skew_cal_done_sync->data_out(global_skew_cal_done_sync);
    gen_global_skew_cal_done_sync->data_out_dly(nc_gen_global_skew_cal_done_sync_data_out_dly);
    gen_burnin_cal_done_int_sync.reset(new mipi_4_rx_dphy_ns_gen_sync__reset_edge0("gen_burnin_cal_done_int_sync"));
    gen_burnin_cal_done_int_sync->clk(cfg_clk_scanable);
    gen_burnin_cal_done_int_sync->rst(sresetz_cfg_clk_scanable);
    gen_burnin_cal_done_int_sync->data_in(burnin_cal_done_int);
    gen_burnin_cal_done_int_sync->scanmode(tie0_);
    gen_burnin_cal_done_int_sync->data_out(burnin_cal_done_int_sync);
    gen_burnin_cal_done_int_sync->data_out_dly(nc_gen_burnin_cal_done_int_sync_data_out_dly);
    gen_rxclk_caldone_int_sync.reset(new mipi_4_rx_dphy_ns_gen_sync__reset_edge0("gen_rxclk_caldone_int_sync"));
    gen_rxclk_caldone_int_sync->clk(cfg_clk_scanable);
    gen_rxclk_caldone_int_sync->rst(sresetz_cfg_clk_scanable);
    gen_rxclk_caldone_int_sync->data_in(rxclk_caldone);
    gen_rxclk_caldone_int_sync->scanmode(tie0_);
    gen_rxclk_caldone_int_sync->data_out(rxclk_caldone_sync);
    gen_rxclk_caldone_int_sync->data_out_dly(nc_gen_rxclk_caldone_int_sync_data_out_dly);
    gen_enableclk_sync.reset(new mipi_4_rx_dphy_ns_gen_sync__reset_edge0("gen_enableclk_sync"));
    gen_enableclk_sync->clk(cfg_clk_scanable);
    gen_enableclk_sync->rst(sresetz_cfg_clk_scanable);
    gen_enableclk_sync->data_in(enableclk);
    gen_enableclk_sync->scanmode(tie0_);
    gen_enableclk_sync->data_out(enableclk_sync);
    gen_enableclk_sync->data_out_dly(nc_gen_enableclk_sync_data_out_dly);
    I01dphyifpowerctrlrx.reset(new mipi_4_rx_dphy_ns_dphyifpowerctrlrx("I01dphyifpowerctrlrx"));
    I01dphyifpowerctrlrx->cfg_clk(cfg_clk_scanable);
    I01dphyifpowerctrlrx->resetz(sresetz_cfg_clk_scanable);
    I01dphyifpowerctrlrx->force_state(force_state);
    I01dphyifpowerctrlrx->target_state(target_state);
    I01dphyifpowerctrlrx->state_main(state_main);
    I01dphyifpowerctrlrx->twakeupulpm(twakeupulpm);
    I01dphyifpowerctrlrx->txstarttime(txstarttime);
    I01dphyifpowerctrlrx->txdindlytime(txdindlytime);
    I01dphyifpowerctrlrx->txulpsexit(txulpsexit_global_sync);
    I01dphyifpowerctrlrx->lptxpon(lptxpon_ifpowerctrl);
    I01dphyifpowerctrlrx->lptxena(lptxena_ifpowerctrl);
    I01dphyifpowerctrlrx->lptxdin(lptxdin_ifpowerctrl);
    I01dphyifpowerctrlrx->lptxenpulldwn(lptxenpulldwn_ifpowerctrl);
    I01dphyifpowerctrlrx->bintpon(bintpon_ifpowerctrl);
    I01dphyifpowerctrlrx->ulpsactivenot(ulpsactivenot_global_sync);
    I01dphyifpowerctrlrx->hsrxpon_clklane(hsrxpon_clklane_ifpowerctrl);
    I01dphyifpowerctrlrx->hsrxpon_datalane(hsrxpon_datalane_ifpowerctrl);
    I01dphyifpowerctrlrx->offsetcal_clklane_done(rxclk_caldone_sync);
    I01dphyifpowerctrlrx->offsetcal_lanes_done(global_offset_cal_done_sync);
    I01dphyifpowerctrlrx->hsrxencal(hsrxencal_offset_ifpowerctrl);
    I01dphyifpowerctrlrx->des_en_for_datalane_offset_cal(des_en_for_datalane_offset_cal_ifpowerctrl);
    I01dphyifpowerctrlrx->rxclk_feed_int_clk(rxclk_feed_int_clk_ifpowerctrl);
    I01dphyifpowerctrlrx->rxlane_feed_int_clk(rxlane_feed_int_clk_ifpowerctrl);
    I01dphyifpowerctrlrx->hsrxencal_lane_offset(hsrxencal_lane_offset_ifpowerctrl);
    I01dphyifpowerctrlrx->perform_skew(perform_skew_sync);
    I01dphyifpowerctrlrx->rxpontime(rxpontime);
    I01dphyifpowerctrlrx->cb_cal_en(cb_cal_en_ifpowerctrl);
    I01dphyifpowerctrlrx->rescal_done(rescal_done);
    I01dphyifpowerctrlrx->rescal_en(rescal_en);
    I01dphyifpowerctrlrx->cb_cal_en_ovr_en(cb_cal_en_ovr_en_sync);
    I01dphyifpowerctrlrx->noext_burnin_res_cal(noext_burnin_res_cal_int);
    I01dphyifpowerctrlrx->burnin_cal_done(burnin_cal_done_int_sync);
    I01dphyifpowerctrlrx->burnin_cal_en(burnin_cal_en_int);
    I01dphyifpowerctrlrx->burnin_cal_en_ovr_en(burnin_cal_en_ovr_en_sync);
    I01dphyifpowerctrlrx->bypass_offset_machine(bypass_offset_machine_sync);
    I01dphyifpowerctrlrx->bypass_skew_machine(bypass_skew_machine_sync);
    I01dphyifpowerctrlrx->bypass_ddltunning_machine(bypass_ddltunning_machine_sync);
    I01dphyifpowerctrlrx->ddltunning_en(ddltunning_en_ifpowerctrl);
    I01dphyifpowerctrlrx->ddltunning_done(global_ddltunning_done_sync);
    I01dphyifpowerctrlrx->skew_back_comp_en(skew_back_comp_en_ifpowerctrl);
    I01dphyifpowerctrlrx->skew_back_comp_done(global_skew_cal_done_sync);
    I01dphyifpowerctrlrx->enable_lanes(enable_lanes_int);
    I01dphyifpowerctrlrx->timeinitbg(timeinitbg);
    I01dphyifpowerctrlrx->bist_control(bist_control_int);
    I01dphyifpowerctrlrx->pll_boot_fsm(pll_boot_fsm_int);
    I01dphyifpowerctrlrx->rx_rxhs_prog_mux_pwron(rx_rxhs_prog_mux_pwron_int);
    I01dphyifpowerctrlrx->enableclk(enableclk_sync);
    I01dphyifpowerctrlrx->mastermacro(mastermacro_int);
    I01dphyifpowerctrlrx->do_offs_and_skew_enableclk_not(do_offs_and_skew_enableclk_not);
    // TESTER
    I01dphytester.reset(new mipi_4_rx_dphy_ns_dphy4rxtester("I01dphytester"));
    I01dphytester->adc_done(adc_done);
    I01dphytester->align_error(align_error);
    I01dphytester->bintpon_if_clklane(bintpon_if_clklane);
    I01dphytester->bintpon_if_lane0(bintpon_if_lane0);
    I01dphytester->bintpon_if_lane1(bintpon_if_lane1);
    I01dphytester->bintpon_if_lane2(bintpon_if_lane2);
    I01dphytester->bintpon_if_lane3(bintpon_if_lane3);
    I01dphytester->bintpon_ifpowerctrl(bintpon_ifpowerctrl);
    I01dphytester->bistdone(bistdone);
    I01dphytester->bistok(bistok);
    I01dphytester->biston_lanes(biston_lanes);
    I01dphytester->burnin_cal_en_if(burnin_cal_en_if);
    I01dphytester->cb_atb_comp_clk(rx_atb_comp_clk_if_tester);
    I01dphytester->cb_atb_on(rx_atb_on_if);
    I01dphytester->cb_bg_en(rx_cb_bg_en_if);
    I01dphytester->cb_cal_en(rx_cb_cal_en_if);
    I01dphytester->cb_cal_en_ifpowerctrl(cb_cal_en_ifpowerctrl);
    I01dphytester->cb_comp_out(rx_cb_comp_out_if);
    I01dphytester->cb_det_supply(rx_cb_det_supply_if);
    I01dphytester->cb_ical_x4(rx_cb_ical_x4_if);
    I01dphytester->cb_lpibias_en(rx_cb_lpibias_en_if);
    I01dphytester->cb_on(rx_cb_on_if);
    I01dphytester->clk_en_lanes(clk_en_lanes);
    I01dphytester->ddl_tune_en_lane0_if(ddl_tune_en_lane0_if);
    I01dphytester->ddl_tune_en_lane1_if(ddl_tune_en_lane1_if);
    I01dphytester->ddl_tune_en_lane2_if(ddl_tune_en_lane2_if);
    I01dphytester->ddl_tune_en_lane3_if(ddl_tune_en_lane3_if);
    I01dphytester->ddltunning_en_ifpowerctrl(ddltunning_en_ifpowerctrl);
    I01dphytester->des_en_for_datalane_offset_cal_ifpowerctrl(des_en_for_datalane_offset_cal_ifpowerctrl);
    I01dphytester->deskew_legacyz(deskew_legacyz);
    I01dphytester->enable_clklane(enable_clklane);
    I01dphytester->enable_lane0(enable_lane0);
    I01dphytester->enable_lane1(enable_lane1);
    I01dphytester->enable_lane2(enable_lane2);
    I01dphytester->enable_lane3(enable_lane3);
    I01dphytester->enable_lanes(enable_lanes);
    I01dphytester->error_gating_out(error_gating_out);
    I01dphytester->fsm_trigger(fsm_trigger);
    I01dphytester->global_ddltunning_done(global_ddltunning_done);
    I01dphytester->global_offset_cal_done(global_offset_cal_done);
    I01dphytester->global_skew_cal_done(global_skew_cal_done);
    I01dphytester->hsdirect_if(hsdirect_if);
    I01dphytester->hsrx_en_if_clklane(hsrx_en_if_clklane);
    I01dphytester->hsrx_en_if_lane0(hsrx_en_if_lane0);
    I01dphytester->hsrx_en_if_lane1(hsrx_en_if_lane1);
    I01dphytester->hsrx_en_if_lane2(hsrx_en_if_lane2);
    I01dphytester->hsrx_en_if_lane3(hsrx_en_if_lane3);
    I01dphytester->hsrxencal_lane_offset_ifpowerctrl(hsrxencal_lane_offset_ifpowerctrl);
    I01dphytester->hsrxpon_clklane_ifpowerctrl(hsrxpon_clklane_ifpowerctrl);
    I01dphytester->hsrxpon_datalane_ifpowerctrl(hsrxpon_datalane_ifpowerctrl);
    I01dphytester->launch_des_en_in(launch_des_en_in);
    I01dphytester->launch_des_en_out(launch_des_en_out);
    I01dphytester->lptxdin_ifpowerctrl(lptxdin_ifpowerctrl);
    I01dphytester->lptxena_if_lane0(lptxena_if_lane0);
    I01dphytester->lptxena_ifpowerctrl(lptxena_ifpowerctrl);
    I01dphytester->lptxenpulldn_if_lane0(lptxenpulldn_if_lane0);
    I01dphytester->lptxenpulldwn_ifpowerctrl(lptxenpulldwn_ifpowerctrl);
    I01dphytester->lptxenulp_if_lane0(lptxenulp_if_lane0);
    I01dphytester->lptxpon_if_lane0(lptxpon_if_lane0);
    I01dphytester->lptxpon_ifpowerctrl(lptxpon_ifpowerctrl);
    I01dphytester->lsrxclk_clklane(lsrxclk_clklane);
    I01dphytester->lsrxclk_lane0(lsrxclk_lane0);
    I01dphytester->lsrxclk_lane1(lsrxclk_lane1);
    I01dphytester->lsrxclk_lane2(lsrxclk_lane2);
    I01dphytester->lsrxclk_lane3(lsrxclk_lane3);
    I01dphytester->noext_burn_error(noext_burn_error);
    I01dphytester->norexttune_done(burnin_cal_done_int);
    I01dphytester->onpll(onpll);
    I01dphytester->perform_skew(perform_skew);
    I01dphytester->pg_pat_strobe(pg_pat_strobe_if);
    I01dphytester->pg_sel(pg_sel_if);
    I01dphytester->pll_clksel_en(pll_clksel_en);
    I01dphytester->pll_gear_shift(pll_gear_shift);
    I01dphytester->pll_lock(pll_lock);
    I01dphytester->pll_lock_det_on(pll_lock_det_on);
    I01dphytester->pll_lock_int(pll_lock_int);
    I01dphytester->pll_pwron(pll_pwron_int);
    I01dphytester->pll_reset(pll_reset);
    I01dphytester->pll_startup_done(pll_startup_done);
    I01dphytester->pll_testlock(pll_testlock_int);
    I01dphytester->pll_vpl_det(pll_vpl_det_int);
    I01dphytester->rescal_done(rescal_done);
    I01dphytester->rescal_en(rescal_en);
    I01dphytester->rescal_error(rescal_error);
    I01dphytester->rx0_cal_done(rx0_cal_done);
    I01dphytester->rx0_cal_error(rx0_cal_error_int);
    I01dphytester->rx0_ddl_error_flag_if(rx0_ddl_error_flag_if);
    I01dphytester->rx0_ddl_finished_if(rx0_ddl_finished_int);
    I01dphytester->rx0_ddl_start_calibration_if(rx0_ddl_start_calibration_if);
    I01dphytester->rx0_deskewcaldone_if(rx0_deskewcaldone_if);
    I01dphytester->rx0_deskewcalfailed_if(rx0_deskewcalfailed_if);
    I01dphytester->rx0_error_latch_if(rx0_error_latch_if);
    I01dphytester->rx0_pm_error_pm(rx0_pm_error_pm_if);
    I01dphytester->rx0_pm_start(rx0_pm_start_control_if);
    I01dphytester->rx0_rxhs_data_offset_cal_en(rx0_rxhs_data_offset_cal_en_if);
    I01dphytester->rx0_rxhs_ddl_phase_change_p_if(rx0_rxhs_ddl_phase_change_p_if);
    I01dphytester->rx0_rxhs_des_en_if(rx0_rxhs_des_en_if);
    I01dphytester->rx0_rxhs_deserial_data_en_if(rx0_rxhs_deserial_data_en_if);
    I01dphytester->rx0_rxhs_feed_int_clk_if(rx0_rxhs_feed_int_clk_if);
    I01dphytester->rx0_rxhs_rx_en_if(rx0_rxhs_rx_en_if);
    I01dphytester->rx0_rxhs_rx_enlogic_if(rx0_rxhs_rx_enlogic_if);
    I01dphytester->rx0_rxhs_rx_enterm_if(rx0_rxhs_rx_enterm_if);
    I01dphytester->rx0_rxhs_rx_pon_if(rx0_rxhs_rx_pon_if);
    I01dphytester->rx0_rxhs_word_clk_if(rx0_rxhs_word_clk_if);
    I01dphytester->rx0_start_calibration(rx0_start_calibration_int);
    I01dphytester->rx1_cal_done(rx1_cal_done);
    I01dphytester->rx1_cal_error(rx1_cal_error_int);
    I01dphytester->rx1_ddl_error_flag_if(rx1_ddl_error_flag_if);
    I01dphytester->rx1_ddl_finished_if(rx1_ddl_finished_int);
    I01dphytester->rx1_ddl_start_calibration_if(rx1_ddl_start_calibration_if);
    I01dphytester->rx1_deskewcaldone_if(rx1_deskewcaldone_if);
    I01dphytester->rx1_deskewcalfailed_if(rx1_deskewcalfailed_if);
    I01dphytester->rx1_error_latch_if(rx1_error_latch_if);
    I01dphytester->rx1_pm_error_pm(rx1_pm_error_pm_if);
    I01dphytester->rx1_pm_start(rx1_pm_start_control_if);
    I01dphytester->rx1_rxhs_data_offset_cal_en(rx1_rxhs_data_offset_cal_en_if);
    I01dphytester->rx1_rxhs_ddl_phase_change_p_if(rx1_rxhs_ddl_phase_change_p_if);
    I01dphytester->rx1_rxhs_des_en_if(rx1_rxhs_des_en_if);
    I01dphytester->rx1_rxhs_deserial_data_en_if(rx1_rxhs_deserial_data_en_if);
    I01dphytester->rx1_rxhs_feed_int_clk_if(rx1_rxhs_feed_int_clk_if);
    I01dphytester->rx1_rxhs_rx_en_if(rx1_rxhs_rx_en_if);
    I01dphytester->rx1_rxhs_rx_enlogic_if(rx1_rxhs_rx_enlogic_if);
    I01dphytester->rx1_rxhs_rx_enterm_if(rx1_rxhs_rx_enterm_if);
    I01dphytester->rx1_rxhs_rx_pon_if(rx1_rxhs_rx_pon_if);
    I01dphytester->rx1_rxhs_word_clk_if(rx1_rxhs_word_clk_if);
    I01dphytester->rx1_rxlp_poncd_if(rx1_rxlp_poncd_if);
    I01dphytester->rx1_rxlp_ponlp_if(rx1_rxlp_ponlp_if);
    I01dphytester->rx1_rxlp_ponulp_if(rx1_rxlp_ponulp_if);
    I01dphytester->rx1_start_calibration(rx1_start_calibration_int);
    I01dphytester->rx2_cal_done(rx2_cal_done);
    I01dphytester->rx2_cal_error(rx2_cal_error_int);
    I01dphytester->rx2_ddl_error_flag_if(rx2_ddl_error_flag_if);
    I01dphytester->rx2_ddl_finished_if(rx2_ddl_finished_int);
    I01dphytester->rx2_ddl_start_calibration_if(rx2_ddl_start_calibration_if);
    I01dphytester->rx2_deskewcaldone_if(rx2_deskewcaldone_if);
    I01dphytester->rx2_deskewcalfailed_if(rx2_deskewcalfailed_if);
    I01dphytester->rx2_error_latch_if(rx2_error_latch_if);
    I01dphytester->rx2_pm_error_pm(rx2_pm_error_pm_if);
    I01dphytester->rx2_pm_start(rx2_pm_start_control_if);
    I01dphytester->rx2_rxhs_data_offset_cal_en(rx2_rxhs_data_offset_cal_en_if);
    I01dphytester->rx2_rxhs_ddl_phase_change_p_if(rx2_rxhs_ddl_phase_change_p_if);
    I01dphytester->rx2_rxhs_des_en_if(rx2_rxhs_des_en_if);
    I01dphytester->rx2_rxhs_deserial_data_en_if(rx2_rxhs_deserial_data_en_if);
    I01dphytester->rx2_rxhs_feed_int_clk_if(rx2_rxhs_feed_int_clk_if);
    I01dphytester->rx2_rxhs_rx_en_if(rx2_rxhs_rx_en_if);
    I01dphytester->rx2_rxhs_rx_enlogic_if(rx2_rxhs_rx_enlogic_if);
    I01dphytester->rx2_rxhs_rx_enterm_if(rx2_rxhs_rx_enterm_if);
    I01dphytester->rx2_rxhs_rx_pon_if(rx2_rxhs_rx_pon_if);
    I01dphytester->rx2_rxhs_word_clk_if(rx2_rxhs_word_clk_if);
    I01dphytester->rx2_rxlp_poncd_if(rx2_rxlp_poncd_if);
    I01dphytester->rx2_rxlp_ponlp_if(rx2_rxlp_ponlp_if);
    I01dphytester->rx2_rxlp_ponulp_if(rx2_rxlp_ponulp_if);
    I01dphytester->rx2_start_calibration(rx2_start_calibration_int);
    I01dphytester->rx3_cal_done(rx3_cal_done);
    I01dphytester->rx3_cal_error(rx3_cal_error_int);
    I01dphytester->rx3_ddl_error_flag_if(rx3_ddl_error_flag_if);
    I01dphytester->rx3_ddl_finished_if(rx3_ddl_finished_int);
    I01dphytester->rx3_ddl_start_calibration_if(rx3_ddl_start_calibration_if);
    I01dphytester->rx3_deskewcaldone_if(rx3_deskewcaldone_if);
    I01dphytester->rx3_deskewcalfailed_if(rx3_deskewcalfailed_if);
    I01dphytester->rx3_error_latch_if(rx3_error_latch_if);
    I01dphytester->rx3_pm_error_pm(rx3_pm_error_pm_if);
    I01dphytester->rx3_pm_start(rx3_pm_start_control_if);
    I01dphytester->rx3_rxhs_data_offset_cal_en(rx3_rxhs_data_offset_cal_en_if);
    I01dphytester->rx3_rxhs_ddl_phase_change_p_if(rx3_rxhs_ddl_phase_change_p_if);
    I01dphytester->rx3_rxhs_des_en_if(rx3_rxhs_des_en_if);
    I01dphytester->rx3_rxhs_deserial_data_en_if(rx3_rxhs_deserial_data_en_if);
    I01dphytester->rx3_rxhs_feed_int_clk_if(rx3_rxhs_feed_int_clk_if);
    I01dphytester->rx3_rxhs_rx_en_if(rx3_rxhs_rx_en_if);
    I01dphytester->rx3_rxhs_rx_enlogic_if(rx3_rxhs_rx_enlogic_if);
    I01dphytester->rx3_rxhs_rx_enterm_if(rx3_rxhs_rx_enterm_if);
    I01dphytester->rx3_rxhs_rx_pon_if(rx3_rxhs_rx_pon_if);
    I01dphytester->rx3_rxhs_word_clk_if(rx3_rxhs_word_clk_if);
    I01dphytester->rx3_rxlp_poncd_if(rx3_rxlp_poncd_if);
    I01dphytester->rx3_rxlp_ponlp_if(rx3_rxlp_ponlp_if);
    I01dphytester->rx3_rxlp_ponulp_if(rx3_rxlp_ponulp_if);
    I01dphytester->rx3_start_calibration(rx3_start_calibration_int);
    I01dphytester->rx_rxhs_compatibility_mode(rx_rxhs_compatibility_mode);
    I01dphytester->rxclk_caldone(rxclk_caldone);
    I01dphytester->rxclk_feed_int_clk_ifpowerctrl(rxclk_feed_int_clk_ifpowerctrl);
    I01dphytester->rxclk_lb_sel_if(rxclk_lb_sel_if);
    I01dphytester->rxclk_lbck_clk_en_if(rxclk_lbck_clk_en_if);
    I01dphytester->rxclk_lbck_div_en_if(rxclk_lbck_div_en_if);
    I01dphytester->rxclk_lbck_serial_clk_en_if(rxclk_lbck_serial_clk_en_if);
    I01dphytester->rxclk_lbck_serial_en_if(rxclk_lbck_serial_en_if);
    I01dphytester->rxclk_lbdata_clk_en_if(rxclk_lbdata_clk_en_if);
    I01dphytester->rxclk_lbdata_div_en_if(rxclk_lbdata_div_en_if);
    I01dphytester->rxclk_lbdata_serial_clk_en_if(rxclk_lbdata_serial_clk_en_if);
    I01dphytester->rxclk_lbdata_serial_en_if(rxclk_lbdata_serial_en_if);
    I01dphytester->rxclk_rxhs_clk_en_in(rxclk_rxhs_clk_en_in);
    I01dphytester->rxclk_rxhs_clk_en_out(rxclk_rxhs_clk_en_out);
    I01dphytester->rxclk_rxhs_clk_m_cal(rxclk_rxhs_clk_m_cal_if);
    I01dphytester->rxclk_rxhs_clk_p_cal(rxclk_rxhs_clk_p_cal_if);
    I01dphytester->rxclk_rxhs_ddr_clk_if(rxclk_rxhs_ddr_clk_if);
    I01dphytester->rxclk_rxhs_feed_int_clk_if(rxclk_rxhs_feed_int_clk_if);
    I01dphytester->rxclk_rxhs_feed_int_clk_in(phy_rxclk_rxhs_feed_int_clk_in);
    I01dphytester->rxclk_rxhs_feed_int_clk_out(phy_rxclk_rxhs_feed_int_clk_out);
    I01dphytester->rxclk_rxhs_osc_out_if(rxclk_rxhs_osc_out_if);
    I01dphytester->rxclk_rxhs_osc_pon(rxclk_rxhs_osc_pon_if);
    I01dphytester->rxclk_rxhs_osc_res_en(rxclk_rxhs_osc_res_en_if);
    I01dphytester->rxclk_rxhs_rx_en_if(rxclk_rxhs_rx_en_if);
    I01dphytester->rxclk_rxhs_rx_en_in(rxclk_rxhs_rx_en_in);
    I01dphytester->rxclk_rxhs_rx_en_out(rxclk_rxhs_rx_en_out);
    I01dphytester->rxclk_rxhs_rx_enlogic_if(rxclk_rxhs_rx_enlogic_if);
    I01dphytester->rxclk_rxhs_rx_enlogic_in(rxclk_rxhs_rx_enlogic_in);
    I01dphytester->rxclk_rxhs_rx_enlogic_out(rxclk_rxhs_rx_enlogic_out);
    I01dphytester->rxclk_rxhs_rx_enterm_if(rxclk_rxhs_rx_enterm_if);
    I01dphytester->rxclk_rxhs_rx_pon_if(rxclk_rxhs_rx_pon_if);
    I01dphytester->rxclk_rxhs_rx_pon_in(rxclk_rxhs_rx_pon_in);
    I01dphytester->rxclk_rxhs_rx_pon_out(rxclk_rxhs_rx_pon_out);
    I01dphytester->rxclk_rxhs_start_calibration(rxclk_rxhs_clk_offset_cal_en_if);
    I01dphytester->rxclk_rxlp_poncd_if(rxclk_rxlp_poncd_if);
    I01dphytester->rxclk_rxlp_ponlp_if(rxclk_rxlp_ponlp_if);
    I01dphytester->rxclk_rxlp_ponulp_if(rxclk_rxlp_ponulp_if);
    I01dphytester->rxclk_start_calibration(rxclk_start_calibration);
    I01dphytester->rxlane_feed_int_clk_ifpowerctrl(rxlane_feed_int_clk_ifpowerctrl);
    I01dphytester->shutdownz_tester(shutdownz);
    I01dphytester->skew_back_comp_en(skew_back_comp_en);
    I01dphytester->skew_back_comp_en_ifpowerctrl(skew_back_comp_en_ifpowerctrl);
    I01dphytester->testclk(testclk_scanable);
    I01dphytester->testclkinv(testclkinv_scanable);
    I01dphytester->testclr(testclr_scanable);
    I01dphytester->testen(testen);
    I01dphytester->tx0_rxlp_poncd(rx0_rxlp_poncd_if);
    I01dphytester->tx0_rxlp_ponlp(rx0_rxlp_ponlp_if);
    I01dphytester->tx0_rxlp_ponulp(rx0_rxlp_ponulp_if);
    I01dphytester->tx0_txlp_ena(rx0_txlp_ena_if);
    I01dphytester->tx0_txlp_enlogic(rx0_txlp_enlogic_if);
    I01dphytester->tx0_txlp_enpulldwn(rx0_txlp_enpulldwn_if);
    I01dphytester->tx0_txlp_enulp(rx0_txlp_enulp_if);
    I01dphytester->tx0_txlp_pon(rx0_txlp_pon_if);
    I01dphytester->tx0_txlp_sr_bypass(rx0_txlp_sr_bypass_if);
    I01dphytester->txulpsexit_global(txulpsexit_global);
    I01dphytester->ulpsactivenot_global(ulpsactivenot_global);
    I01dphytester->ulpsactivenotclk_in(phy_ulpsactivenotclk_in);
    I01dphytester->cont_data(cont_data);
    I01dphytester->rx0_freq_meas_out_if(rx0_freq_meas_out_if);
    I01dphytester->rx0_osc_freq_target_if(rx0_osc_freq_target_if);
    I01dphytester->rx1_freq_meas_out_if(rx1_freq_meas_out_if);
    I01dphytester->rx1_osc_freq_target_if(rx1_osc_freq_target_if);
    I01dphytester->rx2_freq_meas_out_if(rx2_freq_meas_out_if);
    I01dphytester->rx2_osc_freq_target_if(rx2_osc_freq_target_if);
    I01dphytester->rx3_freq_meas_out_if(rx3_freq_meas_out_if);
    I01dphytester->rx3_osc_freq_target_if(rx3_osc_freq_target_if);
    I01dphytester->rx0_pm_error_counter(rx0_pm_error_counter_if);
    I01dphytester->rx1_pm_error_counter(rx1_pm_error_counter_if);
    I01dphytester->rx2_pm_error_counter(rx2_pm_error_counter_if);
    I01dphytester->rx3_pm_error_counter(rx3_pm_error_counter_if);
    I01dphytester->lprxdoutcd_clklane_if(lprxdoutcd_clklane_if);
    I01dphytester->lprxdoutcd_lane0(lprxdoutcd_lane0_if);
    I01dphytester->lprxdoutcd_lane1(lprxdoutcd_lane1_if);
    I01dphytester->lprxdoutcd_lane2(lprxdoutcd_lane2_if);
    I01dphytester->lprxdoutcd_lane3(lprxdoutcd_lane3_if);
    I01dphytester->lprxdoutlp_clklane_if(lprxdoutlp_clklane_if);
    I01dphytester->lprxdoutlp_lane0(lprxdoutlp_lane0_if);
    I01dphytester->lprxdoutlp_lane1(lprxdoutlp_lane1_if);
    I01dphytester->lprxdoutlp_lane2(lprxdoutlp_lane2_if);
    I01dphytester->lprxdoutlp_lane3(lprxdoutlp_lane3_if);
    I01dphytester->lprxdoutulp_clklane_if(lprxdoutulp_clklane_if);
    I01dphytester->lprxdoutulp_lane0(lprxdoutulp_lane0_if);
    I01dphytester->lprxdoutulp_lane1(lprxdoutulp_lane1_if);
    I01dphytester->lprxdoutulp_lane2(lprxdoutulp_lane2_if);
    I01dphytester->lprxdoutulp_lane3(lprxdoutulp_lane3_if);
    I01dphytester->lptxdin_if_lane0(lptxdin_if_lane0);
    I01dphytester->rx0_txlp_iocont_if(rx0_txlp_iocont_if);
    I01dphytester->rx_rxhs_gmode(rx_rxhs_gmode_int);
    I01dphytester->rx_rxhs_prog(rx_rxhs_prog_if);
    I01dphytester->rx_rxhs_prog_in(phy_rx_rxhs_prog_in);
    I01dphytester->rx_rxhs_prog_out(phy_rx_rxhs_prog_out);
    I01dphytester->rxclk_rxhs_ddr_clk_div(rxclk_rxhs_ddr_clk_div_int);
    I01dphytester->tx0_txlp_din(rx0_txlp_din_if);
    I01dphytester->rx_rxhs_setr(rx_rxhs_setrx_if);
    I01dphytester->bist_state(bist_state);
    I01dphytester->cb_cal_repl(rx_cb_cal_repl_if);
    I01dphytester->counter_for_des_en_if(counter_for_des_en_if);
    I01dphytester->pll_n(pll_n);
    I01dphytester->rx0_rxhs_ddl_phase_data_if(rx0_rxhs_ddl_phase_data_if);
    I01dphytester->rx1_rxhs_ddl_phase_data_if(rx1_rxhs_ddl_phase_data_if);
    I01dphytester->rx2_rxhs_ddl_phase_data_if(rx2_rxhs_ddl_phase_data_if);
    I01dphytester->rx3_rxhs_ddl_phase_data_if(rx3_rxhs_ddl_phase_data_if);
    I01dphytester->rxclk_errcal(errcal_clklane);
    I01dphytester->setr_burnin(setr_burnin_int);
    I01dphytester->state_main(state_main);
    I01dphytester->rx0_rxhs_ddl_phase_clk_if(rx0_rxhs_ddl_phase_clk_if);
    I01dphytester->rx0_rxhs_ddl_tune_if(rx0_rxhs_ddl_tune_if);
    I01dphytester->rx1_rxhs_ddl_phase_clk_if(rx1_rxhs_ddl_phase_clk_if);
    I01dphytester->rx1_rxhs_ddl_tune_if(rx1_rxhs_ddl_tune_if);
    I01dphytester->rx2_rxhs_ddl_phase_clk_if(rx2_rxhs_ddl_phase_clk_if);
    I01dphytester->rx2_rxhs_ddl_tune_if(rx2_rxhs_ddl_tune_if);
    I01dphytester->rx3_rxhs_ddl_phase_clk_if(rx3_rxhs_ddl_phase_clk_if);
    I01dphytester->rx3_rxhs_ddl_tune_if(rx3_rxhs_ddl_tune_if);
    I01dphytester->pll_vco_cntrl(pll_vco_cntrl);
    I01dphytester->rx0_rxhs_data_offset_cal(rx0_rxhs_data_offset_cal_if);
    I01dphytester->rx1_rxhs_data_offset_cal(rx1_rxhs_data_offset_cal_if);
    I01dphytester->rx2_rxhs_data_offset_cal(rx2_rxhs_data_offset_cal_if);
    I01dphytester->rx3_rxhs_data_offset_cal(rx3_rxhs_data_offset_cal_if);
    I01dphytester->rxclk_rxhs_clk_offset_cal(rxclk_rxhs_clk_offset_cal_if);
    I01dphytester->hsfreqrange(hsfreqrange);
    I01dphytester->hsfreqrange_top(hsfreqrange_int);
    I01dphytester->hsrxthssettle_datalanes(hsrxthssettle_if_datalanes);
    I01dphytester->del_datalane(del_datalane);
    I01dphytester->rx0_rxhs_data_if(rx0_rxhs_data_if);
    I01dphytester->rx0_rxhs_data_int(rx0_rxhs_data_int);
    I01dphytester->rx0_total_ones(rx0_total_ones);
    I01dphytester->rx1_rxhs_data_if(rx1_rxhs_data_if);
    I01dphytester->rx1_rxhs_data_int(rx1_rxhs_data_int);
    I01dphytester->rx1_total_ones(rx1_total_ones);
    I01dphytester->rx2_rxhs_data_if(rx2_rxhs_data_if);
    I01dphytester->rx2_rxhs_data_int(rx2_rxhs_data_int);
    I01dphytester->rx2_total_ones(rx2_total_ones);
    I01dphytester->rx3_rxhs_data_if(rx3_rxhs_data_if);
    I01dphytester->rx3_rxhs_data_int(rx3_rxhs_data_int);
    I01dphytester->rx3_total_ones(rx3_total_ones);
    I01dphytester->testdin(testdin);
    I01dphytester->timebase(rx_timebase_if);
    I01dphytester->cb_dac_prog(rx_cb_dac_prog_if);
    I01dphytester->count_osc0(count_osc0);
    I01dphytester->count_osc1(count_osc1);
    I01dphytester->pll_m(pll_m);
    I01dphytester->adc_done_clear(adc_done_clear);
    I01dphytester->adc_en(adc_en);
    I01dphytester->atb_lptx1200_on_lane0(atb_lptx1200_on_lane0_tester);
    I01dphytester->bintpon_bypass_clklane(bintpon_bypass_clklane_tester);
    I01dphytester->bintpon_bypass_lane0(bintpon_bypass_lane0_tester);
    I01dphytester->bintpon_bypass_lane1(bintpon_bypass_lane1_tester);
    I01dphytester->bintpon_bypass_lane2(bintpon_bypass_lane2_tester);
    I01dphytester->bintpon_bypass_lane3(bintpon_bypass_lane3_tester);
    I01dphytester->bintpon_clklane(bintpon_clklane_tester);
    I01dphytester->bintpon_lane0(bintpon_lane0_tester);
    I01dphytester->bintpon_lane1(bintpon_lane1_tester);
    I01dphytester->bintpon_lane2(bintpon_lane2_tester);
    I01dphytester->bintpon_lane3(bintpon_lane3_tester);
    I01dphytester->bist_manual(bist_manual);
    I01dphytester->bist_run_lane0(bist_run_lane0);
    I01dphytester->bist_run_lane1(bist_run_lane1);
    I01dphytester->bist_run_lane2(bist_run_lane2);
    I01dphytester->bist_run_lane3(bist_run_lane3);
    I01dphytester->burnin_cal_en_ovr(burnin_cal_en_ovr);
    I01dphytester->burnin_cal_en_ovr_en(burnin_cal_en_ovr_en);
    I01dphytester->bypass_ddltunning_machine(bypass_ddltunning_machine);
    I01dphytester->bypass_offset_machine(bypass_offset_machine);
    I01dphytester->bypass_skew_machine(bypass_skew_machine);
    I01dphytester->cb_atb_comp_clk_ovr(cb_atb_comp_clk_ovr);
    I01dphytester->cb_atb_comp_clk_ovr_en(cb_atb_comp_clk_ovr_en);
    I01dphytester->cb_atb_ext_con(cb_atb_ext_con_int);
    I01dphytester->cb_atb_on_ovr(cb_atb_on_ovr);
    I01dphytester->cb_atb_on_ovr_en(cb_atb_on_ovr_en);
    I01dphytester->cb_atb_vbe_sel(cb_atb_vbe_sel_int);
    I01dphytester->cb_bg_en_ovr(cb_bg_en_ovr);
    I01dphytester->cb_bg_en_ovr_en(cb_bg_en_ovr_en);
    I01dphytester->cb_cal_en_ovr(cb_cal_en_ovr);
    I01dphytester->cb_cal_en_ovr_en(cb_cal_en_ovr_en);
    I01dphytester->cb_cal_repl_ovr_en(cb_cal_repl_ovr_en);
    I01dphytester->cb_chop_clk_en(cb_chop_clk_en_int);
    I01dphytester->cb_dac_prog_ovr_en(cb_dac_prog_ovr_en);
    I01dphytester->cb_en_45_ohm(cb_en_45_ohm_int);
    I01dphytester->cb_ical_x4_ovr(cb_ical_x4_ovr);
    I01dphytester->cb_ical_x4_ovr_en(cb_ical_x4_ovr_en);
    I01dphytester->cb_lpibias_en_ovr(cb_lpibias_en_ovr);
    I01dphytester->cb_lpibias_en_ovr_en(cb_lpibias_en_ovr_en);
    I01dphytester->cb_on_ovr(cb_on_ovr);
    I01dphytester->cb_on_ovr_en(cb_on_ovr_en);
    I01dphytester->cb_sel_chop_clk(cb_sel_chop_clk_int);
    I01dphytester->cb_sel_vreflptx_prog(cb_sel_vreflptx_prog_int);
    I01dphytester->clk_en_lanes_bypass(clk_en_lanes_bypass_tester);
    I01dphytester->clk_en_lanes_tester(clk_en_lanes_tester);
    I01dphytester->cmp_polarity(cmp_polarity);
    I01dphytester->comp_after_strobes(comp_after_strobes);
    I01dphytester->counter_for_des_en_bypass(counter_for_des_en_bypass);
    I01dphytester->ddl_osc_freq_target_ovr_en(ddl_osc_freq_target_ovr_en);
    I01dphytester->del_datalane_ovr_en(del_datalane_ovr_en);
    I01dphytester->deskew_legacyz_ovr(deskew_legacyz_ovr);
    I01dphytester->deskew_legacyz_ovr_en(deskew_legacyz_ovr_en);
    I01dphytester->deskew_numedges_update(deskew_numedges_update);
    I01dphytester->deskew_overflow(deskew_overflow);
    I01dphytester->deskew_polarity(deskew_polarity);
    I01dphytester->enable_lanes_ovr(enable_lanes_ovr);
    I01dphytester->enable_lanes_ovr_en(enable_lanes_ovr_en);
    I01dphytester->force_state(force_state);
    I01dphytester->force_state_bist(force_state_bist);
    I01dphytester->fsm_trigger_ovr(fsm_trigger_ovr);
    I01dphytester->fsm_trigger_ovr_en(fsm_trigger_ovr_en);
    I01dphytester->hsdirect(hsdirect_int);
    I01dphytester->hsfreqrange_ovr_en(hsfreqrange_ovr_en);
    I01dphytester->hsrx_en_bypass_clklane(hsrx_en_if_bypass_clklane_tester);
    I01dphytester->hsrx_en_bypass_lane0(hsrx_en_if_bypass_lane0_tester);
    I01dphytester->hsrx_en_bypass_lane1(hsrx_en_if_bypass_lane1_tester);
    I01dphytester->hsrx_en_bypass_lane2(hsrx_en_if_bypass_lane2_tester);
    I01dphytester->hsrx_en_bypass_lane3(hsrx_en_if_bypass_lane3_tester);
    I01dphytester->hsrx_en_if_clklane_tester(hsrx_en_if_clklane_tester);
    I01dphytester->hsrx_en_if_lane0_tester(hsrx_en_if_lane0_tester);
    I01dphytester->hsrx_en_if_lane1_tester(hsrx_en_if_lane1_tester);
    I01dphytester->hsrx_en_if_lane2_tester(hsrx_en_if_lane2_tester);
    I01dphytester->hsrx_en_if_lane3_tester(hsrx_en_if_lane3_tester);
    I01dphytester->hsrxenterm_bypass_clklane(hsrxenterm_bypass_clklane);
    I01dphytester->hsrxenterm_bypass_lane0(hsrxenterm_bypass_lane0);
    I01dphytester->hsrxenterm_bypass_lane1(hsrxenterm_bypass_lane1);
    I01dphytester->hsrxenterm_bypass_lane2(hsrxenterm_bypass_lane2);
    I01dphytester->hsrxenterm_bypass_lane3(hsrxenterm_bypass_lane3);
    I01dphytester->hsrxenterm_if_clklane(hsrxenterm_if_clklane);
    I01dphytester->hsrxenterm_if_lane0(hsrxenterm_if_lane0);
    I01dphytester->hsrxenterm_if_lane1(hsrxenterm_if_lane1);
    I01dphytester->hsrxenterm_if_lane2(hsrxenterm_if_lane2);
    I01dphytester->hsrxenterm_if_lane3(hsrxenterm_if_lane3);
    I01dphytester->hsrxpon_bypass_clklane(hsrxpon_bypass_clklane_tester);
    I01dphytester->hsrxpon_bypass_lane0(hsrxpon_bypass_lane0_tester);
    I01dphytester->hsrxpon_bypass_lane1(hsrxpon_bypass_lane1_tester);
    I01dphytester->hsrxpon_bypass_lane2(hsrxpon_bypass_lane2_tester);
    I01dphytester->hsrxpon_bypass_lane3(hsrxpon_bypass_lane3_tester);
    I01dphytester->hsrxpon_if_clklane(hsrxpon_clklane_tester);
    I01dphytester->hsrxpon_if_lane0(hsrxpon_lane0_tester);
    I01dphytester->hsrxpon_if_lane1(hsrxpon_lane1_tester);
    I01dphytester->hsrxpon_if_lane2(hsrxpon_lane2_tester);
    I01dphytester->hsrxpon_if_lane3(hsrxpon_lane3_tester);
    I01dphytester->hsrxtclkmisspd_ovr(hsrxtclkmisspd_ovr);
    I01dphytester->hsrxtclkmisspd_ovr_en(hsrxtclkmisspd_ovr_en);
    I01dphytester->hsrxthssettle_ovr_en_datalanes(hsrxthssettle_ovr_en_datalanes);
    I01dphytester->lb_pll_bustie_if(lb_pll_bustie_if);
    I01dphytester->lprxponcd_bypass_clklane(lprxponcd_bypass_clklane_tester);
    I01dphytester->lprxponcd_bypass_lane0(lprxponcd_bypass_lane0_tester);
    I01dphytester->lprxponcd_bypass_lane1(lprxponcd_bypass_lane1_tester);
    I01dphytester->lprxponcd_bypass_lane2(lprxponcd_bypass_lane2_tester);
    I01dphytester->lprxponcd_bypass_lane3(lprxponcd_bypass_lane3_tester);
    I01dphytester->lprxponcd_clklane(lprxponcd_clklane_tester);
    I01dphytester->lprxponcd_lane0(lprxponcd_lane0_tester);
    I01dphytester->lprxponcd_lane1(lprxponcd_lane1_tester);
    I01dphytester->lprxponcd_lane2(lprxponcd_lane2_tester);
    I01dphytester->lprxponcd_lane3(lprxponcd_lane3_tester);
    I01dphytester->lprxponlp_bypass_clklane(lprxponlp_bypass_clklane_tester);
    I01dphytester->lprxponlp_bypass_lane0(lprxponlp_bypass_lane0_tester);
    I01dphytester->lprxponlp_bypass_lane1(lprxponlp_bypass_lane1_tester);
    I01dphytester->lprxponlp_bypass_lane2(lprxponlp_bypass_lane2_tester);
    I01dphytester->lprxponlp_bypass_lane3(lprxponlp_bypass_lane3_tester);
    I01dphytester->lprxponlp_clklane(lprxponlp_clklane_tester);
    I01dphytester->lprxponlp_lane0(lprxponlp_lane0_tester);
    I01dphytester->lprxponlp_lane1(lprxponlp_lane1_tester);
    I01dphytester->lprxponlp_lane2(lprxponlp_lane2_tester);
    I01dphytester->lprxponlp_lane3(lprxponlp_lane3_tester);
    I01dphytester->lprxponulp_bypass_clklane(lprxponulp_bypass_clklane_tester);
    I01dphytester->lprxponulp_bypass_lane0(lprxponulp_bypass_lane0_tester);
    I01dphytester->lprxponulp_bypass_lane1(lprxponulp_bypass_lane1_tester);
    I01dphytester->lprxponulp_bypass_lane2(lprxponulp_bypass_lane2_tester);
    I01dphytester->lprxponulp_bypass_lane3(lprxponulp_bypass_lane3_tester);
    I01dphytester->lprxponulp_clklane(lprxponulp_clklane_tester);
    I01dphytester->lprxponulp_lane0(lprxponulp_lane0_tester);
    I01dphytester->lprxponulp_lane1(lprxponulp_lane1_tester);
    I01dphytester->lprxponulp_lane2(lprxponulp_lane2_tester);
    I01dphytester->lprxponulp_lane3(lprxponulp_lane3_tester);
    I01dphytester->lptxdin_bypass_lane0(lptxdin_bypass_lane0_tester);
    I01dphytester->lptxena_bypass_lane0(lptxena_bypass_lane0_tester);
    I01dphytester->lptxena_lane0(lptxena_lane0_tester);
    I01dphytester->lptxenpulldwn_bypass_lane0(lptxenpulldwn_bypass_lane0_tester);
    I01dphytester->lptxenpulldwn_lane0(lptxenpulldwn_lane0_tester);
    I01dphytester->lptxenulp_bypass_lane0(lptxenulp_bypass_lane0_tester);
    I01dphytester->lptxenulp_lane0(lptxenulp_lane0_tester);
    I01dphytester->lptxpon_bypass_lane0(lptxpon_bypass_lane0_tester);
    I01dphytester->lptxpon_lane0(lptxpon_lane0_tester);
    I01dphytester->mastermacro(mastermacro_int);
    I01dphytester->noalign_error(noalign_error);
    I01dphytester->noext_burnin_res_cal(noext_burnin_res_cal_int);
    I01dphytester->norexttune_done_ovr(norexttune_done_ovr);
    I01dphytester->norexttune_done_ovr_en(norexttune_done_ovr_en);
    I01dphytester->onpll_ovr(onpll_ovr);
    I01dphytester->onpll_ovr_en(onpll_ovr_en);
    I01dphytester->pg_bist_error(pg_bist_error_if);
    I01dphytester->pg_data_polarity(pg_data_polarity_if);
    I01dphytester->pg_data_swap(pg_data_swap_if);
    I01dphytester->pg_reg_err_r(pg_reg_err_r_if);
    I01dphytester->pll_atb_sense_sel(pll_atb_sense_sel_int);
    I01dphytester->pll_boot_from_start(pll_boot_from_start);
    I01dphytester->pll_clkouten_left(pll_clkouten_left_int);
    I01dphytester->pll_clkouten_right(pll_clkouten_right_int);
    I01dphytester->pll_clksel_en_ovr(pll_clksel_en_ovr);
    I01dphytester->pll_clksel_en_ovr_en(pll_clksel_en_ovr_en);
    I01dphytester->pll_force_fsm_state(pll_force_fsm_state);
    I01dphytester->pll_force_lock(pll_force_lock_int);
    I01dphytester->pll_gear_shift_ovr(pll_gear_shift_ovr);
    I01dphytester->pll_gear_shift_ovr_en(pll_gear_shift_ovr_en);
    I01dphytester->pll_lock_det_on_ovr(pll_lock_det_on_ovr);
    I01dphytester->pll_lock_det_on_ovr_en(pll_lock_det_on_ovr_en);
    I01dphytester->pll_lock_ovr(pll_lock_ovr);
    I01dphytester->pll_lock_ovr_en(pll_lock_ovr_en);
    I01dphytester->pll_lock_sel(pll_lock_sel_int);
    I01dphytester->pll_lockdet_mode(pll_lockdet_mode_int);
    I01dphytester->pll_m_ovr_en(pll_m_ovr_en);
    I01dphytester->pll_n_ovr_en(pll_n_ovr_en);
    I01dphytester->pll_pwron_ovr(pll_pwron_ovr);
    I01dphytester->pll_pwron_ovr_en(pll_pwron_ovr_en);
    I01dphytester->pll_reset_ovr(pll_reset_ovr);
    I01dphytester->pll_reset_ovr_en(pll_reset_ovr_en);
    I01dphytester->pll_vco_cntrl_ovr_en(pll_vco_cntrl_ovr_en);
    I01dphytester->polarity_clklane(polarity_clklane_int);
    I01dphytester->polarity_lane0(polarity_lane0_int);
    I01dphytester->polarity_lane1(polarity_lane1_int);
    I01dphytester->polarity_lane2(polarity_lane2_int);
    I01dphytester->polarity_lane3(polarity_lane3_int);
    I01dphytester->recal_lane0_ovr(recal_lane0_ovr);
    I01dphytester->recal_lane0_ovr_en(recal_lane0_ovr_en);
    I01dphytester->recal_lane1_ovr(recal_lane1_ovr);
    I01dphytester->recal_lane1_ovr_en(recal_lane1_ovr_en);
    I01dphytester->recal_lane2_ovr(recal_lane2_ovr);
    I01dphytester->recal_lane2_ovr_en(recal_lane2_ovr_en);
    I01dphytester->recal_lane3_ovr(recal_lane3_ovr);
    I01dphytester->recal_lane3_ovr_en(recal_lane3_ovr_en);
    I01dphytester->rx0_cal_done_ovr(rx0_cal_done_ovr);
    I01dphytester->rx0_cal_done_ovr_en(rx0_cal_done_ovr_en);
    I01dphytester->rx0_data_retiming_out_swap_sel(rx0_data_retiming_out_swap_sel);
    I01dphytester->rx0_ddl_finished_ovr(rx0_ddl_finished_ovr);
    I01dphytester->rx0_ddl_finished_ovr_en(rx0_ddl_finished_ovr_en);
    I01dphytester->rx0_ddl_start_calibration_ovr(rx0_ddl_start_calibration_ovr);
    I01dphytester->rx0_ddl_start_calibration_ovr_en(rx0_ddl_start_calibration_ovr_en);
    I01dphytester->rx0_lb_res_cm_en_if(rx0_lb_res_cm_en_if);
    I01dphytester->rx0_lb_res_dn_en_if(rx0_lb_res_dn_en_if);
    I01dphytester->rx0_lb_res_dp_en_if(rx0_lb_res_dp_en_if);
    I01dphytester->rx0_osc_freq_target_if_ovr_en(rx0_osc_freq_target_if_ovr_en);
    I01dphytester->rx0_pm_man_error_latch(rx0_pm_man_error_latch);
    I01dphytester->rx0_pm_sample_error_counter(rx0_pm_sample_error_counter_int);
    I01dphytester->rx0_pm_start_control_ovr(rx0_pm_start_control_ovr);
    I01dphytester->rx0_pm_start_control_ovr_en(rx0_pm_start_control_ovr_en);
    I01dphytester->rx0_rxhs_clk_from_long_channel_if(rx0_rxhs_clk_from_long_channel_if);
    I01dphytester->rx0_rxhs_data_offset_cal_en_ovr(rx0_rxhs_data_offset_cal_en_ovr);
    I01dphytester->rx0_rxhs_data_offset_cal_en_ovr_en(rx0_rxhs_data_offset_cal_en_ovr_en);
    I01dphytester->rx0_rxhs_data_offset_cal_ovr_en(rx0_rxhs_data_offset_cal_ovr_en);
    I01dphytester->rx0_rxhs_data_ovr_en(rx0_rxhs_data_ovr_en);
    I01dphytester->rx0_rxhs_ddl_phase_change_p_ovr(rx0_rxhs_ddl_phase_change_p_ovr);
    I01dphytester->rx0_rxhs_ddl_phase_change_p_ovr_en(rx0_rxhs_ddl_phase_change_p_ovr_en);
    I01dphytester->rx0_rxhs_ddl_phase_clk_ovr_en(rx0_rxhs_ddl_phase_clk_ovr_en);
    I01dphytester->rx0_rxhs_ddl_phase_data_ovr_en(rx0_rxhs_ddl_phase_data_ovr_en);
    I01dphytester->rx0_rxhs_ddl_tune_en_bypass(rx0_rxhs_ddl_tune_en_bypass);
    I01dphytester->rx0_rxhs_ddl_tune_en_if(rx0_rxhs_ddl_tune_en_tester);
    I01dphytester->rx0_rxhs_ddl_tune_ovr_en(rx0_rxhs_ddl_tune_ovr_en);
    I01dphytester->rx0_rxhs_feed_int_clk_ovr(rx0_rxhs_feed_int_clk_ovr);
    I01dphytester->rx0_rxhs_feed_int_clk_ovr_en(rx0_rxhs_feed_int_clk_ovr_en);
    I01dphytester->rx0_rxhs_repeatcal(rx0_rxhs_repeatcal);
    I01dphytester->rx0_start_calibration_ovr(rx0_start_calibration_ovr);
    I01dphytester->rx0_start_calibration_ovr_en(rx0_start_calibration_ovr_en);
    I01dphytester->rx0_txlp_sr_bypass_if(rx0_txlp_sr_bypass_if);
    I01dphytester->rx1_cal_done_ovr(rx1_cal_done_ovr);
    I01dphytester->rx1_cal_done_ovr_en(rx1_cal_done_ovr_en);
    I01dphytester->rx1_data_retiming_out_swap_sel(rx1_data_retiming_out_swap_sel);
    I01dphytester->rx1_ddl_finished_ovr(rx1_ddl_finished_ovr);
    I01dphytester->rx1_ddl_finished_ovr_en(rx1_ddl_finished_ovr_en);
    I01dphytester->rx1_ddl_start_calibration_ovr(rx1_ddl_start_calibration_ovr);
    I01dphytester->rx1_ddl_start_calibration_ovr_en(rx1_ddl_start_calibration_ovr_en);
    I01dphytester->rx1_lb_res_cm_en_if(rx1_lb_res_cm_en_if);
    I01dphytester->rx1_lb_res_dn_en_if(rx1_lb_res_dn_en_if);
    I01dphytester->rx1_lb_res_dp_en_if(rx1_lb_res_dp_en_if);
    I01dphytester->rx1_osc_freq_target_if_ovr_en(rx1_osc_freq_target_if_ovr_en);
    I01dphytester->rx1_pm_man_error_latch(rx1_pm_man_error_latch);
    I01dphytester->rx1_pm_sample_error_counter(rx1_pm_sample_error_counter_int);
    I01dphytester->rx1_pm_start_control_ovr(rx1_pm_start_control_ovr);
    I01dphytester->rx1_pm_start_control_ovr_en(rx1_pm_start_control_ovr_en);
    I01dphytester->rx1_rxhs_clk_from_long_channel_if(rx1_rxhs_clk_from_long_channel_if);
    I01dphytester->rx1_rxhs_data_offset_cal_en_ovr(rx1_rxhs_data_offset_cal_en_ovr);
    I01dphytester->rx1_rxhs_data_offset_cal_en_ovr_en(rx1_rxhs_data_offset_cal_en_ovr_en);
    I01dphytester->rx1_rxhs_data_offset_cal_ovr_en(rx1_rxhs_data_offset_cal_ovr_en);
    I01dphytester->rx1_rxhs_data_ovr_en(rx1_rxhs_data_ovr_en);
    I01dphytester->rx1_rxhs_ddl_phase_change_p_ovr(rx1_rxhs_ddl_phase_change_p_ovr);
    I01dphytester->rx1_rxhs_ddl_phase_change_p_ovr_en(rx1_rxhs_ddl_phase_change_p_ovr_en);
    I01dphytester->rx1_rxhs_ddl_phase_clk_ovr_en(rx1_rxhs_ddl_phase_clk_ovr_en);
    I01dphytester->rx1_rxhs_ddl_phase_data_ovr_en(rx1_rxhs_ddl_phase_data_ovr_en);
    I01dphytester->rx1_rxhs_ddl_tune_en_bypass(rx1_rxhs_ddl_tune_en_bypass);
    I01dphytester->rx1_rxhs_ddl_tune_en_if(rx1_rxhs_ddl_tune_en_tester);
    I01dphytester->rx1_rxhs_ddl_tune_ovr_en(rx1_rxhs_ddl_tune_ovr_en);
    I01dphytester->rx1_rxhs_feed_int_clk_ovr(rx1_rxhs_feed_int_clk_ovr);
    I01dphytester->rx1_rxhs_feed_int_clk_ovr_en(rx1_rxhs_feed_int_clk_ovr_en);
    I01dphytester->rx1_rxhs_repeatcal(rx1_rxhs_repeatcal);
    I01dphytester->rx1_start_calibration_ovr(rx1_start_calibration_ovr);
    I01dphytester->rx1_start_calibration_ovr_en(rx1_start_calibration_ovr_en);
    I01dphytester->rx2_cal_done_ovr(rx2_cal_done_ovr);
    I01dphytester->rx2_cal_done_ovr_en(rx2_cal_done_ovr_en);
    I01dphytester->rx2_data_retiming_out_swap_sel(rx2_data_retiming_out_swap_sel);
    I01dphytester->rx2_ddl_finished_ovr(rx2_ddl_finished_ovr);
    I01dphytester->rx2_ddl_finished_ovr_en(rx2_ddl_finished_ovr_en);
    I01dphytester->rx2_ddl_start_calibration_ovr(rx2_ddl_start_calibration_ovr);
    I01dphytester->rx2_ddl_start_calibration_ovr_en(rx2_ddl_start_calibration_ovr_en);
    I01dphytester->rx2_lb_res_cm_en_if(rx2_lb_res_cm_en_if);
    I01dphytester->rx2_lb_res_dn_en_if(rx2_lb_res_dn_en_if);
    I01dphytester->rx2_lb_res_dp_en_if(rx2_lb_res_dp_en_if);
    I01dphytester->rx2_osc_freq_target_if_ovr_en(rx2_osc_freq_target_if_ovr_en);
    I01dphytester->rx2_pm_man_error_latch(rx2_pm_man_error_latch);
    I01dphytester->rx2_pm_sample_error_counter(rx2_pm_sample_error_counter_int);
    I01dphytester->rx2_pm_start_control_ovr(rx2_pm_start_control_ovr);
    I01dphytester->rx2_pm_start_control_ovr_en(rx2_pm_start_control_ovr_en);
    I01dphytester->rx2_rxhs_clk_from_long_channel_if(rx2_rxhs_clk_from_long_channel_if);
    I01dphytester->rx2_rxhs_data_offset_cal_en_ovr(rx2_rxhs_data_offset_cal_en_ovr);
    I01dphytester->rx2_rxhs_data_offset_cal_en_ovr_en(rx2_rxhs_data_offset_cal_en_ovr_en);
    I01dphytester->rx2_rxhs_data_offset_cal_ovr_en(rx2_rxhs_data_offset_cal_ovr_en);
    I01dphytester->rx2_rxhs_data_ovr_en(rx2_rxhs_data_ovr_en);
    I01dphytester->rx2_rxhs_ddl_phase_change_p_ovr(rx2_rxhs_ddl_phase_change_p_ovr);
    I01dphytester->rx2_rxhs_ddl_phase_change_p_ovr_en(rx2_rxhs_ddl_phase_change_p_ovr_en);
    I01dphytester->rx2_rxhs_ddl_phase_clk_ovr_en(rx2_rxhs_ddl_phase_clk_ovr_en);
    I01dphytester->rx2_rxhs_ddl_phase_data_ovr_en(rx2_rxhs_ddl_phase_data_ovr_en);
    I01dphytester->rx2_rxhs_ddl_tune_en_bypass(rx2_rxhs_ddl_tune_en_bypass);
    I01dphytester->rx2_rxhs_ddl_tune_en_if(rx2_rxhs_ddl_tune_en_tester);
    I01dphytester->rx2_rxhs_ddl_tune_ovr_en(rx2_rxhs_ddl_tune_ovr_en);
    I01dphytester->rx2_rxhs_feed_int_clk_ovr(rx2_rxhs_feed_int_clk_ovr);
    I01dphytester->rx2_rxhs_feed_int_clk_ovr_en(rx2_rxhs_feed_int_clk_ovr_en);
    I01dphytester->rx2_rxhs_repeatcal(rx2_rxhs_repeatcal);
    I01dphytester->rx2_start_calibration_ovr(rx2_start_calibration_ovr);
    I01dphytester->rx2_start_calibration_ovr_en(rx2_start_calibration_ovr_en);
    I01dphytester->rx3_cal_done_ovr(rx3_cal_done_ovr);
    I01dphytester->rx3_cal_done_ovr_en(rx3_cal_done_ovr_en);
    I01dphytester->rx3_data_retiming_out_swap_sel(rx3_data_retiming_out_swap_sel);
    I01dphytester->rx3_ddl_finished_ovr(rx3_ddl_finished_ovr);
    I01dphytester->rx3_ddl_finished_ovr_en(rx3_ddl_finished_ovr_en);
    I01dphytester->rx3_ddl_start_calibration_ovr(rx3_ddl_start_calibration_ovr);
    I01dphytester->rx3_ddl_start_calibration_ovr_en(rx3_ddl_start_calibration_ovr_en);
    I01dphytester->rx3_lb_res_cm_en_if(rx3_lb_res_cm_en_if);
    I01dphytester->rx3_lb_res_dn_en_if(rx3_lb_res_dn_en_if);
    I01dphytester->rx3_lb_res_dp_en_if(rx3_lb_res_dp_en_if);
    I01dphytester->rx3_osc_freq_target_if_ovr_en(rx3_osc_freq_target_if_ovr_en);
    I01dphytester->rx3_pm_man_error_latch(rx3_pm_man_error_latch);
    I01dphytester->rx3_pm_sample_error_counter(rx3_pm_sample_error_counter_int);
    I01dphytester->rx3_pm_start_control_ovr(rx3_pm_start_control_ovr);
    I01dphytester->rx3_pm_start_control_ovr_en(rx3_pm_start_control_ovr_en);
    I01dphytester->rx3_rxhs_clk_from_long_channel_if(rx3_rxhs_clk_from_long_channel_if);
    I01dphytester->rx3_rxhs_data_offset_cal_en_ovr(rx3_rxhs_data_offset_cal_en_ovr);
    I01dphytester->rx3_rxhs_data_offset_cal_en_ovr_en(rx3_rxhs_data_offset_cal_en_ovr_en);
    I01dphytester->rx3_rxhs_data_offset_cal_ovr_en(rx3_rxhs_data_offset_cal_ovr_en);
    I01dphytester->rx3_rxhs_data_ovr_en(rx3_rxhs_data_ovr_en);
    I01dphytester->rx3_rxhs_ddl_phase_change_p_ovr(rx3_rxhs_ddl_phase_change_p_ovr);
    I01dphytester->rx3_rxhs_ddl_phase_change_p_ovr_en(rx3_rxhs_ddl_phase_change_p_ovr_en);
    I01dphytester->rx3_rxhs_ddl_phase_clk_ovr_en(rx3_rxhs_ddl_phase_clk_ovr_en);
    I01dphytester->rx3_rxhs_ddl_phase_data_ovr_en(rx3_rxhs_ddl_phase_data_ovr_en);
    I01dphytester->rx3_rxhs_ddl_tune_en_bypass(rx3_rxhs_ddl_tune_en_bypass);
    I01dphytester->rx3_rxhs_ddl_tune_en_if(rx3_rxhs_ddl_tune_en_tester);
    I01dphytester->rx3_rxhs_ddl_tune_ovr_en(rx3_rxhs_ddl_tune_ovr_en);
    I01dphytester->rx3_rxhs_feed_int_clk_ovr(rx3_rxhs_feed_int_clk_ovr);
    I01dphytester->rx3_rxhs_feed_int_clk_ovr_en(rx3_rxhs_feed_int_clk_ovr_en);
    I01dphytester->rx3_rxhs_repeatcal(rx3_rxhs_repeatcal);
    I01dphytester->rx3_start_calibration_ovr(rx3_start_calibration_ovr);
    I01dphytester->rx3_start_calibration_ovr_en(rx3_start_calibration_ovr_en);
    I01dphytester->rx_cb_ibias_en_if(rx_cb_ibias_en_if);
    I01dphytester->rx_cb_lb_en_rext_if(rx_cb_lb_en_rext_if);
    I01dphytester->rx_cb_pre_iref_on(rx_cb_pre_iref_on_if);
    I01dphytester->rx_des_en_bypass_lane0(rx_des_en_bypass_lane0_tester);
    I01dphytester->rx_des_en_bypass_lane1(rx_des_en_bypass_lane1_tester);
    I01dphytester->rx_des_en_bypass_lane2(rx_des_en_bypass_lane2_tester);
    I01dphytester->rx_des_en_bypass_lane3(rx_des_en_bypass_lane3_tester);
    I01dphytester->rx_des_en_lane0(rx_des_en_lane0_tester);
    I01dphytester->rx_des_en_lane1(rx_des_en_lane1_tester);
    I01dphytester->rx_des_en_lane2(rx_des_en_lane2_tester);
    I01dphytester->rx_des_en_lane3(rx_des_en_lane3_tester);
    I01dphytester->rx_rxhs_compatibility_mode_ovr(rx_rxhs_compatibility_mode_ovr);
    I01dphytester->rx_rxhs_compatibility_mode_ovr_en(rx_rxhs_compatibility_mode_ovr_en);
    I01dphytester->rx_rxhs_gmode_if_ovr_en(rx_rxhs_gmode_if_ovr_en);
    I01dphytester->rx_rxhs_prog_ovr_en(rx_rxhs_prog_ovr_en);
    I01dphytester->rx_rxhs_setr_ovr_en(rx_rxhs_setr_ovr_en);
    I01dphytester->rx_rxlp_bias_prog(rx_rxlp_bias_prog_int);
    I01dphytester->rxclk_caldone_ovr(rxclk_caldone_ovr);
    I01dphytester->rxclk_caldone_ovr_en(rxclk_caldone_ovr_en);
    I01dphytester->rxclk_lb_res_cm_en_if(rxclk_lb_res_cm_en_if);
    I01dphytester->rxclk_lb_res_dn_en_if(rxclk_lb_res_dn_en_if);
    I01dphytester->rxclk_lb_res_dp_en_if(rxclk_lb_res_dp_en_if);
    I01dphytester->rxclk_lb_sel_ovr(rxclk_lb_sel_ovr);
    I01dphytester->rxclk_lb_sel_ovr_en(rxclk_lb_sel_ovr_en);
    I01dphytester->rxclk_lbck_clk_en_ovr(rxclk_lbck_clk_en_ovr);
    I01dphytester->rxclk_lbck_clk_en_ovr_en(rxclk_lbck_clk_en_ovr_en);
    I01dphytester->rxclk_lbck_clk_kill_if(rxclk_lbck_clk_kill_if);
    I01dphytester->rxclk_lbck_div_en_ovr(rxclk_lbck_div_en_ovr);
    I01dphytester->rxclk_lbck_div_en_ovr_en(rxclk_lbck_div_en_ovr_en);
    I01dphytester->rxclk_lbck_serial_clk_en_ovr(rxclk_lbck_serial_clk_en_ovr);
    I01dphytester->rxclk_lbck_serial_clk_en_ovr_en(rxclk_lbck_serial_clk_en_ovr_en);
    I01dphytester->rxclk_lbck_serial_en_ovr(rxclk_lbck_serial_en_ovr);
    I01dphytester->rxclk_lbck_serial_en_ovr_en(rxclk_lbck_serial_en_ovr_en);
    I01dphytester->rxclk_lbdata_clk_en_ovr(rxclk_lbdata_clk_en_ovr);
    I01dphytester->rxclk_lbdata_clk_en_ovr_en(rxclk_lbdata_clk_en_ovr_en);
    I01dphytester->rxclk_lbdata_clk_kill_if(rxclk_lbdata_clk_kill_if);
    I01dphytester->rxclk_lbdata_div_en_ovr(rxclk_lbdata_div_en_ovr);
    I01dphytester->rxclk_lbdata_div_en_ovr_en(rxclk_lbdata_div_en_ovr_en);
    I01dphytester->rxclk_lbdata_serial_clk_en_ovr(rxclk_lbdata_serial_clk_en_ovr);
    I01dphytester->rxclk_lbdata_serial_clk_en_ovr_en(rxclk_lbdata_serial_clk_en_ovr_en);
    I01dphytester->rxclk_lbdata_serial_en_ovr(rxclk_lbdata_serial_en_ovr);
    I01dphytester->rxclk_lbdata_serial_en_ovr_en(rxclk_lbdata_serial_en_ovr_en);
    I01dphytester->rxclk_rxhs_clk_offset_cal_ovr_en(rxclk_rxhs_clk_offset_cal_ovr_en);
    I01dphytester->rxclk_rxhs_clk_to_long_channel_if(rxclk_rxhs_clk_to_long_channel_if);
    I01dphytester->rxclk_rxhs_ddr_clk_div_ovr_en(rxclk_rxhs_ddr_clk_div_ovr_en);
    I01dphytester->rxclk_rxhs_ddr_clk_en_bypass(rxclk_rxhs_ddr_clk_en_bypass);
    I01dphytester->rxclk_rxhs_ddr_clk_en_if(rxclk_rxhs_ddr_clk_en_if);
    I01dphytester->rxclk_rxhs_ddr_clk_sel_if(rxclk_rxhs_ddr_clk_sel_if);
    I01dphytester->rxclk_rxhs_feed_int_clk_ovr(rxclk_rxhs_feed_int_clk_ovr);
    I01dphytester->rxclk_rxhs_feed_int_clk_ovr_en(rxclk_rxhs_feed_int_clk_ovr_en);
    I01dphytester->rxclk_rxhs_int_clk_sel(rxclk_rxhs_int_clk_sel_int);
    I01dphytester->rxclk_rxhs_osc_pon_ovr(rxclk_rxhs_osc_pon_ovr);
    I01dphytester->rxclk_rxhs_osc_pon_ovr_en(rxclk_rxhs_osc_pon_ovr_en);
    I01dphytester->rxclk_rxhs_osc_res_en_ovr(rxclk_rxhs_osc_res_en_ovr);
    I01dphytester->rxclk_rxhs_osc_res_en_ovr_en(rxclk_rxhs_osc_res_en_ovr_en);
    I01dphytester->rxclk_rxhs_pull_long_channel_if(rxclk_rxhs_pull_long_channel_if);
    I01dphytester->rxclk_rxhs_rx_en200mode_if(rxclk_rxhs_rx_en200mode_if);
    I01dphytester->rxclk_rxhs_start_calibration_ovr(rxclk_rxhs_start_calibration_ovr);
    I01dphytester->rxclk_rxhs_start_calibration_ovr_en(rxclk_rxhs_start_calibration_ovr_en);
    I01dphytester->rxclk_start_calibration_ovr(rxclk_start_calibration_ovr);
    I01dphytester->rxclk_start_calibration_ovr_en(rxclk_start_calibration_ovr_en);
    I01dphytester->skew_back_comp_en_ovr(skew_back_comp_en_ovr);
    I01dphytester->skew_back_comp_en_ovr_en(skew_back_comp_en_ovr_en);
    I01dphytester->skew_mux_run(skew_mux_run);
    I01dphytester->skew_mux_sel(skew_mux_sel);
    I01dphytester->tafailsafez(tafailsafez_int);
    I01dphytester->tclk_miss_div2_ovr(tclk_miss_div2_ovr);
    I01dphytester->tclk_miss_div2_ovr_en(tclk_miss_div2_ovr_en);
    I01dphytester->timebase_ovr_en(timebase_ovr_en);
    I01dphytester->ddl_osc_freq_target_ovr(ddl_osc_freq_target_ovr);
    I01dphytester->rx0_osc_freq_target_if_ovr(rx0_osc_freq_target_if_ovr);
    I01dphytester->rx1_osc_freq_target_if_ovr(rx1_osc_freq_target_if_ovr);
    I01dphytester->rx2_osc_freq_target_if_ovr(rx2_osc_freq_target_if_ovr);
    I01dphytester->rx3_osc_freq_target_if_ovr(rx3_osc_freq_target_if_ovr);
    I01dphytester->rx_ddl_offset_error(rx_ddl_offset_error_tester);
    I01dphytester->pll_mpll_prog(pll_mpll_prog_int);
    I01dphytester->pll_meas_iv(pll_meas_iv_int);
    I01dphytester->aligner_dk_cnf(aligner_dk_cnf);
    I01dphytester->cb_atb_sel(cb_atb_sel_int);
    I01dphytester->cb_sel_vref_lprx(cb_sel_vref_lprx_int);
    I01dphytester->cb_sel_vrefcd_lprx(cb_sel_vrefcd_lprx_int);
    I01dphytester->deskew_jump2steps(deskew_jump2steps);
    I01dphytester->deskew_latency(deskew_latency);
    I01dphytester->lptxdin_lane0(lptxdin_lane0_tester);
    I01dphytester->pll_gmp_cntrl(pll_gmp_cntrl_int);
    I01dphytester->rx0_txlp_eniocont_if(rx0_txlp_eniocont_if);
    I01dphytester->rx_rxhs_ddl_clk_out_div(rx_rxhs_ddl_clk_out_div_if);
    I01dphytester->rx_rxhs_gmode_if_ovr(rx_rxhs_gmode_if_ovr);
    I01dphytester->rx_rxhs_prog_ovr(rx_rxhs_prog_ovr);
    I01dphytester->rxclk_rxhs_ddr_clk_div_ovr(rxclk_rxhs_ddr_clk_div_ovr);
    I01dphytester->tclk_miss_opt(tclk_miss_opt_int);
    I01dphytester->cb_sel_v400_prog(cb_sel_v400_prog_int);
    I01dphytester->cb_vref_mpll_reg_sel(rx_cb_vref_mpll_reg_sel_if);
    I01dphytester->number_of_ready_count(number_of_ready_count);
    I01dphytester->pll_tstplldig(pll_tstplldig_int);
    I01dphytester->rx0_rxhs_data_eq_if(rx0_rxhs_data_eq_if);
    I01dphytester->rx1_rxhs_data_eq_if(rx1_rxhs_data_eq_if);
    I01dphytester->rx2_rxhs_data_eq_if(rx2_rxhs_data_eq_if);
    I01dphytester->rx3_rxhs_data_eq_if(rx3_rxhs_data_eq_if);
    I01dphytester->rx_cb_lb_setrx_if(rx_cb_lb_setrx_if);
    I01dphytester->rx_rxhs_setr_ovr(rx_rxhs_setr_ovr);
    I01dphytester->txlp_prog(rx_txlp_prog_if);
    I01dphytester->atb_sel_clklane(atb_sel_clklane_tester);
    I01dphytester->atb_sel_lane0(atb_sel_lane0_tester);
    I01dphytester->atb_sel_lane1(atb_sel_lane1_tester);
    I01dphytester->atb_sel_lane2(atb_sel_lane2_tester);
    I01dphytester->atb_sel_lane3(atb_sel_lane3_tester);
    I01dphytester->cb_cal_repl_ovr(cb_cal_repl_ovr);
    I01dphytester->counter_for_des_en_config_if(counter_for_des_en_config_if);
    I01dphytester->pg_mode_r(pg_mode_r_if);
    I01dphytester->pll_n_ovr(pll_n_ovr);
    I01dphytester->pll_startup_fsm_state(pll_startup_fsm_state);
    I01dphytester->rx0_lb_prog_if(rx0_lb_prog_if);
    I01dphytester->rx0_pm_mode_r(rx0_pm_mode_r_if);
    I01dphytester->rx0_rxhs_ddl_phase_data_ovr(rx0_rxhs_ddl_phase_data_ovr);
    I01dphytester->rx1_lb_prog_if(rx1_lb_prog_if);
    I01dphytester->rx1_pm_mode_r(rx1_pm_mode_r_if);
    I01dphytester->rx1_rxhs_ddl_phase_data_ovr(rx1_rxhs_ddl_phase_data_ovr);
    I01dphytester->rx2_lb_prog_if(rx2_lb_prog_if);
    I01dphytester->rx2_pm_mode_r(rx2_pm_mode_r_if);
    I01dphytester->rx2_rxhs_ddl_phase_data_ovr(rx2_rxhs_ddl_phase_data_ovr);
    I01dphytester->rx3_lb_prog_if(rx3_lb_prog_if);
    I01dphytester->rx3_pm_mode_r(rx3_pm_mode_r_if);
    I01dphytester->rx3_rxhs_ddl_phase_data_ovr(rx3_rxhs_ddl_phase_data_ovr);
    I01dphytester->rxclk_lb_prog_if(rxclk_lb_prog_if);
    I01dphytester->setr_max(setr_max);
    I01dphytester->setr_min(setr_min);
    I01dphytester->setr_typ(setr_typ);
    I01dphytester->target_state(target_state);
    I01dphytester->target_state_bist(target_state_bist);
    I01dphytester->pll_opmode(pll_opmode_int);
    I01dphytester->rx0_rxhs_ddl_phase_clk_ovr(rx0_rxhs_ddl_phase_clk_ovr);
    I01dphytester->rx0_rxhs_ddl_tune_ovr(rx0_rxhs_ddl_tune_ovr);
    I01dphytester->rx1_rxhs_ddl_phase_clk_ovr(rx1_rxhs_ddl_phase_clk_ovr);
    I01dphytester->rx1_rxhs_ddl_tune_ovr(rx1_rxhs_ddl_tune_ovr);
    I01dphytester->rx2_rxhs_ddl_phase_clk_ovr(rx2_rxhs_ddl_phase_clk_ovr);
    I01dphytester->rx2_rxhs_ddl_tune_ovr(rx2_rxhs_ddl_tune_ovr);
    I01dphytester->rx3_rxhs_ddl_phase_clk_ovr(rx3_rxhs_ddl_phase_clk_ovr);
    I01dphytester->rx3_rxhs_ddl_tune_ovr(rx3_rxhs_ddl_tune_ovr);
    I01dphytester->pll_int_cntrl(pll_int_cntrl_int);
    I01dphytester->pll_prop_cntrl(pll_prop_cntrl_int);
    I01dphytester->pll_th_delay(pll_th_delay_int);
    I01dphytester->pll_vco_cntrl_ovr(pll_vco_cntrl_ovr);
    I01dphytester->rx0_rxhs_data_offset_cal_ovr(rx0_rxhs_data_offset_cal_ovr);
    I01dphytester->rx1_rxhs_data_offset_cal_ovr(rx1_rxhs_data_offset_cal_ovr);
    I01dphytester->rx2_rxhs_data_offset_cal_ovr(rx2_rxhs_data_offset_cal_ovr);
    I01dphytester->rx3_rxhs_data_offset_cal_ovr(rx3_rxhs_data_offset_cal_ovr);
    I01dphytester->rxclk_rxhs_clk_offset_cal_ovr(rxclk_rxhs_clk_offset_cal_ovr);
    I01dphytester->ttago(ttago_datalanes_int);
    I01dphytester->ttasure(ttasure_datalanes_int);
    I01dphytester->ttawait(ttawait_datalanes_int);
    I01dphytester->hsfreqrange_ovr(hsfreqrange_ovr);
    I01dphytester->hsrxthssettle_datalanes_ovr(hsrxthssettle_datalanes_ovr);
    I01dphytester->pll_cpbias_cntrl(pll_cpbias_cntrl_int);
    I01dphytester->del_datalane_ovr(del_datalane_ovr);
    I01dphytester->deskew_numedges(deskew_numedges);
    I01dphytester->offset_comp_threshold(offset_comp_threshold);
    I01dphytester->pg_usert_pat(pg_usert_pat_if);
    I01dphytester->pipe_clean_time(pipe_clean_time);
    I01dphytester->pll_th2(pll_th2_int);
    I01dphytester->pll_th3(pll_th3_int);
    I01dphytester->rx0_rxhs_data_ovr(rx0_rxhs_data_ovr);
    I01dphytester->rx1_rxhs_data_ovr(rx1_rxhs_data_ovr);
    I01dphytester->rx2_rxhs_data_ovr(rx2_rxhs_data_ovr);
    I01dphytester->rx3_rxhs_data_ovr(rx3_rxhs_data_ovr);
    I01dphytester->rxclk_lbck_data_if(rxclk_lbck_data_if);
    I01dphytester->rxpontime(rxpontime);
    I01dphytester->stability_time(stability_time_tester);
    I01dphytester->testdout(testdout);
    I01dphytester->timebase_ovr(timebase_ovr);
    I01dphytester->tolerance(tolerance);
    I01dphytester->pll_gear_shift_time(pll_gear_shift_time);
    I01dphytester->pll_lock_det_time(pll_lock_det_time);
    I01dphytester->pll_rst_time(pll_rst_time);
    I01dphytester->cb_dac_prog_ovr(cb_dac_prog_ovr);
    I01dphytester->count_ref(count_ref);
    I01dphytester->pll_m_ovr(pll_m_ovr);
    I01dphytester->pll_th1(pll_th1_int);
    I01dphytester->timeinitbg(timeinitbg);
    I01dphytester->twakeupulpm(twakeupulpm);
    I01dphytester->txdindlytime(txdindlytime);
    I01dphytester->txstarttime(txstarttime);
    // trigger from top pin or by manual bist: only when IF calibrations are finished
    gen_pll_lock_sync.reset(new mipi_4_rx_dphy_ns_gen_sync__reset_edge0("gen_pll_lock_sync"));
    gen_pll_lock_sync->clk(cfg_clk_scanable);
    gen_pll_lock_sync->rst(sresetz_cfg_clk_scanable);
    gen_pll_lock_sync->data_in(pll_lock_int);
    gen_pll_lock_sync->scanmode(tie0_);
    gen_pll_lock_sync->data_out(pll_lock_int_sync);
    gen_pll_lock_sync->data_out_dly(nc_gen_pll_lock_sync_data_out_dly);
    // check this value  ( comes from tester )
    gen_comp_after_strobes_sync.reset(new mipi_4_rx_dphy_ns_gen_sync__reset_edge0("gen_comp_after_strobes_sync"));
    gen_comp_after_strobes_sync->clk(cfg_clk_scanable);
    gen_comp_after_strobes_sync->rst(sresetz_cfg_clk_scanable);
    gen_comp_after_strobes_sync->data_in(comp_after_strobes);
    gen_comp_after_strobes_sync->scanmode(tie0_);
    gen_comp_after_strobes_sync->data_out(comp_after_strobes_sync);
    gen_comp_after_strobes_sync->data_out_dly(nc_gen_comp_after_strobes_sync_data_out_dly);
    SC_METHOD(glue_I01bist_machine_rst);
    sensitive << sresetz_cfg_clk_scanable;
    SC_METHOD(glue_I01bist_machine_ch_error);
    sensitive << rx3_pm_error_pm_if << rx2_pm_error_pm_if << rx1_pm_error_pm_if << rx0_pm_error_pm_if;
    SC_METHOD(split_I01bist_machine_ch_error_latch);
    sensitive << c_I01bist_machine_ch_error_latch;
    dont_initialize();
    SC_METHOD(glue_I01bist_machine_scanrst);
    sensitive << scan_set_rst_n;
    I01bist_machine.reset(new mipi_4_rx_dphy_ns_bist_machine_rx("I01bist_machine"));
    I01bist_machine->clk(cfg_clk_scanable);
    I01bist_machine->rst(c_I01bist_machine_rst);
    I01bist_machine->bist_en(biston_int);
    I01bist_machine->fsm_done(fsm_trigger);
    I01bist_machine->onpll(onpll_int);
    I01bist_machine->bist_sync_en(bist_sync_en_int);
    I01bist_machine->ch_pat_strobe(pg_pat_strobe_if);
    I01bist_machine->comp_after_strobes(comp_after_strobes_sync);
    I01bist_machine->ch_error(c_I01bist_machine_ch_error);
    I01bist_machine->ch_error_latch(c_I01bist_machine_ch_error_latch);
    I01bist_machine->ch_bist_run(lane_bist_to_run);
    I01bist_machine->error_gating_out(error_gating_out);
    I01bist_machine->bist_done(bistdone);
    I01bist_machine->bist_ok(bistok);
    I01bist_machine->clk_en_clklane(rxclk_lbck_clk_en_int);
    I01bist_machine->div_en_clklane(rxclk_lbck_div_en_int);
    I01bist_machine->serial_clk_en_clklane(rxclk_lbck_serial_clk_en_int);
    I01bist_machine->serial_en_clklane(rxclk_lbck_serial_en_int);
    I01bist_machine->clksel_en(pll_clksel_en_int);
    I01bist_machine->clk_en_datalane(rxclk_lbdata_clk_en_int);
    I01bist_machine->div_en_datalane(rxclk_lbdata_div_en_int);
    I01bist_machine->serial_data_en_datalane(rxclk_lbdata_serial_clk_en_int);
    I01bist_machine->serial_en_datalane(rxclk_lbdata_serial_en_int);
    I01bist_machine->del_datalane(del_datalane);
    I01bist_machine->trigger_hsdirect(trigger_hsdirect_int);
    I01bist_machine->switch_on_lb_mux(rxclk_lb_sel_int);
    I01bist_machine->feed_int_in_bist(bist_control_int);
    I01bist_machine->force_state(force_state_bist);
    I01bist_machine->target_state(target_state_bist);
    I01bist_machine->bist_state(bist_state);
    I01bist_machine->pipe_clean_time(pipe_clean_time);
    I01bist_machine->scanmode(scan_mode);
    I01bist_machine->scanclk(cfg_clk_scanable);
    I01bist_machine->scanrst(c_I01bist_machine_scanrst);
    // PLL STARTUP FSM
    I01pllstartup.reset(new mipi_4_rx_dphy_ns_mpll_startup("I01pllstartup"));
    I01pllstartup->rstz(sresetz_cfg_clk_scanable);
    I01pllstartup->cfgclk(cfg_clk_scanable);
    I01pllstartup->onpll(onpll);
    I01pllstartup->force_state(pll_force_fsm_state);
    I01pllstartup->main_fsm_state(pll_startup_fsm_state);
    I01pllstartup->mpll_rst_time(pll_rst_time);
    I01pllstartup->mpll_gear_shift_time(pll_gear_shift_time);
    I01pllstartup->mpll_lock_det_time(pll_lock_det_time);
    I01pllstartup->mpll_pwron(pll_pwron_int);
    I01pllstartup->mpll_reset(pll_reset_int);
    I01pllstartup->mpll_gear_shift(pll_gear_shift_int);
    I01pllstartup->lock_det_on(pll_lock_det_on_int);
    I01pllstartup->fsm_done(pll_startup_done);
    SC_METHOD(proc_always_2833);
    sensitive << cb_cal_repl_ovr_en << cb_cal_repl_ovr << noext_burnin_res_cal_int << setr_burnin_int << setr_termcal_rext;
    SC_METHOD(proc_always_2872);
    sensitive << cfg_clk_scanable.posedge_event() << sresetz_cfg_clk_scanable.negedge_event();
    dont_initialize();
    SC_METHOD(proc_always_2888);
    sensitive << cfg_clk_scanable.posedge_event() << sresetz_cfg_clk_scanable.negedge_event();
    dont_initialize();
    SC_METHOD(proc_always_2930);
    sensitive << cfg_clk_scanable.posedge_event() << sresetz_cfg_clk_scanable.negedge_event();
    dont_initialize();
    SC_METHOD(proc_always_2939);
    sensitive << cfg_clk_scanable.posedge_event() << sresetz_cfg_clk_scanable.negedge_event();
    dont_initialize();
    SC_METHOD(proc_always_2970);
    sensitive << cfg_clk_scanable.posedge_event() << sresetz_cfg_clk_scanable.negedge_event();
    dont_initialize();
    SC_METHOD(proc_always_3001);
    sensitive << cfg_clk_scanable.posedge_event() << sresetz_cfg_clk_scanable.negedge_event() << adc_done_scan.posedge_event();
    dont_initialize();
    SC_METHOD(proc_always_3029);
    sensitive << rx_rxhs_setr_ovr_en << rx_rxhs_setr_ovr << rx_cb_cal_repl_if << rx_cb_cal_repl_if_sum;
    SC_METHOD(proc_always_3574);
    sensitive << rx_timebase_if;
    SC_METHOD(proc_always_3590);
    sensitive << hsfreqrange_int << pll_m_ovr_en << pll_m_ovr;
    SC_METHOD(proc_always_3660);
    sensitive << hsfreqrange_int << pll_vco_cntrl_ovr_en << pll_vco_cntrl_ovr;
    SC_METHOD(proc_always_3729);
    sensitive << hsfreqrange_int << hsrxthssettle_ovr_en_datalanes << hsrxthssettle_datalanes_ovr;
    SC_METHOD(proc_always_3798);
    sensitive << hsfreqrange_int << rx_rxhs_compatibility_mode_ovr_en << rx_rxhs_compatibility_mode_ovr;
    SC_METHOD(proc_always_3866);
    sensitive << hsfreqrange_int << rx_rxhs_gmode_if_ovr_en << rx_rxhs_gmode_if_ovr;
    SC_METHOD(proc_always_3937);
    sensitive << hsfreqrange_int << rxclk_rxhs_ddr_clk_div_ovr_en << rxclk_rxhs_ddr_clk_div_ovr;
    SC_METHOD(proc_always_4010);
    sensitive << deskew_legacyz << ddl_osc_freq_target_ovr_en << ddl_osc_freq_target_ovr << hsfreqrange_int;
    SC_METHOD(proc_always_4961);
    sensitive << cfg_clk_scanable.posedge_event() << arstz_scanable.negedge_event();
    dont_initialize();
}

// Functionality
// Wiring to CORE
// Clock lane
// IF -> CORE
void mipi_4_rx_dphy_ns_if::assign_rstzrx_clklane() {
    vl::wr(rstzrx_clklane, vl::rd(resetz) & vl::rd(enableclk));
}

void mipi_4_rx_dphy_ns_if::assign_enable_clklane() {
    vl::wr(enable_clklane, vl::land(vl::rd(enableclk), vl::rd(enable_lanes)));
}

void mipi_4_rx_dphy_ns_if::assign_polarity_clklane() {
    vl::wr(polarity_clklane, vl::rd(polarity_clklane_int));
}

void mipi_4_rx_dphy_ns_if::assign_atb_sel_clklane() {
    vl::wr(atb_sel_clklane, vl::rd(atb_sel_clklane_tester));
}

// assign rxclk_rxhs_feed_int_clk_if   = (rxclk_rxhs_feed_int_clk_ovr_en)?rxclk_rxhs_feed_int_clk_ovr:( (rxclk_feed_int_clk_ifpowerctrl && enableclk) || do_offs_and_skew_enableclk_not)   ;
// CORE -> IF
void mipi_4_rx_dphy_ns_if::assign_rxbyteclkhs() {
    vl::wr(rxbyteclkhs, vl::rd(rxbyteclkhs_if));
}

void mipi_4_rx_dphy_ns_if::assign_rxclkactivehs() {
    vl::wr(rxclkactivehs, vl::rd(rxclkactivehs_clklane));
}

void mipi_4_rx_dphy_ns_if::assign_stopstateclk() {
    vl::wr(stopstateclk, vl::rd(stopstaterx_clklane));
}

// assign ulpsactivenotclk             = ulpsactivenotrx_clklane       ;
void mipi_4_rx_dphy_ns_if::assign_rxulpsclknot() {
    vl::wr(rxulpsclknot, vl::rd(rxulpsclknot_clklane));
}

// Data lane 0
// IF -> CORE
void mipi_4_rx_dphy_ns_if::assign_txrequestesc_lane0() {
    vl::wr(txrequestesc_lane0, vl::rd(txrequestesc_0));
}

void mipi_4_rx_dphy_ns_if::assign_txlpdtesc_lane0() {
    vl::wr(txlpdtesc_lane0, vl::rd(txlpdtesc_0));
}

void mipi_4_rx_dphy_ns_if::assign_txulpsexit_lane0() {
    vl::wr(txulpsexit_lane0, vl::rd(txulpsexit_0));
}

void mipi_4_rx_dphy_ns_if::assign_txulpsesc_lane0() {
    vl::wr(txulpsesc_lane0, vl::rd(txulpsesc_0));
}

void mipi_4_rx_dphy_ns_if::assign_txtriggeresc_lane0() {
    vl::wr(txtriggeresc_lane0, vl::rd(txtriggeresc_0));
}

void mipi_4_rx_dphy_ns_if::assign_txdataesc_lane0() {
    vl::wr(txdataesc_lane0, vl::rd(txdataesc_0));
}

void mipi_4_rx_dphy_ns_if::assign_txvalidesc_lane0() {
    vl::wr(txvalidesc_lane0, vl::rd(txvalidesc_0));
}

void mipi_4_rx_dphy_ns_if::assign_turnrequest_lane0() {
    vl::wr(turnrequest_lane0, vl::rd(turnrequest_0));
}

void mipi_4_rx_dphy_ns_if::assign_turndisable_lane0() {
    vl::wr(turndisable_lane0, vl::rd(turndisable_0));
}

void mipi_4_rx_dphy_ns_if::assign_forcerxmode_lane0() {
    vl::wr(forcerxmode_lane0, vl::rd(forcerxmode_0));
}

void mipi_4_rx_dphy_ns_if::assign_forcetxstopmode_lane0() {
    vl::wr(forcetxstopmode_lane0, vl::rd(forcetxstopmode_0));
}

void mipi_4_rx_dphy_ns_if::assign_enable_lane0() {
    vl::wr(enable_lane0, vl::land(vl::rd(enable_0), vl::rd(enable_lanes)));
}

void mipi_4_rx_dphy_ns_if::assign_basedir_lane0() {
    vl::wr(basedir_lane0, vl::rd(basedir_0));
}

void mipi_4_rx_dphy_ns_if::assign_polarity_lane0() {
    vl::wr(polarity_lane0, vl::rd(polarity_lane0_int));
}

void mipi_4_rx_dphy_ns_if::assign_tafailsafez_lane0() {
    vl::wr(tafailsafez_lane0, vl::rd(tafailsafez_int));
}

void mipi_4_rx_dphy_ns_if::assign_rx0_txlp_atb_lptx1200_on_if() {
    vl::wr(rx0_txlp_atb_lptx1200_on_if, vl::rd(atb_lptx1200_on_lane0_tester));
}

void mipi_4_rx_dphy_ns_if::assign_atb_sel_lane0() {
    vl::wr(atb_sel_lane0, vl::rd(atb_sel_lane0_tester));
}

void mipi_4_rx_dphy_ns_if::assign_rx0_rxhs_feed_int_clk_if() {
    vl::wr(rx0_rxhs_feed_int_clk_if, vl::mux(vl::rd(rx0_rxhs_feed_int_clk_ovr_en), vl::rd(rx0_rxhs_feed_int_clk_ovr), vl::land(vl::rd(rxlane_feed_int_clk_ifpowerctrl), vl::rd(enable_0))));
}

// CORE -> IF
void mipi_4_rx_dphy_ns_if::assign_txreadyesc_0() {
    vl::wr(txreadyesc_0, vl::rd(txreadyesc_lane0));
}

void mipi_4_rx_dphy_ns_if::assign_rxdatahs_0() {
    vl::wr(rxdatahs_0, vl::rd(rxdatahs_lane0));
}

// assign rxvalidhs_0                  = rxvalidhs_lane0               ;
void mipi_4_rx_dphy_ns_if::assign_rxvalidhs_0() {
    vl::wr(rxvalidhs_0, vl::rd(rxvalidhs_lane0) & (~vl::rd(rxskewcalhs_if)));
}

void mipi_4_rx_dphy_ns_if::assign_rxactivehs_0() {
    vl::wr(rxactivehs_0, vl::rd(rxactivehs_lane0));
}

void mipi_4_rx_dphy_ns_if::assign_rxsynchs_0() {
    vl::wr(rxsynchs_0, vl::rd(rxsynchs_lane0));
}

void mipi_4_rx_dphy_ns_if::assign_errsoths_0() {
    vl::wr(errsoths_0, vl::rd(errsoths_lane0));
}

void mipi_4_rx_dphy_ns_if::assign_errsotsynchs_0() {
    vl::wr(errsotsynchs_0, vl::rd(errsotsynchs_lane0));
}

void mipi_4_rx_dphy_ns_if::assign_rxclkesc_0() {
    vl::wr(rxclkesc_0, vl::rd(rxclkesc_lane0));
}

void mipi_4_rx_dphy_ns_if::assign_rxlpdtesc_0() {
    vl::wr(rxlpdtesc_0, vl::rd(rxlpdtesc_lane0));
}

void mipi_4_rx_dphy_ns_if::assign_rxulpsesc_0() {
    vl::wr(rxulpsesc_0, vl::rd(rxulpsesc_lane0));
}

void mipi_4_rx_dphy_ns_if::assign_rxtriggeresc_0() {
    vl::wr(rxtriggeresc_0, vl::rd(rxtriggeresc_lane0));
}

void mipi_4_rx_dphy_ns_if::assign_rxdataesc_0() {
    vl::wr(rxdataesc_0, vl::rd(rxdataesc_lane0));
}

void mipi_4_rx_dphy_ns_if::assign_rxvalidesc_0() {
    vl::wr(rxvalidesc_0, vl::rd(rxvalidesc_lane0));
}

void mipi_4_rx_dphy_ns_if::assign_direction_0() {
    vl::wr(direction_0, vl::rd(direction_lane0));
}

void mipi_4_rx_dphy_ns_if::assign_stopstatedata_0() {
    vl::wr(stopstatedata_0, vl::rd(stopstatedata_0_int));
}

void mipi_4_rx_dphy_ns_if::assign_ulpsactivenot_0() {
    vl::wr(ulpsactivenot_0, vl::rd(ulpsactivenot_0_int));
}

void mipi_4_rx_dphy_ns_if::assign_erresc_0() {
    vl::wr(erresc_0, vl::rd(erresc_lane0));
}

void mipi_4_rx_dphy_ns_if::assign_errsyncesc_0() {
    vl::wr(errsyncesc_0, vl::rd(errsyncesc_lane0));
}

void mipi_4_rx_dphy_ns_if::assign_errcontrol_0() {
    vl::wr(errcontrol_0, vl::rd(errcontrol_lane0));
}

void mipi_4_rx_dphy_ns_if::assign_errcontentionlp0_0() {
    vl::wr(errcontentionlp0_0, vl::rd(errcontentionlp0_lane0));
}

void mipi_4_rx_dphy_ns_if::assign_errcontentionlp1_0() {
    vl::wr(errcontentionlp1_0, vl::rd(errcontentionlp1_lane0));
}

// Data lane 1
// IF -> CORE
void mipi_4_rx_dphy_ns_if::assign_rstzrx_lane1() {
    vl::wr(rstzrx_lane1, vl::rd(resetz) & vl::rd(enable_1));
}

void mipi_4_rx_dphy_ns_if::assign_enable_lane1() {
    vl::wr(enable_lane1, vl::land(vl::rd(enable_1), vl::rd(enable_lanes)));
}

void mipi_4_rx_dphy_ns_if::assign_polarity_lane1() {
    vl::wr(polarity_lane1, vl::rd(polarity_lane1_int));
}

void mipi_4_rx_dphy_ns_if::assign_forcerxmode_lane1() {
    vl::wr(forcerxmode_lane1, vl::rd(forcerxmode_1));
}

void mipi_4_rx_dphy_ns_if::assign_atb_sel_lane1() {
    vl::wr(atb_sel_lane1, vl::rd(atb_sel_lane1_tester));
}

void mipi_4_rx_dphy_ns_if::assign_rx1_rxhs_feed_int_clk_if() {
    vl::wr(rx1_rxhs_feed_int_clk_if, vl::mux(vl::rd(rx1_rxhs_feed_int_clk_ovr_en), vl::rd(rx1_rxhs_feed_int_clk_ovr), vl::land(vl::rd(rxlane_feed_int_clk_ifpowerctrl), vl::rd(enable_1))));
}

// CORE -> IF
void mipi_4_rx_dphy_ns_if::assign_rxdatahs_1() {
    vl::wr(rxdatahs_1, vl::rd(rxdatahs_lane1));
}

// assign rxvalidhs_1                  = rxvalidhs_lane1               ;
void mipi_4_rx_dphy_ns_if::assign_rxvalidhs_1() {
    vl::wr(rxvalidhs_1, vl::rd(rxvalidhs_lane1) & (~vl::rd(rxskewcalhs_if)));
}

void mipi_4_rx_dphy_ns_if::assign_rxactivehs_1() {
    vl::wr(rxactivehs_1, vl::rd(rxactivehs_lane1));
}

void mipi_4_rx_dphy_ns_if::assign_rxsynchs_1() {
    vl::wr(rxsynchs_1, vl::rd(rxsynchs_lane1));
}

void mipi_4_rx_dphy_ns_if::assign_rxclkesc_1() {
    vl::wr(rxclkesc_1, vl::rd(rxclkesc_lane1));
}

void mipi_4_rx_dphy_ns_if::assign_rxlpdtesc_1() {
    vl::wr(rxlpdtesc_1, vl::rd(rxlpdtesc_lane1));
}

void mipi_4_rx_dphy_ns_if::assign_rxulpsesc_1() {
    vl::wr(rxulpsesc_1, vl::rd(rxulpsesc_lane1));
}

void mipi_4_rx_dphy_ns_if::assign_rxtriggeresc_1() {
    vl::wr(rxtriggeresc_1, vl::rd(rxtriggeresc_lane1));
}

void mipi_4_rx_dphy_ns_if::assign_rxdataesc_1() {
    vl::wr(rxdataesc_1, vl::rd(rxdataesc_lane1));
}

void mipi_4_rx_dphy_ns_if::assign_rxvalidesc_1() {
    vl::wr(rxvalidesc_1, vl::rd(rxvalidesc_lane1));
}

void mipi_4_rx_dphy_ns_if::assign_errsoths_1() {
    vl::wr(errsoths_1, vl::rd(errsoths_lane1));
}

void mipi_4_rx_dphy_ns_if::assign_errsotsynchs_1() {
    vl::wr(errsotsynchs_1, vl::rd(errsotsynchs_lane1));
}

void mipi_4_rx_dphy_ns_if::assign_erresc_1() {
    vl::wr(erresc_1, vl::rd(erresc_lane1));
}

void mipi_4_rx_dphy_ns_if::assign_errsyncesc_1() {
    vl::wr(errsyncesc_1, vl::rd(errsyncesc_lane1));
}

void mipi_4_rx_dphy_ns_if::assign_errcontrol_1() {
    vl::wr(errcontrol_1, vl::rd(errcontrol_lane1));
}

void mipi_4_rx_dphy_ns_if::assign_stopstatedata_1() {
    vl::wr(stopstatedata_1, vl::rd(stopstaterx_lane1));
}

void mipi_4_rx_dphy_ns_if::assign_ulpsactivenot_1() {
    vl::wr(ulpsactivenot_1, vl::rd(ulpsactivenotrx_lane1));
}

// Data lane 2
// IF -> CORE
void mipi_4_rx_dphy_ns_if::assign_rstzrx_lane2() {
    vl::wr(rstzrx_lane2, vl::rd(resetz) & vl::rd(enable_2));
}

void mipi_4_rx_dphy_ns_if::assign_enable_lane2() {
    vl::wr(enable_lane2, vl::land(vl::rd(enable_2), vl::rd(enable_lanes)));
}

void mipi_4_rx_dphy_ns_if::assign_polarity_lane2() {
    vl::wr(polarity_lane2, vl::rd(polarity_lane2_int));
}

void mipi_4_rx_dphy_ns_if::assign_forcerxmode_lane2() {
    vl::wr(forcerxmode_lane2, vl::rd(forcerxmode_2));
}

void mipi_4_rx_dphy_ns_if::assign_atb_sel_lane2() {
    vl::wr(atb_sel_lane2, vl::rd(atb_sel_lane2_tester));
}

void mipi_4_rx_dphy_ns_if::assign_rx2_rxhs_feed_int_clk_if() {
    vl::wr(rx2_rxhs_feed_int_clk_if, vl::mux(vl::rd(rx2_rxhs_feed_int_clk_ovr_en), vl::rd(rx2_rxhs_feed_int_clk_ovr), vl::land(vl::rd(rxlane_feed_int_clk_ifpowerctrl), vl::rd(enable_2))));
}

// CORE -> IF
void mipi_4_rx_dphy_ns_if::assign_rxdatahs_2() {
    vl::wr(rxdatahs_2, vl::rd(rxdatahs_lane2));
}

// assign rxvalidhs_2                  = rxvalidhs_lane2               ;
void mipi_4_rx_dphy_ns_if::assign_rxvalidhs_2() {
    vl::wr(rxvalidhs_2, vl::rd(rxvalidhs_lane2) & (~vl::rd(rxskewcalhs_if)));
}

void mipi_4_rx_dphy_ns_if::assign_rxactivehs_2() {
    vl::wr(rxactivehs_2, vl::rd(rxactivehs_lane2));
}

void mipi_4_rx_dphy_ns_if::assign_rxsynchs_2() {
    vl::wr(rxsynchs_2, vl::rd(rxsynchs_lane2));
}

void mipi_4_rx_dphy_ns_if::assign_rxclkesc_2() {
    vl::wr(rxclkesc_2, vl::rd(rxclkesc_lane2));
}

void mipi_4_rx_dphy_ns_if::assign_rxlpdtesc_2() {
    vl::wr(rxlpdtesc_2, vl::rd(rxlpdtesc_lane2));
}

void mipi_4_rx_dphy_ns_if::assign_rxulpsesc_2() {
    vl::wr(rxulpsesc_2, vl::rd(rxulpsesc_lane2));
}

void mipi_4_rx_dphy_ns_if::assign_rxtriggeresc_2() {
    vl::wr(rxtriggeresc_2, vl::rd(rxtriggeresc_lane2));
}

void mipi_4_rx_dphy_ns_if::assign_rxdataesc_2() {
    vl::wr(rxdataesc_2, vl::rd(rxdataesc_lane2));
}

void mipi_4_rx_dphy_ns_if::assign_rxvalidesc_2() {
    vl::wr(rxvalidesc_2, vl::rd(rxvalidesc_lane2));
}

void mipi_4_rx_dphy_ns_if::assign_errsoths_2() {
    vl::wr(errsoths_2, vl::rd(errsoths_lane2));
}

void mipi_4_rx_dphy_ns_if::assign_errsotsynchs_2() {
    vl::wr(errsotsynchs_2, vl::rd(errsotsynchs_lane2));
}

void mipi_4_rx_dphy_ns_if::assign_erresc_2() {
    vl::wr(erresc_2, vl::rd(erresc_lane2));
}

void mipi_4_rx_dphy_ns_if::assign_errsyncesc_2() {
    vl::wr(errsyncesc_2, vl::rd(errsyncesc_lane2));
}

void mipi_4_rx_dphy_ns_if::assign_errcontrol_2() {
    vl::wr(errcontrol_2, vl::rd(errcontrol_lane2));
}

void mipi_4_rx_dphy_ns_if::assign_stopstatedata_2() {
    vl::wr(stopstatedata_2, vl::rd(stopstaterx_lane2));
}

void mipi_4_rx_dphy_ns_if::assign_ulpsactivenot_2() {
    vl::wr(ulpsactivenot_2, vl::rd(ulpsactivenotrx_lane2));
}

// Data lane 3
// IF -> CORE
void mipi_4_rx_dphy_ns_if::assign_rstzrx_lane3() {
    vl::wr(rstzrx_lane3, vl::rd(resetz) & vl::rd(enable_3));
}

void mipi_4_rx_dphy_ns_if::assign_enable_lane3() {
    vl::wr(enable_lane3, vl::land(vl::rd(enable_3), vl::rd(enable_lanes)));
}

void mipi_4_rx_dphy_ns_if::assign_polarity_lane3() {
    vl::wr(polarity_lane3, vl::rd(polarity_lane3_int));
}

void mipi_4_rx_dphy_ns_if::assign_forcerxmode_lane3() {
    vl::wr(forcerxmode_lane3, vl::rd(forcerxmode_3));
}

void mipi_4_rx_dphy_ns_if::assign_atb_sel_lane3() {
    vl::wr(atb_sel_lane3, vl::rd(atb_sel_lane3_tester));
}

void mipi_4_rx_dphy_ns_if::assign_rx3_rxhs_feed_int_clk_if() {
    vl::wr(rx3_rxhs_feed_int_clk_if, vl::mux(vl::rd(rx3_rxhs_feed_int_clk_ovr_en), vl::rd(rx3_rxhs_feed_int_clk_ovr), vl::land(vl::rd(rxlane_feed_int_clk_ifpowerctrl), vl::rd(enable_3))));
}

// CORE -> IF
void mipi_4_rx_dphy_ns_if::assign_rxdatahs_3() {
    vl::wr(rxdatahs_3, vl::rd(rxdatahs_lane3));
}

// assign rxvalidhs_3                  = rxvalidhs_lane3               ;
void mipi_4_rx_dphy_ns_if::assign_rxvalidhs_3() {
    vl::wr(rxvalidhs_3, vl::rd(rxvalidhs_lane3) & (~vl::rd(rxskewcalhs_if)));
}

void mipi_4_rx_dphy_ns_if::assign_rxactivehs_3() {
    vl::wr(rxactivehs_3, vl::rd(rxactivehs_lane3));
}

void mipi_4_rx_dphy_ns_if::assign_rxsynchs_3() {
    vl::wr(rxsynchs_3, vl::rd(rxsynchs_lane3));
}

void mipi_4_rx_dphy_ns_if::assign_rxclkesc_3() {
    vl::wr(rxclkesc_3, vl::rd(rxclkesc_lane3));
}

void mipi_4_rx_dphy_ns_if::assign_rxlpdtesc_3() {
    vl::wr(rxlpdtesc_3, vl::rd(rxlpdtesc_lane3));
}

void mipi_4_rx_dphy_ns_if::assign_rxulpsesc_3() {
    vl::wr(rxulpsesc_3, vl::rd(rxulpsesc_lane3));
}

void mipi_4_rx_dphy_ns_if::assign_rxtriggeresc_3() {
    vl::wr(rxtriggeresc_3, vl::rd(rxtriggeresc_lane3));
}

void mipi_4_rx_dphy_ns_if::assign_rxdataesc_3() {
    vl::wr(rxdataesc_3, vl::rd(rxdataesc_lane3));
}

void mipi_4_rx_dphy_ns_if::assign_rxvalidesc_3() {
    vl::wr(rxvalidesc_3, vl::rd(rxvalidesc_lane3));
}

void mipi_4_rx_dphy_ns_if::assign_errsoths_3() {
    vl::wr(errsoths_3, vl::rd(errsoths_lane3));
}

void mipi_4_rx_dphy_ns_if::assign_errsotsynchs_3() {
    vl::wr(errsotsynchs_3, vl::rd(errsotsynchs_lane3));
}

void mipi_4_rx_dphy_ns_if::assign_erresc_3() {
    vl::wr(erresc_3, vl::rd(erresc_lane3));
}

void mipi_4_rx_dphy_ns_if::assign_errsyncesc_3() {
    vl::wr(errsyncesc_3, vl::rd(errsyncesc_lane3));
}

void mipi_4_rx_dphy_ns_if::assign_errcontrol_3() {
    vl::wr(errcontrol_3, vl::rd(errcontrol_lane3));
}

void mipi_4_rx_dphy_ns_if::assign_stopstatedata_3() {
    vl::wr(stopstatedata_3, vl::rd(stopstaterx_lane3));
}

void mipi_4_rx_dphy_ns_if::assign_ulpsactivenot_3() {
    vl::wr(ulpsactivenot_3, vl::rd(ulpsactivenotrx_lane3));
}

// IF -> CORE
void mipi_4_rx_dphy_ns_if::assign_cont_en_if() {
    vl::wr(cont_en_if, vl::rd(cont_en));
}

// CORE -> IF
void mipi_4_rx_dphy_ns_if::assign_cont_data() {
    vl::wr(cont_data, vl::rd(cont_data_if));
}

// COMMON
// IF -> CORE
void mipi_4_rx_dphy_ns_if::assign_cfgclk_if() {
    vl::wr(cfgclk_if, vl::rd(cfg_clk));
}

void mipi_4_rx_dphy_ns_if::assign_rstz_if() {
    vl::wr(rstz_if, vl::rd(resetz));
}

void mipi_4_rx_dphy_ns_if::assign_txclkesc_if() {
    vl::wr(txclkesc_if, vl::rd(txclkesc));
}

void mipi_4_rx_dphy_ns_if::assign_rxskewcalhs() {
    vl::wr(rxskewcalhs, vl::rd(rxskewcalhs_if));
}

void mipi_4_rx_dphy_ns_if::assign_biston_lanes() {
    vl::wr(biston_lanes, vl::rd(biston_int));
}

void mipi_4_rx_dphy_ns_if::assign_ttasure_datalanes() {
    vl::wr(ttasure_datalanes, vl::rd(ttasure_datalanes_int));
}

void mipi_4_rx_dphy_ns_if::assign_ttago_datalanes() {
    vl::wr(ttago_datalanes, vl::rd(ttago_datalanes_int));
}

void mipi_4_rx_dphy_ns_if::assign_ttawait_datalanes() {
    vl::wr(ttawait_datalanes, vl::rd(ttawait_datalanes_int));
}

void mipi_4_rx_dphy_ns_if::assign_rx_timebase_if() {
    vl::wr(rx_timebase_if, vl::mux(vl::rd(timebase_ovr_en), vl::rd(timebase_ovr), vl::logic<8>(0x44ULL) + vl::rd(cfgclkfreqrange)));
}

void mipi_4_rx_dphy_ns_if::assign_enable_lanes() {
    vl::wr(enable_lanes, vl::mux(vl::rd(enable_lanes_ovr_en), vl::rd(enable_lanes_ovr), vl::rd(enable_lanes_int)));
}

// 2MHz clock signal generation
void mipi_4_rx_dphy_ns_if::assign_timebase_cal() {
    vl::wr(timebase_cal, vl::rd(rx_timebase_if).slice<5>(3));
}

// timebase divided by 8
void mipi_4_rx_dphy_ns_if::assign_rx_cb_on_if() {
    vl::wr(rx_cb_on_if, vl::mux(vl::rd(cb_on_ovr_en), vl::rd(cb_on_ovr), vl::rd(bintpon_ifpowerctrl)));
}

void mipi_4_rx_dphy_ns_if::assign_rx_cb_sel_vrefcd_lprx_if() {
    vl::wr(rx_cb_sel_vrefcd_lprx_if, vl::rd(cb_sel_vrefcd_lprx_int));
}

void mipi_4_rx_dphy_ns_if::assign_rx_cb_sel_vref_lprx_if() {
    vl::wr(rx_cb_sel_vref_lprx_if, vl::rd(cb_sel_vref_lprx_int));
}

void mipi_4_rx_dphy_ns_if::assign_rx_cb_bg_en_if() {
    vl::wr(rx_cb_bg_en_if, vl::mux(vl::rd(cb_bg_en_ovr_en), vl::rd(cb_bg_en_ovr), vl::rd(bintpon_ifpowerctrl)));
}

void mipi_4_rx_dphy_ns_if::assign_rx_cb_chop_clk_en_if() {
    vl::wr(rx_cb_chop_clk_en_if, vl::rd(cb_chop_clk_en_int));
}

void mipi_4_rx_dphy_ns_if::assign_rx_cb_sel_chop_clk_if() {
    vl::wr(rx_cb_sel_chop_clk_if, vl::rd(cb_sel_chop_clk_int));
}

void mipi_4_rx_dphy_ns_if::assign_rx_cb_chop_clk_if() {
    vl::wr(rx_cb_chop_clk_if, vl::rd(cfg_clk));
}

// TBD Add to testability? - Add clock gating?
void mipi_4_rx_dphy_ns_if::assign_rx_cb_sel_v400_prog_if() {
    vl::wr(rx_cb_sel_v400_prog_if, vl::rd(cb_sel_v400_prog_int));
}

void mipi_4_rx_dphy_ns_if::assign_rx_cb_lpibias_en_if() {
    vl::wr(rx_cb_lpibias_en_if, vl::mux(vl::rd(cb_lpibias_en_ovr_en), vl::rd(cb_lpibias_en_ovr), vl::rd(bintpon_ifpowerctrl)));
}

// TBD Add to FSM?
void mipi_4_rx_dphy_ns_if::assign_rx_cb_sel_vreflptx_prog_if() {
    vl::wr(rx_cb_sel_vreflptx_prog_if, vl::rd(cb_sel_vreflptx_prog_int));
}

void mipi_4_rx_dphy_ns_if::assign_rx_atb_on_if() {
    vl::wr(rx_atb_on_if, vl::mux(vl::rd(cb_atb_on_ovr_en), vl::rd(cb_atb_on_ovr), vl::rd(cb_ical_x4_int) | vl::rd(cb_atb_on_rcal_int)));
}

// TBD testability - Add to FSM
void mipi_4_rx_dphy_ns_if::assign_rx_atb_comp_clk_if_tester() {
    vl::wr(rx_atb_comp_clk_if_tester, vl::mux(vl::rd(cb_atb_comp_clk_ovr_en), vl::rd(cb_atb_comp_clk_ovr), vl::mux(vl::rd(rescal_en), vl::rd(cal_clock_comp), vl::rd(cb_atb_comp_clk_int))));
}

void mipi_4_rx_dphy_ns_if::assign_rx_atb_comp_clk_if() {
    vl::wr(rx_atb_comp_clk_if, vl::rd(rx_atb_comp_clk_if_tester) & vl::rd(shutdownz));
}

void mipi_4_rx_dphy_ns_if::assign_rx_atb_ext_con_if() {
    vl::wr(rx_atb_ext_con_if, vl::rd(cb_atb_ext_con_int));
}

void mipi_4_rx_dphy_ns_if::assign_rx_cb_atb_sel_if() {
    vl::wr(rx_cb_atb_sel_if, vl::rd(cb_atb_sel_int));
}

void mipi_4_rx_dphy_ns_if::assign_rx_atb_vbe_sel_if() {
    vl::wr(rx_atb_vbe_sel_if, vl::rd(cb_atb_vbe_sel_int));
}

void mipi_4_rx_dphy_ns_if::assign_rx_cb_cal_en_if() {
    vl::wr(rx_cb_cal_en_if, vl::mux(vl::rd(cb_cal_en_ovr_en), vl::rd(cb_cal_en_ovr), vl::rd(cb_cal_en_ifpowerctrl)));
}

void mipi_4_rx_dphy_ns_if::assign_rx_cb_ical_x4_if() {
    vl::wr(rx_cb_ical_x4_if, vl::mux(vl::rd(cb_ical_x4_ovr_en), vl::rd(cb_ical_x4_ovr), vl::rd(cb_ical_x4_int)));
}

void mipi_4_rx_dphy_ns_if::assign_rx_cb_dac_prog_if() {
    vl::wr(rx_cb_dac_prog_if, vl::mux(vl::rd(cb_dac_prog_ovr_en), vl::rd(cb_dac_prog_ovr), vl::rd(cb_dac_prog_int)));
}

// assign  rx_cb_cal_repl_if           = (cb_cal_repl_ovr_en) ? cb_cal_repl_ovr : cb_cal_repl_int  ;
void mipi_4_rx_dphy_ns_if::assign_rx_cb_en_45_ohm_if() {
    vl::wr(rx_cb_en_45_ohm_if, vl::rd(cb_en_45_ohm_int));
}

// Internal wiring
void mipi_4_rx_dphy_ns_if::assign_resetz() {
    vl::wr(resetz, vl::land(vl::rd(rstz), vl::rd(shutdownz)));
}

// Scanable clocks
// assign cfg_clk_scanable             = scanmode ? scanclk  :   cfg_clk                           ;
// assign testclk_scanable             = scanmode ? scanclk  :   testclk                           ;
void mipi_4_rx_dphy_ns_if::assign_cfg_clk_scanable() {
    vl::wr(cfg_clk_scanable, vl::rd(cfg_clk));
}

void mipi_4_rx_dphy_ns_if::assign_testclk_scanable() {
    vl::wr(testclk_scanable, vl::rd(testclk));
}

// Scanable resets
void mipi_4_rx_dphy_ns_if::assign_testclr_scanable() {
    vl::wr(testclr_scanable, vl::mux(vl::rd(scan_mode), vl::rd(scan_set_rst_n), vl::lnot(vl::rd(testclr))));
}

void mipi_4_rx_dphy_ns_if::assign_aresetz_scanable() {
    vl::wr(aresetz_scanable, vl::mux(vl::rd(scan_mode), vl::rd(scan_set_rst_n), vl::rd(resetz)));
}

// DDL tuning wiring
void mipi_4_rx_dphy_ns_if::assign_rx_ddl_offset_error_if() {
    vl::wr(rx_ddl_offset_error_if, vl::rd(rx_ddl_offset_error_tester));
}

// 1500 Mbps -->  4*Ui = 2.66 ns  --> fosc(MHZ)  = 1/2.66 = 375M --> ( div by 4 )  375/4*4  = 375 cycle ( notes)
void mipi_4_rx_dphy_ns_if::assign_rx0_ddl_start_calibration_if() {
    vl::wr(rx0_ddl_start_calibration_if, vl::mux(vl::rd(rx0_ddl_start_calibration_ovr_en), vl::rd(rx0_ddl_start_calibration_ovr), vl::land(vl::rd(ddltunning_en_ifpowerctrl), vl::rd(enable_0))));
}

void mipi_4_rx_dphy_ns_if::assign_rx0_rxhs_ddl_tune_en_if() {
    vl::wr(rx0_rxhs_ddl_tune_en_if, vl::rd(rx0_rxhs_ddl_tune_en_tester));
}

void mipi_4_rx_dphy_ns_if::assign_rx0_ddl_finished_int() {
    vl::wr(rx0_ddl_finished_int, vl::mux(vl::rd(rx0_ddl_finished_ovr_en), vl::rd(rx0_ddl_finished_ovr), vl::rd(rx0_ddl_finished_if)));
}

void mipi_4_rx_dphy_ns_if::assign_rx1_ddl_start_calibration_if() {
    vl::wr(rx1_ddl_start_calibration_if, vl::mux(vl::rd(rx1_ddl_start_calibration_ovr_en), vl::rd(rx1_ddl_start_calibration_ovr), vl::land(vl::rd(ddltunning_en_ifpowerctrl), vl::rd(enable_1))));
}

void mipi_4_rx_dphy_ns_if::assign_rx1_rxhs_ddl_tune_en_if() {
    vl::wr(rx1_rxhs_ddl_tune_en_if, vl::rd(rx1_rxhs_ddl_tune_en_tester));
}

void mipi_4_rx_dphy_ns_if::assign_rx1_ddl_finished_int() {
    vl::wr(rx1_ddl_finished_int, vl::mux(vl::rd(rx1_ddl_finished_ovr_en), vl::rd(rx1_ddl_finished_ovr), vl::rd(rx1_ddl_finished_if)));
}

void mipi_4_rx_dphy_ns_if::assign_rx2_ddl_start_calibration_if() {
    vl::wr(rx2_ddl_start_calibration_if, vl::mux(vl::rd(rx2_ddl_start_calibration_ovr_en), vl::rd(rx2_ddl_start_calibration_ovr), vl::land(vl::rd(ddltunning_en_ifpowerctrl), vl::rd(enable_2))));
}

void mipi_4_rx_dphy_ns_if::assign_rx2_rxhs_ddl_tune_en_if() {
    vl::wr(rx2_rxhs_ddl_tune_en_if, vl::rd(rx2_rxhs_ddl_tune_en_tester));
}

void mipi_4_rx_dphy_ns_if::assign_rx2_ddl_finished_int() {
    vl::wr(rx2_ddl_finished_int, vl::mux(vl::rd(rx2_ddl_finished_ovr_en), vl::rd(rx2_ddl_finished_ovr), vl::rd(rx2_ddl_finished_if)));
}

void mipi_4_rx_dphy_ns_if::assign_rx3_ddl_start_calibration_if() {
    vl::wr(rx3_ddl_start_calibration_if, vl::mux(vl::rd(rx3_ddl_start_calibration_ovr_en), vl::rd(rx3_ddl_start_calibration_ovr), vl::land(vl::rd(ddltunning_en_ifpowerctrl), vl::rd(enable_3))));
}

void mipi_4_rx_dphy_ns_if::assign_rx3_rxhs_ddl_tune_en_if() {
    vl::wr(rx3_rxhs_ddl_tune_en_if, vl::rd(rx3_rxhs_ddl_tune_en_tester));
}

void mipi_4_rx_dphy_ns_if::assign_rx3_ddl_finished_int() {
    vl::wr(rx3_ddl_finished_int, vl::mux(vl::rd(rx3_ddl_finished_ovr_en), vl::rd(rx3_ddl_finished_ovr), vl::rd(rx3_ddl_finished_if)));
}

// if lane is off then do not wait for done
void mipi_4_rx_dphy_ns_if::assign_global_ddltunning_done() {
    vl::wr(global_ddltunning_done, vl::land(vl::land(vl::land(vl::lor(vl::rd(rx0_ddl_finished_int), ~vl::rd(enable_0)), vl::lor(vl::rd(rx1_ddl_finished_int), ~vl::rd(enable_1))), vl::lor(vl::rd(rx2_ddl_finished_int), ~vl::rd(enable_2))), vl::lor(vl::rd(rx3_ddl_finished_int), ~vl::rd(enable_3))));
}

void mipi_4_rx_dphy_ns_if::assign_rx_rxlp_bias_prog_if() {
    vl::wr(rx_rxlp_bias_prog_if, vl::rd(rx_rxlp_bias_prog_int));
}

void mipi_4_rx_dphy_ns_if::assign_adc_done_scan() {
    vl::wr(adc_done_scan, vl::rd(adc_done_int) & (~vl::rd(scan_mode)));
}

void mipi_4_rx_dphy_ns_if::assign_rx_rxhs_compatibility_mode_if() {
    vl::wr(rx_rxhs_compatibility_mode_if, vl::rd(rx_rxhs_compatibility_mode) & vl::rd(shutdownz));
}

// compatibily mode
void mipi_4_rx_dphy_ns_if::assign_deskew_legacyz() {
    vl::wr(deskew_legacyz, vl::mux(vl::rd(deskew_legacyz_ovr_en), vl::rd(deskew_legacyz_ovr), ~vl::rd(rx_rxhs_compatibility_mode)));
}

// typically, algorithm and datapath used are paired but it can be overridden
void mipi_4_rx_dphy_ns_if::assign_burnin_cal_en_if() {
    vl::wr(burnin_cal_en_if, vl::mux(vl::rd(burnin_cal_en_ovr_en_sync), vl::rd(burnin_cal_en_ovr_sync), vl::rd(burnin_cal_en_int)));
}

void mipi_4_rx_dphy_ns_if::assign_stability_time() {
    vl::wr(stability_time, vl::cat(vl::logic<1>(0x0ULL), vl::rd(stability_time_tester)) + vl::cat(vl::rd(rx_timebase_if), vl::logic<1>(0x0ULL)));
}

void mipi_4_rx_dphy_ns_if::assign_rxclk_rxhs_osc_pon_if() {
    vl::wr(rxclk_rxhs_osc_pon_if, vl::mux(vl::rd(rxclk_rxhs_osc_pon_ovr_en), vl::rd(rxclk_rxhs_osc_pon_ovr), vl::rd(rxclk_rxhs_osc_pon_int)));
}

void mipi_4_rx_dphy_ns_if::assign_rxclk_rxhs_osc_res_en_if() {
    vl::wr(rxclk_rxhs_osc_res_en_if, vl::mux(vl::rd(rxclk_rxhs_osc_res_en_ovr_en), vl::rd(rxclk_rxhs_osc_res_en_ovr), vl::rd(rxclk_rxhs_osc_res_en_int)));
}

// assign rx_rxhs_setrx_if             = (rx_rxhs_setr_ovr_en          ) ? rx_rxhs_setr_ovr            :   setr_burnin_int                 ;
void mipi_4_rx_dphy_ns_if::assign_burnin_cal_done_int() {
    vl::wr(burnin_cal_done_int, vl::mux(vl::rd(norexttune_done_ovr_en), vl::rd(norexttune_done_ovr), vl::rd(burnin_cal_done_norext_machine)));
}

void mipi_4_rx_dphy_ns_if::assign_rx_cb_cal_repl_if_sum() {
    vl::wr(rx_cb_cal_repl_if_sum, vl::cat(vl::logic<1>(0x0ULL), vl::rd(rx_cb_cal_repl_if).slice<3>(1)) + vl::cat(vl::logic<3>(0x0ULL), vl::rd(rx_cb_cal_repl_if).bit(0)));
}

void mipi_4_rx_dphy_ns_if::assign_rxclk_start_calibration() {
    vl::wr(rxclk_start_calibration, vl::mux(vl::rd(rxclk_start_calibration_ovr_en_sync), vl::rd(rxclk_start_calibration_ovr_sync), vl::land(vl::rd(hsrxencal_offset_ifpowerctrl), vl::rd(enableclk))));
}

void mipi_4_rx_dphy_ns_if::assign_rxclk_rxhs_clk_offset_cal_en_if() {
    vl::wr(rxclk_rxhs_clk_offset_cal_en_if, vl::mux(vl::rd(rxclk_rxhs_start_calibration_ovr_en_sync), vl::rd(rxclk_rxhs_start_calibration_ovr_sync), vl::rd(clklane_startcal_int)));
}

void mipi_4_rx_dphy_ns_if::assign_hsrxpon_if_clklane() {
    vl::wr(hsrxpon_if_clklane, vl::mux(vl::rd(hsrxpon_bypass_clklane), vl::rd(hsrxpon_clklane_tester), vl::lor(vl::land(vl::rd(hsrxpon_clklane_ifpowerctrl), vl::rd(enableclk)), vl::rd(do_offs_and_skew_enableclk_not))));
}

// /  activate both in offset datalane and in deskew comp modee
void mipi_4_rx_dphy_ns_if::assign_hsrx_en_bypass_clklane() {
    vl::wr(hsrx_en_bypass_clklane, vl::rd(hsrx_en_if_bypass_clklane_tester));
}

void mipi_4_rx_dphy_ns_if::assign_hsrx_en_if_clklane() {
    vl::wr(hsrx_en_if_clklane, vl::mux(vl::rd(hsrx_en_bypass_clklane), vl::rd(hsrx_en_if_clklane_tester), vl::lor(vl::lor(vl::rd(des_en_for_datalane_offset_cal_ifpowerctrl), vl::rd(clklane_startcal_int)) & vl::rd(enableclk), vl::rd(do_offs_and_skew_enableclk_not))));
}

void mipi_4_rx_dphy_ns_if::assign_clk_en_lanes_bypass() {
    vl::wr(clk_en_lanes_bypass, vl::rd(clk_en_lanes_bypass_tester));
}

void mipi_4_rx_dphy_ns_if::assign_clk_en_lanes() {
    vl::wr(clk_en_lanes, vl::mux(vl::rd(clk_en_lanes_bypass), vl::rd(clk_en_lanes_tester), vl::lor(vl::rd(des_en_for_datalane_offset_cal_ifpowerctrl) & vl::rd(enableclk), vl::rd(do_offs_and_skew_enableclk_not))));
}

void mipi_4_rx_dphy_ns_if::assign_rxclk_rxhs_clk_offset_cal_if() {
    vl::wr(rxclk_rxhs_clk_offset_cal_if, vl::rd(rxclk_rxhs_clk_offset_cal_int) & vl::rep<6>(vl::rd(shutdownz)));
}

void mipi_4_rx_dphy_ns_if::assign_rxclk_caldone() {
    vl::wr(rxclk_caldone, vl::mux(vl::rd(rxclk_caldone_ovr_en), vl::rd(rxclk_caldone_ovr), vl::rd(caldone_clklane_int) | (~vl::rd(enableclk))));
}

// muxing of the dat in case of isolated test
void mipi_4_rx_dphy_ns_if::assign_rx0_rxhs_data_int() {
    vl::wr(rx0_rxhs_data_int, vl::mux(vl::rd(rx0_rxhs_data_ovr_en), vl::rd(rx0_rxhs_data_ovr), vl::rd(rx0_rxhs_data_if)));
}

// cal enable and cal[5:0] value
void mipi_4_rx_dphy_ns_if::assign_rx0_rxhs_data_offset_cal_en_if() {
    vl::wr(rx0_rxhs_data_offset_cal_en_if, vl::mux(vl::rd(rx0_rxhs_data_offset_cal_en_ovr_en), vl::rd(rx0_rxhs_data_offset_cal_en_ovr), vl::land(vl::rd(rx0_rxhs_data_offset_cal_en_int), vl::rd(enable_0))));
}

void mipi_4_rx_dphy_ns_if::assign_rx0_rxhs_data_offset_cal_if() {
    vl::wr(rx0_rxhs_data_offset_cal_if, vl::mux(vl::rd(rx0_rxhs_data_offset_cal_ovr_en), vl::rd(rx0_rxhs_data_offset_cal_ovr), (vl::rd(rx0_rxhs_data_offset_cal_int) & vl::rep<6>(vl::rd(enable_0))) & vl::rep<6>(vl::rd(shutdownz))));
}

void mipi_4_rx_dphy_ns_if::assign_hsrx_des_en_bypass_lane0() {
    vl::wr(hsrx_des_en_bypass_lane0, vl::rd(rx_des_en_bypass_lane0_tester));
}

void mipi_4_rx_dphy_ns_if::assign_hsrx_des_en_if_lane0() {
    vl::wr(hsrx_des_en_if_lane0, vl::mux(vl::rd(hsrx_des_en_bypass_lane0), vl::rd(rx_des_en_lane0_tester), vl::land(vl::rd(des_en_for_datalane_offset_cal_ifpowerctrl), vl::rd(enable_0))));
}

void mipi_4_rx_dphy_ns_if::assign_hsrxpon_if_lane0() {
    vl::wr(hsrxpon_if_lane0, vl::mux(vl::rd(hsrxpon_bypass_lane0), vl::rd(hsrxpon_lane0_tester), vl::land(vl::rd(hsrxpon_datalane_ifpowerctrl), vl::rd(enable_0))));
}

void mipi_4_rx_dphy_ns_if::assign_rx0_start_calibration_int() {
    vl::wr(rx0_start_calibration_int, vl::mux(vl::rd(rx0_start_calibration_ovr_en), vl::rd(rx0_start_calibration_ovr), vl::land(vl::land(vl::rd(hsrxencal_lane_offset_ifpowerctrl), vl::rd(enable_0)), vl::rd(direction_lane0_sync))));
}

// consifer lane0 configured as Rx
// cal_done ovreride
void mipi_4_rx_dphy_ns_if::assign_rx0_cal_done() {
    vl::wr(rx0_cal_done, vl::mux(vl::rd(rx0_cal_done_ovr_en), vl::rd(rx0_cal_done_ovr), vl::rd(rx0_cal_done_int)));
}

// /  activate on backward comp mode  skew
void mipi_4_rx_dphy_ns_if::assign_hsrx_en_bypass_lane0() {
    vl::wr(hsrx_en_bypass_lane0, vl::rd(hsrx_en_if_bypass_lane0_tester));
}

void mipi_4_rx_dphy_ns_if::assign_hsrx_en_if_lane0() {
    vl::wr(hsrx_en_if_lane0, vl::mux(vl::rd(hsrx_en_bypass_lane0), vl::rd(hsrx_en_if_lane0_tester), vl::rd(des_en_for_datalane_offset_cal_ifpowerctrl) & vl::rd(enable_0)));
}

// muxing of the dat in case of isolated test
void mipi_4_rx_dphy_ns_if::assign_rx1_rxhs_data_int() {
    vl::wr(rx1_rxhs_data_int, vl::mux(vl::rd(rx1_rxhs_data_ovr_en), vl::rd(rx1_rxhs_data_ovr), vl::rd(rx1_rxhs_data_if)));
}

// cal enable and cal[5:0] value
void mipi_4_rx_dphy_ns_if::assign_rx1_rxhs_data_offset_cal_en_if() {
    vl::wr(rx1_rxhs_data_offset_cal_en_if, vl::mux(vl::rd(rx1_rxhs_data_offset_cal_en_ovr_en), vl::rd(rx1_rxhs_data_offset_cal_en_ovr), vl::land(vl::rd(rx1_rxhs_data_offset_cal_en_int), vl::rd(enable_1))));
}

void mipi_4_rx_dphy_ns_if::assign_rx1_rxhs_data_offset_cal_if() {
    vl::wr(rx1_rxhs_data_offset_cal_if, vl::mux(vl::rd(rx1_rxhs_data_offset_cal_ovr_en), vl::rd(rx1_rxhs_data_offset_cal_ovr), (vl::rd(rx1_rxhs_data_offset_cal_int) & vl::rep<6>(vl::rd(enable_1))) & vl::rep<6>(vl::rd(shutdownz))));
}

void mipi_4_rx_dphy_ns_if::assign_hsrx_des_en_bypass_lane1() {
    vl::wr(hsrx_des_en_bypass_lane1, vl::rd(rx_des_en_bypass_lane1_tester));
}

void mipi_4_rx_dphy_ns_if::assign_hsrx_des_en_if_lane1() {
    vl::wr(hsrx_des_en_if_lane1, vl::mux(vl::rd(hsrx_des_en_bypass_lane1), vl::rd(rx_des_en_lane1_tester), vl::land(vl::rd(des_en_for_datalane_offset_cal_ifpowerctrl), vl::rd(enable_1))));
}

void mipi_4_rx_dphy_ns_if::assign_hsrxpon_if_lane1() {
    vl::wr(hsrxpon_if_lane1, vl::mux(vl::rd(hsrxpon_bypass_lane1), vl::rd(hsrxpon_lane1_tester), vl::land(vl::rd(hsrxpon_datalane_ifpowerctrl), vl::rd(enable_1))));
}

void mipi_4_rx_dphy_ns_if::assign_rx1_start_calibration_int() {
    vl::wr(rx1_start_calibration_int, vl::mux(vl::rd(rx1_start_calibration_ovr_en), vl::rd(rx1_start_calibration_ovr), vl::land(vl::rd(hsrxencal_lane_offset_ifpowerctrl), vl::rd(enable_1))));
}

void mipi_4_rx_dphy_ns_if::assign_rx1_cal_done() {
    vl::wr(rx1_cal_done, vl::mux(vl::rd(rx1_cal_done_ovr_en), vl::rd(rx1_cal_done_ovr), vl::rd(rx1_cal_done_int)));
}

void mipi_4_rx_dphy_ns_if::assign_hsrx_en_bypass_lane1() {
    vl::wr(hsrx_en_bypass_lane1, vl::rd(hsrx_en_if_bypass_lane1_tester));
}

void mipi_4_rx_dphy_ns_if::assign_hsrx_en_if_lane1() {
    vl::wr(hsrx_en_if_lane1, vl::mux(vl::rd(hsrx_en_bypass_lane1), vl::rd(hsrx_en_if_lane1_tester), vl::rd(des_en_for_datalane_offset_cal_ifpowerctrl) & vl::rd(enable_1)));
}

// muxing of the dat in case of isolated test
void mipi_4_rx_dphy_ns_if::assign_rx2_rxhs_data_int() {
    vl::wr(rx2_rxhs_data_int, vl::mux(vl::rd(rx2_rxhs_data_ovr_en), vl::rd(rx2_rxhs_data_ovr), vl::rd(rx2_rxhs_data_if)));
}

void mipi_4_rx_dphy_ns_if::assign_rx2_rxhs_data_offset_cal_en_if() {
    vl::wr(rx2_rxhs_data_offset_cal_en_if, vl::mux(vl::rd(rx2_rxhs_data_offset_cal_en_ovr_en), vl::rd(rx2_rxhs_data_offset_cal_en_ovr), vl::land(vl::rd(rx2_rxhs_data_offset_cal_en_int), vl::rd(enable_2))));
}

void mipi_4_rx_dphy_ns_if::assign_rx2_rxhs_data_offset_cal_if() {
    vl::wr(rx2_rxhs_data_offset_cal_if, vl::mux(vl::rd(rx2_rxhs_data_offset_cal_ovr_en), vl::rd(rx2_rxhs_data_offset_cal_ovr), (vl::rd(rx2_rxhs_data_offset_cal_int) & vl::rep<6>(vl::rd(enable_2))) & vl::rep<6>(vl::rd(shutdownz))));
}

void mipi_4_rx_dphy_ns_if::assign_hsrx_des_en_bypass_lane2() {
    vl::wr(hsrx_des_en_bypass_lane2, vl::rd(rx_des_en_bypass_lane2_tester));
}

void mipi_4_rx_dphy_ns_if::assign_hsrx_des_en_if_lane2() {
    vl::wr(hsrx_des_en_if_lane2, vl::mux(vl::rd(hsrx_des_en_bypass_lane2), vl::rd(rx_des_en_lane2_tester), vl::land(vl::rd(des_en_for_datalane_offset_cal_ifpowerctrl), vl::rd(enable_2))));
}

void mipi_4_rx_dphy_ns_if::assign_hsrxpon_if_lane2() {
    vl::wr(hsrxpon_if_lane2, vl::mux(vl::rd(hsrxpon_bypass_lane2), vl::rd(hsrxpon_lane2_tester), vl::land(vl::rd(hsrxpon_datalane_ifpowerctrl), vl::rd(enable_2))));
}

void mipi_4_rx_dphy_ns_if::assign_rx2_start_calibration_int() {
    vl::wr(rx2_start_calibration_int, vl::mux(vl::rd(rx2_start_calibration_ovr_en), vl::rd(rx2_start_calibration_ovr), vl::land(vl::rd(hsrxencal_lane_offset_ifpowerctrl), vl::rd(enable_2))));
}

void mipi_4_rx_dphy_ns_if::assign_rx2_cal_done() {
    vl::wr(rx2_cal_done, vl::mux(vl::rd(rx2_cal_done_ovr_en), vl::rd(rx2_cal_done_ovr), vl::rd(rx2_cal_done_int)));
}

void mipi_4_rx_dphy_ns_if::assign_hsrx_en_bypass_lane2() {
    vl::wr(hsrx_en_bypass_lane2, vl::rd(hsrx_en_if_bypass_lane2_tester));
}

void mipi_4_rx_dphy_ns_if::assign_hsrx_en_if_lane2() {
    vl::wr(hsrx_en_if_lane2, vl::mux(vl::rd(hsrx_en_bypass_lane2), vl::rd(hsrx_en_if_lane2_tester), vl::rd(des_en_for_datalane_offset_cal_ifpowerctrl) & vl::rd(enable_2)));
}

// muxing of the dat in case of isolated test
void mipi_4_rx_dphy_ns_if::assign_rx3_rxhs_data_int() {
    vl::wr(rx3_rxhs_data_int, vl::mux(vl::rd(rx3_rxhs_data_ovr_en), vl::rd(rx3_rxhs_data_ovr), vl::rd(rx3_rxhs_data_if)));
}

void mipi_4_rx_dphy_ns_if::assign_rx3_rxhs_data_offset_cal_en_if() {
    vl::wr(rx3_rxhs_data_offset_cal_en_if, vl::mux(vl::rd(rx3_rxhs_data_offset_cal_en_ovr_en), vl::rd(rx3_rxhs_data_offset_cal_en_ovr), vl::land(vl::rd(rx3_rxhs_data_offset_cal_en_int), vl::rd(enable_3))));
}

void mipi_4_rx_dphy_ns_if::assign_rx3_rxhs_data_offset_cal_if() {
    vl::wr(rx3_rxhs_data_offset_cal_if, vl::mux(vl::rd(rx3_rxhs_data_offset_cal_ovr_en), vl::rd(rx3_rxhs_data_offset_cal_ovr), (vl::rd(rx3_rxhs_data_offset_cal_int) & vl::rep<6>(vl::rd(enable_3))) & vl::rep<6>(vl::rd(shutdownz))));
}

void mipi_4_rx_dphy_ns_if::assign_hsrx_des_en_bypass_lane3() {
    vl::wr(hsrx_des_en_bypass_lane3, vl::rd(rx_des_en_bypass_lane3_tester));
}

void mipi_4_rx_dphy_ns_if::assign_hsrx_des_en_if_lane3() {
    vl::wr(hsrx_des_en_if_lane3, vl::mux(vl::rd(hsrx_des_en_bypass_lane3), vl::rd(rx_des_en_lane3_tester), vl::land(vl::rd(des_en_for_datalane_offset_cal_ifpowerctrl), vl::rd(enable_3))));
}

void mipi_4_rx_dphy_ns_if::assign_hsrxpon_if_lane3() {
    vl::wr(hsrxpon_if_lane3, vl::mux(vl::rd(hsrxpon_bypass_lane3), vl::rd(hsrxpon_lane3_tester), vl::land(vl::rd(hsrxpon_datalane_ifpowerctrl), vl::rd(enable_3))));
}

void mipi_4_rx_dphy_ns_if::assign_rx3_start_calibration_int() {
    vl::wr(rx3_start_calibration_int, vl::mux(vl::rd(rx3_start_calibration_ovr_en), vl::rd(rx3_start_calibration_ovr), vl::land(vl::rd(hsrxencal_lane_offset_ifpowerctrl), vl::rd(enable_3))));
}

void mipi_4_rx_dphy_ns_if::assign_rx3_cal_done() {
    vl::wr(rx3_cal_done, vl::mux(vl::rd(rx3_cal_done_ovr_en), vl::rd(rx3_cal_done_ovr), vl::rd(rx3_cal_done_int)));
}

void mipi_4_rx_dphy_ns_if::assign_hsrx_en_bypass_lane3() {
    vl::wr(hsrx_en_bypass_lane3, vl::rd(hsrx_en_if_bypass_lane3_tester));
}

void mipi_4_rx_dphy_ns_if::assign_hsrx_en_if_lane3() {
    vl::wr(hsrx_en_if_lane3, vl::mux(vl::rd(hsrx_en_bypass_lane3), vl::rd(hsrx_en_if_lane3_tester), vl::rd(des_en_for_datalane_offset_cal_ifpowerctrl) & vl::rd(enable_3)));
}

// Interface with PPI
// assign rstztxlp_clklane       = resetz && ( enableclk ) ;
void mipi_4_rx_dphy_ns_if::assign_rstztxlp_lane0() {
    vl::wr(rstztxlp_lane0, vl::land(vl::rd(resetz), vl::land(vl::rd(enable_0), vl::lnot(vl::rd(direction_lane0)))));
}

void mipi_4_rx_dphy_ns_if::assign_rstzrx_lane0() {
    vl::wr(rstzrx_lane0, vl::land(vl::rd(resetz), vl::land(vl::rd(enable_0), vl::rd(direction_lane0))));
}

void mipi_4_rx_dphy_ns_if::assign_rstzta_lane0() {
    vl::wr(rstzta_lane0, vl::land(vl::rd(resetz), vl::rd(enable_0)));
}

// TESTER
void mipi_4_rx_dphy_ns_if::assign_bintpon_bypass_clklane() {
    vl::wr(bintpon_bypass_clklane, vl::rd(bintpon_bypass_clklane_tester));
}

void mipi_4_rx_dphy_ns_if::assign_hsrxpon_bypass_clklane() {
    vl::wr(hsrxpon_bypass_clklane, vl::rd(hsrxpon_bypass_clklane_tester));
}

// TESTER
void mipi_4_rx_dphy_ns_if::assign_lptxpon_bypass_lane0() {
    vl::wr(lptxpon_bypass_lane0, vl::mux(vl::rd(lptxpon_bypass_lane0_tester), vl::logic<1>(0x1ULL), vl::rd(hsdirect)));
}

void mipi_4_rx_dphy_ns_if::assign_lptxena_bypass_lane0() {
    vl::wr(lptxena_bypass_lane0, vl::rd(lptxena_bypass_lane0_tester));
}

void mipi_4_rx_dphy_ns_if::assign_lptxdin_bypass_lane0() {
    vl::wr(lptxdin_bypass_lane0, vl::rd(lptxdin_bypass_lane0_tester));
}

void mipi_4_rx_dphy_ns_if::assign_lptxenpulldwn_bypass_lane0() {
    vl::wr(lptxenpulldwn_bypass_lane0, vl::rd(lptxenpulldwn_bypass_lane0_tester));
}

void mipi_4_rx_dphy_ns_if::assign_lptxenulp_bypass_lane0() {
    vl::wr(lptxenulp_bypass_lane0, vl::mux(vl::rd(lptxenulp_bypass_lane0_tester), vl::logic<1>(0x1ULL), vl::rd(hsdirect)));
}

void mipi_4_rx_dphy_ns_if::assign_bintpon_bypass_lane0() {
    vl::wr(bintpon_bypass_lane0, vl::rd(bintpon_bypass_lane0_tester));
}

void mipi_4_rx_dphy_ns_if::assign_lprxponlp_bypass_lane0() {
    vl::wr(lprxponlp_bypass_lane0, vl::rd(lprxponlp_bypass_lane0_tester));
}

void mipi_4_rx_dphy_ns_if::assign_lprxponulp_bypass_lane0() {
    vl::wr(lprxponulp_bypass_lane0, vl::rd(lprxponulp_bypass_lane0_tester));
}

void mipi_4_rx_dphy_ns_if::assign_lprxponcd_bypass_lane0() {
    vl::wr(lprxponcd_bypass_lane0, vl::rd(lprxponcd_bypass_lane0_tester));
}

void mipi_4_rx_dphy_ns_if::assign_hsrxpon_bypass_lane0() {
    vl::wr(hsrxpon_bypass_lane0, vl::rd(hsrxpon_bypass_lane0_tester));
}

// TESTER
void mipi_4_rx_dphy_ns_if::assign_bintpon_bypass_lane1() {
    vl::wr(bintpon_bypass_lane1, vl::rd(bintpon_bypass_lane1_tester));
}

void mipi_4_rx_dphy_ns_if::assign_hsrxpon_bypass_lane1() {
    vl::wr(hsrxpon_bypass_lane1, vl::rd(hsrxpon_bypass_lane1_tester));
}

// TESTER
void mipi_4_rx_dphy_ns_if::assign_bintpon_bypass_lane2() {
    vl::wr(bintpon_bypass_lane2, vl::rd(bintpon_bypass_lane2_tester));
}

void mipi_4_rx_dphy_ns_if::assign_hsrxpon_bypass_lane2() {
    vl::wr(hsrxpon_bypass_lane2, vl::rd(hsrxpon_bypass_lane2_tester));
}

// TESTER
void mipi_4_rx_dphy_ns_if::assign_bintpon_bypass_lane3() {
    vl::wr(bintpon_bypass_lane3, vl::rd(bintpon_bypass_lane3_tester));
}

void mipi_4_rx_dphy_ns_if::assign_hsrxpon_bypass_lane3() {
    vl::wr(hsrxpon_bypass_lane3, vl::rd(hsrxpon_bypass_lane3_tester));
}

void mipi_4_rx_dphy_ns_if::assign_skew_back_comp_en() {
    vl::wr(skew_back_comp_en, vl::mux(vl::rd(skew_back_comp_en_ovr_en), vl::rd(skew_back_comp_en_ovr), vl::rd(skew_back_comp_en_ifpowerctrl)));
}

// CLOCK LANE
void mipi_4_rx_dphy_ns_if::assign_bintpon_if_clklane() {
    vl::wr(bintpon_if_clklane, vl::mux(vl::rd(bintpon_bypass_clklane_tester), vl::rd(bintpon_clklane_tester), vl::lor(vl::land(vl::rd(bintpon_ifpowerctrl), vl::rd(enableclk)), vl::rd(do_offs_and_skew_enableclk_not))));
}

void mipi_4_rx_dphy_ns_if::assign_lprxponlp_if_clklane() {
    vl::wr(lprxponlp_if_clklane, vl::rd(lprxponlp_clklane_tester));
}

void mipi_4_rx_dphy_ns_if::assign_lprxponulp_if_clklane() {
    vl::wr(lprxponulp_if_clklane, vl::rd(lprxponulp_clklane_tester));
}

void mipi_4_rx_dphy_ns_if::assign_lprxponcd_if_clklane() {
    vl::wr(lprxponcd_if_clklane, vl::rd(lprxponcd_clklane_tester));
}

void mipi_4_rx_dphy_ns_if::assign_lprxponlp_bypass_clklane() {
    vl::wr(lprxponlp_bypass_clklane, vl::rd(lprxponlp_bypass_clklane_tester));
}

void mipi_4_rx_dphy_ns_if::assign_lprxponulp_bypass_clklane() {
    vl::wr(lprxponulp_bypass_clklane, vl::rd(lprxponulp_bypass_clklane_tester));
}

void mipi_4_rx_dphy_ns_if::assign_lprxponcd_bypass_clklane() {
    vl::wr(lprxponcd_bypass_clklane, vl::rd(lprxponcd_bypass_clklane_tester));
}

// LANE0
void mipi_4_rx_dphy_ns_if::assign_bintpon_if_lane0() {
    vl::wr(bintpon_if_lane0, vl::mux(vl::rd(bintpon_bypass_lane0_tester), vl::rd(bintpon_lane0_tester), vl::land(vl::rd(bintpon_ifpowerctrl), vl::rd(enable_0))));
}

void mipi_4_rx_dphy_ns_if::assign_lprxponlp_if_lane0() {
    vl::wr(lprxponlp_if_lane0, vl::rd(lprxponlp_lane0_tester));
}

void mipi_4_rx_dphy_ns_if::assign_lprxponulp_if_lane0() {
    vl::wr(lprxponulp_if_lane0, vl::rd(lprxponulp_lane0_tester));
}

void mipi_4_rx_dphy_ns_if::assign_lprxponcd_if_lane0() {
    vl::wr(lprxponcd_if_lane0, vl::rd(lprxponcd_lane0_tester));
}

void mipi_4_rx_dphy_ns_if::assign_lptxpon_if_lane0() {
    vl::wr(lptxpon_if_lane0, vl::mux(vl::rd(lptxpon_bypass_lane0_tester), vl::rd(lptxpon_lane0_tester), vl::mux(vl::rd(hsdirect), vl::logic<1>(0x0ULL), vl::land(vl::rd(lptxpon_ifpowerctrl), vl::rd(enable_0)))));
}

void mipi_4_rx_dphy_ns_if::assign_lptxena_if_lane0() {
    vl::wr(lptxena_if_lane0, vl::mux(vl::rd(lptxena_bypass_lane0_tester), vl::rd(lptxena_lane0_tester), vl::land(vl::land(vl::rd(lptxena_ifpowerctrl), vl::rd(enable_0)), ~vl::rd(direction_lane0))));
}

void mipi_4_rx_dphy_ns_if::assign_lptxdin_if_lane0() {
    vl::wr(lptxdin_if_lane0, vl::mux(vl::rd(lptxdin_bypass_lane0_tester), vl::rd(lptxdin_lane0_tester), (vl::rep<2>(vl::rd(lptxdin_ifpowerctrl)) & vl::rep<2>(vl::rd(enable_0))) & (~vl::rep<2>(vl::rd(direction_lane0)))));
}

void mipi_4_rx_dphy_ns_if::assign_lptxenpulldn_if_lane0() {
    vl::wr(lptxenpulldn_if_lane0, vl::mux(vl::rd(lptxenpulldwn_bypass_lane0_tester), vl::rd(lptxenpulldwn_lane0_tester), vl::land(vl::land(vl::rd(lptxenpulldwn_ifpowerctrl), vl::rd(enable_0)), ~vl::rd(direction_lane0))));
}

void mipi_4_rx_dphy_ns_if::assign_lptxenulp_if_lane0() {
    vl::wr(lptxenulp_if_lane0, vl::mux(vl::rd(lptxenulp_bypass_lane0_tester), vl::rd(lptxenulp_lane0_tester), vl::logic<1>(0x0ULL)));
}

// LANE1
void mipi_4_rx_dphy_ns_if::assign_bintpon_if_lane1() {
    vl::wr(bintpon_if_lane1, vl::mux(vl::rd(bintpon_bypass_lane1_tester), vl::rd(bintpon_lane1_tester), vl::land(vl::rd(bintpon_ifpowerctrl), vl::rd(enable_1))));
}

void mipi_4_rx_dphy_ns_if::assign_lprxponlp_if_lane1() {
    vl::wr(lprxponlp_if_lane1, vl::rd(lprxponlp_lane1_tester));
}

void mipi_4_rx_dphy_ns_if::assign_lprxponulp_if_lane1() {
    vl::wr(lprxponulp_if_lane1, vl::rd(lprxponulp_lane1_tester));
}

void mipi_4_rx_dphy_ns_if::assign_lprxponcd_if_lane1() {
    vl::wr(lprxponcd_if_lane1, vl::rd(lprxponcd_lane1_tester));
}

void mipi_4_rx_dphy_ns_if::assign_lprxponlp_bypass_lane1() {
    vl::wr(lprxponlp_bypass_lane1, vl::rd(lprxponlp_bypass_lane1_tester));
}

void mipi_4_rx_dphy_ns_if::assign_lprxponulp_bypass_lane1() {
    vl::wr(lprxponulp_bypass_lane1, vl::rd(lprxponulp_bypass_lane1_tester));
}

void mipi_4_rx_dphy_ns_if::assign_lprxponcd_bypass_lane1() {
    vl::wr(lprxponcd_bypass_lane1, vl::rd(lprxponcd_bypass_lane1_tester));
}

// LANE2
void mipi_4_rx_dphy_ns_if::assign_bintpon_if_lane2() {
    vl::wr(bintpon_if_lane2, vl::mux(vl::rd(bintpon_bypass_lane2_tester), vl::rd(bintpon_lane2_tester), vl::land(vl::rd(bintpon_ifpowerctrl), vl::rd(enable_2))));
}

void mipi_4_rx_dphy_ns_if::assign_lprxponlp_if_lane2() {
    vl::wr(lprxponlp_if_lane2, vl::rd(lprxponlp_lane2_tester));
}

void mipi_4_rx_dphy_ns_if::assign_lprxponulp_if_lane2() {
    vl::wr(lprxponulp_if_lane2, vl::rd(lprxponulp_lane2_tester));
}

void mipi_4_rx_dphy_ns_if::assign_lprxponcd_if_lane2() {
    vl::wr(lprxponcd_if_lane2, vl::rd(lprxponcd_lane2_tester));
}

void mipi_4_rx_dphy_ns_if::assign_lprxponlp_bypass_lane2() {
    vl::wr(lprxponlp_bypass_lane2, vl::rd(lprxponlp_bypass_lane2_tester));
}

void mipi_4_rx_dphy_ns_if::assign_lprxponulp_bypass_lane2() {
    vl::wr(lprxponulp_bypass_lane2, vl::rd(lprxponulp_bypass_lane2_tester));
}

void mipi_4_rx_dphy_ns_if::assign_lprxponcd_bypass_lane2() {
    vl::wr(lprxponcd_bypass_lane2, vl::rd(lprxponcd_bypass_lane2_tester));
}

// LANE3
void mipi_4_rx_dphy_ns_if::assign_bintpon_if_lane3() {
    vl::wr(bintpon_if_lane3, vl::mux(vl::rd(bintpon_bypass_lane3_tester), vl::rd(bintpon_lane3_tester), vl::land(vl::rd(bintpon_ifpowerctrl), vl::rd(enable_3))));
}

void mipi_4_rx_dphy_ns_if::assign_lprxponlp_if_lane3() {
    vl::wr(lprxponlp_if_lane3, vl::rd(lprxponlp_lane3_tester));
}

void mipi_4_rx_dphy_ns_if::assign_lprxponulp_if_lane3() {
    vl::wr(lprxponulp_if_lane3, vl::rd(lprxponulp_lane3_tester));
}

void mipi_4_rx_dphy_ns_if::assign_lprxponcd_if_lane3() {
    vl::wr(lprxponcd_if_lane3, vl::rd(lprxponcd_lane3_tester));
}

void mipi_4_rx_dphy_ns_if::assign_lprxponlp_bypass_lane3() {
    vl::wr(lprxponlp_bypass_lane3, vl::rd(lprxponlp_bypass_lane3_tester));
}

void mipi_4_rx_dphy_ns_if::assign_lprxponulp_bypass_lane3() {
    vl::wr(lprxponulp_bypass_lane3, vl::rd(lprxponulp_bypass_lane3_tester));
}

void mipi_4_rx_dphy_ns_if::assign_lprxponcd_bypass_lane3() {
    vl::wr(lprxponcd_bypass_lane3, vl::rd(lprxponcd_bypass_lane3_tester));
}

// consider if the lanes are on or off ( PPI control )  // consider lane 0 in RX/TX condition
void mipi_4_rx_dphy_ns_if::assign_global_offset_cal_done() {
    vl::wr(global_offset_cal_done, vl::land(vl::land(vl::land(vl::lor(vl::lor(vl::rd(rx0_cal_done), ~vl::rd(enable_0)), ~vl::rd(direction_lane0)), vl::lor(vl::rd(rx1_cal_done), ~vl::rd(enable_1))), vl::lor(vl::rd(rx2_cal_done), ~vl::rd(enable_2))), vl::lor(vl::rd(rx3_cal_done), ~vl::rd(enable_3))));
}

// when all the offset machines are done ( datalane)
void mipi_4_rx_dphy_ns_if::assign_txulpsexit_global() {
    vl::wr(txulpsexit_global, (vl::rd(txulpsexit_lane0) | vl::rd(direction_lane0)) | (~vl::rd(enable_0)));
}

// global txulpsexit to be provided into the IFPOWER FSM
void mipi_4_rx_dphy_ns_if::assign_ulpsactivenot_global() {
    vl::wr(ulpsactivenot_global, ((((vl::rd(ulpsactivenotclk) & vl::rd(enableclk)) | (vl::rd(ulpsactivenot_0) & vl::rd(enable_0))) | (vl::rd(ulpsactivenot_1) & vl::rd(enable_1))) | (vl::rd(ulpsactivenot_2) & vl::rd(enable_2))) | (vl::rd(ulpsactivenot_3) & vl::rd(enable_3)));
}

// global ulpsactivenot to be provided into the IFPOWER FSM
// global skew
void mipi_4_rx_dphy_ns_if::assign_global_skew_cal_done() {
    vl::wr(global_skew_cal_done, vl::land(vl::land(vl::land(vl::lor(vl::rd(rx0_deskewcaldone_if), ~vl::rd(enable_0)), vl::lor(vl::rd(rx1_deskewcaldone_if), ~vl::rd(enable_1))), vl::lor(vl::rd(rx2_deskewcaldone_if), ~vl::rd(enable_2))), vl::lor(vl::rd(rx3_deskewcaldone_if), ~vl::rd(enable_3))));
}

// when all the offset machines are done ( datalane)
// Selection bewteen burnin oscillator and cfg clock for data lane offset configuration
void mipi_4_rx_dphy_ns_if::assign_rxclk_rxhs_int_clk_sel_if() {
    vl::wr(rxclk_rxhs_int_clk_sel_if, vl::rd(rxclk_rxhs_int_clk_sel_int));
}

// from tester
// skew bypass
void mipi_4_rx_dphy_ns_if::assign_perform_skew() {
    vl::wr(perform_skew, vl::mux(vl::rd(skew_mux_sel), vl::rd(skew_mux_run), ~vl::rd(deskew_legacyz)));
}

void mipi_4_rx_dphy_ns_if::assign_hsfreqrange_int() {
    vl::wr(hsfreqrange_int, vl::mux(vl::rd(hsfreqrange_ovr_en), vl::rd(hsfreqrange_ovr), vl::rd(hsfreqrange)));
}

void mipi_4_rx_dphy_ns_if::assign_rx0_osc_freq_target_if() {
    vl::wr(rx0_osc_freq_target_if, vl::mux(vl::rd(rx0_osc_freq_target_if_ovr_en), vl::rd(rx0_osc_freq_target_if_ovr), vl::rd(ddl_osc_freq_target)));
}

void mipi_4_rx_dphy_ns_if::assign_rx1_osc_freq_target_if() {
    vl::wr(rx1_osc_freq_target_if, vl::mux(vl::rd(rx1_osc_freq_target_if_ovr_en), vl::rd(rx1_osc_freq_target_if_ovr), vl::rd(ddl_osc_freq_target)));
}

void mipi_4_rx_dphy_ns_if::assign_rx2_osc_freq_target_if() {
    vl::wr(rx2_osc_freq_target_if, vl::mux(vl::rd(rx2_osc_freq_target_if_ovr_en), vl::rd(rx2_osc_freq_target_if_ovr), vl::rd(ddl_osc_freq_target)));
}

void mipi_4_rx_dphy_ns_if::assign_rx3_osc_freq_target_if() {
    vl::wr(rx3_osc_freq_target_if, vl::mux(vl::rd(rx3_osc_freq_target_if_ovr_en), vl::rd(rx3_osc_freq_target_if_ovr), vl::rd(ddl_osc_freq_target)));
}

void mipi_4_rx_dphy_ns_if::assign_tclk_miss_opt() {
    vl::wr(tclk_miss_opt, vl::cat(vl::mux(vl::rd(tclk_miss_div2_ovr_en), vl::rd(tclk_miss_div2_ovr), vl::rd(tclk_miss_div2)), vl::rd(tclk_miss_opt_int)));
}

void mipi_4_rx_dphy_ns_if::assign_pll_n_int() {
    vl::wr(pll_n_int, vl::mux(vl::rd(pll_n_ovr_en), vl::rd(pll_n_ovr), vl::logic<4>(0xbULL)));
}

void mipi_4_rx_dphy_ns_if::assign_rx_rxhs_gmode_if() {
    vl::wr(rx_rxhs_gmode_if, vl::rd(rx_rxhs_gmode_int) & vl::rep<2>(vl::rd(shutdownz)));
}

void mipi_4_rx_dphy_ns_if::assign_rxclk_rxhs_ddr_clk_div_if() {
    vl::wr(rxclk_rxhs_ddr_clk_div_if, vl::rd(rxclk_rxhs_ddr_clk_div_int) & vl::rep<2>(vl::rd(shutdownz)));
}

void mipi_4_rx_dphy_ns_if::assign_arstz_scanable() {
    vl::wr(arstz_scanable, vl::mux(vl::rd(scan_mode), vl::rd(scan_set_rst_n), vl::logic<1>(0x0ULL)));
}

// PLL analog inputs tied to "0" in scanmode
void mipi_4_rx_dphy_ns_if::assign_pll_refclk() {
    vl::wr(pll_refclk, (~vl::rd(scan_mode)) & vl::rd(refclk));
}

void mipi_4_rx_dphy_ns_if::assign_pll_pwron() {
    vl::wr(pll_pwron, (~vl::rd(scan_mode)) & vl::mux(vl::rd(pll_pwron_ovr_en), vl::rd(pll_pwron_ovr), vl::rd(pll_pwron_int)));
}

void mipi_4_rx_dphy_ns_if::assign_pll_reset() {
    vl::wr(pll_reset, (~vl::rd(scan_mode)) & vl::mux(vl::rd(pll_reset_ovr_en), vl::rd(pll_reset_ovr), vl::rd(pll_reset_int)));
}

void mipi_4_rx_dphy_ns_if::assign_pll_gear_shift() {
    vl::wr(pll_gear_shift, (~vl::rd(scan_mode)) & vl::mux(vl::rd(pll_gear_shift_ovr_en), vl::rd(pll_gear_shift_ovr), vl::rd(pll_gear_shift_int)));
}

void mipi_4_rx_dphy_ns_if::assign_pll_opmode() {
    vl::wr(pll_opmode, vl::rep<5>(~vl::rd(scan_mode)) & vl::rd(pll_opmode_int));
}

void mipi_4_rx_dphy_ns_if::assign_pll_lock_sel() {
    vl::wr(pll_lock_sel, (~vl::rd(scan_mode)) & vl::rd(pll_lock_sel_int));
}

void mipi_4_rx_dphy_ns_if::assign_pll_th1() {
    vl::wr(pll_th1, vl::rep<10>(~vl::rd(scan_mode)) & vl::rd(pll_th1_int));
}

void mipi_4_rx_dphy_ns_if::assign_pll_th2() {
    vl::wr(pll_th2, vl::rep<8>(~vl::rd(scan_mode)) & vl::rd(pll_th2_int));
}

void mipi_4_rx_dphy_ns_if::assign_pll_th3() {
    vl::wr(pll_th3, vl::rep<8>(~vl::rd(scan_mode)) & vl::rd(pll_th3_int));
}

void mipi_4_rx_dphy_ns_if::assign_pll_th_delay() {
    vl::wr(pll_th_delay, vl::rep<6>(~vl::rd(scan_mode)) & vl::rd(pll_th_delay_int));
}

void mipi_4_rx_dphy_ns_if::assign_pll_lock_det_on() {
    vl::wr(pll_lock_det_on, (~vl::rd(scan_mode)) & vl::mux(vl::rd(pll_lock_det_on_ovr_en), vl::rd(pll_lock_det_on_ovr), vl::rd(pll_lock_det_on_int)));
}

void mipi_4_rx_dphy_ns_if::assign_pll_lockdet_mode() {
    vl::wr(pll_lockdet_mode, (~vl::rd(scan_mode)) & vl::rd(pll_lockdet_mode_int));
}

void mipi_4_rx_dphy_ns_if::assign_pll_tstplldig() {
    vl::wr(pll_tstplldig, vl::rep<3>(~vl::rd(scan_mode)) & vl::rd(pll_tstplldig_int));
}

void mipi_4_rx_dphy_ns_if::assign_pll_meas_iv() {
    vl::wr(pll_meas_iv, vl::rep<18>(~vl::rd(scan_mode)) & vl::rd(pll_meas_iv_int));
}

void mipi_4_rx_dphy_ns_if::assign_pll_mpll_prog() {
    vl::wr(pll_mpll_prog, vl::rep<17>(~vl::rd(scan_mode)) & vl::rd(pll_mpll_prog_int));
}

void mipi_4_rx_dphy_ns_if::assign_pll_atb_sense_sel() {
    vl::wr(pll_atb_sense_sel, (~vl::rd(scan_mode)) & vl::rd(pll_atb_sense_sel_int));
}

void mipi_4_rx_dphy_ns_if::assign_pll_vco_cntrl() {
    vl::wr(pll_vco_cntrl, (vl::rep<6>(~vl::rd(scan_mode)) & vl::rd(pll_vco_cntrl_int)) & vl::rep<6>(vl::rd(shutdownz)));
}

void mipi_4_rx_dphy_ns_if::assign_pll_prop_cntrl() {
    vl::wr(pll_prop_cntrl, vl::rep<6>(~vl::rd(scan_mode)) & vl::rd(pll_prop_cntrl_int));
}

void mipi_4_rx_dphy_ns_if::assign_pll_int_cntrl() {
    vl::wr(pll_int_cntrl, vl::rep<6>(~vl::rd(scan_mode)) & vl::rd(pll_int_cntrl_int));
}

void mipi_4_rx_dphy_ns_if::assign_pll_gmp_cntrl() {
    vl::wr(pll_gmp_cntrl, vl::rep<2>(~vl::rd(scan_mode)) & vl::rd(pll_gmp_cntrl_int));
}

void mipi_4_rx_dphy_ns_if::assign_pll_cpbias_cntrl() {
    vl::wr(pll_cpbias_cntrl, vl::rep<7>(~vl::rd(scan_mode)) & vl::rd(pll_cpbias_cntrl_int));
}

void mipi_4_rx_dphy_ns_if::assign_pll_n() {
    vl::wr(pll_n, (vl::rep<4>(~vl::rd(scan_mode)) & vl::rd(pll_n_int)) & vl::rep<4>(vl::rd(shutdownz)));
}

void mipi_4_rx_dphy_ns_if::assign_pll_m() {
    vl::wr(pll_m, (vl::rep<10>(~vl::rd(scan_mode)) & vl::rd(pll_m_int)) & vl::rep<10>(vl::rd(shutdownz)));
}

// assign pll_clksel_en          =    (~scanmode)  & ((pll_clksel_en_ovr_en) ? pll_clksel_en_ovr : pll_clksel_en_int) ;
// assign pll_force_lock         =    (~scanmode)  & clk_ext_mode ? clk_ext_en : pll_force_lock_int ;
void mipi_4_rx_dphy_ns_if::assign_pll_force_lock() {
    vl::wr(pll_force_lock, (~vl::rd(scan_mode)) & vl::rd(pll_force_lock_int));
}

// assign pll_clksel_in_int      = (scanmode) ? {2{scantie}} : pll_clksel_in ;
void mipi_4_rx_dphy_ns_if::assign_pll_testlock_int() {
    vl::wr(pll_testlock_int, vl::mux(vl::rd(scan_mode), vl::rd(scantie), vl::rd(pll_testlock)));
}

void mipi_4_rx_dphy_ns_if::assign_pll_lock_int() {
    vl::wr(pll_lock_int, vl::mux(vl::rd(scan_mode), vl::rd(scantie), vl::mux(vl::rd(pll_lock_ovr_en), vl::rd(pll_lock_ovr), vl::rd(pll_lock))));
}

void mipi_4_rx_dphy_ns_if::assign_pll_clkouten_left() {
    vl::wr(pll_clkouten_left, (~vl::rd(scan_mode)) & vl::rd(pll_clkouten_left_int));
}

void mipi_4_rx_dphy_ns_if::assign_pll_clkouten_right() {
    vl::wr(pll_clkouten_right, (~vl::rd(scan_mode)) & vl::rd(pll_clkouten_right_int));
}

void mipi_4_rx_dphy_ns_if::assign_pll_vpl_det_int() {
    vl::wr(pll_vpl_det_int, vl::mux(vl::rd(scan_mode), vl::rd(scantie), vl::rd(pll_vpl_det)));
}

void mipi_4_rx_dphy_ns_if::assign_lane_bist_to_run() {
    vl::wr(lane_bist_to_run, vl::cat(vl::rd(bist_run_lane3), vl::rd(bist_run_lane2), vl::rd(bist_run_lane1), vl::rd(bist_run_lane0)));
}

// run all lanes as default
void mipi_4_rx_dphy_ns_if::assign_biston_int() {
    vl::wr(biston_int, vl::land(vl::rd(biston) | vl::rd(bist_manual), vl::rd(enable_lanes)));
}

void mipi_4_rx_dphy_ns_if::assign_fsm_trigger_int() {
    vl::wr(fsm_trigger_int, vl::land(vl::rd(pll_lock_int_sync), vl::rd(pll_startup_done)));
}

// triggers with lock+startup done
void mipi_4_rx_dphy_ns_if::assign_fsm_trigger() {
    vl::wr(fsm_trigger, vl::mux(vl::rd(fsm_trigger_ovr_en), vl::rd(fsm_trigger_ovr), vl::rd(fsm_trigger_int)));
}

void mipi_4_rx_dphy_ns_if::assign_del_datalane_int() {
    vl::wr(del_datalane_int, vl::cat(vl::logic<3>(0x0ULL), vl::rd(rx_timebase_if).slice<5>(3)));
}

// clkprepare + dalalane lowest baudrate ( 8UI + (145+ 10 UI) +slack ) target 500n
void mipi_4_rx_dphy_ns_if::assign_del_datalane() {
    vl::wr(del_datalane, vl::mux(vl::rd(del_datalane_ovr_en), vl::rd(del_datalane_ovr), vl::rd(del_datalane_int)));
}

// error latch switch
void mipi_4_rx_dphy_ns_if::assign_rx0_pm_sample_error_counter_if() {
    vl::wr(rx0_pm_sample_error_counter_if, vl::mux(vl::rd(rx0_pm_man_error_latch), vl::rd(rx0_pm_sample_error_counter_int), vl::rd(bistdone)));
}

// triggers in auto when the two first strobes or when manual control is given
void mipi_4_rx_dphy_ns_if::assign_rx1_pm_sample_error_counter_if() {
    vl::wr(rx1_pm_sample_error_counter_if, vl::mux(vl::rd(rx1_pm_man_error_latch), vl::rd(rx1_pm_sample_error_counter_int), vl::rd(bistdone)));
}

void mipi_4_rx_dphy_ns_if::assign_rx2_pm_sample_error_counter_if() {
    vl::wr(rx2_pm_sample_error_counter_if, vl::mux(vl::rd(rx2_pm_man_error_latch), vl::rd(rx2_pm_sample_error_counter_int), vl::rd(bistdone)));
}

void mipi_4_rx_dphy_ns_if::assign_rx3_pm_sample_error_counter_if() {
    vl::wr(rx3_pm_sample_error_counter_if, vl::mux(vl::rd(rx3_pm_man_error_latch), vl::rd(rx3_pm_sample_error_counter_int), vl::rd(bistdone)));
}

void mipi_4_rx_dphy_ns_if::assign_rxclk_lbck_clk_en_if() {
    vl::wr(rxclk_lbck_clk_en_if, vl::mux(vl::rd(rxclk_lbck_clk_en_ovr_en), vl::rd(rxclk_lbck_clk_en_ovr), vl::rd(rxclk_lbck_clk_en_int)));
}

void mipi_4_rx_dphy_ns_if::assign_rxclk_lbck_div_en_if() {
    vl::wr(rxclk_lbck_div_en_if, vl::mux(vl::rd(rxclk_lbck_div_en_ovr_en), vl::rd(rxclk_lbck_div_en_ovr), vl::rd(rxclk_lbck_div_en_int)));
}

void mipi_4_rx_dphy_ns_if::assign_rxclk_lbck_serial_clk_en_if() {
    vl::wr(rxclk_lbck_serial_clk_en_if, vl::mux(vl::rd(rxclk_lbck_serial_clk_en_ovr_en), vl::rd(rxclk_lbck_serial_clk_en_ovr), vl::rd(rxclk_lbck_serial_clk_en_int)));
}

void mipi_4_rx_dphy_ns_if::assign_rxclk_lbck_serial_en_if() {
    vl::wr(rxclk_lbck_serial_en_if, vl::mux(vl::rd(rxclk_lbck_serial_en_ovr_en), vl::rd(rxclk_lbck_serial_en_ovr), vl::rd(rxclk_lbck_serial_en_int)));
}

void mipi_4_rx_dphy_ns_if::assign_rxclk_lbdata_clk_en_if() {
    vl::wr(rxclk_lbdata_clk_en_if, vl::mux(vl::rd(rxclk_lbdata_clk_en_ovr_en), vl::rd(rxclk_lbdata_clk_en_ovr), vl::rd(rxclk_lbdata_clk_en_int)));
}

void mipi_4_rx_dphy_ns_if::assign_rxclk_lbdata_div_en_if() {
    vl::wr(rxclk_lbdata_div_en_if, vl::mux(vl::rd(rxclk_lbdata_div_en_ovr_en), vl::rd(rxclk_lbdata_div_en_ovr), vl::rd(rxclk_lbdata_div_en_int)));
}

void mipi_4_rx_dphy_ns_if::assign_rxclk_lbdata_serial_clk_en_if() {
    vl::wr(rxclk_lbdata_serial_clk_en_if, vl::mux(vl::rd(rxclk_lbdata_serial_clk_en_ovr_en), vl::rd(rxclk_lbdata_serial_clk_en_ovr), vl::rd(rxclk_lbdata_serial_clk_en_int)));
}

void mipi_4_rx_dphy_ns_if::assign_rxclk_lbdata_serial_en_if() {
    vl::wr(rxclk_lbdata_serial_en_if, vl::mux(vl::rd(rxclk_lbdata_serial_en_ovr_en), vl::rd(rxclk_lbdata_serial_en_ovr), vl::rd(rxclk_lbdata_serial_en_int)));
}

void mipi_4_rx_dphy_ns_if::assign_rxclk_lb_sel_if() {
    vl::wr(rxclk_lb_sel_if, vl::mux(vl::rd(rxclk_lb_sel_ovr_en), vl::rd(rxclk_lb_sel_ovr), vl::rd(rxclk_lb_sel_int)));
}

void mipi_4_rx_dphy_ns_if::assign_rx0_pm_start_control_if() {
    vl::wr(rx0_pm_start_control_if, vl::mux(vl::rd(rx0_pm_start_control_ovr_en), vl::rd(rx0_pm_start_control_ovr), vl::land(vl::rd(bist_sync_en_int), vl::rd(bist_run_lane0))));
}

// bist is only triggered if we have the mask)
void mipi_4_rx_dphy_ns_if::assign_rx1_pm_start_control_if() {
    vl::wr(rx1_pm_start_control_if, vl::mux(vl::rd(rx1_pm_start_control_ovr_en), vl::rd(rx1_pm_start_control_ovr), vl::land(vl::rd(bist_sync_en_int), vl::rd(bist_run_lane1))));
}

// bist is only triggered if we have the mask)
void mipi_4_rx_dphy_ns_if::assign_rx2_pm_start_control_if() {
    vl::wr(rx2_pm_start_control_if, vl::mux(vl::rd(rx2_pm_start_control_ovr_en), vl::rd(rx2_pm_start_control_ovr), vl::land(vl::rd(bist_sync_en_int), vl::rd(bist_run_lane2))));
}

// bist is only triggered if we have the mask)
void mipi_4_rx_dphy_ns_if::assign_rx3_pm_start_control_if() {
    vl::wr(rx3_pm_start_control_if, vl::mux(vl::rd(rx3_pm_start_control_ovr_en), vl::rd(rx3_pm_start_control_ovr), vl::land(vl::rd(bist_sync_en_int), vl::rd(bist_run_lane3))));
}

// bist is only triggered if we have the mask)
// hsdirect  is controlled by the bist machine when entering in bist mode ; all the other  situations are carried out with hsdirect
void mipi_4_rx_dphy_ns_if::assign_hsdirect() {
    vl::wr(hsdirect, vl::lor(vl::rd(trigger_hsdirect_int), vl::rd(hsdirect_int)));
}

void mipi_4_rx_dphy_ns_if::assign_hsdirect_if() {
    vl::wr(hsdirect_if, vl::land(vl::rd(hsdirect), vl::rd(enable_lanes)));
}

// output to the core : it is gated with enable_lanes: only when the calibration is finished
// Muxing between havonf PLL booted up from the begining or not
void mipi_4_rx_dphy_ns_if::assign_onpll() {
    vl::wr(onpll, vl::mux(vl::rd(onpll_ovr_en), vl::rd(onpll_ovr), vl::mux(vl::rd(pll_boot_from_start), vl::rd(pll_boot_fsm_int), vl::rd(onpll_int))));
}

// BIST FSM INSTANCE
void mipi_4_rx_dphy_ns_if::assign_pll_clksel_en() {
    vl::wr(pll_clksel_en, (~vl::rd(scan_mode)) & vl::mux(vl::rd(pll_clksel_en_ovr_en), vl::rd(pll_clksel_en_ovr), vl::rd(pll_clksel_en_int)));
}

// Internal feed muxing power on ( cfg+burn+loopback on mux )
void mipi_4_rx_dphy_ns_if::assign_rx_rxhs_prog_int() {
    vl::wr(rx_rxhs_prog_int, vl::cat(vl::logic<1>(0x0ULL), vl::rd(rx_rxhs_prog_mux_pwron_int)));
}

// 2 bits ( lsb is the one controlling the mux but 2-bit bus)
// assign rx_rxhs_prog_if  = rx_rxhs_prog_ovr_en ? rx_rxhs_prog_ovr : rx_rxhs_prog_int ;
// 8L support
void mipi_4_rx_dphy_ns_if::assign_mastermacro() {
    vl::wr(mastermacro, vl::rd(mastermacro_int));
}

// assign phy_hsrxtclkmisspd_out = hsrxtclkmisspd_out    ;
// assign hsrxtclkmisspd_in      = phy_hsrxtclkmisspd_in ;
void mipi_4_rx_dphy_ns_if::assign_phy_launch_des_en_out() {
    vl::wr(phy_launch_des_en_out, vl::rd(launch_des_en_out));
}

void mipi_4_rx_dphy_ns_if::assign_launch_des_en_in() {
    vl::wr(launch_des_en_in, vl::rd(phy_launch_des_en_in));
}

void mipi_4_rx_dphy_ns_if::assign_phy_rxclk_rxhs_clk_en_out() {
    vl::wr(phy_rxclk_rxhs_clk_en_out, vl::rd(rxclk_rxhs_clk_en_out));
}

void mipi_4_rx_dphy_ns_if::assign_rxclk_rxhs_clk_en_in() {
    vl::wr(rxclk_rxhs_clk_en_in, vl::rd(phy_rxclk_rxhs_clk_en_in));
}

void mipi_4_rx_dphy_ns_if::assign_phy_rxclk_rxhs_rx_en_out() {
    vl::wr(phy_rxclk_rxhs_rx_en_out, vl::rd(rxclk_rxhs_rx_en_out));
}

void mipi_4_rx_dphy_ns_if::assign_rxclk_rxhs_rx_en_in() {
    vl::wr(rxclk_rxhs_rx_en_in, vl::rd(phy_rxclk_rxhs_rx_en_in));
}

void mipi_4_rx_dphy_ns_if::assign_phy_rxclk_rxhs_rx_pon_out() {
    vl::wr(phy_rxclk_rxhs_rx_pon_out, vl::rd(rxclk_rxhs_rx_pon_out));
}

void mipi_4_rx_dphy_ns_if::assign_rxclk_rxhs_rx_pon_in() {
    vl::wr(rxclk_rxhs_rx_pon_in, vl::rd(phy_rxclk_rxhs_rx_pon_in));
}

void mipi_4_rx_dphy_ns_if::assign_phy_rxclk_rxhs_rx_enlogic_out() {
    vl::wr(phy_rxclk_rxhs_rx_enlogic_out, vl::rd(rxclk_rxhs_rx_enlogic_out));
}

void mipi_4_rx_dphy_ns_if::assign_rxclk_rxhs_rx_enlogic_in() {
    vl::wr(rxclk_rxhs_rx_enlogic_in, vl::rd(phy_rxclk_rxhs_rx_enlogic_in));
}

void mipi_4_rx_dphy_ns_if::assign_rx_rxhs_prog_if() {
    vl::wr(rx_rxhs_prog_if, vl::mux(vl::rd(rx_rxhs_prog_ovr_en), vl::rd(rx_rxhs_prog_ovr), vl::rep<2>(vl::rd(mastermacro)) & (vl::rd(rx_rxhs_prog_int) | vl::rd(phy_rx_rxhs_prog_in))));
}

void mipi_4_rx_dphy_ns_if::assign_phy_rx_rxhs_prog_out() {
    vl::wr(phy_rx_rxhs_prog_out, vl::rd(rx_rxhs_prog_int));
}

void mipi_4_rx_dphy_ns_if::assign_rxclk_rxhs_feed_int_clk_if() {
    vl::wr(rxclk_rxhs_feed_int_clk_if, vl::mux(vl::rd(rxclk_rxhs_feed_int_clk_ovr_en), vl::rd(rxclk_rxhs_feed_int_clk_ovr), vl::land(vl::rd(mastermacro), vl::lor(vl::land(vl::lor(vl::rd(rxclk_feed_int_clk_ifpowerctrl), vl::rd(phy_rxclk_rxhs_feed_int_clk_in)), vl::rd(enableclk)), vl::rd(do_offs_and_skew_enableclk_not)))));
}

void mipi_4_rx_dphy_ns_if::assign_phy_rxclk_rxhs_feed_int_clk_out() {
    vl::wr(phy_rxclk_rxhs_feed_int_clk_out, vl::lor(vl::land(vl::rd(rxclk_feed_int_clk_ifpowerctrl), vl::rd(enableclk)), vl::rd(do_offs_and_skew_enableclk_not)));
}

void mipi_4_rx_dphy_ns_if::assign_ulpsactivenotclk() {
    vl::wr(ulpsactivenotclk, vl::mux(vl::rd(mastermacro), vl::rd(ulpsactivenotrx_clklane), vl::rd(phy_ulpsactivenotclk_in)));
}

void mipi_4_rx_dphy_ns_if::glue_testclkinv_scanmux_d0() { vl::wr(c_testclkinv_scanmux_d0, ~vl::rd(testclk)); }

void mipi_4_rx_dphy_ns_if::glue_gen_stopstatetx_lane0_sync_pulse_in() { vl::wr(c_gen_stopstatetx_lane0_sync_pulse_in, vl::mux(vl::rd(scan_mode), vl::logic<1>(0x0ULL), vl::rd(stopstatetx_lane0))); }

void mipi_4_rx_dphy_ns_if::glue_gen_stopstaterx_lane0_sync_pulse_in() { vl::wr(c_gen_stopstaterx_lane0_sync_pulse_in, vl::mux(vl::rd(scan_mode), vl::logic<1>(0x0ULL), vl::rd(stopstaterx_lane0))); }

void mipi_4_rx_dphy_ns_if::glue_gen_ulpsactivenottx_lane0_sync_pulse_in() { vl::wr(c_gen_ulpsactivenottx_lane0_sync_pulse_in, vl::mux(vl::rd(scan_mode), vl::logic<1>(0x0ULL), vl::rd(ulpsactivenottx_lane0))); }

void mipi_4_rx_dphy_ns_if::glue_gen_ulpsactivenotrx_lane0_sync_pulse_in() { vl::wr(c_gen_ulpsactivenotrx_lane0_sync_pulse_in, vl::mux(vl::rd(scan_mode), vl::logic<1>(0x0ULL), vl::rd(ulpsactivenotrx_lane0))); }

void mipi_4_rx_dphy_ns_if::glue_I01dphytermcal_timertermcal() { vl::wr(c_I01dphytermcal_timertermcal, vl::logic<3>(0x2ULL)); }

void mipi_4_rx_dphy_ns_if::glue_I01_rcal_async_reset() { vl::wr(c_I01_rcal_async_reset, ~vl::rd(sresetz_cfg_clk_scanable)); }

void mipi_4_rx_dphy_ns_if::glue_I01_rcal_d_tx_term() { vl::wr(c_I01_rcal_d_tx_term, vl::logic<3>(0x0ULL)); }

void mipi_4_rx_dphy_ns_if::glue_I01dphytune_timebase() { vl::wr(c_I01dphytune_timebase, vl::cat(vl::logic<1>(0x0ULL), vl::rd(rx_timebase_if))); }

void mipi_4_rx_dphy_ns_if::glue_I01dphyhsrxoffscal_hsrxdouta() { vl::wr(c_I01dphyhsrxoffscal_hsrxdouta, vl::cat(vl::rd(rxclk_rxhs_clk_p_cal_if), vl::rd(rxclk_rxhs_clk_m_cal_if))); }

void mipi_4_rx_dphy_ns_if::glue_I00offset_process_core_slicer_deserializer_0() { vl::wr(c_I00offset_process_core_slicer_deserializer_0, vl::rd(rx0_rxhs_data_int).slice<4>(4)); }

void mipi_4_rx_dphy_ns_if::glue_I00offset_process_core_slicer_deserializer_1() { vl::wr(c_I00offset_process_core_slicer_deserializer_1, vl::rd(rx0_rxhs_data_int).slice<4>(0)); }

void mipi_4_rx_dphy_ns_if::glue_I00offset_process_core_cfgclkfreq() { vl::wr(c_I00offset_process_core_cfgclkfreq, vl::shr(vl::rd(rx_timebase_if), 2)); }

void mipi_4_rx_dphy_ns_if::glue_I01offset_process_core_slicer_deserializer_0() { vl::wr(c_I01offset_process_core_slicer_deserializer_0, vl::rd(rx1_rxhs_data_int).slice<4>(4)); }

void mipi_4_rx_dphy_ns_if::glue_I01offset_process_core_slicer_deserializer_1() { vl::wr(c_I01offset_process_core_slicer_deserializer_1, vl::rd(rx1_rxhs_data_int).slice<4>(0)); }

void mipi_4_rx_dphy_ns_if::glue_I01offset_process_core_cfgclkfreq() { vl::wr(c_I01offset_process_core_cfgclkfreq, vl::shr(vl::rd(rx_timebase_if), 2)); }

void mipi_4_rx_dphy_ns_if::glue_I02offset_process_core_slicer_deserializer_0() { vl::wr(c_I02offset_process_core_slicer_deserializer_0, vl::rd(rx2_rxhs_data_int).slice<4>(4)); }

void mipi_4_rx_dphy_ns_if::glue_I02offset_process_core_slicer_deserializer_1() { vl::wr(c_I02offset_process_core_slicer_deserializer_1, vl::rd(rx2_rxhs_data_int).slice<4>(0)); }

void mipi_4_rx_dphy_ns_if::glue_I02offset_process_core_cfgclkfreq() { vl::wr(c_I02offset_process_core_cfgclkfreq, vl::shr(vl::rd(rx_timebase_if), 2)); }

void mipi_4_rx_dphy_ns_if::glue_I03offset_process_core_slicer_deserializer_0() { vl::wr(c_I03offset_process_core_slicer_deserializer_0, vl::rd(rx3_rxhs_data_int).slice<4>(4)); }

void mipi_4_rx_dphy_ns_if::glue_I03offset_process_core_slicer_deserializer_1() { vl::wr(c_I03offset_process_core_slicer_deserializer_1, vl::rd(rx3_rxhs_data_int).slice<4>(0)); }

void mipi_4_rx_dphy_ns_if::glue_I03offset_process_core_cfgclkfreq() { vl::wr(c_I03offset_process_core_cfgclkfreq, vl::shr(vl::rd(rx_timebase_if), 2)); }

void mipi_4_rx_dphy_ns_if::glue_I01bist_machine_rst() { vl::wr(c_I01bist_machine_rst, ~vl::rd(sresetz_cfg_clk_scanable)); }

void mipi_4_rx_dphy_ns_if::glue_I01bist_machine_ch_error() { vl::wr(c_I01bist_machine_ch_error, vl::cat(vl::rd(rx3_pm_error_pm_if), vl::rd(rx2_pm_error_pm_if), vl::rd(rx1_pm_error_pm_if), vl::rd(rx0_pm_error_pm_if))); }

void mipi_4_rx_dphy_ns_if::split_I01bist_machine_ch_error_latch() {
    const vl::logic<4> v = vl::rd(c_I01bist_machine_ch_error_latch);
    vl::wr(rx0_error_latch_if, v.bit(0));
    vl::wr(rx1_error_latch_if, v.bit(1));
    vl::wr(rx2_error_latch_if, v.bit(2));
    vl::wr(rx3_error_latch_if, v.bit(3));
}

void mipi_4_rx_dphy_ns_if::glue_I01bist_machine_scanrst() { vl::wr(c_I01bist_machine_scanrst, ~vl::rd(scan_set_rst_n)); }

void mipi_4_rx_dphy_ns_if::proc_always_2833() {
    vl::logic<4> rx_cb_cal_repl_if_sh = vl::rd(rx_cb_cal_repl_if);
    if (vl::rd(cb_cal_repl_ovr_en).test()) {
        rx_cb_cal_repl_if_sh = vl::rd(cb_cal_repl_ovr);
        vl::wr(rx_cb_cal_repl_if, rx_cb_cal_repl_if_sh);
    } else if (vl::rd(noext_burnin_res_cal_int).test()) {
        rx_cb_cal_repl_if_sh = vl::rd(setr_burnin_int);
        vl::wr(rx_cb_cal_repl_if, rx_cb_cal_repl_if_sh);
    } else {
        rx_cb_cal_repl_if_sh = vl::rd(setr_termcal_rext);
        vl::wr(rx_cb_cal_repl_if, rx_cb_cal_repl_if_sh);
    }
}

void mipi_4_rx_dphy_ns_if::proc_always_2872() {
    if (vl::rd(sresetz_cfg_clk_scanable).truth_val() == 0) {
        vl::wr(stopstatedata_0_int, vl::logic<1>(0x0ULL));
    } else {
        vl::wr(stopstatedata_0_int, vl::mux(vl::rd(direction_lane0_sync), vl::rd(stopstaterx_lane0_sync), vl::rd(stopstatetx_lane0_sync)));
    }
}

void mipi_4_rx_dphy_ns_if::proc_always_2888() {
    if (vl::rd(sresetz_cfg_clk_scanable).truth_val() == 0) {
        vl::wr(ulpsactivenot_0_int, vl::logic<1>(0x0ULL));
    } else {
        vl::wr(ulpsactivenot_0_int, vl::mux(vl::rd(direction_lane0_sync), vl::rd(ulpsactivenotrx_lane0_sync), vl::rd(ulpsactivenottx_lane0_sync)));
    }
}

// -------------------OLDSTUFF---------------------------------------------
void mipi_4_rx_dphy_ns_if::proc_always_2930() {
    // 2MHz clock
    if (vl::rd(sresetz_cfg_clk_scanable).truth_val() == 0) {
        vl::wr(clk2m_cal, vl::logic<5>(0x0ULL));
    } else if (vl::eq(vl::rd(clk2m_cal), vl::rd(timebase_cal)).test()) {
        vl::wr(clk2m_cal, vl::logic<5>(0x0ULL));
    } else {
        vl::wr(clk2m_cal, vl::rd(clk2m_cal) + vl::logic<5>(0x1ULL));
    }
}

void mipi_4_rx_dphy_ns_if::proc_always_2939() {
    // 2MHz clock
    if (vl::rd(sresetz_cfg_clk_scanable).truth_val() == 0) {
        vl::wr(cal_clock, vl::logic<1>(0x0ULL));
    } else if (vl::eq(vl::rd(clk2m_cal), vl::rd(timebase_cal)).test()) {
        vl::wr(cal_clock, ~vl::rd(cal_clock));
    } else {
        vl::wr(cal_clock, vl::rd(cal_clock));
    }
}

// Dividing cfg_clk clock to keep rcal_adc clock below 42.5MHz (timing between machine and analog comparator)
void mipi_4_rx_dphy_ns_if::proc_always_2970() {
    if (vl::rd(sresetz_cfg_clk_scanable).truth_val() == 0) {
        vl::wr(cfg_clk_div2, vl::logic<1>(0x0ULL));
    } else {
        vl::wr(cfg_clk_div2, ~vl::rd(cfg_clk_div2));
    }
}

void mipi_4_rx_dphy_ns_if::proc_always_3001() {
    if (vl::rd(sresetz_cfg_clk_scanable).truth_val() == 0) {
        vl::wr(adc_done, vl::logic<1>(0x0ULL));
    } else if (vl::rd(adc_done_scan).test()) {
        vl::wr(adc_done, vl::logic<1>(0x1ULL));
    } else if (vl::rd(adc_done_clear_sync).test()) {
        vl::wr(adc_done, vl::logic<1>(0x0ULL));
    } else {
        vl::wr(adc_done, vl::rd(adc_done));
    }
}

void mipi_4_rx_dphy_ns_if::proc_always_3029() {
    vl::logic<3> rx_rxhs_setrx_if_sh = vl::rd(rx_rxhs_setrx_if);
    if (vl::rd(rx_rxhs_setr_ovr_en).test()) {
        rx_rxhs_setrx_if_sh = vl::rd(rx_rxhs_setr_ovr);
        vl::wr(rx_rxhs_setrx_if, rx_rxhs_setrx_if_sh);
    } else {
        rx_rxhs_setrx_if_sh = vl::mux(vl::eq(vl::rd(rx_cb_cal_repl_if).slice<3>(1), vl::logic<3>(0x7ULL)), vl::logic<3>(0x7ULL), vl::rd(rx_cb_cal_repl_if_sum).slice<3>(0));
        vl::wr(rx_rxhs_setrx_if, rx_rxhs_setrx_if_sh);
    }
}

void mipi_4_rx_dphy_ns_if::proc_always_3574() {
    vl::logic<1> tclk_miss_div2_sh = vl::rd(tclk_miss_div2);
    if (vl::le(vl::rd(rx_timebase_if), vl::logic<8>(0x88ULL)).test()) {
        tclk_miss_div2_sh = vl::logic<1>(0x0ULL);
        vl::wr(tclk_miss_div2, tclk_miss_div2_sh);
    } else {
        tclk_miss_div2_sh = vl::logic<1>(0x1ULL);
        vl::wr(tclk_miss_div2, tclk_miss_div2_sh);
    }
}

// PLL configuration using refclk at 24MHz
// HS clock = datarate / 2
// HS clock = fvco / pll_vco_cntrl[5:4]
// fvco = M / N * refclk
// M = ((pll_m / 2) + 1)*2 + pll_m[0]
// N = pll_n + 1
void mipi_4_rx_dphy_ns_if::proc_always_3590() {
    vl::logic<10> pll_m_int_sh = vl::rd(pll_m_int);
    {
        const vl::logic<7> sel = vl::rd(hsfreqrange_int);
        if (vl::case_eq(sel, vl::logic<7>(0x0ULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0x9eULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x10ULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0xb2ULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x20ULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0xc6ULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x30ULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0xdaULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x1ULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0xeeULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x11ULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0x102ULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x21ULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0x116ULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x31ULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0x12aULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x2ULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0x9eULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x12ULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0xa8ULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x22ULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0xb2ULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x32ULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0xbcULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x3ULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0xccULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x13ULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0xdaULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x23ULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0xeaULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x33ULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0xf8ULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x4ULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0x112ULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x14ULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0x12aULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x25ULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0xa2ULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x35ULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0xaeULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x5ULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0xc6ULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x16ULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0xe0ULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x26ULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0xf8ULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x37ULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0x112ULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x7ULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0x12aULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x18ULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0xa2ULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x28ULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0xaeULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x39ULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0xbaULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x9ULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0xc6ULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x19ULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0xd4ULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x29ULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0xe0ULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x3aULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0xecULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0xaULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0xf8ULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x1aULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0x106ULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x2aULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0x112ULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x3bULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0x11eULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0xbULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0x12aULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x1bULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0x138ULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x2bULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0x144ULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x3cULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0x150ULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0xcULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0x15cULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x1cULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0x16aULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x2cULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0x176ULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x3dULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0x182ULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0xdULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0x18eULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x1dULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0x19cULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x2eULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0x1a8ULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x3eULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0x1b4ULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0xeULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0x1c0ULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x1eULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0x1ceULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x2fULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0x1daULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x3fULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0x1e6ULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0xfULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0x1f2ULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x40ULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0x200ULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x41ULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0x20cULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x42ULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0x218ULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x43ULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0x224ULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x44ULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0x232ULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x45ULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0x23eULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x46ULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0x24aULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x47ULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0x256ULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x48ULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0x264ULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else if (vl::case_eq(sel, vl::logic<7>(0x49ULL))) {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0x270ULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        } else {
            pll_m_int_sh = vl::mux(vl::rd(pll_m_ovr_en), vl::rd(pll_m_ovr), vl::logic<10>(0x9eULL));
            vl::wr(pll_m_int, pll_m_int_sh);
        }
    }
}

// Reference clock set to 2MHz clock when refclk is 24MHz
void mipi_4_rx_dphy_ns_if::proc_always_3660() {
    vl::logic<6> pll_vco_cntrl_int_sh = vl::rd(pll_vco_cntrl_int);
    {
        const vl::logic<7> sel_3 = vl::rd(hsfreqrange_int);
        if (vl::case_eq(sel_3, vl::logic<7>(0x0ULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0x3fULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x10ULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0x3fULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x20ULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0x3fULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x30ULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0x3fULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x1ULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0x3fULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x11ULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0x3fULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x21ULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0x3fULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x31ULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0x37ULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x2ULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0x2fULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x12ULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0x2fULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x22ULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0x2fULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x32ULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0x2fULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x3ULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0x2fULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x13ULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0x2fULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x23ULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0x2fULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x33ULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0x2fULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x4ULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0x2fULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x14ULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0x27ULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x25ULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0x1fULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x35ULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0x1fULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x5ULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0x1fULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x16ULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0x1fULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x26ULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0x1fULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x37ULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0x1fULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x7ULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0x17ULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x18ULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0xfULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x28ULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0xfULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x39ULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0xfULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x9ULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0xfULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x19ULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0xfULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x29ULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0xfULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x3aULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0xfULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0xaULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0xfULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x1aULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0xfULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x2aULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0xfULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x3bULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0xfULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0xbULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0x7ULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x1bULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0x7ULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x2bULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0x7ULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x3cULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0x7ULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0xcULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0x7ULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x1cULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0x7ULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x2cULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0x7ULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x3dULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0x7ULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0xdULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0x7ULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x1dULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0x7ULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x2eULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0x7ULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x3eULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0x7ULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0xeULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0x7ULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x1eULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0x3ULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x2fULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0x3ULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x3fULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0x3ULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0xfULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0x3ULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x40ULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0x3ULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x41ULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0x3ULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x42ULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0x3ULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x43ULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0x3ULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x44ULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0x3ULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x45ULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0x3ULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x46ULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0x3ULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x47ULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0x3ULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x48ULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0x3ULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else if (vl::case_eq(sel_3, vl::logic<7>(0x49ULL))) {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0x3ULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        } else {
            pll_vco_cntrl_int_sh = vl::mux(vl::rd(pll_vco_cntrl_ovr_en), vl::rd(pll_vco_cntrl_ovr), vl::logic<6>(0x3fULL));
            vl::wr(pll_vco_cntrl_int, pll_vco_cntrl_int_sh);
        }
    }
}

void mipi_4_rx_dphy_ns_if::proc_always_3729() {
    vl::logic<7> hsrxthssettle_if_datalanes_sh = vl::rd(hsrxthssettle_if_datalanes);
    {
        const vl::logic<7> sel_5 = vl::rd(hsfreqrange_int);
        if (vl::case_eq(sel_5, vl::logic<7>(0x0ULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x3ULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x10ULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x3ULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x20ULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x3ULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x30ULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x4ULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x1ULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x4ULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x11ULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x5ULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x21ULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x6ULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x31ULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x6ULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x2ULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x7ULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x12ULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x7ULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x22ULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x8ULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x32ULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x8ULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x3ULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x9ULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x13ULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0xaULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x23ULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0xbULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x33ULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0xcULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x4ULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0xdULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x14ULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0xeULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x25ULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0xfULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x35ULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x11ULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x5ULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x14ULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x16ULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x16ULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x26ULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x18ULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x37ULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x1bULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x7ULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x1eULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x18ULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0xfULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x28ULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x10ULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x39ULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x11ULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x9ULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x13ULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x19ULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x14ULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x29ULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x16ULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x3aULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x17ULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0xaULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x18ULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x1aULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x19ULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x2aULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x1aULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x3bULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x1bULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0xbULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x1cULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x1bULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x1eULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x2bULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x1fULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x3cULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0xfULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0xcULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x10ULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x1cULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x11ULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x2cULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x12ULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x3dULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x13ULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0xdULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x13ULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x1dULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x14ULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x2eULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x15ULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x3eULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x15ULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0xeULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x16ULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x1eULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x17ULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x2fULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x17ULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x3fULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x17ULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0xfULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x17ULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x40ULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x18ULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x41ULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x18ULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x42ULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x19ULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x43ULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x19ULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x44ULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x1aULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x45ULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x1bULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x46ULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x1cULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x47ULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x1dULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x48ULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x1dULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else if (vl::case_eq(sel_5, vl::logic<7>(0x49ULL))) {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x1dULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        } else {
            hsrxthssettle_if_datalanes_sh = vl::mux(vl::rd(hsrxthssettle_ovr_en_datalanes), vl::rd(hsrxthssettle_datalanes_ovr), vl::logic<7>(0x3ULL));
            vl::wr(hsrxthssettle_if_datalanes, hsrxthssettle_if_datalanes_sh);
        }
    }
}

void mipi_4_rx_dphy_ns_if::proc_always_3798() {
    vl::logic<1> rx_rxhs_compatibility_mode_sh = vl::rd(rx_rxhs_compatibility_mode);
    {
        const vl::logic<7> sel_7 = vl::rd(hsfreqrange_int);
        if (vl::case_eq(sel_7, vl::logic<7>(0x0ULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x1ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x10ULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x1ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x20ULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x1ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x30ULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x1ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x1ULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x1ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x11ULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x1ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x21ULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x1ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x31ULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x1ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x2ULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x1ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x12ULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x1ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x22ULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x1ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x32ULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x1ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x3ULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x1ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x13ULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x1ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x23ULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x1ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x33ULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x1ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x4ULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x1ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x14ULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x1ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x25ULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x1ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x35ULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x1ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x5ULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x1ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x16ULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x1ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x26ULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x1ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x37ULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x1ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x7ULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x1ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x18ULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x1ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x28ULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x1ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x39ULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x1ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x9ULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x1ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x19ULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x1ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x29ULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x1ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x3aULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x1ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0xaULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x1ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x1aULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x1ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x2aULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x1ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x3bULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x1ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0xbULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x1ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x1bULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x1ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x2bULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x1ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x3cULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x1ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0xcULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x1ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x1cULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x1ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x2cULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x1ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x3dULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x0ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0xdULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x0ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x1dULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x0ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x2eULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x0ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x3eULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x0ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0xeULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x0ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x1eULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x0ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x2fULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x0ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x3fULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x0ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0xfULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x0ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x40ULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x0ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x41ULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x0ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x42ULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x0ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x43ULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x0ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x44ULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x0ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x45ULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x0ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x46ULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x0ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x47ULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x0ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x48ULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x0ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else if (vl::case_eq(sel_7, vl::logic<7>(0x49ULL))) {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x0ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        } else {
            rx_rxhs_compatibility_mode_sh = vl::mux(vl::rd(rx_rxhs_compatibility_mode_ovr_en), vl::rd(rx_rxhs_compatibility_mode_ovr), vl::logic<1>(0x1ULL));
            vl::wr(rx_rxhs_compatibility_mode, rx_rxhs_compatibility_mode_sh);
        }
    }
}

void mipi_4_rx_dphy_ns_if::proc_always_3866() {
    vl::logic<2> rx_rxhs_gmode_int_sh = vl::rd(rx_rxhs_gmode_int);
    {
        const vl::logic<7> sel_9 = vl::rd(hsfreqrange_int);
        if (vl::case_eq(sel_9, vl::logic<7>(0x0ULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x0ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x10ULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x0ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x20ULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x0ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x30ULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x0ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x1ULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x0ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x11ULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x0ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x21ULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x0ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x31ULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x0ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x2ULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x0ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x12ULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x0ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x22ULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x0ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x32ULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x0ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x3ULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x0ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x13ULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x0ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x23ULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x0ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x33ULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x0ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x4ULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x0ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x14ULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x0ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x25ULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x0ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x35ULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x0ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x5ULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x0ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x16ULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x0ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x26ULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x0ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x37ULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x1ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x7ULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x1ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x18ULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x1ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x28ULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x1ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x39ULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x1ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x9ULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x1ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x19ULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x1ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x29ULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x1ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x3aULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x1ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0xaULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x1ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x1aULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x2ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x2aULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x2ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x3bULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x2ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0xbULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x2ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x1bULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x2ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x2bULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x2ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x3cULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x2ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0xcULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x2ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x1cULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x2ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x2cULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x2ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x3dULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x3ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0xdULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x3ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x1dULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x3ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x2eULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x3ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x3eULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x3ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0xeULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x3ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x1eULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x3ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x2fULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x3ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x3fULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x3ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0xfULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x3ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x40ULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x3ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x41ULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x3ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x42ULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x3ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x43ULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x3ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x44ULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x3ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x45ULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x3ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x46ULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x3ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x47ULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x3ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x48ULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x3ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else if (vl::case_eq(sel_9, vl::logic<7>(0x49ULL))) {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x3ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        } else {
            rx_rxhs_gmode_int_sh = vl::mux(vl::rd(rx_rxhs_gmode_if_ovr_en), vl::rd(rx_rxhs_gmode_if_ovr), vl::logic<2>(0x0ULL));
            vl::wr(rx_rxhs_gmode_int, rx_rxhs_gmode_int_sh);
        }
    }
}

void mipi_4_rx_dphy_ns_if::proc_always_3937() {
    vl::logic<2> rxclk_rxhs_ddr_clk_div_int_sh = vl::rd(rxclk_rxhs_ddr_clk_div_int);
    {
        const vl::logic<7> sel_11 = vl::rd(hsfreqrange_int);
        if (vl::case_eq(sel_11, vl::logic<7>(0x0ULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x0ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x10ULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x0ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x20ULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x0ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x30ULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x0ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x1ULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x0ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x11ULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x0ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x21ULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x0ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x31ULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x0ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x2ULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x0ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x12ULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x0ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x22ULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x0ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x32ULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x0ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x3ULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x0ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x13ULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x0ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x23ULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x0ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x33ULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x0ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x4ULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x0ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x14ULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x0ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x25ULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x0ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x35ULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x0ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x5ULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x0ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x16ULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x0ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x26ULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x0ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x37ULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x0ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x7ULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x0ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x18ULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x1ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x28ULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x1ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x39ULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x1ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x9ULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x1ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x19ULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x1ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x29ULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x1ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x3aULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x1ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0xaULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x1ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x1aULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x1ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x2aULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x1ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x3bULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x1ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0xbULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x1ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x1bULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x1ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x2bULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x1ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x3cULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x2ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0xcULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x2ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x1cULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x2ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x2cULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x2ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x3dULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x2ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0xdULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x2ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x1dULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x2ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x2eULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x2ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x3eULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x2ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0xeULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x2ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x1eULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x2ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x2fULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x2ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x3fULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x2ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0xfULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x2ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x40ULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x2ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x41ULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x2ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x42ULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x2ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x43ULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x2ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x44ULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x2ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x45ULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x2ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x46ULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x2ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x47ULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x2ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x48ULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x2ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else if (vl::case_eq(sel_11, vl::logic<7>(0x49ULL))) {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x2ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        } else {
            rxclk_rxhs_ddr_clk_div_int_sh = vl::mux(vl::rd(rxclk_rxhs_ddr_clk_div_ovr_en), vl::rd(rxclk_rxhs_ddr_clk_div_ovr), vl::logic<2>(0x0ULL));
            vl::wr(rxclk_rxhs_ddr_clk_div_int, rxclk_rxhs_ddr_clk_div_int_sh);
        }
    }
}

// DDL
void mipi_4_rx_dphy_ns_if::proc_always_4010() {
    vl::logic<12> ddl_osc_freq_target_sh = vl::rd(ddl_osc_freq_target);
    if (vl::eq(vl::rd(deskew_legacyz), vl::logic<1>(0x0ULL)).test()) {
        ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x123ULL));
        vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
    } else {
        {
            const vl::logic<7> sel_13 = vl::rd(hsfreqrange_int);
            if (vl::case_eq(sel_13, vl::logic<7>(0x0ULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x10ULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x10ULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x12ULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x20ULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x14ULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x30ULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x16ULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x1ULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x18ULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x11ULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x1aULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x21ULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x1bULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x31ULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x1dULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x2ULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x1fULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x12ULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x21ULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x22ULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x23ULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x32ULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x25ULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x3ULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x28ULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x13ULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x2bULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x23ULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x2eULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x33ULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x30ULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x4ULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x35ULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x14ULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x3aULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x25ULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x3fULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x35ULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x43ULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x5ULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x4dULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x16ULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x56ULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x26ULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x5fULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x37ULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x69ULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x7ULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x72ULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x18ULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x7bULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x28ULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x84ULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x39ULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x8dULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x9ULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x96ULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x19ULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x9fULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x29ULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0xa8ULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x3aULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0xb1ULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0xaULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0xbaULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x1aULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0xc3ULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x2aULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0xccULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x3bULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0xd5ULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0xbULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0xdeULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x1bULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0xe6ULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x2bULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0xefULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x3cULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0xf8ULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0xcULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x100ULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x1cULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x109ULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x2cULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x111ULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x3dULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x11aULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0xdULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x123ULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x1dULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x12bULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x2eULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x133ULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x3eULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x13cULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0xeULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x144ULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x1eULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x14cULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x2fULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x155ULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x3fULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x15dULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0xfULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x165ULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x40ULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x16dULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x41ULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x176ULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x42ULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x17eULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x43ULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x186ULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x44ULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x18eULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x45ULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x196ULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x46ULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x19eULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x47ULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x1a6ULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x48ULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x1aeULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else if (vl::case_eq(sel_13, vl::logic<7>(0x49ULL))) {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x1b6ULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            } else {
                ddl_osc_freq_target_sh = vl::mux(vl::rd(ddl_osc_freq_target_ovr_en), vl::rd(ddl_osc_freq_target_ovr), vl::logic<12>(0x10ULL));
                vl::wr(ddl_osc_freq_target, ddl_osc_freq_target_sh);
            }
        }
    }
}

void mipi_4_rx_dphy_ns_if::proc_always_4961() {
    if (vl::rd(arstz_scanable).truth_val() == 0) {
        vl::wr(scantie, vl::logic<1>(0x0ULL));
    } else {
        vl::wr(scantie, vl::rd(scan_mode));
    }
}
