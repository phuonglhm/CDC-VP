#include "top.h"

Top::Top(sc_module_name name) : 
    sc_module(name),
    router("router"),
    host_uart0("host_uart0"),
    host_uart1("host_uart1"),
    ext_uart0("ext_uart0"),
    sec_uart0("sec_uart0")
{
    router.host_uart0_out.bind(host_uart0.bus);
    router.host_uart1_out.bind(host_uart1.bus);
    router.ext_uart0_out.bind(ext_uart0.bus);
    router.sec_uart0_out.bind(sec_uart0.bus);

    host_uart0.tx.bind(host0_tx_sig);
    host_uart1.tx.bind(host1_tx_sig);
    ext_uart0.tx.bind(ext0_tx_sig);
    sec_uart0.tx.bind(sec0_tx_sig);
}
