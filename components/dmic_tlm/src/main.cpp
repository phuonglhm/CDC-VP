#include <systemc>
#include <iostream>
#include "dmic.h"
#include "testbench.h"
using namespace sc_core;

int sc_main(int argc, char* argv[]) {
    PDM_Source mic("microphone");
    DmicTLM    dmic("dmic_peripheral");
    Host_CPU   cpu("host_processor");

    sc_signal<bool, SC_MANY_WRITERS> dmic_irq_line("dmic_irq");
    mic.initiator_socket.bind(dmic.pdm_target_socket);
    cpu.bus_socket.bind(dmic.bus_target_socket);
    dmic.irq_out.bind(dmic_irq_line);
    cpu.irq_in.bind(dmic_irq_line);

    std::cout << "Starting DMIC Simulation...\n\n";
    sc_start();
    std::cout << "\nSimulation finished successfully.\n";

    return 0;
}