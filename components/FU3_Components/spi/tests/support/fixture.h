#pragma once
#include <fu3_spi/spi.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <iostream>
struct Pins:sc_core::sc_module {
    sc_core::sc_signal<bool> reset_n{"reset_n"},apb_reset_n{"apb_reset_n"};
    sc_core::sc_signal<bool> si{"si"},select{"select"},mi{"mi"},input{"input"},ta{"ta"},ra{"ra"};
    sc_core::sc_signal<bool> sck{"sck"},mosi{"mosi"},miso{"miso"},so{"so"},sso{"sso"},mo{"mo"},mio{"mio"},irq{"irq"},tr{"tr"},rr{"rr"};
    sc_core::sc_signal<sc_dt::sc_uint<4>> ss{"ss"};
    explicit Pins(sc_core::sc_module_name n):sc_module(n){select=true;}
    void bind(fu3_spi::Spi& d,bool loopback=true){
        d.reset_n(reset_n);d.apb_reset_n(apb_reset_n);d.sck_in(si);d.ss_n_in(select);d.mosi_in(mi);d.miso_in(loopback?mosi:input);
        d.dma_tx_ack(ta);d.dma_rx_ack(ra);d.sck(sck);d.ss_n(ss);d.mosi(mosi);d.miso(miso);
        d.sck_oe(so);d.ss_oe(sso);d.mosi_oe(mo);d.miso_oe(mio);d.irq(irq);d.dma_tx_req(tr);d.dma_rx_req(rr);
    }
};
struct Checks {
    unsigned checks=0,failures=0;bool finished=false;
    void expect(bool ok,const char* msg){++checks;if(!ok){++failures;std::cerr<<"FAIL @ "<<sc_core::sc_time_stamp()<<": "<<msg<<'\n';}}
};
