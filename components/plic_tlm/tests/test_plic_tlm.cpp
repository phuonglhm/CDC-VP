#include <cstdint>
#include <string>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include "memory_tlm.h"
#include "plic_tlm.h"
#include "tlm_probe.h"

namespace {

struct mock_cpu : public cdc::cpu::cpu_base {
    tlm_utils::simple_initiator_socket<mock_cpu> isock;
    bool meip = false;

    explicit mock_cpu(sc_core::sc_module_name name)
        : cpu_base(name, cdc::cpu::cpu_config{}), isock("isock") {}

    tlm::tlm_initiator_socket<>& instr_bus() override { return isock; }
    tlm::tlm_initiator_socket<>& data_bus() override { return isock; }
    void load_elf(const std::string&) override {}
    void reset_cpu() override {}
    std::uint64_t get_pc() const override { return 0; }
    std::string backend_name() const override { return "mock"; }

    void set_irq(unsigned cause, bool level) override
    {
        if (cause == 11) {
            meip = level;
        }
    }
};

// PLIC register offsets (context 0).
constexpr std::uint64_t PRIORITY1 = 0x4;
constexpr std::uint64_t ENABLE = 0x2000;
constexpr std::uint64_t THRESHOLD = 0x200000;
constexpr std::uint64_t CLAIM = 0x200004;

} // namespace

int sc_main(int, char*[])
{
    mock_cpu cpu("cpu");
    cdc::components::memory_tlm dummy("dummy", 0x100);
    cpu.isock.bind(dummy.socket);

    cdc::components::plic_tlm plic("plic", cpu, /*num_sources=*/1);
    sc_core::sc_signal<bool> src("src");
    plic.irq_in[0](src);

    cdc::test::tlm_probe probe("probe");
    probe.socket.bind(plic.socket);

    sc_core::sc_spawn([&] {
        // Configure: source 1 priority=1, enabled, threshold 0.
        std::uint32_t v = 1;
        probe.write(PRIORITY1, &v, 4);
        v = (1U << 1);
        probe.write(ENABLE, &v, 4);
        v = 0;
        probe.write(THRESHOLD, &v, 4);

        wait(sc_core::SC_ZERO_TIME);
        CDC_CHECK(cpu.meip == false);  // no source asserted yet

        // Assert the source -> external interrupt pending.
        src.write(true);
        wait(sc_core::SC_ZERO_TIME);
        CDC_CHECK(cpu.meip == true);

        // Claim -> returns source id 1, and (gateway) drops MEIP.
        std::uint32_t id = 0;
        CDC_CHECK(probe.read(CLAIM, &id, 4) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK(id == 1);
        wait(sc_core::SC_ZERO_TIME);
        CDC_CHECK(cpu.meip == false);

        // Source de-asserts, then complete -> stays low.
        src.write(false);
        wait(sc_core::SC_ZERO_TIME);
        std::uint32_t done = 1;
        probe.write(CLAIM, &done, 4);
        wait(sc_core::SC_ZERO_TIME);
        CDC_CHECK(cpu.meip == false);

        sc_core::sc_stop();
    });

    sc_core::sc_start();
    return cdc::test::failures() == 0 ? 0 : 1;
}
