#define TOP_H

#include <systemc>
#include "router.h"
#include "uart.h"
using namespace sc_core;

class Top : public sc_module {
    public:
    RouterTLM router;
    UartTLM host_uart0;
    UartTLM host_uart1;
    UartTLM ext_uart0;
    UartTLM sec_uart0;

    sc_signal<unsigned char> host0_tx_sig;
    sc_signal<unsigned char> host1_tx_sig;
    sc_signal<unsigned char> ext0_tx_sig;
    sc_signal<unsigned char> sec0_tx_sig;

    SC_HAS_PROCESS(Top);
    Top(sc_module_name name);
};
#endif
