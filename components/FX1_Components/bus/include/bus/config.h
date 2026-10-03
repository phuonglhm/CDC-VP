#pragma once
#include <cstdint>
#include <vector>
#include <string>

namespace bus {
struct Region {
    std::string name;
    std::uint64_t begin;
    std::uint64_t end; // exclusive
    unsigned port;
    bool translate; // only leaf targets receive local offsets
};

// EXAMPLE map and timings: the workbook does not specify numeric values.
namespace config {
constexpr std::uint64_t ROM = 0x00000000, ISRAM = 0x10000000, DSRAM = 0x20000000;
constexpr std::uint64_t PP1 = 0x40000000, SB0 = 0x50000000;
constexpr std::uint64_t AES = 0x50000000, QSPI = 0x51000000, PP0 = 0x52000000;
constexpr unsigned PERIPHERALS = 4;
constexpr std::uint64_t PERIPHERAL_SIZE = 0x1000;
constexpr unsigned ROUTER_NS = 2, APB_CYCLE_NS = 10;
inline std::vector<Region> sysbus1_map() {
    return {{"ROM", ROM, ROM+0x10000, 0, true},
            {"ISRAM", ISRAM, ISRAM+0x20000, 1, true},
            {"DSRAM", DSRAM, DSRAM+0x20000, 2, true},
            {"PP1", PP1, PP1+0x10000, 3, false},
            {"SB0", SB0, 0x60000000, 4, false}};
}
inline std::vector<Region> sysbus0_map() {
    return {{"AES", AES, AES+0x10000, 0, true},
            {"QSPI", QSPI, QSPI+0x1000000, 1, true},
            {"PP0", PP0, PP0+0x10000, 2, false}};
}
inline std::vector<Region> peribus_map(std::uint64_t base) {
    std::vector<Region> result;
    for (unsigned i = 0; i < PERIPHERALS; ++i)
        result.push_back({"PP/CFG", base+i*PERIPHERAL_SIZE,
                          base+(i+1)*PERIPHERAL_SIZE, i, true});
    return result;
}
} // namespace config

// Platform-owned configuration. Port indices are topology, addresses are policy:
// SYSBUS1: ROM=0, ISRAM=1, DSRAM=2, PP1=3, SB0=4.
// SYSBUS0: AES=0, QSPI=1, PP0=2. PERIBUS: external pp[i].
struct BusConfig {
    std::vector<Region> sysbus1 = config::sysbus1_map();
    std::vector<Region> sysbus0 = config::sysbus0_map();
    std::vector<Region> peribus1 = config::peribus_map(config::PP1);
    std::vector<Region> peribus0 = config::peribus_map(config::PP0);
    unsigned pp1_ports = config::PERIPHERALS;
    unsigned pp0_ports = config::PERIPHERALS;
    // Throws invalid_argument for overlapping, unreachable or misrouted regions.
    void validate() const;
};
} // namespace bus
