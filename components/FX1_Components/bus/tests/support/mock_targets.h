#pragma once
#include "memory.h"
#include "bus/bus_system.h"
#include <array>
#include <memory>
#include <string>

namespace bus::test {
constexpr unsigned MEMORY_NS = 10, REGISTER_NS = 5; // Test IP latency only.
// Test-only SoC targets. The reusable bus library neither owns nor links these.
class MockTargets : public sc_core::sc_module {
public:
    Memory rom, isram, dsram, aes, qspi;
    std::array<std::unique_ptr<Memory>, config::PERIPHERALS> pp1, pp0;
    MockTargets(sc_core::sc_module_name name, BusSystem& fabric)
        : sc_module(name),
          rom("ROM", 0x10000, true, MEMORY_NS),
          isram("ISRAM", 0x20000, false, MEMORY_NS),
          dsram("DSRAM", 0x20000, false, MEMORY_NS),
          aes("AES_registers", 0x10000, false, REGISTER_NS),
          qspi("QSPI_storage", 0x1000000, false, MEMORY_NS) {
        fabric.rom.socket.bind(rom.target);
        fabric.isram.socket.bind(isram.target);
        fabric.dsram.socket.bind(dsram.target);
        fabric.aes.socket.bind(aes.target);
        fabric.qspi.socket.bind(qspi.target);
        for (unsigned i = 0; i < config::PERIPHERALS; ++i) {
            const auto n1 = "PP1_CFG_" + std::to_string(i);
            const auto n0 = "PP0_CFG_" + std::to_string(i);
            pp1[i] = std::make_unique<Memory>(n1.c_str(), config::PERIPHERAL_SIZE, false, REGISTER_NS);
            pp0[i] = std::make_unique<Memory>(n0.c_str(), config::PERIPHERAL_SIZE, false, REGISTER_NS);
            fabric.pp1[i].socket.bind(pp1[i]->target);
            fabric.pp0[i].socket.bind(pp0[i]->target);
        }
    }
};
} // namespace bus::test
