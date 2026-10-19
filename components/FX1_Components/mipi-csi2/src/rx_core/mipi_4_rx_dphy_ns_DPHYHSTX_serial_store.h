//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSTX_serial_store
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYHSTX_SERIAL_STORE_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYHSTX_SERIAL_STORE_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_4_rx_dphy_ns_DPHYHSTX_serial_lax23_neg_lvt.h"
#include "mipi_4_rx_dphy_ns_sc_dlax23_ar_lvt.h"
#include "mipi_4_rx_dphy_ns_sc_dlax23_as_lvt.h"
#include "mipi_4_rx_dphy_ns_sc_ffx23_lvt.h"
#include "mipi_4_rx_dphy_ns_sc_tie_hilow_lvt.h"
#include "rx_cells.h"

class mipi_4_rx_dphy_ns_DPHYHSTX_serial_store : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> clk_buf_m{"clk_buf_m"};
    sc_core::sc_in<sc_dt::sc_logic> clk_buf_p{"clk_buf_p"};
    sc_core::sc_in<sc_dt::sc_lv<4>> clk_m{"clk_m"};
    sc_core::sc_in<sc_dt::sc_lv<4>> clk_p{"clk_p"};
    sc_core::sc_in<sc_dt::sc_logic> div_en{"div_en"};
    sc_core::sc_in<sc_dt::sc_logic> local_clk{"local_clk"};
    sc_core::sc_in<sc_dt::sc_logic> serial_en{"serial_en"};
    sc_core::sc_in<sc_dt::sc_logic> tra_en{"tra_en"};
    sc_core::sc_in<sc_dt::sc_lv<8>> tx_data{"tx_data"};
    sc_core::sc_out<sc_dt::sc_logic> sym_a{"sym_a"};
    sc_core::sc_out<sc_dt::sc_logic> sym_b{"sym_b"};
    sc_core::sc_out<sc_dt::sc_logic> tra_a{"tra_a"};
    sc_core::sc_out<sc_dt::sc_logic> tra_b{"tra_b"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSTX_serial_store);
    explicit mipi_4_rx_dphy_ns_DPHYHSTX_serial_store(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_vector<sc_core::sc_signal_resolved> clk_m_b{"clk_m_b", 4, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> clk_p_b{"clk_p_b", 4, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> tx_data_b{"tx_data_b", 8, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> q_n{"q_n", 8, vl::make_z1};
    sc_core::sc_signal_resolved tie_high{"tie_high", sc_dt::SC_LOGIC_Z};
    sc_core::sc_vector<sc_core::sc_signal_resolved> data{"data", 8, vl::make_z1};
    sc_core::sc_signal_resolved tdown1{"tdown1", sc_dt::SC_LOGIC_Z};
    sc_core::sc_vector<sc_core::sc_signal_resolved> q{"q", 8, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> tx_datazz{"tx_datazz", 8, vl::make_z1};
    sc_core::sc_signal_resolved tdown2{"tdown2", sc_dt::SC_LOGIC_Z};
    sc_core::sc_vector<sc_core::sc_signal_resolved> tx_dataz{"tx_dataz", 8, vl::make_z1};
    sc_core::sc_signal_resolved store_on{"store_on", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved tra_enz{"tra_enz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved xsym_a{"xsym_a", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved xsym_b{"xsym_b", sc_dt::SC_LOGIC_Z};
    sc_core::sc_vector<sc_core::sc_signal_resolved> pqz{"pqz", 4, vl::make_z1};
    sc_core::sc_signal_resolved net878{"net878", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net880{"net880", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net882{"net882", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved tie_low{"tie_low", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net884{"net884", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved sdown1{"sdown1", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved sdown2{"sdown2", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved sdown3{"sdown3", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved sdown4{"sdown4", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved sup1{"sup1", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved sup2{"sup2", sc_dt::SC_LOGIC_Z};
    sc_core::sc_vector<sc_core::sc_signal_resolved> pq{"pq", 4, vl::make_z1};
    sc_core::sc_signal_resolved store_off{"store_off", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net676{"net676", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved xtra_a{"xtra_a", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved xtra_b{"xtra_b", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved tup1{"tup1", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved tup2{"tup2", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved tup3{"tup3", sc_dt::SC_LOGIC_Z};
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_dlax23_as_lvt> LA1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_dlax23_as_lvt> LA3;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_dlax23_as_lvt> LA4;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_dlax23_as_lvt> LA5;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_dlax23_as_lvt> LA8;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_ffx23_lvt> ff_in_7;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_ffx23_lvt> ff_in_6;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_ffx23_lvt> ff_in_5;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_ffx23_lvt> ff_in_4;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_ffx23_lvt> ff_in_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_ffx23_lvt> ff_in_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_ffx23_lvt> ff_in_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_ffx23_lvt> ff_in_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_dlax23_ar_lvt> LA2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_dlax23_ar_lvt> LA6;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_dlax23_ar_lvt> LA7;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_dlax23_ar_lvt> LA9;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx32_lvt> I67;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx32_lvt> I68;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx32_lvt> I69;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx32_lvt> I70;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_tgate_x12_lvt> I201;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx128_lvt> I221;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx128_lvt> I222;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx128_lvt> I223;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx128_lvt> I224;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSTX_serial_lax23_neg_lvt> la_in_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSTX_serial_lax23_neg_lvt> la_in_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSTX_serial_lax23_neg_lvt> la_in_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSTX_serial_lax23_neg_lvt> la_in_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na2x12_lvt> I71;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_na2x12_lvt> I151;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSTX_serial_imux4x_lvt> MUX_DOWN;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSTX_serial_imux4x_lvt> MUX_UP;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_tie_hilow_lvt> I210;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x12_lvt> I40;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x12_lvt> no_7;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x12_lvt> no_6;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x12_lvt> no_5;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x12_lvt> no_4;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x12_lvt> no_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x12_lvt> no_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x12_lvt> no_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_no2x12_lvt> no_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> I55;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> I137;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> I184;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> INV2_7;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> INV2_6;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> INV2_5;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> INV2_4;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> INV2_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> INV2_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> INV2_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> INV2_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> inv_7;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> inv_6;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> inv_5;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> inv_4;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> inv_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> inv_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> inv_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx12_lvt> inv_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx8_lvt> IN1_7;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx8_lvt> IN1_6;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx8_lvt> IN1_5;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx8_lvt> IN1_4;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx8_lvt> IN1_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx8_lvt> IN1_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx8_lvt> IN1_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx8_lvt> IN1_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx8_lvt> INV3;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx8_lvt> INV4_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx8_lvt> INV4_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx8_lvt> INV4_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx8_lvt> INV4_0;

    void split_clk_m();
    void split_clk_p();
    void split_tx_data();
    vl::logic<4> rd_clk_m() const;
    vl::logic<4> rd_clk_p() const;
    vl::logic<8> rd_data() const;
    vl::logic<4> rd_pq() const;
    vl::logic<4> rd_pqz() const;
    vl::logic<8> rd_q() const;
    vl::logic<8> rd_q_n() const;
    vl::logic<8> rd_tx_data() const;
    vl::logic<8> rd_tx_dataz() const;
    vl::logic<8> rd_tx_datazz() const;
};

#endif
