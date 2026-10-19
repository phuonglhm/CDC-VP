//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYLPTX_log
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYLPTX_LOG_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYLPTX_LOG_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "rx_cells.h"

class mipi_4_rx_dphy_ns_DPHYLPTX_log : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> en{"en"};
    sc_core::sc_in<sc_dt::sc_logic> enulp{"enulp"};
    sc_core::sc_in<sc_dt::sc_logic> pon{"pon"};
    sc_core::sc_in<sc_dt::sc_lv<3>> proghv{"proghv"};
    sc_core::sc_in<sc_dt::sc_lv<3>> setrhv{"setrhv"};
    sc_core::sc_out<sc_dt::sc_logic> enable{"enable"};
    sc_core::sc_out<sc_dt::sc_logic> enablez{"enablez"};
    sc_core::sc_out<sc_dt::sc_logic> onb{"onb"};
    sc_core::sc_out<sc_dt::sc_logic> onbz{"onbz"};
    sc_core::sc_out<sc_dt::sc_lv<3>> proghvz{"proghvz"};
    sc_core::sc_out<sc_dt::sc_lv<3>> setrz{"setrz"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYLPTX_log);
    explicit mipi_4_rx_dphy_ns_DPHYLPTX_log(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_vector<sc_core::sc_signal_resolved> proghv_b{"proghv_b", 3, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> setrhv_b{"setrhv_b", 3, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> proghvz_b{"proghvz_b", 3, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> setrz_b{"setrz_b", 3, vl::make_z1};
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_core_log_inv0> I261;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_core_log_inv0> I265;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_core_log_inv0> inv_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_core_log_inv0> inv_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_core_log_inv0> inv_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_core_log_inv0> setra_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_core_log_inv0> setra_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_core_log_inv0> setra_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_core_log_nand2> I249;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_core_log_nor2> I250;

    void split_proghv();
    void split_setrhv();
    void merge_proghvz();
    void merge_setrz();
    vl::logic<3> rd_proghv() const;
    vl::logic<3> rd_proghvz() const;
    vl::logic<3> rd_setrhv() const;
    vl::logic<3> rd_setrz() const;
};

#endif
