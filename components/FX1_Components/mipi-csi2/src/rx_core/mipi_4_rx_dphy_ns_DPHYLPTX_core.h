//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYLPTX_core
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYLPTX_CORE_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYLPTX_CORE_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"
#include "mipi_4_rx_dphy_ns_DPHYLPTX_core_keys.h"
#include "mipi_4_rx_dphy_ns_DPHYLPTX_core_log.h"
#include "rx_cells.h"

class mipi_4_rx_dphy_ns_DPHYLPTX_core : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> biasn{"biasn"};
    sc_core::sc_in<sc_dt::sc_logic> biasn_fb{"biasn_fb"};
    sc_core::sc_in<sc_dt::sc_logic> biasp{"biasp"};
    sc_core::sc_in<sc_dt::sc_logic> cascn{"cascn"};
    sc_core::sc_in<sc_dt::sc_logic> cascp{"cascp"};
    sc_core::sc_in<sc_dt::sc_logic> din{"din"};
    sc_core::sc_in<sc_dt::sc_logic> dinz{"dinz"};
    sc_core::sc_in<sc_dt::sc_logic> enable{"enable"};
    sc_core::sc_in<sc_dt::sc_logic> enablez{"enablez"};
    sc_core::sc_in<sc_dt::sc_logic> enpulldwn{"enpulldwn"};
    sc_core::sc_in<sc_dt::sc_logic> enulp{"enulp"};
    sc_core::sc_in<sc_dt::sc_logic> loopin{"loopin"};
    sc_core::sc_in<sc_dt::sc_logic> onb{"onb"};
    sc_core::sc_in<sc_dt::sc_logic> onbz{"onbz"};
    sc_core::sc_in<sc_dt::sc_lv<3>> setr{"setr"};
    sc_core::sc_in<sc_dt::sc_lv<3>> setrz{"setrz"};
    sc_core::sc_in<sc_dt::sc_logic> sr_bypasshv{"sr_bypasshv"};
    sc_core::sc_in<sc_dt::sc_logic> tie_high{"tie_high"};
    sc_core::sc_in<sc_dt::sc_logic> tie_low{"tie_low"};
    sc_core::sc_out<sc_dt::sc_logic> loopout{"loopout"};
    sc_core::sc_inout<sc_dt::sc_logic> dout{"dout"};
    sc_core::sc_inout<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYLPTX_core);
    explicit mipi_4_rx_dphy_ns_DPHYLPTX_core(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_vector<sc_core::sc_signal_resolved> setr_b{"setr_b", 3, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> setrz_b{"setrz_b", 3, vl::make_z1};
    sc_core::sc_vector<sc_core::sc_signal_resolved> dout_int{"dout_int", 2, vl::make_z1};
    sc_core::sc_signal_resolved enn{"enn", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<8> net1948{"net1948", vl::logic<8>::all_z().lv()};
    sc_core::sc_signal_resolved yyy{"yyy", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved enp{"enp", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net1705{"net1705", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net1776{"net1776", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_rv<2> net1865{"net1865", vl::logic<2>::all_z().lv()};
    sc_core::sc_signal_rv<2> net1949{"net1949", vl::logic<2>::all_z().lv()};
    sc_core::sc_signal_resolved net1468{"net1468", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved yyy1{"yyy1", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved yyy2{"yyy2", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net1715{"net1715", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved gpmos{"gpmos", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved sr_enhz{"sr_enhz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_vector<sc_core::sc_signal_resolved> dout_intr{"dout_intr", 3, vl::make_z1};
    sc_core::sc_signal_resolved srpxx{"srpxx", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net1646{"net1646", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved xxx1{"xxx1", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net1647{"net1647", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved xxx2{"xxx2", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved gnmos{"gnmos", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved sr_enh{"sr_enh", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved cn0{"cn0", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved xxx{"xxx", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved cn{"cn", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved cp{"cp", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net1667{"net1667", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net1915{"net1915", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net1916{"net1916", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved cp0{"cp0", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved enpullupz{"enpullupz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved biasp1{"biasp1", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved supply_ok{"supply_ok", sc_dt::SC_LOGIC_Z};
    sc_core::sc_event gate_PD_ev;
    vl::logic<1> gate_PD_pv = vl::logic<1>::all_z();
    bool gate_PD_ev_pend = false;
    sc_core::sc_event gate_LPDRV_ev;
    vl::logic<1> gate_LPDRV_pv = vl::logic<1>::all_z();
    bool gate_LPDRV_ev_pend = false;
    sc_core::sc_event gate_I01POWER_AWARE_ev;
    vl::logic<1> gate_I01POWER_AWARE_pv = vl::logic<1>::all_z();
    bool gate_I01POWER_AWARE_ev_pend = false;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_switch> I150;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_switch> I151;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_switch> I245;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_switch> I246;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_switch> I342;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_switch> I343;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_switch> I345;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_switch> I346;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_core_log> LOG;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_inv9x> sctg_I356;
    std::unique_ptr<mipi_4_rx_dphy_ns_sctg_inv9x> sctg_I357;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_core_keys> I259_29;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_core_keys> I259_28;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_core_keys> I259_27;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_core_keys> I259_26;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_core_keys> I259_25;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_core_keys> I259_24;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_core_keys> I259_23;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_core_keys> I259_22;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_core_keys> I259_21;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_core_keys> I259_20;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_core_keys> I259_19;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_core_keys> I259_18;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_core_keys> I259_17;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_core_keys> I259_16;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_core_keys> I259_15;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_core_keys> I259_14;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_core_keys> I259_13;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_core_keys> I259_12;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_core_keys> I259_11;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_core_keys> I259_10;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_core_keys> I259_9;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_core_keys> I259_8;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_core_keys> I259_7;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_core_keys> I259_6;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_core_keys> I259_5;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_core_keys> I259_4;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_core_keys> I259_3;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_core_keys> I259_2;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_core_keys> I259_1;
    std::unique_ptr<mipi_4_rx_dphy_ns_DPHYLPTX_core_keys> I259_0;

    void split_setr();
    void split_setrz();
    vl::logic<2> rd_dout_int() const;
    vl::logic<3> rd_dout_intr() const;
    vl::logic<3> rd_setr() const;
    vl::logic<3> rd_setrz() const;
    void assign_supply_ok();
    void gate_PD();
    void gate_PD_upd();
    void gate_LPDRV();
    void gate_LPDRV_upd();
    void gate_I01POWER_AWARE();
    void gate_I01POWER_AWARE_upd();
    void proc_always_160211();
};

#endif
