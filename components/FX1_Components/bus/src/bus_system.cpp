#include "bus/bus_system.h"

namespace bus {
namespace {
const BusConfig& checked_config(const BusConfig& cfg) {
    cfg.validate();
    return cfg;
}
}
BusSystem::BusSystem(sc_core::sc_module_name name, bool trace)
    : BusSystem(name, BusConfig{}, trace) {}

BusSystem::BusSystem(sc_core::sc_module_name name, const BusConfig& cfg, bool trace)
    : sc_module(name),
      sysbus1("SYSBUS_1", checked_config(cfg).sysbus1, 5, trace),
      sysbus0("SYSBUS_0", cfg.sysbus0, 3, trace),
      peribus1("PERIBUS_1", cfg.peribus1, cfg.pp1_ports, trace),
      peribus0("PERIBUS_0", cfg.peribus0, cfg.pp0_ports, trace),
      a2p_pp1("A2P_PP1"), a2p_pp0("A2P_PP0"), target(sysbus1.target),
      rom("rom"), isram("isram"), dsram("dsram"), aes("aes"), qspi("qspi"),
      pp1("pp1", cfg.pp1_ports), pp0("pp0", cfg.pp0_ports) {
    // Binding order is the route port index in config.h.
    sysbus1.out.bind(rom.input);
    sysbus1.out.bind(isram.input);
    sysbus1.out.bind(dsram.input);
    sysbus1.out.bind(a2p_pp1.target);
    sysbus1.out.bind(sysbus0.target);
    sysbus0.out.bind(aes.input);
    sysbus0.out.bind(qspi.input);
    sysbus0.out.bind(a2p_pp0.target);
    a2p_pp1.out.bind(peribus1.target);
    a2p_pp0.out.bind(peribus0.target);
    for (unsigned i = 0; i < cfg.pp1_ports; ++i) {
        peribus1.out.bind(pp1[i].input);
    }
    for (unsigned i = 0; i < cfg.pp0_ports; ++i) {
        peribus0.out.bind(pp0[i].input);
    }
}
} // namespace bus
