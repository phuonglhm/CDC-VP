#include "top.h"

Top::Top(sc_module_name name) : 
    sc_module(name),
    host_uart0("host_uart0")
{
    host_uart0.tx.bind(host0_tx_sig);
}
//for future use with multiple uarts