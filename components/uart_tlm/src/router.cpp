#include "router.h"
using namespace sc_core;

RouterTLM::RouterTLM(sc_module_name name) : sc_module(name),
    bus_in("bus_in"),
    host_uart0_out("host_uart0_out"),
    host_uart1_out("host_uart1_out"),
    ext_uart0_out("ext_uart0_out"),
    sec_uart0_out("sec_uart0_out") {
    bus_in.register_b_transport(this, &RouterTLM::b_transport);
}

void RouterTLM::b_transport(tlm::tlm_generic_payload& trans, sc_time& delay) {
    sc_dt::uint64 addr = trans.get_address();
    sc_dt::uint64 offset = 0;

    if (addr >= ADDR_HOST_UART0 && addr <= ADDR_HOST_UART0 + 0x10000) { //64KB
       offset = addr - ADDR_HOST_UART0;
       trans.set_address(offset);
       host_uart0_out->b_transport(trans,delay);
    } else if (addr >= ADDR_HOST_UART1 && addr <= ADDR_HOST_UART0 + 0x10000) { //64KB
        offset = addr - ADDR_HOST_UART1;
        trans.set_address(offset);
        host_uart1_out->b_transport(trans,delay);
    } else if (addr >= ADDR_EXT_UART0 && addr <=  ADDR_EXT_UART0 + 0x100) {
        offset = addr - ADDR_EXT_UART0;
        trans.set_address(offset);
        ext_uart0_out->b_transport(trans,delay);
    } else if (addr >= ADDR_SEC_UART0 && addr <=  ADDR_SEC_UART0 + 0x100) {
        offset = addr - ADDR_SEC_UART0;
        trans.set_address(offset);
        sec_uart0_out->b_transport(trans,delay);
    } else {
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }
    trans.set_address(addr);
}
