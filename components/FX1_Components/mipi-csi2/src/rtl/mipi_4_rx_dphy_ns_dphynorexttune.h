//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_dphynorexttune
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYNOREXTTUNE_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYNOREXTTUNE_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_4_rx_dphy_ns_gen_sync__reset_edge0.h"
#include "mipi_4_rx_dphy_ns_gen_sync_bus__reset_edge0_bus_width10.h"

// Parameters: TIME_BASE=9, COUNTER_SIZE=10, IDLE=0, CONFIGURE_REGISTERS=1, WAIT_STABILITY_TIME=2, COUNT=3, STORE=4, RST_COUNTERS=5, CHECK_COUNT_COMPLETE=6, ANALYZE=7
class mipi_4_rx_dphy_ns_dphynorexttune : public sc_core::sc_module {
public:
    // to count posedge of 150 MHz clock in a window of 4 us it is needed 600 counting steps.
    // Port declarations
    sc_core::sc_in<sc_dt::sc_logic> cfg_clk{"cfg_clk"};
    sc_core::sc_in<sc_dt::sc_logic> rstz{"rstz"};
    sc_core::sc_in<sc_dt::sc_logic> osc_in{"osc_in"};
    sc_core::sc_in<sc_dt::sc_logic> resetz_osc_in{"resetz_osc_in"};
    sc_core::sc_in<sc_dt::sc_lv<9>> timebase{"timebase"};
    sc_core::sc_in<sc_dt::sc_lv<4>> setr_max{"setr_max"};
    sc_core::sc_in<sc_dt::sc_lv<4>> setr_typ{"setr_typ"};
    sc_core::sc_in<sc_dt::sc_lv<4>> setr_min{"setr_min"};
    sc_core::sc_in<sc_dt::sc_lv<10>> count_ref{"count_ref"};
    sc_core::sc_in<sc_dt::sc_lv<9>> stability_time{"stability_time"};
    // After changing osc_en, counter_osc needs to wait stability_time before start counting
    sc_core::sc_in<sc_dt::sc_logic> start_tune{"start_tune"};
    sc_core::sc_out<sc_dt::sc_logic> osc_pon{"osc_pon"};
    sc_core::sc_out<sc_dt::sc_logic> osc_en{"osc_en"};
    sc_core::sc_out<sc_dt::sc_lv<4>> setr{"setr"};
    sc_core::sc_out<sc_dt::sc_logic> tune_done{"tune_done"};
    sc_core::sc_out<sc_dt::sc_lv<10>> count_osc0{"count_osc0"};
    sc_core::sc_out<sc_dt::sc_lv<10>> count_osc1{"count_osc1"};
    sc_core::sc_out<sc_dt::sc_logic> noext_burn_error{"noext_burn_error"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_dphynorexttune);
    explicit mipi_4_rx_dphy_ns_dphynorexttune(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000.0;  // time precision (fs)
    // Internal Registers and wires
    sc_core::sc_signal<sc_dt::sc_lv<10>> counter_osc{"counter_osc", vl::logic<10>().lv()};
    sc_core::sc_signal<sc_dt::sc_lv<10>> shadow_count_osc{"shadow_count_osc", vl::logic<10>().lv()};
    sc_core::sc_signal_rv<10> counter_cfg{"counter_cfg", vl::logic<10>::all_z().lv()};
    sc_core::sc_signal<sc_dt::sc_lv<9>> count_time{"count_time", vl::logic<9>().lv()};
    sc_core::sc_signal<sc_dt::sc_lv<3>> fsm_state{"fsm_state", vl::logic<3>().lv()};
    sc_core::sc_signal<sc_dt::sc_lv<3>> fsm_state_osc{"fsm_state_osc", vl::logic<3>().lv()};
    sc_core::sc_signal<sc_dt::sc_logic> rst_done{"rst_done", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_logic> sample_count{"sample_count", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_logic> count_done{"count_done", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_lv<2>> start_tune_samp{"start_tune_samp", vl::logic<2>().lv()};
    sc_core::sc_signal<sc_dt::sc_logic> start_tune_edge{"start_tune_edge", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_logic> osc_stable{"osc_stable", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_logic> store_complete{"store_complete", vl::logic<1>().sl()};
    sc_core::sc_signal_resolved sample_count_osc{"sample_count_osc", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved sample_count_cfg{"sample_count_cfg", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved rst_done_osc{"rst_done_osc", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved osc_stable_osc{"osc_stable_osc", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_gen_sync__reset_edge0> sample_count_osc_sync;
    sc_core::sc_signal_resolved tie0_{"tie0_", sc_dt::SC_LOGIC_0};
    vl::tie tie0__drv{"tie0__drv", sc_dt::SC_LOGIC_0};
    sc_core::sc_signal_resolved nc_sample_count_osc_sync_data_out_dly{"nc_sample_count_osc_sync_data_out_dly", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_gen_sync__reset_edge0> sample_count_cfg_sync;
    sc_core::sc_signal_resolved nc_sample_count_cfg_sync_data_out_dly{"nc_sample_count_cfg_sync_data_out_dly", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_gen_sync__reset_edge0> osc_stable_sync;
    sc_core::sc_signal_resolved nc_osc_stable_sync_data_out_dly{"nc_osc_stable_sync_data_out_dly", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_gen_sync__reset_edge0> reset_done_sync;
    sc_core::sc_signal_resolved nc_reset_done_sync_data_out_dly{"nc_reset_done_sync_data_out_dly", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_gen_sync_bus__reset_edge0_bus_width10> counter_cfg_bus_sync;

    void proc_always_128();
    void proc_always_138();
    void proc_always_188();
    void proc_always_269();
    void proc_always_304();
    void proc_always_324();
    void proc_always_355();
    void proc_always_389();
    void proc_always_408();
    void proc_always_429();
    void proc_always_449();
    void proc_always_470();
    void proc_always_491();
    void proc_always_514();
    void proc_always_531();
    void proc_always_551();
    void proc_always_568();
    void proc_always_588();
    void proc_always_612();
};

#endif
