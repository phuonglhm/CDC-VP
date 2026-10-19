//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_dphyhsrxoffscal
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYHSRXOFFSCAL_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYHSRXOFFSCAL_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_4_rx_dphy_ns_gen_mux.h"
#include "mipi_4_rx_dphy_ns_gen_sync__reset_edge0.h"

// Parameters: IDLE=0, STARTCAL=1, RAMPDWN=2, RAMPUP=3, ENDCAL=4, WAIT_STATE=5, START_RAMPUP=6, ENDRAMP=7, INITVAL=8
class mipi_4_rx_dphy_ns_dphyhsrxoffscal : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> resetz{"resetz"};
    sc_core::sc_in<sc_dt::sc_logic> cfg_clk{"cfg_clk"};
    sc_core::sc_in<sc_dt::sc_logic> encal{"encal"};
    sc_core::sc_in<sc_dt::sc_lv<2>> hsrxdouta{"hsrxdouta"};
    sc_core::sc_in<sc_dt::sc_lv<6>> setoffs_ovr{"setoffs_ovr"};
    sc_core::sc_in<sc_dt::sc_logic> setoffs_ovr_en{"setoffs_ovr_en"};
    sc_core::sc_out<sc_dt::sc_logic> startcal{"startcal"};
    sc_core::sc_out<sc_dt::sc_lv<6>> setoffs{"setoffs"};
    sc_core::sc_out<sc_dt::sc_logic> caldone{"caldone"};
    sc_core::sc_out<sc_dt::sc_lv<4>> errcal{"errcal"};
    sc_core::sc_in<sc_dt::sc_logic> scanmode{"scanmode"};
    sc_core::sc_in<sc_dt::sc_logic> scanclk{"scanclk"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_dphyhsrxoffscal);
    explicit mipi_4_rx_dphy_ns_dphyhsrxoffscal(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000.0;  // time precision (fs)
    sc_core::sc_vector<sc_core::sc_signal_resolved> hsrxdouta_b{"hsrxdouta_b", 2, vl::make_z1};
    sc_core::sc_signal<sc_dt::sc_lv<3>> masksr{"masksr", vl::logic<3>().lv()};
    sc_core::sc_signal<sc_dt::sc_logic> rampupdwnz{"rampupdwnz", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_lv<6>> offstgglup0{"offstgglup0", vl::logic<6>().lv()};
    sc_core::sc_signal<sc_dt::sc_lv<6>> offstgglup1{"offstgglup1", vl::logic<6>().lv()};
    sc_core::sc_signal<sc_dt::sc_lv<6>> offstggldw0{"offstggldw0", vl::logic<6>().lv()};
    sc_core::sc_signal<sc_dt::sc_lv<6>> offstggldw1{"offstggldw1", vl::logic<6>().lv()};
    sc_core::sc_signal<sc_dt::sc_logic> hsrxsample0{"hsrxsample0", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_logic> hsrxsample1{"hsrxsample1", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_logic> hsrxsample0_d{"hsrxsample0_d", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_logic> hsrxsample1_d{"hsrxsample1_d", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_lv<6>> setoffs_av{"setoffs_av", vl::logic<6>().lv()};
    sc_core::sc_signal<sc_dt::sc_lv<4>> state{"state", vl::logic<4>().lv()};
    sc_core::sc_signal<sc_dt::sc_lv<6>> setoffs_int{"setoffs_int", vl::logic<6>().lv()};
    sc_core::sc_signal<sc_dt::sc_logic> calrdy{"calrdy", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_logic> calrdy_d{"calrdy_d", vl::logic<1>().sl()};
    sc_core::sc_signal_resolved cfg_clk_scanable{"cfg_clk_scanable", sc_dt::SC_LOGIC_Z};
    // wire           arstz_scanable            ;
    // wire           syncrstz_cfg_clk          ;
    sc_core::sc_signal_resolved sresetz_cfg_clk{"sresetz_cfg_clk", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<2> hsrxdouta_sync2{"hsrxdouta_sync2", vl::logic<2>::all_z().lv()};
    sc_core::sc_signal<sc_dt::sc_lv<2>> cfg_clk_counter{"cfg_clk_counter", vl::logic<2>().lv()};
    sc_core::sc_signal_resolved cfg_clk_div4_scanable{"cfg_clk_div4_scanable", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<6> setoffs_sign_adjust{"setoffs_sign_adjust", vl::logic<6>::all_z().lv()};
    sc_core::sc_signal_rv<8> aux_sum{"aux_sum", vl::logic<8>::all_z().lv()};
    // TOP fix hold violation seen in verilog simulation
    sc_core::sc_vector<sc_core::sc_signal_resolved> hsrxdouta_sync2_w{"hsrxdouta_sync2_w", 2, vl::make_z1};
    sc_core::sc_event assign_hsrxdouta_sync2_ev;
    vl::logic<2> assign_hsrxdouta_sync2_pv = vl::logic<2>::all_z();
    bool assign_hsrxdouta_sync2_ev_pend = false;
    // assign cfg_clk_div4_scanable = scanmode ? scanclk : cfg_clk_counter[1] ;
    std::unique_ptr<mipi_4_rx_dphy_ns_gen_mux> cfg_clk_div4_scanmux;
    sc_core::sc_signal_resolved c_cfg_clk_div4_scanmux_d0{"c_cfg_clk_div4_scanmux_d0", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_gen_sync__reset_edge0> hand_hsrxdouta0_sync;
    sc_core::sc_signal_resolved nc_hand_hsrxdouta0_sync_data_out_dly{"nc_hand_hsrxdouta0_sync_data_out_dly", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_gen_sync__reset_edge0> hand_hsrxdouta1_sync;
    sc_core::sc_signal_resolved nc_hand_hsrxdouta1_sync_data_out_dly{"nc_hand_hsrxdouta1_sync_data_out_dly", sc_dt::SC_LOGIC_Z};

    void split_hsrxdouta();
    vl::logic<2> rd_hsrxdouta() const;
    vl::logic<2> rd_hsrxdouta_sync2_w() const;
    void assign_cfg_clk_scanable();
    void assign_sresetz_cfg_clk();
    void assign_setoffs();
    void assign_setoffs_sign_adjust();
    void assign_hsrxdouta_sync2();
    void assign_hsrxdouta_sync2_upd();
    void assign_aux_sum();
    void glue_cfg_clk_div4_scanmux_d0();
    void proc_always_134();
    void proc_always_159();
    void proc_always_196();
    void proc_always_206();
    void proc_always_216();
    void proc_always_227();
    void proc_always_258();
    void proc_always_275();
    void proc_always_307();
    void proc_always_353();
    void proc_always_360();
    void proc_always_367();
    void proc_always_373();
    void proc_always_379();
    void proc_always_388();
    void proc_always_397();
    void proc_always_406();
    void proc_always_421();
    void proc_always_443();
};

#endif
