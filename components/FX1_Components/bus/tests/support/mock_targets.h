#pragma once
#include "bus/bus_system.h"
#include "memory.h"

namespace bus::test {
// Default FX1 targets used only by tests. Production BusSystem never creates IP models.
struct MockTargets : sc_core::sc_module {
    Memory bootrom, clint, plic, uart, memctl_ddr;
    MockTargets(sc_core::sc_module_name name, BusSystem& fabric)
        : sc_module(name), bootrom("bootrom",4096,true), clint("clint",4096),
          plic("plic",4096), uart("uart",4096), memctl_ddr("memctl_ddr",4096) {
        fabric.target("BootROM").bind(bootrom.target);
        fabric.target("CLINT").bind(clint.target);
        fabric.target("PLIC").bind(plic.target);
        fabric.target("UART").bind(uart.target);
        fabric.target("MEMCTL_DDR").bind(memctl_ddr.target);
    }
};
} // namespace bus::test
