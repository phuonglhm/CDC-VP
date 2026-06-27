#ifndef TOP_H
#define TOP_H

#include <systemc>
#include "uart.h"
using namespace sc_core;

#include "uart.h"

class Top : public sc_module {
    public:
    UartTLM host_uart0;

    sc_signal<unsigned char> host0_tx_sig;
    sc_signal<bool> host0_irq_sig;

    // SC_HAS_PROCESS(Top);
    Top(sc_module_name name);
};
#endif