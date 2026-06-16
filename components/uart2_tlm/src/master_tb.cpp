#include "master_tb.h"
#include "uart.h"
using namespace sc_core;
using namespace std;

MasterTB::MasterTB(sc_module_name name) : sc_module(name), bus_initiator("bus_initiator"), host0_tx_mon("host0_tx_mon"), host0_rx_drv("host0_rx_drv"), host1_tx_mon("host1_tx_mon") {
    SC_THREAD(test_thread);
}
void MasterTB::assert_equal(const std::string& test_name, uint32_t expected, uint32_t actual) {
    std::cout << "[" << test_name << "] ";
    if (expected == actual) {
        cout << "\033[1;32mPASSED\033[0m (Value: 0x" << std::hex << actual << ")" << std::endl;
    } else {
        cout << "\033[1;31mFAILED\033[0m (Expected: 0x" << std::hex << expected 
                  << ", Got: 0x" << actual << ")" << std::endl;
    }
}
void MasterTB::do_transaction(tlm::tlm_command cmd, sc_dt::uint64 addr, uint32_t& data, bool wait_for_delay = true) {
    tlm::tlm_generic_payload trans;
    sc_time delay = SC_ZERO_TIME;

    unsigned char byte_en[] = {0xff, 0xff, 0xff, 0xff};
    
    trans.set_command(cmd);
    trans.set_address(addr);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
    trans.set_data_length(4); // Required by your UART model
    trans.set_streaming_width(4);
    trans.set_byte_enable_ptr(byte_en);
    trans.set_byte_enable_length(4);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    bus_initiator->b_transport(trans, delay);
    if (wait_for_delay) wait(delay); 

    if (trans.is_response_error()) {
        SC_REPORT_ERROR("TLM", "Transaction returned an error response!");
    }
}

void MasterTB::test_thread() {
    uint32_t data;
    sc_dt::uint64 host0_base = 0x1A510000ULL;
    cout << "@" << sc_time_stamp() << " Starting Host UART0 Tests..." << endl;

    // Test 1: RX Half-Full Raw Interrupt Status (UARTRIS)
    for (int i = 0; i < 8; i++) {
        host0_rx_drv.write('A' + i);
        wait(1, sc_core::SC_NS); // Allow rxMethod evaluation time
    }
    // Read UARTRIS: Should show UART_RXRIS (0x10)
    do_transaction(tlm::TLM_READ_COMMAND, host0_base + UARTRIS, data);
    assert_equal("CASE 1: RX Half-Full Raw Interrupt (UARTRIS)", UART_RXRIS, data);
    
    //Test 2: RX Masked Interrupt Behavior (UARTMIS & UARTIMSC)
    // UARTMIS (Masked Status) should currently be 0
    do_transaction(tlm::TLM_READ_COMMAND, host0_base + UARTMIS, data);
    assert_equal("CASE 2a: Masked Interrupt before enabling mask", 0x00, data);

    // Write to UARTIMSC to enable RX interrupt masking (0x10)
    data = UART_RXRIS;
    do_transaction(tlm::TLM_WRITE_COMMAND, host0_base + UARTIMSC, data);

    //UARTMIS should propagate and equal UART_RXRIS (0x10)
    do_transaction(tlm::TLM_READ_COMMAND, host0_base + UARTMIS, data);
    assert_equal("CASE 2b: Masked Interrupt after enabling mask", UART_RXRIS, data);

    //Test 3: RX FIFO Full & Saturation Overflow
    // Pump 10 more bytes (8 existing + 10 = 18 bytes total. FIFO caps at 16)
    for (int i = 0; i < 10; i++) {
        host0_rx_drv.write('Z');
        wait(1, sc_core::SC_NS);
    }
    // Check Flag Register (UARTFR): Expect RXFF (0x40) to be set, and RXFE (0x10) cleared
    do_transaction(tlm::TLM_READ_COMMAND, host0_base + UARTFR, data);
    assert_equal("CASE 3a: RX FIFO Full Flag (UARTFR & UART_RXFF)", UART_RXFF, (data & UART_RXFF));
    // Read out data 16 times to verify saturation didn't breach internal queue limit
    int read_count = 0;
    while (true) {
        do_transaction(tlm::TLM_READ_COMMAND, host0_base + UARTFR, data);
        if (data & UART_RXFE) break; // Break when empty
        
        do_transaction(tlm::TLM_READ_COMMAND, host0_base + UARTDR, data);
        read_count++;
    }
    assert_equal("CASE 3b: RX Queue Capped at Max Depth (16)", 16, read_count);
    //Test 4: TX FIFO Full Flag (UART_TXFF)
    data = 'F';
    for (int i = 0; i < 16; i++) {
        do_transaction(tlm::TLM_WRITE_COMMAND, host0_base + UARTDR, data, false);
    }
    // Read Flag Register: Expect TXFF (0x20) flag to be raised
    do_transaction(tlm::TLM_READ_COMMAND, host0_base + UARTFR, data, false);
    assert_equal("CASE 4: TX FIFO Full Flag (UARTFR & UART_TXFF)", UART_TXFF, (data & UART_TXFF));
    //allow busThread to process and empty out the FIFO
    wait(170, sc_core::SC_NS);

    cout << "Tests Complete!" << endl;
    sc_stop();
}
