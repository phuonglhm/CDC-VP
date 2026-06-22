#ifndef DMIC_TESTBENCH_H
#define DMIC_TESTBENCH_H

#include <systemc.h>
#include <tlm.h>
#include "tlm_utils/simple_initiator_socket.h"
#include <cmath>
#include <iostream>
#include "pdm_payload.h"
#include "dmic.h"
using namespace sc_core;

#define DMIC_BASE_ADDR 0x004200000ULL //40-bit

class PDM_Source : public sc_module
{
public:
    tlm_utils::simple_initiator_socket<PDM_Source> initiator_socket;

    SC_HAS_PROCESS(PDM_Source);
    PDM_Source(sc_module_name name) : sc_module(name)
    {
        SC_THREAD(stimulus_process);
    }

private:
    void stimulus_process();
};

class Host_CPU : public sc_module {
public:
    tlm_utils::simple_initiator_socket<Host_CPU> bus_socket;
    sc_in<bool> irq_in; // Interrupt from GIC
    sc_out<bool> reset_n; // Reset output to components

    SC_HAS_PROCESS(Host_CPU);
    Host_CPU(sc_module_name name) : sc_module(name) {
        SC_THREAD(cpu_firmware);
    }

private:
    uint32_t read_reg(uint64_t addr);
    void write_reg(uint64_t addr, uint32_t data);
    void cpu_firmware();
};

#endif