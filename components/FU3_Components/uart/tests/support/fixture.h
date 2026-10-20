#pragma once
#include <fu3_uart/uart.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <iostream>

struct Pins : sc_core::sc_module {
    sc_core::sc_signal<bool> reset{"reset"}, rx{"rx"}, tx{"tx"};
    sc_core::sc_signal<bool> cts{"cts"}, dsr{"dsr"}, ri{"ri"}, dcd{"dcd"};
    sc_core::sc_signal<bool> rts{"rts"}, dtr{"dtr"}, out1{"out1"}, out2{"out2"};
    sc_core::sc_signal<bool> irq{"irq"}, tx_req{"tx_req"}, rx_req{"rx_req"}, tx_clear{"tx_clear"}, rx_clear{"rx_clear"};
    explicit Pins(sc_core::sc_module_name name):sc_module(name) {
        rx=true; cts=true; dsr=true; ri=true; dcd=true;
    }
    void bind(fu3_uart::Uart& uart) {
        uart.reset_n(reset); uart.rxd(rx); uart.txd(tx);
        uart.cts_n(cts); uart.dsr_n(dsr); uart.ri_n(ri); uart.dcd_n(dcd);
        uart.rts_n(rts); uart.dtr_n(dtr); uart.out1_n(out1); uart.out2_n(out2);
        uart.irq(irq); uart.dma_tx_request(tx_req); uart.dma_rx_request(rx_req);
        uart.dma_tx_clear(tx_clear); uart.dma_rx_clear(rx_clear);
    }
};
struct Checks {
    unsigned checks=0, failures=0;
    bool finished=false;
    void expect(bool ok, const char* message) {
        ++checks;
        if (!ok) {++failures; std::cerr<<"FAIL @ "<<sc_core::sc_time_stamp()<<": "<<message<<'\n';}
    }
};
