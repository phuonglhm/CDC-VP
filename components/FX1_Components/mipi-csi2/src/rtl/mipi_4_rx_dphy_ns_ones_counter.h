//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_ones_counter
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_ONES_COUNTER_H
#define VL_MIPI_4_RX_DPHY_NS_ONES_COUNTER_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"

// Parameters: DESERIALIZER_RATIO=4, NUM_ONES_SPACE_ALLOCATION_BITS=3
class mipi_4_rx_dphy_ns_ones_counter : public sc_core::sc_module {
public:
    // number of bits needed to save the detected ones ( four  1's)
    sc_core::sc_in<sc_dt::sc_logic> wordclk{"wordclk"};
    sc_core::sc_in<sc_dt::sc_logic> enable_counter{"enable_counter"};
    sc_core::sc_in<sc_dt::sc_logic> resetz{"resetz"};
    sc_core::sc_in<sc_dt::sc_lv<4>> slicer_deserializer_n_to_1{"slicer_deserializer_n_to_1"};
    sc_core::sc_out<sc_dt::sc_lv<3>> detected_ones{"detected_ones"};
    sc_core::sc_out<sc_dt::sc_logic> done{"done"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_ones_counter);
    explicit mipi_4_rx_dphy_ns_ones_counter(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000.0;  // time precision (fs)
    sc_core::sc_signal<sc_dt::sc_lv<3>> detected_ones_combi{"detected_ones_combi", vl::logic<3>().lv()};
    sc_core::sc_signal<sc_dt::sc_logic> done_int{"done_int", vl::logic<1>().sl()};
    sc_core::sc_signal<sc_dt::sc_lv<4>> sampled_slicer_data{"sampled_slicer_data", vl::logic<4>().lv()};

    void assign_detected_ones();
    void proc_always_67();
    void proc_always_88();
    void proc_always_98();
};

#endif
