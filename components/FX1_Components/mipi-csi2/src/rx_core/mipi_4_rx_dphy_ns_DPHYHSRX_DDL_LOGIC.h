//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_DDL_LOGIC
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_DDL_LOGIC_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_DDL_LOGIC_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "rx_cells.h"

class mipi_4_rx_dphy_ns_DPHYHSRX_DDL_LOGIC : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_lv<3>> phase{"phase"};
    sc_core::sc_in<sc_dt::sc_logic> pon{"pon"};
    sc_core::sc_in<sc_dt::sc_logic> tune_en{"tune_en"};
    sc_core::sc_out<sc_dt::sc_logic> tune_enz{"tune_enz"};
    sc_core::sc_out<sc_dt::sc_lv<8>> y{"y"};
    sc_core::sc_out<sc_dt::sc_lv<8>> yz{"yz"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_DDL_LOGIC);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_DDL_LOGIC(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_vector<sc_core::sc_signal_resolved> phase_b{"phase_b", 3, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> y_b{"y_b", 8, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> yz_b{"yz_b", 8, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> net167{"net167", 8, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> z{"z", 8, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> xz{"xz", 8, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> net166{"net166", 8, vl::make_z1};
    sc_core::sc_signal_resolved tune_en_in{"tune_en_in", sc_dt::SC_LOGIC_Z};
    sc_core::sc_vector<sc_core::sc_signal_resolved> x{"x", 8, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> net131{"net131", 8, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> phasez{"phasez", 3, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> phase_in{"phase_in", 3, vl::make_z1};
    sc_core::sc_signal_resolved ponz{"ponz", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na2x6_lvt> nand_tune_en;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x6_lvt> I10_7;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x6_lvt> I10_6;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x6_lvt> I10_5;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x6_lvt> I10_4;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x6_lvt> I10_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x6_lvt> I10_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x6_lvt> I10_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x6_lvt> I10_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x6_lvt> I21_7;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x6_lvt> I21_6;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x6_lvt> I21_5;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x6_lvt> I21_4;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x6_lvt> I21_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x6_lvt> I21_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x6_lvt> I21_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x6_lvt> I21_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x6_lvt> I24_6;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x6_lvt> I24_5;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x6_lvt> I24_4;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x6_lvt> I24_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x6_lvt> I24_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x6_lvt> I24_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x6_lvt> I24_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na3x6_lvt> I0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na3x6_lvt> I1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na3x6_lvt> I2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na3x6_lvt> I3;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na3x6_lvt> I4;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na3x6_lvt> I5;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na3x6_lvt> I6;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na3x6_lvt> I7;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> I8_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> I8_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> I8_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> I20_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> I20_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> I20_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> I26;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> I27_7;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> I27_6;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> I27_5;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> I27_4;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> I27_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> I27_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> I27_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> I27_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> inv_pon;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> inv_tune_en;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx32_lvt> I9_7;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx32_lvt> I9_6;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx32_lvt> I9_5;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx32_lvt> I9_4;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx32_lvt> I9_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx32_lvt> I9_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx32_lvt> I9_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx32_lvt> I9_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx32_lvt> I28_7;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx32_lvt> I28_6;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx32_lvt> I28_5;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx32_lvt> I28_4;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx32_lvt> I28_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx32_lvt> I28_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx32_lvt> I28_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx32_lvt> I28_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx16_lvt> I29_7;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx16_lvt> I29_6;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx16_lvt> I29_5;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx16_lvt> I29_4;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx16_lvt> I29_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx16_lvt> I29_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx16_lvt> I29_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx16_lvt> I29_0;

    void split_phase();
    void merge_y();
    void merge_yz();
    vl::logic<8> rd_net131() const;
    vl::logic<8> rd_net166() const;
    vl::logic<8> rd_net167() const;
    vl::logic<3> rd_phase() const;
    vl::logic<3> rd_phase_in() const;
    vl::logic<3> rd_phasez() const;
    vl::logic<8> rd_x() const;
    vl::logic<8> rd_xz() const;
    vl::logic<8> rd_y() const;
    vl::logic<8> rd_yz() const;
    vl::logic<8> rd_z() const;
};

#endif
