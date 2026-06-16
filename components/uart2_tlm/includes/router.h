#ifndef ROUTER_H
#define ROUTER_H

#include <systemc>
#include "tlm.h"
#include "tlm_utils/simple_target_socket.h"
#include "tlm_utils/simple_initiator_socket.h"

#define ADDR_HOST_UART0 0x001A510000ULL
#define ADDR_HOST_UART1 0x001A520000ULL
#define ADDR_EXT_UART0 0x40002000
#define ADDR_SEC_UART0 0x50090000

class RouterTLM : public sc_core::sc_module {
    public:
    tlm_utils::simple_target_socket<RouterTLM> bus_in;
    tlm_utils::simple_initiator_socket<RouterTLM> host_uart0_out;
    tlm_utils::simple_initiator_socket<RouterTLM> host_uart1_out;
    tlm_utils::simple_initiator_socket<RouterTLM> ext_uart0_out;
    tlm_utils::simple_initiator_socket<RouterTLM> sec_uart0_out;

    SC_HAS_PROCESS(RouterTLM);
    RouterTLM(sc_core::sc_module_name name);
    
    private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
};

#endif
