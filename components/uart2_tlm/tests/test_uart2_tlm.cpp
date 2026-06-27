#include <systemc>
#include "top.h"
#include <iostream>
#include "master_tb.h"
using namespace sc_core;

int sc_main(int argc, char* argv[]) {
    Top top("top_inst");
    MasterTB master("master_tb_inst");
    master.bus_initiator.bind(top.host_uart0.bus);
    master.host0_tx_mon.bind(top.host0_tx_sig);
    // bind master driver to top UART rx channel
    master.host0_rx_drv.bind(top.host_uart0.rx);
    master.host0_irq_mon.bind(top.host0_irq_sig);

    std::cout << "Starting simulation" << std::endl;
    sc_start();
    std::cout << "Simulation done" << std::endl;
    return MasterTB::failure_count() == 0 ? 0 : 1;
}
