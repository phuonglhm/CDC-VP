#include <cstdint>

#include <systemc>
#include <tlm>

#include "bus_router.h"
#include "memory_tlm.h"
#include "tlm_probe.h"

int sc_main(int, char*[])
{
    cdc::components::bus_router bus("bus", 2);
    cdc::components::memory_tlm m0("m0", 0x100);
    cdc::components::memory_tlm m1("m1", 0x100);

    bus.add_target(0x1000, 0x100).bind(m0.socket);
    bus.add_target(0x2000, 0x100).bind(m1.socket);

    cdc::test::tlm_probe probe("probe");
    probe.socket.bind(bus.target_socket);

    sc_core::sc_spawn([&] {
        const std::uint32_t a = 0x11111111U;
        const std::uint32_t b = 0x22222222U;

        // Writes routed to the two regions.
        CDC_CHECK(probe.write(0x1004, &a, 4) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK(probe.write(0x2008, &b, 4) == tlm::TLM_OK_RESPONSE);

        // Read back through the router.
        std::uint32_t rb = 0;
        CDC_CHECK(probe.read(0x1004, &rb, 4) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK(rb == a);
        CDC_CHECK(probe.read(0x2008, &rb, 4) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK(rb == b);

        // Translation isolation: offset 0x4 holds 'a', offset 0x0 is untouched,
        // and the same local offset in the other region is independent.
        rb = 0xDEADBEEF;
        CDC_CHECK(probe.read(0x1000, &rb, 4) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK(rb == 0);
        rb = 0xDEADBEEF;
        CDC_CHECK(probe.read(0x2004, &rb, 4) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK(rb == 0);

        // Unmapped address -> error.
        CDC_CHECK(probe.read(0x3000, &rb, 4) == tlm::TLM_ADDRESS_ERROR_RESPONSE);

        sc_core::sc_stop();
    });

    sc_core::sc_start();
    return cdc::test::failures() == 0 ? 0 : 1;
}
