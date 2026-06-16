#include <cstdint>

#include <systemc>
#include <tlm>

#include "memory_tlm.h"
#include "tlm_probe.h"

int sc_main(int, char*[])
{
    cdc::components::memory_tlm ram("ram", 0x100);
    cdc::components::memory_tlm rom("rom", 0x100, /*read_only=*/true);

    cdc::test::tlm_probe ram_probe("ram_probe");
    cdc::test::tlm_probe rom_probe("rom_probe");
    ram_probe.socket.bind(ram.socket);
    rom_probe.socket.bind(rom.socket);

    sc_core::sc_spawn([&] {
        // RAM write -> read roundtrip.
        const std::uint32_t pattern = 0xCAFEBABEU;
        CDC_CHECK(ram_probe.write(0x10, &pattern, 4) == tlm::TLM_OK_RESPONSE);
        std::uint32_t rb = 0;
        CDC_CHECK(ram_probe.read(0x10, &rb, 4) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK(rb == pattern);

        // Out-of-bounds access -> address error (addr == size).
        CDC_CHECK(ram_probe.read(0x100, &rb, 4) == tlm::TLM_ADDRESS_ERROR_RESPONSE);

        // ROM rejects timed writes, allows reads.
        CDC_CHECK(rom_probe.write(0x0, &pattern, 4) == tlm::TLM_COMMAND_ERROR_RESPONSE);
        CDC_CHECK(rom_probe.read(0x0, &rb, 4) == tlm::TLM_OK_RESPONSE);

        // Backdoor (transport_dbg) write + read on RAM is untimed and works.
        const std::uint32_t dbg_val = 0x12345678U;
        CDC_CHECK(ram_probe.debug(tlm::TLM_WRITE_COMMAND, 0x20,
                                  const_cast<std::uint32_t*>(&dbg_val), 4) == 4);
        std::uint32_t dbg_rb = 0;
        CDC_CHECK(ram_probe.debug(tlm::TLM_READ_COMMAND, 0x20, &dbg_rb, 4) == 4);
        CDC_CHECK(dbg_rb == dbg_val);

        sc_core::sc_stop();
    });

    sc_core::sc_start();
    return cdc::test::failures() == 0 ? 0 : 1;
}
