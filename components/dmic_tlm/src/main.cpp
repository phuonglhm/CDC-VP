//author: linhtk55-fpt

#include <systemc>
#include <iostream>
#include "dmic.h"
#include "testbench.h"
using namespace sc_core;

int sc_main(int argc, char* argv[]) {
    TestBench mic("microphone");
    DmicTLM dmic("dmic", 64);//decimate by 64
    PCM_Monitor monitor("monitor");
    sc_fifo<int> pcm_fifo("pcm_buffer", 128);
    mic.initiator_socket.bind(dmic.target_socket);
    dmic.pcm_out_port.bind(pcm_fifo);
    monitor.pcm_in_port.bind(pcm_fifo);
    std::cout << "Starting simulation" << std::endl;
    sc_start();
    std::cout << "Simulation done" << std::endl;
    return 0;
}