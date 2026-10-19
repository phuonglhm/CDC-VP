//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_bist_machine_rx
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_BIST_MACHINE_RX_H
#define VL_MIPI_4_RX_DPHY_NS_BIST_MACHINE_RX_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_4_rx_dphy_ns_dynamic_pulse_catcher.h"
#include "mipi_4_rx_dphy_ns_gen_mux.h"
#include "mipi_4_rx_dphy_ns_gen_sync__reset_edge1.h"
#include "mipi_4_rx_dphy_ns_gen_sync_bus__reset_edge1_bus_width4.h"
#include "mipi_4_rx_dphy_ns_static_pulse_catcher__reset_edge1.h"

// Parameters: IDLE=0, WAIT4PLL=1, EN_BIST=2, WAIT4LOOP=3, SYNC_BIST=4, WAIT4STROBE1=5, WAIT4STROBE2=6, FINISH=7, CLKOFF_SYN=8, SER_DATA=9, RELEASE_DATA=10, RELEASE_CLOCK=11, SER_PIPE=12, PRP_COUNT=13, DATA_SER_DEL=14, NSER_TX=1, NLANES_RX=4
class mipi_4_rx_dphy_ns_bist_machine_rx : public sc_core::sc_module {
public:
    // the number of deserializers used /equivalent to the number of lanes
    // -- INPUT AND OUTPUT PORTS  --------------------------------------------------
    sc_core::sc_in<sc_dt::sc_logic> clk{"clk"};
    sc_core::sc_in<sc_dt::sc_logic> rst{"rst"};
    sc_core::sc_in<sc_dt::sc_logic> bist_en{"bist_en"};
    sc_core::sc_in<sc_dt::sc_logic> fsm_done{"fsm_done"};
    // output          bistpath_en     ;
    sc_core::sc_out<sc_dt::sc_logic> onpll{"onpll"};
    sc_core::sc_out<sc_dt::sc_logic> bist_sync_en{"bist_sync_en"};
    sc_core::sc_in<sc_dt::sc_logic> ch_pat_strobe{"ch_pat_strobe"};
    // number of serializers used in loopback
    sc_core::sc_in<sc_dt::sc_logic> comp_after_strobes{"comp_after_strobes"};
    // input           ch0_pat_strobe  ;
    // input           ch1_pat_strobe  ;
    // input           ch2_pat_strobe  ;
    sc_core::sc_in<sc_dt::sc_lv<4>> ch_error{"ch_error"};
    // one error signal per lane tested per lane tested in BIST
    // input           ch0_error       ;
    // input           ch1_error       ;
    // input           ch2_error       ;
    sc_core::sc_out<sc_dt::sc_lv<4>> ch_error_latch{"ch_error_latch"};
    // one latch error signal per lane tested in BIST
    // output          ch0_error_latch ;
    // output          ch1_error_latch ;
    // output          ch2_error_latch ;
    sc_core::sc_in<sc_dt::sc_lv<4>> ch_bist_run{"ch_bist_run"};
    // what lanes to be considered while performing the BIST
    sc_core::sc_out<sc_dt::sc_logic> error_gating_out{"error_gating_out"};
    sc_core::sc_out<sc_dt::sc_logic> bist_done{"bist_done"};
    sc_core::sc_out<sc_dt::sc_logic> bist_ok{"bist_ok"};
    // serializer control
    sc_core::sc_out<sc_dt::sc_logic> clk_en_clklane{"clk_en_clklane"};
    sc_core::sc_out<sc_dt::sc_logic> div_en_clklane{"div_en_clklane"};
    sc_core::sc_out<sc_dt::sc_logic> serial_clk_en_clklane{"serial_clk_en_clklane"};
    sc_core::sc_out<sc_dt::sc_logic> serial_en_clklane{"serial_en_clklane"};
    sc_core::sc_out<sc_dt::sc_logic> clksel_en{"clksel_en"};
    sc_core::sc_out<sc_dt::sc_logic> clk_en_datalane{"clk_en_datalane"};
    sc_core::sc_out<sc_dt::sc_logic> div_en_datalane{"div_en_datalane"};
    sc_core::sc_out<sc_dt::sc_logic> serial_data_en_datalane{"serial_data_en_datalane"};
    sc_core::sc_out<sc_dt::sc_logic> serial_en_datalane{"serial_en_datalane"};
    // delay before turning on serialzer for datalane
    sc_core::sc_in<sc_dt::sc_lv<8>> del_datalane{"del_datalane"};
    sc_core::sc_out<sc_dt::sc_logic> trigger_hsdirect{"trigger_hsdirect"};
    sc_core::sc_out<sc_dt::sc_logic> switch_on_lb_mux{"switch_on_lb_mux"};
    // allows to switch on the lb mux functionality
    sc_core::sc_out<sc_dt::sc_logic> feed_int_in_bist{"feed_int_in_bist"};
    // state override
    sc_core::sc_in<sc_dt::sc_logic> force_state{"force_state"};
    sc_core::sc_in<sc_dt::sc_lv<4>> target_state{"target_state"};
    sc_core::sc_out<sc_dt::sc_lv<4>> bist_state{"bist_state"};
    sc_core::sc_in<sc_dt::sc_lv<8>> pipe_clean_time{"pipe_clean_time"};
    sc_core::sc_in<sc_dt::sc_logic> scanmode{"scanmode"};
    sc_core::sc_in<sc_dt::sc_logic> scanclk{"scanclk"};
    sc_core::sc_in<sc_dt::sc_logic> scanrst{"scanrst"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_bist_machine_rx);
    explicit mipi_4_rx_dphy_ns_bist_machine_rx(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000.0;  // time precision (fs)
    sc_core::sc_vector<sc_core::sc_signal_resolved> ch_error_latch_b{"ch_error_latch_b", 4, vl::make_z1};
    // -- WIRES DECLARATION --------------------------------------------------------
    sc_core::sc_vector<sc_core::sc_signal_resolved> bist_en_sync{"bist_en_sync", 2, vl::make_z1};
    sc_core::sc_signal_resolved clk_scanable{"clk_scanable", sc_dt::SC_LOGIC_Z};
    // wire            strobe_rst_scanable     ;
    // reg     [2:0]   pg_pat_strobe_latch     ;
    // wire            strobe_latch0_sync      ;
    // wire            strobe_latch1_sync      ;
    // wire            strobe_latch2_sync      ;
    // reg     [2:0]   sync_strobes            ;
    sc_core::sc_signal<sc_dt::sc_logic> sync_strobes{"sync_strobes", vl::logic<1>().sl()};
    // register to save the received strobes from the pattern generator
    sc_core::sc_signal_resolved master_pat_strobe{"master_pat_strobe", sc_dt::SC_LOGIC_Z};
    // wire    [2:0]   error_strobe            ;
    sc_core::sc_signal_resolved error_rst_scanable{"error_rst_scanable", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal<sc_dt::sc_lv<8>> counter{"counter", vl::logic<8>().lv()};
    sc_core::sc_signal<sc_dt::sc_lv<2>> error_gating{"error_gating", vl::logic<2>().lv()};
    // wire            ch0_pat_strobe_scan     ;
    // wire            ch1_pat_strobe_scan     ;
    // wire            ch2_pat_strobe_scan     ;
    sc_core::sc_signal_resolved ch_pat_strobe_scanable{"ch_pat_strobe_scanable", sc_dt::SC_LOGIC_Z};
    // wire            ch0_pat_strobe_scanable ;
    // wire            ch1_pat_strobe_scanable ;
    // wire            ch2_pat_strobe_scanable ;
    sc_core::sc_signal_resolved ch_pat_strobe_pulse{"ch_pat_strobe_pulse", sc_dt::SC_LOGIC_Z};
    // wire            ch0_pat_strobe_pulse    ;
    // wire            ch1_pat_strobe_pulse    ;
    // wire            ch2_pat_strobe_pulse    ;
    sc_core::sc_vector<sc_core::sc_signal_resolved> ch_error_scanable{"ch_error_scanable", 4, vl::make_z1};
    // wire            ch0_error_scanable      ;
    // wire            ch1_error_scanable      ;
    // wire            ch2_error_scanable      ;
    sc_core::sc_vector<sc_core::sc_signal_resolved> ch_error_pulse{"ch_error_pulse", 4, vl::make_z1};
    // wire            ch0_error_pulse         ;
    // wire            ch1_error_pulse         ;
    // wire            ch2_error_pulse         ;
    sc_core::sc_signal<sc_dt::sc_logic> error_rst{"error_rst", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_lv<6>> ser_shift_boot_seq{"ser_shift_boot_seq", vl::logic<6>().lv()};
    sc_core::sc_signal<sc_dt::sc_lv<2>> ser_shift_boot_seq_datalane{"ser_shift_boot_seq_datalane", vl::logic<2>().lv()};
    sc_core::sc_signal<sc_dt::sc_logic> ser_boot_done{"ser_boot_done", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_logic> trigger_pg{"trigger_pg", vl::logic<1>().sl()};
    // switch it on when the pll has reached lock state
    // state override capabilities
    sc_core::sc_signal_resolved force_state_sync{"force_state_sync", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<4> target_state_sync{"target_state_sync", vl::logic<4>::all_z().lv()};
    // wiring for N lanes
    // -- WIRING LOGIC -------------------------------------------------------------
    std::unique_ptr<mipi_4_rx_dphy_ns_gen_sync__reset_edge1> bist_enable_sync;
    sc_core::sc_signal_resolved tie0_{"tie0_", sc_dt::SC_LOGIC_0};
    vl::tie tie0__drv{"tie0__drv", sc_dt::SC_LOGIC_0};
    std::unique_ptr<mipi_4_rx_dphy_ns_gen_mux> hand_clk_scan_mux;
    std::unique_ptr<mipi_4_rx_dphy_ns_gen_mux> hand_error_rst_mux;
    sc_core::sc_signal_resolved c_hand_error_rst_mux_d1{"c_hand_error_rst_mux_d1", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_gen_sync__reset_edge1> gen_force_state_sync;
    sc_core::sc_signal_resolved nc_gen_force_state_sync_data_out_dly{"nc_gen_force_state_sync_data_out_dly", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_gen_sync_bus__reset_edge1_bus_width4> target_state_sync_bus;
    std::unique_ptr<mipi_4_rx_dphy_ns_gen_mux> GEN_MUX_CREATE_0_hand_ch_pat_strobe_mux;
    std::unique_ptr<mipi_4_rx_dphy_ns_dynamic_pulse_catcher> DYNAMIC_PULSE_CREATE_0_hand_pat_strobe_pcatch;
    std::unique_ptr<mipi_4_rx_dphy_ns_static_pulse_catcher__reset_edge1> STATIC_PULSE_CREATE_0_hand_ch_error_pcatch;
    std::unique_ptr<mipi_4_rx_dphy_ns_static_pulse_catcher__reset_edge1> STATIC_PULSE_CREATE_1_hand_ch_error_pcatch;
    std::unique_ptr<mipi_4_rx_dphy_ns_static_pulse_catcher__reset_edge1> STATIC_PULSE_CREATE_2_hand_ch_error_pcatch;
    std::unique_ptr<mipi_4_rx_dphy_ns_static_pulse_catcher__reset_edge1> STATIC_PULSE_CREATE_3_hand_ch_error_pcatch;

    void merge_ch_error_latch();
    vl::logic<2> rd_bist_en_sync() const;
    vl::logic<4> rd_ch_error_latch() const;
    vl::logic<4> rd_ch_error_pulse() const;
    vl::logic<4> rd_ch_error_scanable() const;
    void assign_master_pat_strobe();
    void assign_error_gating_out();
    void assign_ch_error_scanable();
    void assign_switch_on_lb_mux();
    void assign_feed_int_in_bist();
    void assign_trigger_hsdirect();
    void assign_clk_en_clklane();
    void assign_div_en_clklane();
    void assign_clksel_en();
    void assign_serial_clk_en_clklane();
    void assign_serial_en_clklane();
    void assign_clk_en_datalane();
    void assign_div_en_datalane();
    void assign_serial_data_en_datalane();
    void assign_serial_en_datalane();
    void glue_hand_error_rst_mux_d1();
    void proc_always_279();
    void proc_always_287();
    void proc_always_377();
    void proc_always_450();
    void proc_always_471();
    void proc_always_525();
    void proc_always_535();
    void proc_always_551();
    void proc_always_574();
    void proc_always_581();
    void proc_always_593();
    void proc_always_607();
    void proc_always_630();
};

#endif
