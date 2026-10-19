//--------------------------------------------------------------------------------------------------
// module mipi_4_rx_dphy_ns_rtune_comp
//--------------------------------------------------------------------------------------------------
#include "mipi_4_rx_dphy_ns_rtune_comp.h"

mipi_4_rx_dphy_ns_rtune_comp::mipi_4_rx_dphy_ns_rtune_comp(sc_core::sc_module_name nm) : sc_core::sc_module(nm) {
    SC_METHOD(assign_ref_dac);
    sensitive << vin;
    SC_METHOD(assign_supply_ok);
    sensitive << vph << gd;
    SC_METHOD(assign_rt_pwron_h_dly);
    sensitive << rt_pwron_h;
    SC_METHOD(assign_rt_pwron_h_dly_upd);
    sensitive << assign_rt_pwron_h_dly_ev;
    dont_initialize();
    SC_METHOD(assign_disable_message);
    SC_METHOD(gate_buf_out);
    sensitive << vph << gd;
    SC_METHOD(proc_initial_161362);
    SC_THREAD(proc_always_161367);
    set_stack_size(0x20000);
    SC_METHOD(proc_always_161410);
    sensitive << supply_ok << rt_pwron_h_dly << latch_reg << ref_dac;
    SC_METHOD(proc_always_161432);
    sensitive << supply_ok << rt_pwron_h_dly << r_h << latch_enable << comp_result_h_int;
    SC_METHOD(proc_always_161459);
    sensitive << r_h << vin << latch_reg;
}

void mipi_4_rx_dphy_ns_rtune_comp::assign_ref_dac() {
    vl::wr(ref_dac, vl::mux(vl::rd(vin).slice<6>(4).truth(), vl::logic<10>(0x202ULL), vl::logic<10>(0x6ULL)));
}

void mipi_4_rx_dphy_ns_rtune_comp::assign_supply_ok() {
    vl::wr(supply_ok, vl::land(vl::ceq(vl::rd(vph), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))));
}

void mipi_4_rx_dphy_ns_rtune_comp::assign_rt_pwron_h_dly() {
    const vl::logic<1> nv = vl::rd(rt_pwron_h);
    const sc_core::sc_time d = vl::dly(100.0, TU_, TP_);
    if (nv == assign_rt_pwron_h_dly_pv && !assign_rt_pwron_h_dly_ev_pend) return;
    assign_rt_pwron_h_dly_pv = nv;
    assign_rt_pwron_h_dly_ev.cancel();
    assign_rt_pwron_h_dly_ev_pend = true;
    assign_rt_pwron_h_dly_ev.notify(d);
}

void mipi_4_rx_dphy_ns_rtune_comp::assign_rt_pwron_h_dly_upd() {
    assign_rt_pwron_h_dly_ev_pend = false;
    vl::wr(rt_pwron_h_dly, assign_rt_pwron_h_dly_pv);
}

void mipi_4_rx_dphy_ns_rtune_comp::assign_disable_message() {
    vl::wr(disable_message, vl::logic<32>(0x1ULL).ext<1>());
}

void mipi_4_rx_dphy_ns_rtune_comp::gate_buf_out() {
    const vl::logic<1> v = vl::bufif1_(vl::logic<1>(0x1ULL), vl::land(vl::ceq(vl::rd(vph), vl::logic<1>(0x1ULL)), vl::ceq(vl::rd(gd), vl::logic<1>(0x0ULL))));
    vl::wr(nbias, v);
}

void mipi_4_rx_dphy_ns_rtune_comp::proc_initial_161362() {
    vl::logic<1> comp_result_h_int_sh = vl::rd(comp_result_h_int);
    vl::logic<1> latch_enable_sh = vl::rd(latch_enable);
    latch_enable_sh = vl::logic<1>(0x0ULL);
    vl::wr(latch_enable, latch_enable_sh);
    comp_result_h_int_sh = vl::random().ext<1>();
    vl::wr(comp_result_h_int, comp_result_h_int_sh);
}

