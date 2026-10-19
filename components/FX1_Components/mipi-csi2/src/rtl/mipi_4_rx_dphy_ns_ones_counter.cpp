//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_ones_counter
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_ones_counter.h"

mipi_4_rx_dphy_ns_ones_counter::mipi_4_rx_dphy_ns_ones_counter(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(assign_detected_ones);
    sensitive << detected_ones_combi;
    SC_METHOD(proc_always_67);
    sensitive << wordclk.pos() << resetz.neg();
    dont_initialize();
    SC_METHOD(proc_always_88);
    sensitive << enable_counter << sampled_slicer_data;
    SC_METHOD(proc_always_98);
    sensitive << wordclk.pos() << resetz.neg();
    dont_initialize();
}

// samping has to be done relatively
void mipi_4_rx_dphy_ns_ones_counter::assign_detected_ones() {
    vl::wr(detected_ones, vl::rd(detected_ones_combi));
}

void mipi_4_rx_dphy_ns_ones_counter::proc_always_67() {
    if (vl::rd(resetz).truth_val() == 0) {
        // reset the sampling mechanism
        vl::wr(sampled_slicer_data, vl::logic<4>(0x0ULL));
        vl::wr(done_int, vl::logic<1>(0x0ULL));
    } else {
        // sample the slicer data
        if (vl::rd(enable_counter).truth_val() == 0) {
            vl::wr(sampled_slicer_data, vl::logic<4>(0x0ULL));
            vl::wr(done_int, vl::logic<1>(0x0ULL));
        } else {
            vl::wr(sampled_slicer_data, vl::rd(slicer_deserializer_n_to_1));
            vl::wr(done_int, vl::logic<1>(0x1ULL));
        }
    }
}

// detect the number of ones in the sampled data
void mipi_4_rx_dphy_ns_ones_counter::proc_always_88() {
    vl::logic<3> detected_ones_combi_sh = vl::rd(detected_ones_combi);
    if (vl::rd(enable_counter).truth_val() == 0) {
        detected_ones_combi_sh = vl::logic<3>(0x0ULL);
        vl::wr(detected_ones_combi, detected_ones_combi_sh);
    } else {
        detected_ones_combi_sh = ((vl::cat(vl::logic<2>(0x0ULL), vl::rd(sampled_slicer_data).bit(0)) + vl::cat(vl::logic<2>(0x0ULL), vl::rd(sampled_slicer_data).bit(1))) + vl::cat(vl::logic<2>(0x0ULL), vl::rd(sampled_slicer_data).bit(2))) + vl::cat(vl::logic<2>(0x0ULL), vl::rd(sampled_slicer_data).bit(3));
        vl::wr(detected_ones_combi, detected_ones_combi_sh);
    }
}

// now register the outputs done and   detected_ones
void mipi_4_rx_dphy_ns_ones_counter::proc_always_98() {
    if (vl::rd(resetz).truth_val() == 0) {
        // reset the sampling mechanism
        vl::wr(done, vl::logic<1>(0x0ULL));
    } else {
        // indicates that the first sampling is done
        // sample the slicer data
        if (vl::rd(enable_counter).truth_val() == 0) {
            vl::wr(done, vl::logic<1>(0x0ULL));
        } else {
            if (vl::rd(done_int).test()) {
                vl::wr(done, vl::logic<1>(0x1ULL));
            } else {
                vl::wr(done, vl::logic<1>(0x0ULL));
            }
        }
    }
}
