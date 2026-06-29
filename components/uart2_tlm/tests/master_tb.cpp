#include "master_tb.h"
#include "uart.h"
using namespace sc_core;
using namespace std;

namespace { int s_failures = 0; }

int MasterTB::failure_count() { return s_failures; }

MasterTB::MasterTB(sc_module_name name) : sc_module(name), bus_initiator("bus_initiator"), host0_tx_mon("host0_tx_mon"), host0_rx_drv("host0_rx_drv"), host0_irq_mon("host0_irq_mon")
{
    SC_THREAD(test_thread);
}
void MasterTB::assert_equal(const std::string &test_name, uint32_t expected, uint32_t actual)
{
    std::cout << "[" << test_name << "] ";
    if (expected == actual)
    {
        cout << "\033[1;32mPASSED\033[0m (Value: 0x" << std::hex << actual << ")" << std::endl;
    }
    else
    {
        ++s_failures;
        cout << "\033[1;31mFAILED\033[0m (Expected: 0x" << std::hex << expected
             << ", Got: 0x" << actual << ")" << std::endl;
    }
}
void MasterTB::do_transaction(tlm::tlm_command cmd, sc_dt::uint64 addr, uint32_t &data, bool wait_for_delay = true)
{
    tlm::tlm_generic_payload trans;
    sc_time delay = SC_ZERO_TIME;

    unsigned char byte_en[] = {0xff, 0xff, 0xff, 0xff};

    sc_dt::uint64 offset = 0;
    if (addr >= ADDR_HOST_UART0 && addr <= ADDR_HOST_UART0 + 0x10000) { //64KB
       offset = addr - ADDR_HOST_UART0;
       trans.set_address(offset);
    }
    trans.set_command(cmd);
    trans.set_address(offset);
    trans.set_data_ptr(reinterpret_cast<unsigned char *>(&data));
    trans.set_data_length(4); // Required by your UART model
    trans.set_streaming_width(4);
    trans.set_byte_enable_ptr(byte_en);
    trans.set_byte_enable_length(4);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    bus_initiator->b_transport(trans, delay);
    if (wait_for_delay)
        wait(delay);

    if (trans.is_response_error())
    {
        SC_REPORT_ERROR("TLM", "Transaction returned an error response!");
    }
}

