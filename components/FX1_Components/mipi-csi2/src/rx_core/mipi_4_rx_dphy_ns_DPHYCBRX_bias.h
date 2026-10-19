//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYCBRX_bias
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHYCBRX_BIAS_H
#define VL_MIPI_4_RX_DPHY_NS_DPHYCBRX_BIAS_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"

class mipi_4_rx_dphy_ns_DPHYCBRX_bias : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> cb_cal_en_hv{"cb_cal_en_hv"};
    sc_core::sc_in<sc_dt::sc_lv<4>> cb_cal_repl_hv{"cb_cal_repl_hv"};
    sc_core::sc_in<sc_dt::sc_logic> en_45_ohm_hv{"en_45_ohm_hv"};
    sc_core::sc_in<sc_dt::sc_logic> ota_on_hv{"ota_on_hv"};
    sc_core::sc_in<sc_dt::sc_logic> ota_on_hvz{"ota_on_hvz"};
    sc_core::sc_in<sc_dt::sc_logic> ota_on_int_z{"ota_on_int_z"};
    sc_core::sc_in<sc_dt::sc_logic> replv_i{"replv_i"};
    sc_core::sc_in<sc_dt::sc_logic> vref{"vref"};
    sc_core::sc_out<sc_dt::sc_lv<4>> cb_10u_ibias_ddl{"cb_10u_ibias_ddl"};
    sc_core::sc_out<sc_dt::sc_logic> cb_10u_ilptx{"cb_10u_ilptx"};
    sc_core::sc_out<sc_dt::sc_lv<10>> cb_5u_ilprx{"cb_5u_ilprx"};
    sc_core::sc_out<sc_dt::sc_lv<5>> cb_preamp_cm{"cb_preamp_cm"};
    sc_core::sc_out<sc_dt::sc_logic> replv_o{"replv_o"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHYCBRX_bias);
    explicit mipi_4_rx_dphy_ns_DPHYCBRX_bias(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_rv<25> n_a{"n_a", vl::logic<25>::all_z().lv()};
    sc_core::sc_signal_rv<10> n_c{"n_c", vl::logic<10>::all_z().lv()};
    sc_core::sc_signal_resolved casc{"casc", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved vout2{"vout2", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net2382{"net2382", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net2387{"net2387", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net2316{"net2316", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net2394{"net2394", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved bias_10u{"bias_10u", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net2570{"net2570", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved vout{"vout", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net2571{"net2571", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net2576{"net2576", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net2496{"net2496", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net2497{"net2497", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net2498{"net2498", sc_dt::SC_LOGIC_Z};
    // DPHYCBRX_repl  I542( .cb_cal_en_hv(cb_cal_en_hv),
    // .cb_cal_repl_hv(cb_cal_repl_hv), .gd(gd), .head(replv_o) );
    // rppwl  R508( .b(gd), .in(net2387), .out(gd) );
    // rppwl  R519( .b(gd), .in(net2570), .out(vout) );
    // rppwl  R528( .b(gd), .in(net2498), .out(vout) );
    // rppwl  R530( .b(gd), .in(net2497), .out(net2498) );
    // rppwl  R531( .b(gd), .in(net2496), .out(net2497) );
    // rppwl  R532( .b(gd), .in(vout2), .out(net2496) );
    // nfinhv  N1( .B(gd), .D(bias_10u), .G(ota_on_hvz), .S(gd) );
    // pfinhv  P518_200( .B(vph), .D(vph), .G(net2570), .S(vph) );
    // pfinhv  P518_199( .B(vph), .D(vph), .G(net2570), .S(vph) );
    // pfinhv  P518_198( .B(vph), .D(vph), .G(net2570), .S(vph) );
    // pfinhv  P518_197( .B(vph), .D(vph), .G(net2570), .S(vph) );
    // pfinhv  P518_196( .B(vph), .D(vph), .G(net2570), .S(vph) );
    // pfinhv  P518_195( .B(vph), .D(vph), .G(net2570), .S(vph) );
    // pfinhv  P518_194( .B(vph), .D(vph), .G(net2570), .S(vph) );
    // pfinhv  P518_193( .B(vph), .D(vph), .G(net2570), .S(vph) );
    // pfinhv  P518_192( .B(vph), .D(vph), .G(net2570), .S(vph) );
    // pfinhv  P518_191( .B(vph), .D(vph), .G(net2570), .S(vph) );
    // pfinhv  P518_190( .B(vph), .D(vph), .G(net2570), .S(vph) );
    // pfinhv  P518_189( .B(vph), .D(vph), .G(net2570), .S(vph) );
    // pfinhv  P518_188( .B(vph), .D(vph), .G(net2570), .S(vph) );
    // pfinhv  P518_187( .B(vph), .D(vph), .G(net2570), .S(vph) );
    // pfinhv  P518_186( .B(vph), .D(vph), .G(net2570), .S(vph) );
    // pfinhv  P518_185( .B(vph), .D(vph), .G(net2570), .S(vph) );
    // pfinhv  P518_184( .B(vph), .D(vph), .G(net2570), .S(vph) );
    // pfinhv  P518_183( .B(vph), .D(vph), .G(net2570), .S(vph) );
    // pfinhv  P518_182( .B(vph), .D(vph), .G(net2570), .S(vph) );
    // pfinhv  P518_181( .B(vph), .D(vph), .G(net2570), .S(vph) );
    // pfinhv  P518_180( .B(vph), .D(vph), .G(net2570), .S(vph) );
    // pfinhv  P518_179( .B(vph), .D(vph), .G(net2570), .S(vph) );
    // pfinhv  P518_178( .B(vph), .D(vph), .G(net2570), .S(vph) );
    // pfinhv  P518_177( .B(vph), .D(vph), .G(net2570), .S(vph) );
    // pfinhv  P518_176( .B(vph), .D(vph), .G(net2570), .S(vph) );
    // pfinhv  P518_175( .B(vph), .D(vph), .G(net2570), .S(vph) );
    // pfinhv  P518_174( .B(vph), .D(vph), .G(net2570), .S(vph) );
    // pfinhv  P518_173( .B(vph), .D(vph), .G(net2570), .S(vph) );
    // pfinhv  P518_172( .B(vph), .D(vph), .G(net2570), .S(vph) );
    // pfinhv  P518_171( .B(vph), .D(vph), .G(net2570), .S(vph) );
    // pfinhv  P518_170( .B(vph), .D(vph), .G(net2570), .S(vph) );
    sc_core::sc_signal_resolved supply_ok{"supply_ok", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved term_rep_ok{"term_rep_ok", sc_dt::SC_LOGIC_Z};

    void assign_supply_ok();
    void assign_term_rep_ok();
    void gate_cb_10u_ilptx_buf();
    void proc_always_152069();
    void proc_always_152078();
    void proc_always_152087();
};

#endif
