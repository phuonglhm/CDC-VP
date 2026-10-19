//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_offset_process_core
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_OFFSET_PROCESS_CORE_H
#define VL_MIPI_4_RX_DPHY_NS_OFFSET_PROCESS_CORE_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_4_rx_dphy_ns_gen_sync__reset_edge0.h"
#include "mipi_4_rx_dphy_ns_ones_counter.h"
#include "mipi_4_rx_dphy_ns_sar_guard_band.h"

// Parameters: RATIO=4, WORD_RATIO_ACC=16, DETECT_ONES_SIZE=3, CONTROL_WORD=6
class mipi_4_rx_dphy_ns_offset_process_core : public sc_core::sc_module {
public:
    // wordclock
    sc_core::sc_in<sc_dt::sc_lv<4>> slicer_deserializer_0{"slicer_deserializer_0"};
    sc_core::sc_in<sc_dt::sc_lv<4>> slicer_deserializer_1{"slicer_deserializer_1"};
    // Inputs /outputs
    sc_core::sc_in<sc_dt::sc_logic> wordclk{"wordclk"};
    sc_core::sc_in<sc_dt::sc_logic> resetz_wordclk{"resetz_wordclk"};
    sc_core::sc_in<sc_dt::sc_logic> resetz_hsclock{"resetz_hsclock"};
    sc_core::sc_in<sc_dt::sc_logic> start_calibration{"start_calibration"};
    sc_core::sc_in<sc_dt::sc_logic> hsclock{"hsclock"};
    sc_core::sc_out<sc_dt::sc_logic> cal_done{"cal_done"};
    sc_core::sc_out<sc_dt::sc_logic> no_convergence{"no_convergence"};
    sc_core::sc_out<sc_dt::sc_lv<6>> control_current_word{"control_current_word"};
    sc_core::sc_in<sc_dt::sc_lv<8>> cfgclkfreq{"cfgclkfreq"};
    sc_core::sc_in<sc_dt::sc_logic> cmp_polarity{"cmp_polarity"};
    // 6bits
    sc_core::sc_out<sc_dt::sc_lv<8>> total_ones{"total_ones"};
    sc_core::sc_in<sc_dt::sc_lv<8>> offset_comp_threshold{"offset_comp_threshold"};
    sc_core::sc_in<sc_dt::sc_lv<8>> tolerance{"tolerance"};
    sc_core::sc_in<sc_dt::sc_lv<3>> number_of_ready_count{"number_of_ready_count"};
    sc_core::sc_in<sc_dt::sc_logic> repeatcal{"repeatcal"};
    sc_core::sc_out<sc_dt::sc_logic> afe_offset_cal_en{"afe_offset_cal_en"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_offset_process_core);
    explicit mipi_4_rx_dphy_ns_offset_process_core(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    // Registers and wiring
    sc_core::sc_signal<sc_dt::sc_lv<8>> accumulator{"accumulator", vl::logic<8>().lv()};
    sc_core::sc_signal<sc_dt::sc_lv<6>> counter_ratio_1{"counter_ratio_1", vl::logic<6>().lv()};
    sc_core::sc_signal<sc_dt::sc_logic> store_5{"store_5", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_logic> data_ready_5{"data_ready_5", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_logic> flag_5{"flag_5", vl::logic<1>().sl()};
    sc_core::sc_signal_rv<3> detected_ones_slicer_deserializer_0{"detected_ones_slicer_deserializer_0", vl::logic<3>::all_z().lv()};
    sc_core::sc_signal_rv<3> detected_ones_slicer_deserializer_1{"detected_ones_slicer_deserializer_1", vl::logic<3>::all_z().lv()};
    sc_core::sc_signal_resolved global_done{"global_done", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved data_ready{"data_ready", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved data_ready_ack{"data_ready_ack", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved data_ready_at_clock_in{"data_ready_at_clock_in", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved store{"store", sc_dt::SC_LOGIC_Z};
    // counter that accumulates all the results from the previous ones
    sc_core::sc_signal<sc_dt::sc_lv<7>> ones_detector_all_added{"ones_detector_all_added", vl::logic<7>().lv()};
    sc_core::sc_signal_rv<7> ones_detector_complete_sum{"ones_detector_complete_sum", vl::logic<7>::all_z().lv()};
    sc_core::sc_signal_resolved enable_counter{"enable_counter", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved enable_counter_sync{"enable_counter_sync", sc_dt::SC_LOGIC_Z};
    sc_core::sc_vector<sc_core::sc_signal_resolved> done_ones_find{"done_ones_find", 2, vl::make_z1};
    std::unique_ptr<mipi_4_rx_dphy_ns_ones_counter> counter_slicer_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_ones_counter> counter_slicer_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_gen_sync__reset_edge0> gen_sync_enable_counter_sync;
    sc_core::sc_signal_resolved tie0_{"tie0_", sc_dt::SC_LOGIC_0};
    vl::tie tie0__drv{"tie0__drv", sc_dt::SC_LOGIC_0};
    sc_core::sc_signal_resolved nc_gen_sync_enable_counter_sync_data_out_dly{"nc_gen_sync_enable_counter_sync_data_out_dly", sc_dt::SC_LOGIC_Z};
    // gen sync in order to handle with the acknowledge
    std::unique_ptr<mipi_4_rx_dphy_ns_gen_sync__reset_edge0> gen_sync_data_ready_sync;
    sc_core::sc_signal_resolved nc_gen_sync_data_ready_sync_data_out_dly{"nc_gen_sync_data_ready_sync_data_out_dly", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_sar_guard_band> sar_guard_band_I01;

    vl::logic<2> rd_done_ones_find() const;
    void assign_ones_detector_complete_sum();
    void assign_global_done();
    void assign_data_ready();
    void assign_store();
    void proc_always_144();
    void proc_always_166();
    void proc_always_203();
    void proc_always_249();
    void proc_always_268();
};

#endif
