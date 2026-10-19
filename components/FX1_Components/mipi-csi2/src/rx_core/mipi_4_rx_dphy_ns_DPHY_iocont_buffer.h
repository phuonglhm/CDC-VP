//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHY_iocont_buffer
//--------------------------------------------------------------------------------------------------
#ifndef VL_MIPI_4_RX_DPHY_NS_DPHY_IOCONT_BUFFER_H
#define VL_MIPI_4_RX_DPHY_NS_DPHY_IOCONT_BUFFER_H

#include <memory>
#include <vector>
#include "vl/vl_rt.h"

class mipi_4_rx_dphy_ns_DPHY_iocont_buffer : public sc_core::sc_module {
public:
    sc_core::sc_in<sc_dt::sc_logic> a{"a"};
    sc_core::sc_in<sc_dt::sc_logic> enhv{"enhv"};
    sc_core::sc_in<sc_dt::sc_logic> tielow_hv{"tielow_hv"};
    sc_core::sc_out<sc_dt::sc_logic> y{"y"};
    sc_core::sc_in<sc_dt::sc_logic> gd{"gd"};
    sc_core::sc_in<sc_dt::sc_logic> vdd{"vdd"};
    sc_core::sc_in<sc_dt::sc_logic> vp{"vp"};
    sc_core::sc_in<sc_dt::sc_logic> vph{"vph"};
    sc_core::sc_in<sc_dt::sc_logic> vss{"vss"};

    SC_HAS_PROCESS(mipi_4_rx_dphy_ns_DPHY_iocont_buffer);
    explicit mipi_4_rx_dphy_ns_DPHY_iocont_buffer(sc_core::sc_module_name nm);

    // ---- internal state (public for hierarchical access / debugging)
    static constexpr double TU_ = 1000000.0;  // time unit (fs)
    static constexpr double TP_ = 1000000.0;  // time precision (fs)
    sc_core::sc_signal_resolved net126{"net126", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net128{"net128", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved out_inv{"out_inv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved out_inv1{"out_inv1", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net101{"net101", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved in_esd{"in_esd", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved enz{"enz", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved inv{"inv", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved net107{"net107", sc_dt::SC_LOGIC_Z};
    sc_core::sc_signal_resolved supply_ok{"supply_ok", sc_dt::SC_LOGIC_Z};
    // spfinhv  Pbuf( .D(inv), .G(in_esd), .S(net107), .B(vph) );
    // nfinhv  Nbuf( .B(gd), .D(inv), .G(in_esd), .S(net101) );
    // nfinhv  Nclamp( .B(gd), .D(in_esd), .G(tielow_hv), .S(gd) );
    // nfinhv  Nen( .B(gd), .D(enz), .G(enhv), .S(gd) );
    // nfinhv  Nhiz( .B(gd), .D(net101), .G(enhv), .S(gd) );
    // nfinhv  Ninv( .B(gd), .D(out_inv), .G(inv), .S(gd) );
    // nfinhv  Ninv1( .B(vss), .D(out_inv1), .G(out_inv), .S(vss) );
    // nfinhv  Ninv2( .B(vss), .D(y), .G(out_inv1), .S(vss) );
    // rppwl  R31( .b(gd), .in(net128), .out(net126) );
    // rppwl  R32( .b(gd), .in(a), .out(net128) );
    // rppwl  Resd( .b(gd), .in(net126), .out(in_esd) );
    // pfinhv  N19( .B(vph), .D(inv), .G(enhv), .S(vph) );
    // pfinhv  Pen( .B(vph), .D(enz), .G(enhv), .S(vph) );
    // pfinhv  Phiz( .B(vph), .D(net107), .G(enz), .S(vph) );
    // pfinhv  Pinv( .B(vp), .D(out_inv), .G(inv), .S(vp) );
    // pfinhv  Pinv1( .B(vdd), .D(out_inv1), .G(out_inv), .S(vdd) );
    // pfinhv  Pinv2( .B(vdd), .D(y), .G(out_inv1), .S(vdd) );
    sc_core::sc_signal<sc_dt::sc_logic> a_internal{"a_internal", vl::logic<1>().sl()};

    void assign_supply_ok();
    void gate_buf_enz();
    void gate_out_y_buf();
    void gate_pull_up_inv();
    void proc_initial_159432();
};

#endif
