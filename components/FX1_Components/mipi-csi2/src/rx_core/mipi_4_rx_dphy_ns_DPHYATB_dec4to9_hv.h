//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYATB_dec4to9_hv
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYATB_DEC4TO9_HV_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYATB_DEC4TO9_HV_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "rx_cells.h"

class mipi_4_rx_dphy_ns_DPHYATB_dec4to9_hv : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_lv<4>> A_in{"A_in"};
    sc_core::sc_in<sc_dt::sc_lv<4>> A_inz{"A_inz"};
    sc_core::sc_out<sc_dt::sc_lv<9>> Z_out{"Z_out"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYATB_dec4to9_hv);
    explicit mipi_4_rx_dphy_ns_DPHYATB_dec4to9_hv(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_vector<sc_core::sc_signal_resolved> A_in_b{"A_in_b", 4, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> A_inz_b{"A_inz_b", 4, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> Z_out_b{"Z_out_b", 9, vl::make_z1};
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHY3AFE_nor4hv> nor0;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHY3AFE_nor4hv> nor1;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHY3AFE_nor4hv> nor2;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHY3AFE_nor4hv> nor3;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHY3AFE_nor4hv> nor4;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHY3AFE_nor4hv> nor5;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHY3AFE_nor4hv> nor6;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHY3AFE_nor4hv> nor7;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHY3AFE_nor4hv> nor8;

    void split_A_in();
    void split_A_inz();
    void merge_Z_out();
    vl::logic<4> rd_A_in() const;
    vl::logic<4> rd_A_inz() const;
    vl::logic<9> rd_Z_out() const;
};

#endif
