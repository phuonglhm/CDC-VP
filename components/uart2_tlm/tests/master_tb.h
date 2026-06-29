#ifndef MASTER_TB_H
#define MASTER_TB_H

#include <systemc>
#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"
using namespace sc_core;


#define ADDR_HOST_UART0 0x10000000

class MasterTB : public sc_module {
public:
    tlm_utils::simple_initiator_socket<MasterTB> bus_initiator;

    sc_in<unsigned char> host0_tx_mon;
    sc_core::sc_out<unsigned char> host0_rx_drv;
    sc_in<bool> host0_irq_mon; // observe the UART -> PLIC interrupt line
    SC_HAS_PROCESS(MasterTB);
    MasterTB(sc_module_name name);

    static int failure_count(); // non-zero -> the test must fail

private:
    void test_thread();
    // Helper function 
    void do_transaction(tlm::tlm_command cmd, sc_dt::uint64 addr, uint32_t& data, bool wait_for_delay);
    void assert_equal(const std::string& test_name, uint32_t expected, uint32_t actual);
};

#endif