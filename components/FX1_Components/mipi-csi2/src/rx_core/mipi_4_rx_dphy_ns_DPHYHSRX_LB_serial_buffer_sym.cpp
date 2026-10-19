//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSRX_LB_serial_buffer_sym
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYHSRX_LB_serial_buffer_sym.h"

mipi_4_rx_dphy_ns_DPHYHSRX_LB_serial_buffer_sym::mipi_4_rx_dphy_ns_DPHYHSRX_LB_serial_buffer_sym(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    I61.reset(new mipi_4_rx_dphy_ns_sc_invx12_lvt("I61"));
    I61->a(clk_m);
    I61->z(net423);
    I61->gd(gd);
    I61->vp(vp);
    sc_lvt_I46.reset(new mipi_4_rx_dphy_ns_sc_invx12_lvt("sc_lvt_I46"));
    sc_lvt_I46->a(clk_p);
    sc_lvt_I46->z(net405);
    sc_lvt_I46->gd(gd);
    sc_lvt_I46->vp(vp);
    I59.reset(new mipi_4_rx_dphy_ns_DPHYHSTX_serial_buffer_sym("I59"));
    I59->clk_m(net422);
    I59->clk_p(net420);
    I59->gd(gd);
    I59->sel_sym(sel_sym);
    I59->sym_a(sym_a);
    I59->sym_b(sym_b);
    I59->tra_a(tra_a);
    I59->tra_b(tra_b);
    I59->vp(vp);
    I59->data_ser_m(data_ser_m);
    I59->data_ser_p(data_ser_p);
    I60.reset(new mipi_4_rx_dphy_ns_sc_invx24_lvt("I60"));
    I60->a(net423);
    I60->z(net422);
    I60->gd(gd);
    I60->vp(vp);
    sc_lvt_out_inv_p.reset(new mipi_4_rx_dphy_ns_sc_invx24_lvt("sc_lvt_out_inv_p"));
    sc_lvt_out_inv_p->a(net405);
    sc_lvt_out_inv_p->z(net420);
    sc_lvt_out_inv_p->gd(gd);
    sc_lvt_out_inv_p->vp(vp);
}
