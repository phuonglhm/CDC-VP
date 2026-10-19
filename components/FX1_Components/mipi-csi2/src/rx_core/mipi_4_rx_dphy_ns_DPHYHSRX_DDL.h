//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_DDL
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_DDL_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYHSRX_DDL_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_DDL_BIAS.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_DDL_DIFF_BUFF_CELL.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_DDL_DIFF_BUFF_CELL_REP.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_DDL_LOGIC.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_DDL_MUX_2_1.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_DDL_PI_BIASED.h"
#include "mipi_4_rx_dphy_ns_DPHYHSRX_DDL_gate.h"
#include "mipi_4_rx_dphy_ns_sc_syncx10_as_lvt.h"
#include "mipi_4_rx_dphy_ns_sc_tie_hilow_lvt.h"
#include "rx_cells.h"

class mipi_4_rx_dphy_ns_DPHYHSRX_DDL : public sc_core::sc_module {
public:
    sc_core::sc_inout<sc_dt::sc_logic> atb_line{"atb_line"};
    sc_core::sc_in<sc_dt::sc_lv<4>> atb_sw_hv{"atb_sw_hv"};
    sc_core::sc_out<sc_dt::sc_logic> bn{"bn"};
    sc_core::sc_out<sc_dt::sc_logic> bp{"bp"};
    sc_core::sc_in<sc_dt::sc_logic> clk_inn{"clk_inn"};
    sc_core::sc_in<sc_dt::sc_logic> clk_inp{"clk_inp"};
    sc_core::sc_out<sc_dt::sc_logic> clk_outn{"clk_outn"};
    sc_core::sc_out<sc_dt::sc_logic> clk_outp{"clk_outp"};
    sc_core::sc_out<sc_dt::sc_logic> ddl_clk_outn{"ddl_clk_outn"};
    sc_core::sc_out<sc_dt::sc_logic> ddl_clk_outp{"ddl_clk_outp"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> iref{"iref"};
    sc_core::sc_in<sc_dt::sc_logic> on{"on"};
    sc_core::sc_in<sc_dt::sc_logic> onz{"onz"};
    sc_core::sc_in<sc_dt::sc_lv<5>> phase{"phase"};
    sc_core::sc_in<sc_dt::sc_logic> phase_sync{"phase_sync"};
    sc_core::sc_in<sc_dt::sc_lv<5>> tune{"tune"};
    sc_core::sc_in<sc_dt::sc_logic> tune_en{"tune_en"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};
    sc_core::sc_in<sc_dt::sc_logic> gate{"gate"};
    sc_core::sc_in<sc_dt::sc_logic> on_hv{"on_hv"};
    sc_core::sc_in<sc_dt::sc_logic> gatez{"gatez"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYHSRX_DDL);
    explicit mipi_4_rx_dphy_ns_DPHYHSRX_DDL(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_vector<sc_core::sc_signal_resolved> atb_sw_hv_b{"atb_sw_hv_b", 4, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> phase_b{"phase_b", 5, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> net2193{"net2193", 5, vl::make_z1};
    sc_core::sc_signal_resolved tie_high{"tie_high", sc_dt::SC_LOGIC_Z};
    sc_core::sc_vector<sc_core::sc_signal_resolved> out_bufp_f{"out_bufp_f", 7, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> atb_sw_hvz{"atb_sw_hvz", 4, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> out_bufp_b{"out_bufp_b", 7, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> out_bufn_f{"out_bufn_f", 7, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> out_bufn_b{"out_bufn_b", 7, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> syncd_phase{"syncd_phase", 5, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> y{"y", 8, vl::make_z1};
    sc_core::sc_signal_resolved nextn{"nextn", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved nextp{"nextp", sc_dt::SC_LOGIC_Z};
    sc_core::sc_vector<sc_core::sc_signal_resolved> yz{"yz", 8, vl::make_z1};
    sc_core::sc_signal_resolved out_bufn{"out_bufn", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved out_bufp{"out_bufp", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved lbn{"lbn", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved lbp{"lbp", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved in_bufn{"in_bufn", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved in_bufp{"in_bufp", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved tie_low{"tie_low", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved pi_outn{"pi_outn", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved pi_outp{"pi_outp", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved pi_out2n{"pi_out2n", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved pi_out2p{"pi_out2p", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved tune_enz{"tune_enz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal<double, sc_core::SC_UNCHECKED_WRITERS> ddl_delay{"ddl_delay", 0.0};
    sc_core::sc_signal<double, sc_core::SC_UNCHECKED_WRITERS> factor{"factor", 0.0};
    double inv_tristate_delay = 0.0;
    sc_core::sc_signal_resolved nextn__drv{"nextn__drv", sc_dt::SC_LOGIC_Z};   // structural drivers of nextn
    bool nextn_forced_ = false;
    vl::logic<1> nextn_fval_;
    sc_core::sc_event nextn_fev_;
    sc_core::sc_signal_resolved nextp__drv{"nextp__drv", sc_dt::SC_LOGIC_Z};   // structural drivers of nextp
    bool nextp_forced_ = false;
    vl::logic<1> nextp_fval_;
    sc_core::sc_event nextp_fev_;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_DDL_DIFF_BUFF_CELL_REP> BUF_CELL_NEXT;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_DDL_gate> GATEN;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_DDL_gate> GATEP;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_inv1x> sctg_ATB_NV_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_inv1x> sctg_ATB_NV_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_inv1x> sctg_ATB_NV_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_inv1x> sctg_ATB_NV_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx24_lvt> sc_lvt_I394;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx24_lvt> sc_lvt_I399;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_DDL_MUX_2_1_inv_en> I405;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_DDL_MUX_2_1_inv_en> I406;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_tie_hilow_lvt> sc_lvt_I393;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_DDL_LOGIC> LOGIC;
    sc_core::sc_signal_rv<3> c_LOGIC_phase{"c_LOGIC_phase", vl::logic<3>::all_z().lv()};
    sc_core::sc_signal_rv<8> c_LOGIC_y{"c_LOGIC_y", vl::logic<8>::all_z().lv()};
    sc_core::sc_signal_rv<8> c_LOGIC_yz{"c_LOGIC_yz", vl::logic<8>::all_z().lv()};
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_DDL_PI_BIASED> PI_N_0;
    sc_core::sc_signal_rv<2> c_PI_N_0_word{"c_PI_N_0_word", vl::logic<2>::all_z().lv()};
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_DDL_PI_BIASED> PI_P_0;
    sc_core::sc_signal_rv<2> c_PI_P_0_word{"c_PI_P_0_word", vl::logic<2>::all_z().lv()};
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_syncx10_as_lvt> sc_lvt_FF_4;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_syncx10_as_lvt> sc_lvt_FF_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_syncx10_as_lvt> sc_lvt_FF_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_syncx10_as_lvt> sc_lvt_FF_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_syncx10_as_lvt> sc_lvt_FF_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> sc_lvt_I462_4;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> sc_lvt_I462_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> sc_lvt_I462_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> sc_lvt_I462_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_sc_invx4_lvt> sc_lvt_I462_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_DDL_DIFF_BUFF_CELL> BUF_CELL_0;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_DDL_DIFF_BUFF_CELL> BUF_CELL_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_DDL_DIFF_BUFF_CELL> BUF_CELL_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_DDL_DIFF_BUFF_CELL> BUF_CELL_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_DDL_DIFF_BUFF_CELL> BUF_CELL_4;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_DDL_DIFF_BUFF_CELL> BUF_CELL_5;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_DDL_DIFF_BUFF_CELL> BUF_CELL_6;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_DDL_DIFF_BUFF_CELL> BUF_CELL_7;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_DDL_MUX_2_1> CLK_MUX_P;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_DDL_MUX_2_1> I388;
    // wire_cap  I369( .w(nextp) );
    // wire_cap  I391( .w(nextn) );
    // wire_cap  I419( .w(out_bufp) );
    // wire_cap  I420( .w(out_bufn) );
    // wire_cap  I421( .w(out_bufp_b[0]) );
    // wire_cap  I422( .w(out_bufn_b[0]) );
    // wire_cap  I423( .w(out_bufp_b[1]) );
    // wire_cap  I424( .w(out_bufn_b[1]) );
    // wire_cap  I425( .w(out_bufp_b[2]) );
    // wire_cap  I426( .w(out_bufn_b[2]) );
    // wire_cap  I427( .w(out_bufp_b[3]) );
    // wire_cap  I428( .w(out_bufn_b[3]) );
    // wire_cap  I429( .w(out_bufn_b[4]) );
    // wire_cap  I430( .w(out_bufp_b[4]) );
    // wire_cap  I431( .w(out_bufn_b[5]) );f
    // wire_cap  I432( .w(out_bufp_b[5]) );
    // wire_cap  I433( .w(out_bufn_b[6]) );
    // wire_cap  I434( .w(out_bufp_b[6]) );
    // wire_cap  I435( .w(lbn) );
    // wire_cap  I436( .w(lbp) );
    // wire_cap  I440( .w(in_bufn) );
    // wire_cap  I441( .w(out_bufn_f[4]) );
    // wire_cap  I442( .w(out_bufn_f[5]) );
    // wire_cap  I443( .w(out_bufn_f[6]) );
    // wire_cap  I444( .w(out_bufn_f[3]) );
    // wire_cap  I445( .w(out_bufn_f[1]) );
    // wire_cap  I446( .w(out_bufn_f[2]) );
    // wire_cap  I447( .w(out_bufn_f[0]) );
    // wire_cap  I448( .w(out_bufp_f[4]) );
    // wire_cap  I449( .w(out_bufp_f[5]) );
    // wire_cap  I450( .w(out_bufp_f[6]) );
    // wire_cap  I451( .w(out_bufp_f[3]) );
    // wire_cap  I452( .w(out_bufp_f[1]) );
    // wire_cap  I453( .w(out_bufp_f[2]) );
    // wire_cap  I454( .w(out_bufp_f[0]) );
    // wire_cap  I455( .w(in_bufp) );
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYHSRX_DDL_BIAS> BIAS;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYATB_testswitch_vp> ATB_GD_BUFF;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYATB_testswitch_vp> ATB_GD_PI;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYATB_testswitch_vp> ATB_VDD_BUFF;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYATB_testswitch_vp> ATB_VDD_PI;

    void split_atb_sw_hv();
    void split_phase();
    vl::logic<4> rd_atb_sw_hv() const;
    vl::logic<4> rd_atb_sw_hvz() const;
    vl::logic<5> rd_net2193() const;
    vl::logic<7> rd_out_bufn_b() const;
    vl::logic<7> rd_out_bufn_f() const;
    vl::logic<7> rd_out_bufp_b() const;
    vl::logic<7> rd_out_bufp_f() const;
    vl::logic<5> rd_phase() const;
    vl::logic<5> rd_syncd_phase() const;
    vl::logic<8> rd_y() const;
    vl::logic<8> rd_yz() const;
    void force_mux_nextn();
    void force_mux_nextp();
    void gate_buf_ddl_clk_outp();
    void gate_buf_ddl_clk_outn();
    void glue_LOGIC_phase();
    void split_LOGIC_y();
    void split_LOGIC_yz();
    void glue_PI_N_0_word();
    void glue_PI_P_0_word();
    void proc_initial_155700();
    void proc_initial_155712();
};

#endif