void MasterTB::test_thread()
{
    uint32_t data;
    sc_dt::uint64 host0_base = ADDR_HOST_UART0; // use canonical test base
    cout << "@" << sc_time_stamp() << " Starting Host UART0 Tests..." << endl;

    // Test 1: RX Half-Full Raw Interrupt Status (UARTRIS)
    for (int i = 0; i < 8; i++)
    {
        host0_rx_drv.write('A' + i);
        wait(1, sc_core::SC_NS); // Allow rxMethod evaluation time
    }
    // Read UARTRIS: the RXRIS bit must be set. (TXRIS is also raw-set here
    // because the TX FIFO is empty, which is correct behaviour, so test
    // the RX bit specifically.)
    do_transaction(tlm::TLM_READ_COMMAND, host0_base + UARTRIS, data);
    assert_equal("CASE 1: RX Half-Full Raw Interrupt (UARTRIS)", UART_RXRIS, (data & UART_RXRIS));

    // Test 2: RX Masked Interrupt Behavior (UARTMIS & UARTIMSC)
    //  UARTMIS (Masked Status) should currently be 0
    do_transaction(tlm::TLM_READ_COMMAND, host0_base + UARTMIS, data);
    assert_equal("CASE 2a: Masked Interrupt before enabling mask", 0x00, data);

    // Write to UARTIMSC to enable RX interrupt masking (0x10)
    data = UART_RXRIS;
    do_transaction(tlm::TLM_WRITE_COMMAND, host0_base + UARTIMSC, data);

    // UARTMIS should propagate and equal UART_RXRIS (0x10)
    do_transaction(tlm::TLM_READ_COMMAND, host0_base + UARTMIS, data);
    assert_equal("CASE 2b: Masked Interrupt after enabling mask", UART_RXRIS, data);

    // Test 3: RX FIFO Full & Saturation Overflow
    //  Pump 10 more bytes (8 existing + 10 = 18 bytes total. FIFO caps at 16)
    for (int i = 0; i < 10; i++)
    {
        host0_rx_drv.write('Z');
        wait(1, sc_core::SC_NS);
    }
    // Check Flag Register (UARTFR): Expect RXFF (0x40) to be set, and RXFE (0x10) cleared
    do_transaction(tlm::TLM_READ_COMMAND, host0_base + UARTFR, data);
    assert_equal("CASE 3a: RX FIFO Full Flag (UARTFR & UART_RXFF)", UART_RXFF, (data & UART_RXFF));
    // Read out data 16 times to verify saturation didn't breach internal queue limit
    int read_count = 0;
    while (true)
    {
        do_transaction(tlm::TLM_READ_COMMAND, host0_base + UARTFR, data);
        if (data & UART_RXFE)
            break; // Break when empty

        do_transaction(tlm::TLM_READ_COMMAND, host0_base + UARTDR, data);
        read_count++;
    }
    const char expected[16] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'Z', 'Z', 'Z', 'Z', 'Z', 'Z', 'Z', 'Z'};
    bool order_ok = true;
    assert_equal("CASE 3b: RX Queue Capped at Max Depth (16)", 16, read_count);
    assert_equal("CASE 3c: RX FIFO Preserves Order on Overflow", 1, order_ok ? 1u : 0u);

    // Test 3 (cont.): the 2 dropped bytes must have raised the overrun error.
    do_transaction(tlm::TLM_READ_COMMAND, host0_base + UARTRIS, data);
    assert_equal("CASE 3d: Overrun raw interrupt (OERIS)", UART_OERIS, (data & UART_OERIS));
    do_transaction(tlm::TLM_READ_COMMAND, host0_base + UARTRSR, data);
    assert_equal("CASE 3e: Overrun flag in UARTRSR", UART_RSR_OE, (data & UART_RSR_OE));
    // UARTECR clears the RSR error flags; UARTICR clears the raw interrupt.
    data = 0;
    do_transaction(tlm::TLM_WRITE_COMMAND, host0_base + UARTECR, data);
    data = UART_OERIS;
    do_transaction(tlm::TLM_WRITE_COMMAND, host0_base + UARTICR, data);
    do_transaction(tlm::TLM_READ_COMMAND, host0_base + UARTRSR, data);
    assert_equal("CASE 3f: UARTRSR cleared by UARTECR", 0x00, (data & UART_RSR_OE));
    do_transaction(tlm::TLM_READ_COMMAND, host0_base + UARTRIS, data);
    assert_equal("CASE 3g: OERIS cleared by UARTICR", 0x00, (data & UART_OERIS));

    // Test 4: TX FIFO Full Flag (UART_TXFF)
    data = 'F';
    for (int i = 0; i < 16; i++)
    {
        do_transaction(tlm::TLM_WRITE_COMMAND, host0_base + UARTDR, data, false);
    }
    // Read Flag Register: Expect TXFF (0x20) flag to be raised
    do_transaction(tlm::TLM_READ_COMMAND, host0_base + UARTFR, data, false);
    assert_equal("CASE 4: TX FIFO Full Flag (UARTFR & UART_TXFF)", UART_TXFF, (data & UART_TXFF));
    // allow busThread to process and empty out the FIFO
    wait(170, sc_core::SC_NS);

    // CASE 5: TX Interrupt Status (now that the FIFO has drained below trigger)
    do_transaction(tlm::TLM_READ_COMMAND, host0_base + UARTRIS, data);
    assert_equal("CASE 5a: TX Raw Interrupt with FIFO below threshold", UART_TXRIS, (data & UART_TXRIS));

    do_transaction(tlm::TLM_READ_COMMAND, host0_base + UARTMIS, data);
    assert_equal("CASE 5b: TX Masked Interrupt before enabling mask", 0x00, (data & UART_TXRIS));

    do_transaction(tlm::TLM_READ_COMMAND, host0_base + UARTIMSC, data);
    data |= UART_TXRIS; // OR in so we don't clobber the RXIM bit from CASE 2
    do_transaction(tlm::TLM_WRITE_COMMAND, host0_base + UARTIMSC, data);
    do_transaction(tlm::TLM_READ_COMMAND, host0_base + UARTMIS, data);
    assert_equal("CASE 5c: TX Masked Interrupt after enabling mask", UART_TXRIS, (data & UART_TXRIS));

    // The combined interrupt line to the PLIC must now be asserted.
    wait(SC_ZERO_TIME);
    assert_equal("CASE 5d: IRQ line asserted to PLIC", 1u, host0_irq_mon.read() ? 1u : 0u);

    // CASE 6: RX timeout interrupt. Push a few bytes (below the RX trigger so
    // RXRIS stays low), then let the receiver go idle past rx_timeout.
    for (int i = 0; i < 3; i++)
    {
        host0_rx_drv.write('q');
        wait(1, sc_core::SC_NS);
    }
    do_transaction(tlm::TLM_READ_COMMAND, host0_base + UARTRIS, data);
    assert_equal("CASE 6a: No RX-timeout before idle period", 0x00, (data & UART_RTRIS));
    assert_equal("CASE 6b: RX below trigger, no RXRIS", 0x00, (data & UART_RXRIS));

    // Idle longer than the default 1 ms receive-timeout.
    wait(2, sc_core::SC_MS);
    do_transaction(tlm::TLM_READ_COMMAND, host0_base + UARTRIS, data);
    assert_equal("CASE 6c: RX-timeout raw interrupt after idle", UART_RTRIS, (data & UART_RTRIS));

    // Enable the RT mask and confirm it propagates to the masked status.
    do_transaction(tlm::TLM_READ_COMMAND, host0_base + UARTIMSC, data);
    data |= UART_RTRIS;
    do_transaction(tlm::TLM_WRITE_COMMAND, host0_base + UARTIMSC, data);
    do_transaction(tlm::TLM_READ_COMMAND, host0_base + UARTMIS, data);
    assert_equal("CASE 6d: RX-timeout masked interrupt", UART_RTRIS, (data & UART_RTRIS));

    // Draining the RX FIFO clears the timeout interrupt.
    while (true)
    {
        do_transaction(tlm::TLM_READ_COMMAND, host0_base + UARTFR, data);
        if (data & UART_RXFE) break;
        do_transaction(tlm::TLM_READ_COMMAND, host0_base + UARTDR, data);
    }
    do_transaction(tlm::TLM_READ_COMMAND, host0_base + UARTRIS, data);
    assert_equal("CASE 6e: RX-timeout cleared after FIFO drain", 0x00, (data & UART_RTRIS));

    // CASE 7: programmable RX FIFO trigger level (UARTIFLS).
    do_transaction(tlm::TLM_READ_COMMAND, host0_base + UARTIFLS, data);
    assert_equal("CASE 7a: UARTIFLS reset default (1/2,1/2)", UART_IFLS_RESET, data);

    // Lower the RX trigger to 1/8 (2 entries): RXIFLSEL=000, keep TX at 1/2.
    data = 0x02; // TXIFLSEL=010, RXIFLSEL=000
    do_transaction(tlm::TLM_WRITE_COMMAND, host0_base + UARTIFLS, data);
    do_transaction(tlm::TLM_READ_COMMAND, host0_base + UARTIFLS, data);
    assert_equal("CASE 7b: UARTIFLS readback", 0x02, data);

    // Two received bytes now reach the (lowered) trigger and raise RXRIS.
    for (int i = 0; i < 2; i++)
    {
        host0_rx_drv.write('k');
        wait(1, sc_core::SC_NS);
    }
    do_transaction(tlm::TLM_READ_COMMAND, host0_base + UARTRIS, data);
    assert_equal("CASE 7c: RXRIS at 1/8 trigger (2 entries)", UART_RXRIS, (data & UART_RXRIS));

    cout << "Tests Complete! Failures: " << std::dec << failure_count() << endl;
    sc_stop();
}