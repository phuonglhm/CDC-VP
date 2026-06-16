#include <cstdint>
#include <string>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include "clint_tlm.h"
#include "memory_tlm.h"
#include "tlm_probe.h"

namespace {

// Minimal cpu_base that just records set_irq() calls.
struct mock_cpu : public cdc::cpu::cpu_base {
    tlm_utils::simple_initiator_socket<mock_cpu> isock;
    int timer_asserts = 0;
    int timer_deasserts = 0;
    int sw_asserts = 0;

    explicit mock_cpu(sc_core::sc_module_name name)
        : cpu_base(name, cdc::cpu::cpu_config{})
        , isock("isock")
    {
    }

    tlm::tlm_initiator_socket<>& instr_bus() override { return isock; }
    tlm::tlm_initiator_socket<>& data_bus() override { return isock; }
    void load_elf(const std::string&) override {}
    void reset_cpu() override {}
    std::uint64_t get_pc() const override { return 0; }
    std::string backend_name() const override { return "mock"; }

    void set_irq(unsigned cause, bool level) override
    {
        if (cause == 7) {
            level ? ++timer_asserts : ++timer_deasserts;
        } else if (cause == 3 && level) {
            ++sw_asserts;
        }
    }
};

} // namespace

int sc_main(int, char*[])
{
    mock_cpu cpu("cpu");
    cdc::components::memory_tlm dummy("dummy", 0x100);
    cpu.isock.bind(dummy.socket);  // satisfy the mock's bus binding

    cdc::components::clint_tlm clint("clint", cpu);
    cdc::test::tlm_probe probe("probe");
    probe.socket.bind(clint.socket);

    sc_core::sc_spawn([&] {
        // Arm the timer: mtimecmp = 100 us (mtime starts near 0).
        std::uint64_t cmp = 100;
        CDC_CHECK(probe.write(0x4000, &cmp, 8) == tlm::TLM_OK_RESPONSE);

        // Before the compare time, the timer must not be asserted.
        wait(sc_core::sc_time(50, sc_core::SC_US));
        CDC_CHECK(cpu.timer_asserts == 0);

        // After the compare time, the timer fires (MTIP asserted).
        wait(sc_core::sc_time(100, sc_core::SC_US));
        CDC_CHECK(cpu.timer_asserts >= 1);

        // mtime reads back >= mtimecmp.
        std::uint64_t t = 0;
        CDC_CHECK(probe.read(0xBFF8, &t, 8) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK(t >= cmp);

        // Software interrupt via msip bit0.
        std::uint32_t one = 1;
        CDC_CHECK(probe.write(0x0, &one, 4) == tlm::TLM_OK_RESPONSE);
        CDC_CHECK(cpu.sw_asserts == 1);

        sc_core::sc_stop();
    });

    sc_core::sc_start();
    return cdc::test::failures() == 0 ? 0 : 1;
}
