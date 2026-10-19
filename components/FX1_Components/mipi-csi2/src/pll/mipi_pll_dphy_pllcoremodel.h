//--------------------------------------------------------------------------------------------------
// module mipi_pll_dphy_pllcoremodel
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_PLL_DPHY_PLLCOREMODEL_H
#define VL_MIPI_PLL_DPHY_PLLCOREMODEL_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"

// Parameters: NPHASES=8, FBKDIV=50, INDIV=2, OUTDIV=1, PHASESEL=5, VCO_MIN=1.8, VCO_MAX=0.142, SSC_DEPTH=0.01, SSC_FREQ=100.0, NPOINTS=10, VCO_JITTER=0.005, VCO_ADJUST=4e-05, LOOP_BW=5.0
class mipi_pll_dphy_pllcoremodel : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> on{"on"};
    sc_core::sc_in<sc_dt::sc_logic> clkin{"clkin"};
    sc_core::sc_in<sc_dt::sc_lv<64>> feedbackdiv{"feedbackdiv"};
    sc_core::sc_in<sc_dt::sc_lv<64>> inputdiv{"inputdiv"};
    sc_core::sc_in<sc_dt::sc_lv<64>> outputdiv{"outputdiv"};
    sc_core::sc_in<sc_dt::sc_logic> bypass{"bypass"};
    sc_core::sc_in<sc_dt::sc_logic> deskewen{"deskewen"};
    sc_core::sc_in<sc_dt::sc_logic> deskewin{"deskewin"};
    sc_core::sc_in<sc_dt::sc_lv<5>> phsel{"phsel"};
    sc_core::sc_in<sc_dt::sc_logic> enable_ssc{"enable_ssc"};
    sc_core::sc_in<sc_dt::sc_logic> center_downz{"center_downz"};
    sc_core::sc_in<sc_dt::sc_lv<64>> ssc_depth{"ssc_depth"};
    sc_core::sc_in<sc_dt::sc_lv<64>> ssc_freq{"ssc_freq"};
    sc_core::sc_out<sc_dt::sc_logic> lock{"lock"};
    sc_core::sc_out<sc_dt::sc_logic> refclk{"refclk"};
    sc_core::sc_out<sc_dt::sc_logic> fbclk{"fbclk"};
    sc_core::sc_out<sc_dt::sc_lv<8>> clkout{"clkout"};
    sc_core::sc_out<sc_dt::sc_logic> clkout_div{"clkout_div"};

    SC_HAS_PROCESS(mipi_pll_dphy_pllcoremodel);
    explicit mipi_pll_dphy_pllcoremodel(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1.0;  // time precision (fs)
    sc_core::sc_signal<sc_dt::sc_logic, sc_core::SC_UNCHECKED_WRITERS> watchdog{"watchdog", vl::logic<1>().sl()};
    sc_core::sc_signal<double, sc_core::SC_UNCHECKED_WRITERS> refclk_lastedge{"refclk_lastedge", 0.0};
    sc_core::sc_signal<double, sc_core::SC_UNCHECKED_WRITERS> this_ref_per{"this_ref_per", 0.0};
    sc_core::sc_signal<sc_dt::sc_logic> up{"up", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_logic> dw{"dw", vl::logic<1>().sl()};
    sc_core::sc_signal<double, sc_core::SC_UNCHECKED_WRITERS> half_period{"half_period", 0.0};
    sc_core::sc_signal<sc_dt::sc_logic> pfd_reset{"pfd_reset", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_logic> phase_error_sign{"phase_error_sign", vl::logic<1>().sl()};
    sc_core::sc_signal_resolved phase_error_pulse{"phase_error_pulse", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal<double, sc_core::SC_UNCHECKED_WRITERS> start_error{"start_error", 0.0};
    sc_core::sc_signal<double, sc_core::SC_UNCHECKED_WRITERS> phase_error{"phase_error", 0.0};
    sc_core::sc_signal<double, sc_core::SC_UNCHECKED_WRITERS> rl_fbkdiv{"rl_fbkdiv", 0.0};
    sc_core::sc_signal<double, sc_core::SC_UNCHECKED_WRITERS> rl_indiv{"rl_indiv", 0.0};
    sc_core::sc_signal<double, sc_core::SC_UNCHECKED_WRITERS> rl_outdiv{"rl_outdiv", 0.0};
    sc_core::sc_signal<sc_dt::sc_lv<6>> lock_counter{"lock_counter", vl::logic<6>().lv()};
    sc_core::sc_signal<sc_dt::sc_logic, sc_core::SC_UNCHECKED_WRITERS> vcoclk_nossc{"vcoclk_nossc", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_logic, sc_core::SC_UNCHECKED_WRITERS> vcoclkdiv{"vcoclkdiv", vl::logic<1>().sl()};
    vl::logic<32> phcount;
    sc_core::sc_signal<sc_dt::sc_logic> vcoclkdiv_phase{"vcoclkdiv_phase", vl::logic<1>().sl()};
    sc_core::sc_signal_resolved jitter_on{"jitter_on", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal<double, sc_core::SC_UNCHECKED_WRITERS> rl_sscdepth{"rl_sscdepth", 0.0};
    sc_core::sc_signal<double, sc_core::SC_UNCHECKED_WRITERS> rl_sscfreq{"rl_sscfreq", 0.0};
    sc_core::sc_signal<sc_dt::sc_lv<10>> triwaveform{"triwaveform", vl::logic<10>().lv()};
    sc_core::sc_signal<sc_dt::sc_lv<2>> up_wave{"up_wave", vl::logic<2>().lv()};
    sc_core::sc_signal<sc_dt::sc_logic, sc_core::SC_UNCHECKED_WRITERS> triwaveclk{"triwaveclk", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_logic, sc_core::SC_UNCHECKED_WRITERS> sscclk{"sscclk", vl::logic<1>().sl()};
    sc_core::sc_signal<double, sc_core::SC_UNCHECKED_WRITERS> slope{"slope", 0.0};
    sc_core::sc_signal<double, sc_core::SC_UNCHECKED_WRITERS> ycross{"ycross", 0.0};
    sc_core::sc_signal<double, sc_core::SC_UNCHECKED_WRITERS> ssc_amount{"ssc_amount", 0.0};
    sc_core::sc_signal<double, sc_core::SC_UNCHECKED_WRITERS> ssc_hperiod{"ssc_hperiod", 0.0};
    sc_core::sc_signal_resolved vcoclk{"vcoclk", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved clok_net{"clok_net", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal<double, sc_core::SC_UNCHECKED_WRITERS> phase_error_pos{"phase_error_pos", 0.0};
    vl::vreg<8> clkout_r{"clkout_r", vl::logic<8>().lv()};   // value of output clkout
    sc_core::sc_signal<sc_dt::sc_logic, sc_core::SC_UNCHECKED_WRITERS> fbclk_r{"fbclk_r", vl::logic<1>().sl()};   // value of output fbclk
    sc_core::sc_signal<sc_dt::sc_logic, sc_core::SC_UNCHECKED_WRITERS> lock_r{"lock_r", vl::logic<1>().sl()};   // value of output lock
    sc_core::sc_signal<sc_dt::sc_logic, sc_core::SC_UNCHECKED_WRITERS> refclk_r{"refclk_r", vl::logic<1>().sl()};   // value of output refclk
    vl::action_queue aq_{"aq_"};

    void drive_clkout();
    void drive_fbclk();
    void drive_lock();
    void drive_refclk();
    double abs(double value);
    void assign_phase_error_pulse();
    void assign_jitter_on();
    void assign_vcoclk();
    void assign_clkout_div();
    void assign_clok_net();
    void proc_initial_10699();
    void proc_always_10742();
    void proc_always_10749();
    void proc_always_10756();
    void proc_always_10763();
    void proc_always_10768();
    void proc_always_10784();
    void proc_always_10795();
    void proc_always_10805();
    void proc_always_10808();
    void proc_always_10814();
    void proc_always_10821();
    void proc_always_10849();
    void proc_always_10870();
    void proc_initial_10903();
    void proc_initial_10911();
    void proc_initial_10931();
    void proc_initial_10952();
    void proc_always_10969();
    void proc_initial_10977();
    void proc_initial_10979();
    void proc_always_10991();
    void proc_always_10996();
    void proc_always_10998();
    void proc_always_11012();
    void proc_initial_11023();
    void proc_initial_11024();
    void proc_always_11039();
    void proc_always_11051();
    void proc_always_11062();
    void proc_initial_11079();
    void proc_initial_11080();
};

#endif
