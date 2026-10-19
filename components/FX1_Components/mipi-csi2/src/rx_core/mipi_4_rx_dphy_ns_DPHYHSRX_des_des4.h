//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_des_des4
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_DES_DES4_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_DES_DES4_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_4_rx_dphy_ns_sc_dlax39_ar_lvt.h"
#include "rx_cells.h"

class mipi_4_rx_dphy_ns_DPHYHSRX_des_des4 : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> data{"data"};
    sc_core::sc_in<sc_dt::sc_lv<4>> div5{"div5"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> tie_high{"tie_high"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    sc_core::sc_out<sc_dt::sc_lv<4>> rx_data{"rx_data"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_des_des4);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_des_des4(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_vector<sc_core::sc_signal_resolved> div5_b{"div5_b", 4, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> rx_data_b{"rx_data_b", 4, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> div5_neg{"div5_neg", 4, vl::make_z1};
    sc_core::sc_signal_resolved rx_data_del{"rx_data_del", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> INV_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> INV_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> INV_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> INV_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_dlax39_ar_lvt> I13;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_dlax39_ar_lvt> I18;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_dlax39_ar_lvt> I21;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_dlax39_ar_lvt> I22;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_dlax39_ar_lvt> la0;

    void split_div5();
    void merge_rx_data();
    vl::logic<4> rd_div5() const;
    vl::logic<4> rd_div5_neg() const;
    vl::logic<4> rd_rx_data() const;
};

#endif
