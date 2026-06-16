#include <sstream>

#include <systemc>
#include <tlm>

#include "tlm_probe.h"
#include "uart_tlm.h"

int sc_main(int, char*[])
{
    std::ostringstream captured;
    cdc::components::uart_tlm uart("uart", captured);
    cdc::test::tlm_probe probe("probe");
    probe.socket.bind(uart.socket);

    sc_core::sc_spawn([&] {
        // TXDATA write emits the byte.
        const unsigned char ch = 'A';
        CDC_CHECK(probe.write(0x0, &ch, 1) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK(captured.str() == "A");

        const unsigned char ch2 = 'Z';
        CDC_CHECK(probe.write(0x0, &ch2, 1) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK(captured.str() == "AZ");

        // Reads and non-zero offsets are address errors.
        unsigned char rb = 0;
        CDC_CHECK(probe.read(0x0, &rb, 1) == tlm::TLM_ADDRESS_ERROR_RESPONSE);
        CDC_CHECK(probe.write(0x4, &ch, 1) == tlm::TLM_ADDRESS_ERROR_RESPONSE);

        sc_core::sc_stop();
    });

    sc_core::sc_start();
    return cdc::test::failures() == 0 ? 0 : 1;
}
