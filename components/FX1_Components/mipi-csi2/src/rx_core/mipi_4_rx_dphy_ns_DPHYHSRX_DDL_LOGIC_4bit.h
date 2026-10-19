//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_DDL_LOGIC_4bit
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_DDL_LOGIC_4BIT_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_DDL_LOGIC_4BIT_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "rx_cells.h"

class mipi_4_rx_dphy_ns_DPHYHSRX_DDL_LOGIC_4bit : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_lv<2>> phase{"phase"};
    sc_core::sc_in<sc_dt::sc_logic> pon{"pon"};
    sc_core::sc_in<sc_dt::sc_logic> tune_en{"tune_en"};
    sc_core::sc_out<sc_dt::sc_logic> tune_enz{"tune_enz"};
    sc_core::sc_out<sc_dt::sc_lv<4>> y{"y"};
    sc_core::sc_out<sc_dt::sc_lv<4>> yz{"yz"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_DDL_LOGIC_4bit);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_DDL_LOGIC_4bit(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_vector<sc_core::sc_signal_resolved> phase_b{"phase_b", 2, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> y_b{"y_b", 4, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> yz_b{"yz_b", 4, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> net149{"net149", 4, vl::make_z1};
    sc_core::sc_signal_resolved tune_en_in{"tune_en_in", sc_dt::SC_LOGIC_Z};
    sc_core::sc_vector<sc_core::sc_signal_resolved> phasez{"phasez", 2, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> xz{"xz", 4, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> net143{"net143", 4, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> z{"z", 4, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> net152{"net152", 4, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> x{"x", 4, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> phase_in{"phase_in", 2, vl::make_z1};
    sc_core::sc_signal_resolved ponz{"ponz", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> I8_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> I8_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> I24_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> I24_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> I27_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> I27_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> I27_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> I27_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> I28;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> inv_pon;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> inv_tune_en;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x6_lvt> I10_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x6_lvt> I10_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x6_lvt> I10_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x6_lvt> I10_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x6_lvt> I25_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x6_lvt> I25_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x6_lvt> I25_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x6_lvt> I25_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x6_lvt> I26_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x6_lvt> I26_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x6_lvt> I26_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx32_lvt> I9_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx32_lvt> I9_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx32_lvt> I9_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx32_lvt> I9_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx32_lvt> I29_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx32_lvt> I29_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx32_lvt> I29_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx32_lvt> I29_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na2x6_lvt> I4;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na2x6_lvt> I5;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na2x6_lvt> I6;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na2x6_lvt> I7;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na2x6_lvt> nand_tune_en;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx16_lvt> I30_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx16_lvt> I30_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx16_lvt> I30_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx16_lvt> I30_0;

    void split_phase();
    void merge_y();
    void merge_yz();
    vl::logic<4> rd_net143() const;
    vl::logic<4> rd_net149() const;
    vl::logic<4> rd_net152() const;
    vl::logic<2> rd_phase() const;
    vl::logic<2> rd_phase_in() const;
    vl::logic<2> rd_phasez() const;
    vl::logic<4> rd_x() const;
    vl::logic<4> rd_xz() const;
    vl::logic<4> rd_y() const;
    vl::logic<4> rd_yz() const;
    vl::logic<4> rd_z() const;
};

#endif
