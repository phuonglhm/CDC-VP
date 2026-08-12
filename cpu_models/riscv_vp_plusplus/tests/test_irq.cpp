// SPDX-License-Identifier: Apache-2.0
//
// Interrupt gate: `cdc::cpu::cpu_base::set_irq()` on the VP++ backend.
//
// The method was implemented in Phase 2 and never executed by a test, so
// nothing established that an interrupt is delivered at all, let alone with the
// right cause or that lowering the line stops it.
//
// The host side is driven from the bus rather than from a timer. The firmware
// writes the line it wants raised; this harness raises it and echoes it back.
// Under temporal decoupling a host process that simply waited would be racing
// the core's quantum, and a lost race looks exactly like a dead interrupt line
// — a flaky test that gets "fixed" by lengthening a timeout.
//
// Usage: test_irq <rvv_irq.elf>
// Exit codes: 0 pass, 1 fail, 77 skip (image not built).

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include <cdc/cpu/cpu_base.h>

#include "riscv_vp_plusplus_wrapper.h"

extern "C" {
#include "sim_exit.h"
}

namespace {

constexpr std::uint64_t kMemorySize = 1024 * 1024;
constexpr int kSkip = 77;

int failures = 0;

#define CHECK(cond, what)                                                     \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::cerr << "FAIL: " << (what) << "  [" #cond " @ " << __LINE__   \
                      << "]\n";                                               \
            ++failures;                                                       \
        }                                                                     \
    } while (0)

class irq_memory : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<irq_memory> tsock;
    std::vector<unsigned char> storage;

    cdc::cpu::cpu_base* cpu = nullptr;

    /// Which line is currently asserted, as this harness believes it. Compared
    /// against what the firmware observes, so a wrapper that dropped an assert
    /// or a clear shows up as a disagreement rather than as a hang.
    std::uint32_t asserted = 0;
    unsigned long raises = 0;
    unsigned long clears = 0;

    bool exited = false;
    std::uint32_t exit_kind = 0;
    std::uint32_t exit_status = 0xffff'ffff;

    irq_memory(sc_core::sc_module_name name, std::size_t bytes)
        : sc_core::sc_module(name)
        , tsock("tsock")
        , storage(bytes, 0)
    {
        tsock.register_b_transport(this, &irq_memory::b_transport);
        tsock.register_transport_dbg(this, &irq_memory::transport_dbg);
    }

    std::uint32_t load_word(std::uint64_t address) const
    {
        std::uint32_t value = 0;
        std::memcpy(&value, storage.data() + address, sizeof(value));
        return value;
    }

    void store_word(std::uint64_t address, std::uint32_t value)
    {
        std::memcpy(storage.data() + address, &value, sizeof(value));
    }

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time&)
    {
        const auto address = trans.get_address();
        const auto length = trans.get_data_length();

        if (address >= storage.size() || length > storage.size() - address) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
        if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
            std::memcpy(storage.data() + address, trans.get_data_ptr(), length);
            observe_write(address);
        } else {
            std::memcpy(trans.get_data_ptr(), storage.data() + address, length);
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }

    unsigned int transport_dbg(tlm::tlm_generic_payload& trans)
    {
        const auto address = trans.get_address();
        const auto length = trans.get_data_length();
        if (address >= storage.size() || length > storage.size() - address) {
            return 0;
        }
        if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
            std::memcpy(storage.data() + address, trans.get_data_ptr(), length);
        } else {
            std::memcpy(trans.get_data_ptr(), storage.data() + address, length);
        }
        return length;
    }

    void observe_write(std::uint64_t address)
    {
        if (address == SIM_IRQ_REQUEST) {
            const std::uint32_t wanted = load_word(SIM_IRQ_REQUEST);
            if (wanted == asserted) {
                return;
            }
            if (asserted != 0) {
                cpu->set_irq(asserted, false);
                ++clears;
                asserted = 0;
            }
            if (wanted != 0) {
                cpu->set_irq(wanted, true);
                ++raises;
                asserted = wanted;
            }
            // Echoed only after the line has actually moved, so the firmware's
            // wait is on the state of the model rather than on elapsed time.
            store_word(SIM_IRQ_ACK, asserted);
            return;
        }
        if (address == SIM_EXIT_KIND) {
            exit_kind = load_word(SIM_EXIT_KIND);
            exit_status = load_word(SIM_EXIT_STATUS);
            exited = true;
            sc_core::sc_stop();
        }
    }
};

}  // namespace

