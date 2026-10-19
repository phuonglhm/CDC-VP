//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_DDL_4bit
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_DDL_4BIT_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_DDL_4BIT_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_DDL_DIFF_BUFF_CELL.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_DDL_DIFF_BUFF_CELL_REP.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_DDL_LOGIC_4bit.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_DDL_MUX_2_1.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_DDL_PI_BIASED.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_DDL_gate.h"
#include "mipi_4_rx_dphy_ns_sc_syncx10_as_lvt.h"
#include "mipi_4_rx_dphy_ns_sc_tie_hilow_lvt.h"
#include "rx_cells.h"

class mipi_4_rx_dphy_ns_DPHYHSRX_DDL_4bit : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> bn{"bn"};
    sc_core::sc_in<sc_dt::sc_logic> bp{"bp"};
    sc_core::sc_in<sc_dt::sc_logic> clk_inn{"clk_inn"};
    sc_core::sc_in<sc_dt::sc_logic> clk_inp{"clk_inp"};
    sc_core::sc_out<sc_dt::sc_logic> clk_outn{"clk_outn"};
    sc_core::sc_out<sc_dt::sc_logic> clk_outp{"clk_outp"};
    sc_core::sc_out<sc_dt::sc_logic> ddl_clk_outn{"ddl_clk_outn"};
    sc_core::sc_out<sc_dt::sc_logic> ddl_clk_outp{"ddl_clk_outp"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> on{"on"};
    sc_core::sc_in<sc_dt::sc_logic> onz{"onz"};
    sc_core::sc_in<sc_dt::sc_lv<4>> phase{"phase"};
    sc_core::sc_in<sc_dt::sc_logic> phase_sync{"phase_sync"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_DDL_4bit);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_DDL_4bit(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_vector<sc_core::sc_signal_resolved> phase_b{"phase_b", 4, vl::make_z1};
    sc_core::sc_signal_resolved ddl_clk_outn_int{"ddl_clk_outn_int", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved ddl_clk_outp_int{"ddl_clk_outp_int", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved tie_high{"tie_high", sc_dt::SC_LOGIC_Z};
    sc_core::sc_vector<sc_core::sc_signal_resolved> out_bufn_f{"out_bufn_f", 3, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> y{"y", 4, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> syncd_phase{"syncd_phase", 4, vl::make_z1};
    sc_core::sc_signal_resolved nextn{"nextn", sc_dt::SC_LOGIC_Z};
    sc_core::sc_vector<sc_core::sc_signal_resolved> net2075{"net2075", 4, vl::make_z1};
    sc_core::sc_signal_resolved nextp{"nextp", sc_dt::SC_LOGIC_Z};
    sc_core::sc_vector<sc_core::sc_signal_resolved> yz{"yz", 4, vl::make_z1};
    sc_core::sc_signal_resolved out_bufn{"out_bufn", sc_dt::SC_LOGIC_Z};
    sc_core::sc_vector<sc_core::sc_signal_resolved> out_bufp_f{"out_bufp_f", 3, vl::make_z1};
    sc_core::sc_signal_resolved lbn{"lbn", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved out_bufp{"out_bufp", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved lbp{"lbp", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved in_bufn{"in_bufn", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved in_bufp{"in_bufp", sc_dt::SC_LOGIC_Z};
    sc_core::sc_vector<sc_core::sc_signal_resolved> out_bufn_b{"out_bufn_b", 3, vl::make_z1};
    sc_core::sc_signal_resolved tie_low{"tie_low", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved pi_outn{"pi_outn", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved pi_outp{"pi_outp", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved pi_out2n{"pi_out2n", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved pi_out2p{"pi_out2p", sc_dt::SC_LOGIC_Z};
    sc_core::sc_vector<sc_core::sc_signal_resolved> out_bufp_b{"out_bufp_b", 3, vl::make_z1};
    sc_core::sc_signal_resolved tune_enz{"tune_enz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal<double, sc_core::SC_UNCHECKED_WRITERS> ddl_delay{"ddl_delay", 0.0};
    double inv_tristate_delay = 0.0;
    bool ddl_delay_forced_ = false;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_syncx10_as_lvt> FF_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_syncx10_as_lvt> FF_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_syncx10_as_lvt> FF_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_syncx10_as_lvt> FF_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_tie_hilow_lvt> I393;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_DDL_MUX_2_1_inv_en> I405;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_DDL_MUX_2_1_inv_en> I406;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_DDL_LOGIC_4bit> LOGIC;
    sc_core::sc_signal_rv<2> c_LOGIC_phase{"c_LOGIC_phase", vl::logic<2>::all_z().lv()};
    sc_core::sc_signal_rv<4> c_LOGIC_y{"c_LOGIC_y", vl::logic<4>::all_z().lv()};
    sc_core::sc_signal_rv<4> c_LOGIC_yz{"c_LOGIC_yz", vl::logic<4>::all_z().lv()};
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx24_lvt> I394;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx24_lvt> I399;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_DDL_DIFF_BUFF_CELL> BUF_CELL_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_DDL_DIFF_BUFF_CELL> BUF_CELL_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_DDL_DIFF_BUFF_CELL> BUF_CELL_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_DDL_DIFF_BUFF_CELL> BUF_CELL_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> I462_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> I462_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> I462_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> I462_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_DDL_gate> GATEN;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_DDL_gate> GATEP;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_DDL_PI_BIASED> PI_N_0;
    sc_core::sc_signal_rv<2> c_PI_N_0_word{"c_PI_N_0_word", vl::logic<2>::all_z().lv()};
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_DDL_PI_BIASED> PI_P_0;
    sc_core::sc_signal_rv<2> c_PI_P_0_word{"c_PI_P_0_word", vl::logic<2>::all_z().lv()};
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_DDL_MUX_2_1> CLK_MUX_P;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_DDL_MUX_2_1> I388;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_DDL_DIFF_BUFF_CELL_REP> BUF_CELL_NEXT;

    void split_phase();
    vl::logic<4> rd_net2075() const;
    vl::logic<3> rd_out_bufn_b() const;
    vl::logic<3> rd_out_bufn_f() const;
    vl::logic<3> rd_out_bufp_b() const;
    vl::logic<3> rd_out_bufp_f() const;
    vl::logic<4> rd_phase() const;
    vl::logic<4> rd_syncd_phase() const;
    vl::logic<4> rd_y() const;
    vl::logic<4> rd_yz() const;
    void force_ddl_delay(double v) { ddl_delay_forced_ = true; ddl_delay.write(v); }
    void release_ddl_delay() { ddl_delay_forced_ = false; }
    void gate_ddl_clk_outn01();
    void gate_ddl_clk_outp01();
    void gate_ddl_clk_outn02();
    void gate_ddl_clk_outp02();
    void glue_LOGIC_phase();
    void split_LOGIC_y();
    void split_LOGIC_yz();
    void glue_PI_N_0_word();
    void glue_PI_P_0_word();
    void proc_initial_154668();
};

#endif