void mipi_4_rx_dphy_ns_rtune_comp::proc_always_161367() {
    for (;;) {
        wait(rt_pwron_h.posedge_event());
        vl::wr(latch_enable, vl::logic<1>(0x0ULL));
        while (!(vl::rd(rt_pwron_h_dly).test())) {
            wait(rt_pwron_h_dly.value_changed_event());
        }
        wait(r_h.posedge_event());
        t1 = vl::realtime(TU_);
        for (;;) {
            if (vl::rd(rt_pwron_h_dly).test()) {
                wait(r_h.negedge_event() | rt_pwron_h.negedge_event() | rt_pwron_h_dly.negedge_event());
                t2 = vl::realtime(TU_);
                if (vl::land(vl::land(vl::lbool((t2 - t1) < vl::to_real(vl::logic<32>(0x64ULL), true)), vl::lnot(vl::rd(disable_message))), vl::rd(rt_pwron_h)).test()) {
                    vl::display(this, TU_, true, "%.3fns - Error! [%m] ", {vl::A(vl::realtime(TU_)), vl::A("r_h reset timing is less than 256ns.")});
                }
                if ((vl::land(vl::land(vl::lbool((t2 - t1) > vl::to_real(vl::logic<32>(0x2710ULL), true)), vl::lnot(vl::rd(disable_message))), vl::rd(rt_pwron_h))).test()) {
                    vl::display(this, TU_, true, "%.3fns - Error! [%m] ", {vl::A(vl::realtime(TU_)), vl::A("r_h reset timing is less than 256ns.")});
                }
                if (vl::land(vl::rd(rt_pwron_h_dly), vl::rd(rt_pwron_h)).test()) {
                    wait(vl::dly(256.0, TU_, TP_));
                    vl::wr(latch_enable, vl::logic<1>(0x1ULL));
                    wait(vl::dly(256.0, TU_, TP_));
                    if (vl::land(vl::land(vl::rd(r_h), vl::lnot(vl::rd(disable_message))), vl::rd(rt_pwron_h)).test()) {
                        vl::display(this, TU_, true, "%.3fns - Error! [%m] ", {vl::A(vl::realtime(TU_)), vl::A("The comparator latching window is not wide enough.")});
                    }
                    if (vl::land(vl::rd(rt_pwron_h_dly), vl::rd(rt_pwron_h)).test()) {
                        wait(r_h.posedge_event() | rt_pwron_h.negedge_event() | rt_pwron_h_dly.negedge_event());
                    }
                    vl::wr(latch_enable, vl::logic<1>(0x0ULL));
                    t1 = vl::realtime(TU_);
                    if ((vl::land(vl::land(vl::lbool((t1 - t2) > vl::to_real(vl::logic<32>(0x2710ULL), true)), vl::lnot(vl::rd(disable_message))), vl::rd(rt_pwron_h))).test()) {
                        vl::display(this, TU_, true, "%.3fns - Error! [%m] ", {vl::A(vl::realtime(TU_)), vl::A("r_h compare period is more than maximum limit.")});
                    }
                    if (vl::land(vl::land(vl::lbool((t1 - t2) < vl::to_real(vl::logic<32>(0x64ULL), true)), vl::lnot(vl::rd(disable_message))), vl::rd(rt_pwron_h)).test()) {
                        vl::display(this, TU_, true, "%.3fns - Error! [%m] ", {vl::A(vl::realtime(TU_)), vl::A("r_h compare period is less than minimum limit.")});
                    }
                }
            } else {
                vl::wr(latch_enable, vl::logic<1>(0x0ULL));
                t1 = vl::to_real(vl::logic<32>(0x0ULL), true);
                t2 = vl::to_real(vl::logic<32>(0x0ULL), true);
                wait(rt_pwron_h_dly.posedge_event());
            }
        }
    }
}

void mipi_4_rx_dphy_ns_rtune_comp::proc_always_161410() {
    vl::logic<1> comp_result_h_int_sh_2 = vl::rd(comp_result_h_int);
    if (vl::rd(supply_ok).truth_val() == 0) {
        comp_result_h_int_sh_2 = vl::random().ext<1>();
        vl::wr(comp_result_h_int, comp_result_h_int_sh_2);
    } else if (vl::ceq(vl::rd(rt_pwron_h_dly), vl::logic<1>(0x1ULL)).test()) {
        if (vl::gt(vl::rd(latch_reg).ext<32>(), vl::rd(ref_dac).ext<32>() + vl::logic<32>(0x1ULL)).test()) {
            comp_result_h_int_sh_2 = vl::logic<1>(0x0ULL);
            vl::wr(comp_result_h_int, comp_result_h_int_sh_2);
        } else if (vl::land(vl::eq(vl::rd(ref_dac), vl::logic<10>(0xeULL)), vl::gt(vl::rd(latch_reg), vl::rd(ref_dac))).test()) {
            comp_result_h_int_sh_2 = vl::logic<1>(0x0ULL);
            vl::wr(comp_result_h_int, comp_result_h_int_sh_2);
        } else if (vl::land(vl::eq(vl::rd(ref_dac), vl::logic<10>(0x0ULL)), vl::le(vl::rd(latch_reg), vl::rd(ref_dac))).test()) {
            comp_result_h_int_sh_2 = vl::logic<1>(0x1ULL);
            vl::wr(comp_result_h_int, comp_result_h_int_sh_2);
        } else if (vl::lt(vl::rd(latch_reg), vl::rd(ref_dac)).test()) {
            comp_result_h_int_sh_2 = vl::logic<1>(0x1ULL);
            vl::wr(comp_result_h_int, comp_result_h_int_sh_2);
        } else {
            comp_result_h_int_sh_2 = vl::random().ext<1>();
            vl::wr(comp_result_h_int, comp_result_h_int_sh_2);
        }
    } else {
        comp_result_h_int_sh_2 = vl::random().ext<1>();
        vl::wr(comp_result_h_int, comp_result_h_int_sh_2);
    }
}

void mipi_4_rx_dphy_ns_rtune_comp::proc_always_161432() {
    if (vl::rd(supply_ok).truth_val() == 0) {
        vl::wr(comp_result_h, vl::random().ext<1>());
    } else {
        if (vl::rd(rt_pwron_h_dly).truth_val() == 0) {
            if (vl::rd(r_h).test()) {
                vl::wr(comp_result_h, vl::logic<1>(0x0ULL));
            } else {
                vl::wr(comp_result_h, vl::logic<1>(0x1ULL));
            }
        } else {
            if (vl::rd(r_h).test()) {
                if (vl::ceq(vl::rd(latch_enable), vl::logic<1>(0x1ULL)).test()) {
                    vl::wr(comp_result_h, vl::logic<1>(0x0ULL));
                } else {
                    vl::wr(comp_result_h, vl::logic<1>(0x1ULL));
                }
            } else {
                if (vl::ceq(vl::rd(latch_enable), vl::logic<1>(0x1ULL)).test()) {
                    vl::wr(comp_result_h, vl::rd(comp_result_h_int));
                } else {
                    vl::wr(comp_result_h, vl::random().ext<1>());
                }
            }
        }
    }
}

void mipi_4_rx_dphy_ns_rtune_comp::proc_always_161459() {
    if (vl::rd(r_h).test()) {
        vl::wr(latch_reg, vl::rd(vin));
    } else {
        vl::wr(latch_reg, vl::rd(latch_reg));
    }
}