int sc_main(int argc, char* argv[])
{
    if (argc != 2) {
        std::cerr << "usage: test_irq <rvv_irq.elf>\n";
        return 2;
    }
    const std::string elf_path = argv[1];
    {
        std::ifstream probe(elf_path, std::ios::binary);
        if (!probe) {
            std::cerr << "SKIP: " << elf_path << " was not built\n";
            return kSkip;
        }
    }

    irq_memory mem("mem", kMemorySize);

    cdc::cpu::cpu_config config;
    config.hart_id = 0;
    cdc::cpu::riscv_vp_plusplus_cpu cpu("core", config);
    mem.cpu = &cpu;
    cpu.data_bus().bind(mem.tsock);
    cpu.load_elf(elf_path);

    sc_core::sc_start(sc_core::sc_time(20, sc_core::SC_MS));

    const auto w = [&](std::uint64_t a) { return mem.load_word(a); };

    std::cout << "interrupt gate\n"
              << "  lines raised / cleared : " << mem.raises << " / "
              << mem.clears << '\n'
              << "  software : mcause 0x" << std::hex << w(SIM_IRQ_SOFTWARE_MCAUSE)
              << std::dec << "  taken " << w(SIM_IRQ_SOFTWARE_COUNT) << '\n'
              << "  timer    : mcause 0x" << std::hex << w(SIM_IRQ_TIMER_MCAUSE)
              << std::dec << "  taken " << w(SIM_IRQ_TIMER_COUNT) << '\n'
              << "  external : mcause 0x" << std::hex << w(SIM_IRQ_EXTERNAL_MCAUSE)
              << std::dec << "  taken " << w(SIM_IRQ_EXTERNAL_COUNT) << '\n'
              << "  taken while masked in mie : " << w(SIM_IRQ_MASKED_COUNT)
              << " (must be 0)\n"
              << "  total entries : " << w(SIM_IRQ_TOTAL) << '\n'
              << "  instructions retired : " << cpu.get_instret() << '\n'
              << "  last sampled mstatus / mie / mip : 0x" << std::hex
              << w(SIM_IRQ_MSTATUS) << " / 0x" << w(SIM_IRQ_MIE) << " / 0x"
              << w(SIM_IRQ_MIP) << std::dec << '\n';

    if (!mem.exited) {
        std::cerr << "FAIL: the image never signalled exit; PC 0x" << std::hex
                  << cpu.get_pc() << std::dec
                  << ". An interrupt that is never delivered looks like this.\n";
        return 1;
    }
    if (mem.exit_kind == SIM_EXIT_KIND_TRAPPED) {
        std::cerr << "FAIL: unhandled trap\n";
        return 1;
    }
    if (mem.exit_status != SIM_EXIT_PASS) {
        std::cerr << "FAIL: interrupt check id " << mem.exit_status
                  << " failed (see enum irq_check_id in irq_main.c)\n";
        return 1;
    }

    CHECK(w(SIM_IRQ_SOFTWARE_MCAUSE) == 0x8000'0003u,
          "machine software interrupt reported the wrong mcause");
    CHECK(w(SIM_IRQ_TIMER_MCAUSE) == 0x8000'0007u,
          "machine timer interrupt reported the wrong mcause");
    CHECK(w(SIM_IRQ_EXTERNAL_MCAUSE) == 0x8000'000bu,
          "machine external interrupt reported the wrong mcause");
    CHECK(w(SIM_IRQ_SOFTWARE_COUNT) > 0, "no machine software interrupt taken");
    CHECK(w(SIM_IRQ_TIMER_COUNT) > 0, "no machine timer interrupt taken");
    CHECK(w(SIM_IRQ_EXTERNAL_COUNT) > 0, "no machine external interrupt taken");
    // Without this the three above would also pass on a model that ignored
    // `mie` and delivered whatever the platform asserted.
    CHECK(w(SIM_IRQ_MASKED_COUNT) == 0,
          "an interrupt was taken with its mie bit clear");

    // Four raises: three lines plus the masked probe. Three clears for the
    // three that were lowered while the firmware watched, and the harness
    // lowers the masked one at the end too.
    CHECK(mem.raises == 4, "the harness did not raise each line exactly once");
    CHECK(mem.clears == 4, "the harness did not lower each line exactly once");
    CHECK(mem.asserted == 0, "a line was left asserted at exit");

    if (failures != 0) {
        std::cerr << '\n' << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "\ntest_irq: software, timer and external interrupts are "
                 "delivered, carry the right cause, respect mie, and stop when "
                 "the line is lowered\n";
    return 0;
}
