//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_DPHYHSTX_serial_buffer_sym
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_DPHYHSTX_serial_buffer_sym.h"

mipi_4_rx_dphy_ns_DPHYHSTX_serial_buffer_sym::mipi_4_rx_dphy_ns_DPHYHSTX_serial_buffer_sym(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    mux_m.reset(new mipi_4_rx_dphy_ns_DPHYHSTX_serial_mux2x_diff_buf("mux_m"));
    mux_m->a(data_a);
    mux_m->b(data_b);
    mux_m->gd(gd);
    mux_m->sa(clk_p);
    mux_m->sb(clk_m);
    mux_m->vp(vp);
    mux_m->z(data_ser_m_mux);
    mux_p.reset(new mipi_4_rx_dphy_ns_DPHYHSTX_serial_mux2x_diff_buf("mux_p"));
    mux_p->a(data_an);
    mux_p->b(data_bn);
    mux_p->gd(gd);
    mux_p->sa(clk_p);
    mux_p->sb(clk_m);
    mux_p->vp(vp);
    mux_p->z(data_ser_p_mux);
    I52.reset(new mipi_4_rx_dphy_ns_DPHYHSTX_drv_mux("I52"));
    I52->deemph(tra_a);
    I52->gd(gd);
    I52->sel_preemph(sel_tra);
    I52->sel_sym(sel_sym);
    I52->sym(sym_a);
    I52->vp(vp);
    I52->z(data_a);
    I53.reset(new mipi_4_rx_dphy_ns_DPHYHSTX_drv_mux("I53"));
    I53->deemph(tra_b);
    I53->gd(gd);
    I53->sel_preemph(sel_tra);
    I53->sel_sym(sel_sym);
    I53->sym(sym_b);
    I53->vp(vp);
    I53->z(data_b);
    sc_lvt_I46.reset(new mipi_4_rx_dphy_ns_sc_invx12_lvt("sc_lvt_I46"));
    sc_lvt_I46->a(data_a);
    sc_lvt_I46->z(data_an);
    sc_lvt_I46->gd(gd);
    sc_lvt_I46->vp(vp);
    sc_lvt_I47.reset(new mipi_4_rx_dphy_ns_sc_invx12_lvt("sc_lvt_I47"));
    sc_lvt_I47->a(data_b);
    sc_lvt_I47->z(data_bn);
    sc_lvt_I47->gd(gd);
    sc_lvt_I47->vp(vp);
    sc_lvt_I58.reset(new mipi_4_rx_dphy_ns_sc_invx6_lvt("sc_lvt_I58"));
    sc_lvt_I58->a(sel_sym);
    sc_lvt_I58->z(sel_tra);
    sc_lvt_I58->gd(gd);
    sc_lvt_I58->vp(vp);
    sc_lvt_out_inv_m.reset(new mipi_4_rx_dphy_ns_sc_invx24_lvt("sc_lvt_out_inv_m"));
    sc_lvt_out_inv_m->a(data_ser_p_mux);
    sc_lvt_out_inv_m->z(data_ser_p);
    sc_lvt_out_inv_m->gd(gd);
    sc_lvt_out_inv_m->vp(vp);
    sc_lvt_out_inv_p.reset(new mipi_4_rx_dphy_ns_sc_invx24_lvt("sc_lvt_out_inv_p"));
    sc_lvt_out_inv_p->a(data_ser_m_mux);
    sc_lvt_out_inv_p->z(data_ser_m);
    sc_lvt_out_inv_p->gd(gd);
    sc_lvt_out_inv_p->vp(vp);
}
